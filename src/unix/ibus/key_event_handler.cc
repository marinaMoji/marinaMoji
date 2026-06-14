// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "unix/ibus/key_event_handler.h"

#include <cstddef>

#include "absl/log/check.h"
#include "absl/log/log.h"

namespace mozc {
namespace ibus {

namespace {
bool IsModifierToBeSentOnKeyUp(const commands::KeyEvent& key_event) {
  if (key_event.modifier_keys_size() == 0) {
    return false;
  }

  if (key_event.modifier_keys_size() == 1 &&
      key_event.modifier_keys(0) == commands::KeyEvent::CAPS) {
    return false;
  }

  return true;
}

bool IsControlKeyval(uint keyval) {
  return keyval == IBUS_Control_L || keyval == IBUS_Control_R;
}

bool HasNonShiftNonCtrlModifierKeyval(const std::set<uint>& pressed) {
  for (uint keyval : pressed) {
    if (keyval != IBUS_Shift_L && keyval != IBUS_Shift_R &&
        keyval != IBUS_Control_L && keyval != IBUS_Control_R) {
      return true;
    }
  }
  return false;
}

void SetCtrlLeftShiftModeLockKey(commands::KeyEvent* key_event) {
  key_event->Clear();
  key_event->add_modifier_keys(commands::KeyEvent::CTRL);
  key_event->add_modifier_keys(commands::KeyEvent::LEFT_SHIFT);
}

// Editing/navigation keys must work even when IBus spuriously sets Super
// (Mod4), which otherwise makes GetKeyEvent reject all keys (issue #853).
bool IsEditingKeyval(uint keyval) {
  return keyval == IBUS_BackSpace || keyval == IBUS_Delete ||
         keyval == IBUS_Escape || keyval == IBUS_Return ||
         keyval == IBUS_Tab || keyval == IBUS_Left || keyval == IBUS_Right ||
         keyval == IBUS_Up || keyval == IBUS_Down || keyval == IBUS_Home ||
         keyval == IBUS_End;
}

bool IsEditingKeycode(uint keycode) {
  // Physical Backspace key (X11 keycode 14) even when keyval is nonstandard.
  constexpr uint kBackSpaceKeycode = 14;
  return keycode == kBackSpaceKeycode;
}
}  // namespace

KeyEventHandler::KeyEventHandler() : key_translator_(new KeyTranslator) {
  Clear();
}

bool KeyEventHandler::GetKeyEvent(uint keyval, uint keycode, uint modifiers,
                                  config::Config::PreeditMethod preedit_method,
                                  bool layout_is_jp, commands::KeyEvent* key) {
  DCHECK(key);
  key->Clear();

  // Ignore key events with modifiers, except for the below;
  // - Alt (Mod1) - Mozc uses Alt for shortcuts
  // - NumLock (Mod2) - NumLock shouldn't impact shortcuts
  // - Mod3/Mod5 (AltGr, Level3) - used for RightAlt+key on AZERTY etc.; we map to RIGHT_ALT in KeyTranslator
  // This is needed for handling shortcuts such as Super (Mod4) + Space,
  // IBus's default for switching input methods.
  // https://github.com/google/mozc/issues/853
  constexpr uint kExtraModMask = IBUS_MOD4_MASK;
  uint effective_modifiers = modifiers;
  if (IsEditingKeyval(keyval) || IsEditingKeycode(keycode)) {
    effective_modifiers &= ~kExtraModMask;
  }
  if (effective_modifiers & kExtraModMask) {
    return false;
  }

  if (!key_translator_->Translate(keyval, keycode, effective_modifiers,
                                  preedit_method, layout_is_jp, key)) {
    LOG(ERROR) << "Translate failed";
    return false;
  }

  // On Dvorak etc., Right Alt often sends MOD1 only (not MOD3/MOD5). So
  // Right Alt + vowel would be ALT+vowel, not RIGHT_ALT+vowel. If the physical
  // Right Alt is down (we saw Alt_R in a previous event), treat MOD1 as RIGHT_ALT
  // for character keys so keymap "Ctrl RightAlt a" (macron) matches.
  if (key->has_key_code() && (modifiers & IBUS_MOD1_MASK) &&
      currently_pressed_modifiers_.count(IBUS_Alt_R) != 0) {
    key->add_modifier_keys(commands::KeyEvent::RIGHT_ALT);
  }

  const bool is_key_up = ((effective_modifiers & IBUS_RELEASE_MASK) != 0);
  return ProcessModifiers(is_key_up, keyval, key);
}

void KeyEventHandler::Clear() {
  is_non_modifier_key_pressed_ = false;
  currently_pressed_modifiers_.clear();
  modifiers_to_be_sent_.clear();
  left_shift_in_chord_ = false;
  ctrl_physically_down_ = false;
  ctrl_left_shift_chord_armed_ = false;
  typed_during_ctrl_left_shift_chord_ = false;
  ctrl_held_during_left_shift_press_ = false;
}

bool KeyEventHandler::ProcessModifiers(bool is_key_up, uint keyval,
                                       commands::KeyEvent* key_event) {
  // Manage modifier key event.
  // Modifier key event is sent on key up if non-modifier key has not been
  // pressed since key down of modifier keys and no modifier keys are pressed
  // anymore.
  // Following examples are expected behaviors.
  //
  // E.g.) Shift key is special. If Shift + printable key is pressed, key event
  //       does NOT have shift modifiers. It is handled by KeyTranslator class.
  //    <Event from ibus> <Event to server>
  //     Shift down      | None
  //     "a" down        | A
  //     "a" up          | None
  //     Shift up        | None
  //
  // E.g.) Usual key is sent on key down.  Modifier keys are not sent if usual
  //       key is sent.
  //    <Event from ibus> <Event to server>
  //     Ctrl down       | None
  //     "a" down        | Ctrl+a
  //     "a" up          | None
  //     Ctrl up         | None
  //
  // E.g.) Modifier key is sent on key up.
  //    <Event from ibus> <Event to server>
  //     Shift down      | None
  //     Shift up        | Shift
  //
  // E.g.) Multiple modifier keys are sent on the last key up.
  //    <Event from ibus> <Event to server>
  //     Shift down      | None
  //     Control down    | None
  //     Shift up        | None
  //     Control up      | Control+Shift
  //
  // Essentialy we cannot handle modifier key evnet perfectly because
  // - We cannot get current keyboard status with ibus. If some modifiers
  //   are pressed or released without focusing the target window, we
  //   cannot handle it.
  // E.g.)
  //    <Event from ibus> <Event to server>
  //     Ctrl down       | None
  //     (focuses out, Ctrl up, focuses in)
  //     Shift down      | None
  //     Shift up        | None (But we should send Shift key)
  // To avoid a inconsistent state as much as possible, we clear states
  // when key event without modifier keys is sent.

  const bool is_modifier_only =
      !(key_event->has_key_code() || key_event->has_special_key());

  // We may get only up/down key event when a user moves a focus.
  // This code handles such situation as much as possible.
  // This code has a bug. If we send Shift + 'a', KeyTranslator removes a shift
  // modifier and converts 'a' to 'A'. This codes does NOT consider these
  // situation since we don't have enough data to handle it.
  // TODO(hsumita): Moves the logic about a handling of Shift or Caps keys from
  // KeyTranslator to MozcEngine.
  if (key_event->modifier_keys_size() == 0) {
    Clear();
  }

  if (!currently_pressed_modifiers_.empty() && !is_modifier_only) {
    is_non_modifier_key_pressed_ = true;
    if (ctrl_left_shift_chord_armed_) {
      typed_during_ctrl_left_shift_chord_ = true;
      ctrl_left_shift_chord_armed_ = false;
    }
  }
  if (is_non_modifier_key_pressed_) {
    modifiers_to_be_sent_.clear();
  }

  if (is_key_up) {
    currently_pressed_modifiers_.erase(keyval);

    if (is_modifier_only &&
        ((keyval == IBUS_Shift_L &&
          ctrl_left_shift_chord_armed_ && !typed_during_ctrl_left_shift_chord_ &&
          !HasNonShiftNonCtrlModifierKeyval(currently_pressed_modifiers_)) ||
         (IsControlKeyval(keyval) &&
          ctrl_left_shift_chord_armed_ && !typed_during_ctrl_left_shift_chord_ &&
          !HasNonShiftNonCtrlModifierKeyval(currently_pressed_modifiers_)))) {
      SetCtrlLeftShiftModeLockKey(key_event);
      ctrl_left_shift_chord_armed_ = false;
      typed_during_ctrl_left_shift_chord_ = false;
      ctrl_physically_down_ = false;
      ctrl_held_during_left_shift_press_ = false;
      left_shift_in_chord_ = false;
      modifiers_to_be_sent_.clear();
      is_non_modifier_key_pressed_ = false;
      currently_pressed_modifiers_.clear();
      return true;
    }

    if (!is_modifier_only) {
      return false;
    }
    if (!currently_pressed_modifiers_.empty() ||
        modifiers_to_be_sent_.empty()) {
      is_non_modifier_key_pressed_ = false;
      if (IsControlKeyval(keyval)) {
        ctrl_physically_down_ = false;
      }
      if (keyval == IBUS_Shift_L) {
        left_shift_in_chord_ = false;
        ctrl_held_during_left_shift_press_ = false;
      }
      return false;
    }
    if (is_non_modifier_key_pressed_) {
      return false;
    }
    if (keyval == IBUS_Shift_L && ctrl_held_during_left_shift_press_) {
      left_shift_in_chord_ = false;
      ctrl_held_during_left_shift_press_ = false;
      modifiers_to_be_sent_.clear();
      return false;
    }
    DCHECK(!is_non_modifier_key_pressed_);

    // Modifier key event fires
    key_event->mutable_modifier_keys()->Clear();
    for (std::set<commands::KeyEvent::ModifierKey>::const_iterator it =
             modifiers_to_be_sent_.begin();
         it != modifiers_to_be_sent_.end(); ++it) {
      key_event->add_modifier_keys(*it);
    }
    if (left_shift_in_chord_) {
      key_event->add_modifier_keys(commands::KeyEvent::LEFT_SHIFT);
    }
    modifiers_to_be_sent_.clear();
    left_shift_in_chord_ = false;
    ctrl_held_during_left_shift_press_ = false;
  } else if (is_modifier_only) {
    if (keyval == IBUS_Shift_L) {
      left_shift_in_chord_ = true;
      ctrl_held_during_left_shift_press_ =
          ctrl_physically_down_ ||
          currently_pressed_modifiers_.count(IBUS_Control_L) != 0 ||
          currently_pressed_modifiers_.count(IBUS_Control_R) != 0;
      if (ctrl_held_during_left_shift_press_) {
        ctrl_left_shift_chord_armed_ = true;
      }
    }
    if (IsControlKeyval(keyval)) {
      ctrl_physically_down_ = true;
      if (left_shift_in_chord_ ||
          currently_pressed_modifiers_.count(IBUS_Shift_L) != 0) {
        ctrl_left_shift_chord_armed_ = true;
        ctrl_held_during_left_shift_press_ = true;
      }
    }
    // Right/Left Shift alone: send on key up so toggles fire only when released
    // by themselves, not when used to capitalize another key.
    // TODO(hsumita): Supports a key sequence below.
    // - Ctrl down
    // - a down
    // - Alt down
    // We should add Alt key to |currently_pressed_modifiers|, but current
    // implementation does NOT do it.
    if (currently_pressed_modifiers_.empty() ||
        !modifiers_to_be_sent_.empty()) {
      for (size_t i = 0; i < key_event->modifier_keys_size(); ++i) {
        modifiers_to_be_sent_.insert(key_event->modifier_keys(i));
      }
    }
    currently_pressed_modifiers_.insert(keyval);
    return false;
  }

  // Clear modifier data just in case if |key| has no modifier keys.
  if (!IsModifierToBeSentOnKeyUp(*key_event)) {
    Clear();
  }

  return true;
}

}  // namespace ibus
}  // namespace mozc
