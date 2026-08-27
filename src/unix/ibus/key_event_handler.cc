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
#include <map>
#include <utility>
#include <vector>

#include <ibus.h>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "unix/ibus/ibus_debug_log.h"
#include "unix/ibus/ibus_wrapper.h"

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

bool HasNonShiftNonCtrlModifierKeyval(const std::map<uint, uint>& pressed) {
  for (const auto& [keyval, keycode] : pressed) {
    if (keyval != IBUS_Shift_L && keyval != IBUS_Shift_R &&
        keyval != IBUS_Control_L && keyval != IBUS_Control_R) {
      return true;
    }
  }
  return false;
}

bool IsModifierKeyval(uint keyval) {
  switch (keyval) {
    case IBUS_Shift_L:
    case IBUS_Shift_R:
    case IBUS_Control_L:
    case IBUS_Control_R:
    case IBUS_Alt_L:
    case IBUS_Alt_R:
    case IBUS_Meta_L:
    case IBUS_Meta_R:
    case IBUS_Super_L:
    case IBUS_Super_R:
    case IBUS_Hyper_L:
    case IBUS_Hyper_R:
    case IBUS_Caps_Lock:
    case IBUS_Shift_Lock:
    case IBUS_ISO_Level3_Shift:
      return true;
    default:
      return false;
  }
}

bool HasModifier(const commands::KeyEvent& key_event,
                 commands::KeyEvent::ModifierKey modifier) {
  for (int i = 0; i < key_event.modifier_keys_size(); ++i) {
    if (key_event.modifier_keys(i) == modifier) {
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

// Standard PC evdev keycodes, used as fallback when a tracked entry was
// recorded without a keycode (KEY_LEFTSHIFT / KEY_RIGHTSHIFT / KEY_LEFTCTRL /
// KEY_RIGHTCTRL).
uint FallbackKeycode(uint keyval) {
  switch (keyval) {
    case IBUS_Shift_L:
      return 42;
    case IBUS_Shift_R:
      return 54;
    case IBUS_Control_L:
      return 29;
    case IBUS_Control_R:
      return 97;
    default:
      return 0;
  }
}

// A non-modifier key tracked as held for longer than this is not really held:
// its release was missed (focus change, grab, a press forwarded to an app that
// then moved focus). Forwarding a synthetic release for such an entry injects a
// stray key-up into whatever has focus at the next lifecycle event, so drop it
// instead. Comfortably longer than any deliberate hold that matters here, and
// far shorter than the multi-minute leaks this guards against.
const absl::Duration kMaxTrackedPressAge = absl::Seconds(2);

void ForwardRelease(IbusEngineWrapper* engine, uint keyval, uint keycode) {
  if (engine == nullptr) {
    return;
  }
  if (keycode == 0) {
    keycode = FallbackKeycode(keyval);
  }
  engine->ForwardKeyEvent(keyval, keycode, IBUS_RELEASE_MASK);
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

  const bool is_key_up_raw = ((modifiers & IBUS_RELEASE_MASK) != 0);

  // Track physically pressed non-modifier keys (Return, BackSpace, arrows,
  // ...) by hardware keycode so ForwardTrackedReleases can synthesize a
  // release if the real key-up never reaches us (focus change mid-hold). Keyed
  // on keycode, not keyval: keyval reflects the modifier state of each event
  // individually, so Shift + 'c' is tracked as 'C' on key-down and would be
  // looked up as 'c' on key-up whenever Shift is lifted first. The keycode is
  // the same for both. This bookkeeping is independent of the modifier state
  // machine below.
  if (!IsModifierKeyval(keyval)) {
    if (is_key_up_raw) {
      currently_pressed_non_modifiers_.erase(keycode);
    } else {
      currently_pressed_non_modifiers_[keycode] = PressedKey{
          .keyval = keyval, .press_time = Clock::GetAbslTime()};
    }
  }

  // The hardware modifier mask is authoritative: drop tracked Shift/Ctrl
  // whose release we missed, instead of waiting for a lifecycle event.
  ReconcileStaleModifiers(keyval, is_key_up_raw, modifiers);

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
    MaybeLogIbusDebug("key_handler.get",
                      "reject_mod4 keyval=%u keycode=%u modifiers=0x%x",
                      keyval, keycode, modifiers);
    return false;
  }

  if (!key_translator_->Translate(keyval, keycode, effective_modifiers,
                                  preedit_method, layout_is_jp, key)) {
    LOG(ERROR) << "Translate failed";
    MaybeLogIbusDebug("key_handler.get",
                      "translate_failed keyval=%u keycode=%u modifiers=0x%x "
                      "effective=0x%x",
                      keyval, keycode, modifiers, effective_modifiers);
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
  const bool send = ProcessModifiers(is_key_up, keyval, keycode, key);
  MaybeLogIbusDebug(
      "key_handler.get",
      "end keyval=%u keycode=%u modifiers=0x%x effective=0x%x key_up=%d "
      "send=%d has_key_code=%d has_special=%d mods=%d shift=%d "
      "left_shift=%d right_shift=%d pressed=%zu pending=%zu non_modifier=%d",
      keyval, keycode, modifiers, effective_modifiers, is_key_up, send,
      key->has_key_code(), key->has_special_key(), key->modifier_keys_size(),
      HasModifier(*key, commands::KeyEvent::SHIFT),
      HasModifier(*key, commands::KeyEvent::LEFT_SHIFT),
      HasModifier(*key, commands::KeyEvent::RIGHT_SHIFT),
      currently_pressed_modifiers_.size(), modifiers_to_be_sent_.size(),
      is_non_modifier_key_pressed_);
  return send;
}

void KeyEventHandler::Clear() {
  is_non_modifier_key_pressed_ = false;
  currently_pressed_modifiers_.clear();
  currently_pressed_non_modifiers_.clear();
  modifiers_to_be_sent_.clear();
  ResetChordState();
}

void KeyEventHandler::ResetChordState() {
  left_shift_in_chord_ = false;
  ctrl_left_shift_chord_armed_ = false;
  typed_during_ctrl_left_shift_chord_ = false;
  ctrl_held_during_left_shift_press_ = false;
}

bool KeyEventHandler::HasTrackedCtrl() const {
  return currently_pressed_modifiers_.count(IBUS_Control_L) != 0 ||
         currently_pressed_modifiers_.count(IBUS_Control_R) != 0;
}

bool KeyEventHandler::HasTrackedShift() const {
  return currently_pressed_modifiers_.count(IBUS_Shift_L) != 0 ||
         currently_pressed_modifiers_.count(IBUS_Shift_R) != 0;
}

std::vector<std::pair<uint, uint>> KeyEventHandler::TrackedReleaseKeys()
    const {
  std::vector<std::pair<uint, uint>> result;
  for (const auto& [keyval, keycode] : currently_pressed_modifiers_) {
    // Only Shift/Ctrl: a spurious release for these is harmless, and they are
    // the keys observed to stick. Alt/Super releases could interfere with WM
    // shortcuts, so they are only dropped from tracking via Clear().
    if (keyval == IBUS_Shift_L || keyval == IBUS_Shift_R ||
        IsControlKeyval(keyval)) {
      result.emplace_back(keyval, keycode);
    }
  }
  const absl::Time now = Clock::GetAbslTime();
  for (const auto& [keycode, pressed] : currently_pressed_non_modifiers_) {
    // Skip entries whose press is too old to be a real hold: forwarding a
    // release for those puts a stray key-up into the newly focused client.
    if (now - pressed.press_time > kMaxTrackedPressAge) {
      MaybeLogIbusDebug("key_handler.release",
                        "skip_stale_release keyval=%u keycode=%u age_ms=%d",
                        pressed.keyval, keycode,
                        static_cast<int>(absl::ToInt64Milliseconds(
                            now - pressed.press_time)));
      continue;
    }
    result.emplace_back(pressed.keyval, keycode);
  }
  return result;
}

void KeyEventHandler::NotifyKeyState(uint keyval, uint keycode,
                                     bool is_key_up) {
  if (IsModifierKeyval(keyval)) {
    return;
  }
  if (is_key_up) {
    currently_pressed_non_modifiers_.erase(keycode);
  } else {
    currently_pressed_non_modifiers_[keycode] =
        PressedKey{.keyval = keyval, .press_time = Clock::GetAbslTime()};
  }
}

void KeyEventHandler::ForwardTrackedReleases(IbusEngineWrapper* engine) {
  for (const auto& [keyval, keycode] : TrackedReleaseKeys()) {
    MaybeLogIbusDebug("key_handler.release",
                      "forward_tracked_release keyval=%u keycode=%u", keyval,
                      keycode);
    ForwardRelease(engine, keyval, keycode);
  }
}

void KeyEventHandler::ReconcileStaleModifiers(uint current_keyval,
                                              bool is_key_up, uint modifiers) {
  if (currently_pressed_modifiers_.empty()) {
    return;
  }
  std::vector<uint> stale;
  for (const auto& [keyval, keycode] : currently_pressed_modifiers_) {
    // On the key's own key-up the mask still includes its bit; on its own
    // key-down the mask predates the press, so an existing entry with a clear
    // bit is genuinely stale (a re-press after a missed release).
    if (keyval == current_keyval && is_key_up) {
      continue;
    }
    const bool is_shift = (keyval == IBUS_Shift_L || keyval == IBUS_Shift_R);
    if (is_shift && (modifiers & IBUS_SHIFT_MASK) == 0) {
      stale.push_back(keyval);
    } else if (IsControlKeyval(keyval) &&
               (modifiers & IBUS_CONTROL_MASK) == 0) {
      stale.push_back(keyval);
    }
  }
  for (uint keyval : stale) {
    MaybeLogIbusDebug("key_handler.mod",
                      "prune_stale_modifier keyval=%u modifiers=0x%x", keyval,
                      modifiers);
    currently_pressed_modifiers_.erase(keyval);
    if (keyval == IBUS_Shift_L) {
      modifiers_to_be_sent_.erase(commands::KeyEvent::LEFT_SHIFT);
      left_shift_in_chord_ = false;
      ctrl_held_during_left_shift_press_ = false;
      ctrl_left_shift_chord_armed_ = false;
    } else if (keyval == IBUS_Shift_R) {
      modifiers_to_be_sent_.erase(commands::KeyEvent::RIGHT_SHIFT);
    } else if (IsControlKeyval(keyval) && !HasTrackedCtrl()) {
      modifiers_to_be_sent_.erase(commands::KeyEvent::CTRL);
      ctrl_left_shift_chord_armed_ = false;
    }
    if (!HasTrackedShift()) {
      modifiers_to_be_sent_.erase(commands::KeyEvent::SHIFT);
    }
  }
}

bool KeyEventHandler::ProcessModifiers(bool is_key_up, uint keyval,
                                       uint keycode,
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

  MaybeLogIbusDebug(
      "key_handler.mod",
      "begin keyval=%u key_up=%d modifier_only=%d mods=%d shift=%d "
      "left_shift=%d right_shift=%d pressed=%zu pending=%zu non_modifier=%d "
      "ctrl_left_armed=%d typed_ctrl_left=%d",
      keyval, is_key_up, is_modifier_only, key_event->modifier_keys_size(),
      HasModifier(*key_event, commands::KeyEvent::SHIFT),
      HasModifier(*key_event, commands::KeyEvent::LEFT_SHIFT),
      HasModifier(*key_event, commands::KeyEvent::RIGHT_SHIFT),
      currently_pressed_modifiers_.size(), modifiers_to_be_sent_.size(),
      is_non_modifier_key_pressed_, ctrl_left_shift_chord_armed_,
      typed_during_ctrl_left_shift_chord_);

  // Upstream cleared ALL tracked state here whenever the translated event
  // carried zero modifier keys, as a resync hack for releases missed during
  // focus moves. That wiped valid state too: Shift + 'a' translates to 'A'
  // with no modifier keys, so the still-held Shift lost its tracking (and
  // with it the toggle-chord bookkeeping) on every capitalized keystroke.
  // Stale entries are now pruned per-key in ReconcileStaleModifiers using the
  // hardware modifier mask, so no blanket Clear() is needed here.

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
        (keyval == IBUS_Shift_L || IsControlKeyval(keyval)) &&
        ctrl_left_shift_chord_armed_ && !typed_during_ctrl_left_shift_chord_ &&
        !HasNonShiftNonCtrlModifierKeyval(currently_pressed_modifiers_)) {
      SetCtrlLeftShiftModeLockKey(key_event);
      ResetChordState();
      modifiers_to_be_sent_.clear();
      is_non_modifier_key_pressed_ = false;
      // Keys still physically held (e.g. the chord partner, or an unrelated
      // key like Return) stay tracked; their own key-ups will erase them.
      MaybeLogIbusDebug("key_handler.mod",
                        "return_ctrl_left_shift_lock keyval=%u", keyval);
      return true;
    }

    if (!is_modifier_only) {
      // With no modifiers tracked anymore the chord is over; reset the
      // transient bookkeeping that the removed blanket Clear() used to catch.
      if (currently_pressed_modifiers_.empty()) {
        is_non_modifier_key_pressed_ = false;
        modifiers_to_be_sent_.clear();
        ResetChordState();
      }
      MaybeLogIbusDebug("key_handler.mod",
                        "return_key_up_non_modifier keyval=%u", keyval);
      return false;
    }
    if (!currently_pressed_modifiers_.empty() ||
        modifiers_to_be_sent_.empty()) {
      is_non_modifier_key_pressed_ = false;
      if (keyval == IBUS_Shift_L) {
        left_shift_in_chord_ = false;
        ctrl_held_during_left_shift_press_ = false;
      }
      MaybeLogIbusDebug(
          "key_handler.mod",
          "return_key_up_waiting_or_no_pending keyval=%u pressed=%zu "
          "pending=%zu",
          keyval, currently_pressed_modifiers_.size(),
          modifiers_to_be_sent_.size());
      return false;
    }
    if (is_non_modifier_key_pressed_) {
      MaybeLogIbusDebug("key_handler.mod",
                        "return_key_up_after_typing keyval=%u", keyval);
      return false;
    }
    if (keyval == IBUS_Shift_L && ctrl_held_during_left_shift_press_) {
      left_shift_in_chord_ = false;
      ctrl_held_during_left_shift_press_ = false;
      modifiers_to_be_sent_.clear();
      MaybeLogIbusDebug("key_handler.mod",
                        "return_left_shift_ctrl_held keyval=%u", keyval);
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
      ctrl_held_during_left_shift_press_ = HasTrackedCtrl();
      if (ctrl_held_during_left_shift_press_) {
        ctrl_left_shift_chord_armed_ = true;
      }
    }
    if (IsControlKeyval(keyval)) {
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
    currently_pressed_modifiers_[keyval] = keycode;
    MaybeLogIbusDebug(
        "key_handler.mod",
        "return_modifier_down_tracked keyval=%u pressed=%zu pending=%zu",
        keyval, currently_pressed_modifiers_.size(),
        modifiers_to_be_sent_.size());
    return false;
  }

  // Reset transient modifier bookkeeping when a plain key goes out with
  // nothing tracked as held. Unlike the upstream blanket Clear(), keys that
  // are still physically down (e.g. Shift while typing capitals) keep their
  // tracking so their releases are handled correctly.
  if (!IsModifierToBeSentOnKeyUp(*key_event) &&
      currently_pressed_modifiers_.empty()) {
    is_non_modifier_key_pressed_ = false;
    modifiers_to_be_sent_.clear();
    ResetChordState();
  }

  MaybeLogIbusDebug(
      "key_handler.mod",
      "return_send keyval=%u mods=%d pressed=%zu pending=%zu non_modifier=%d",
      keyval, key_event->modifier_keys_size(),
      currently_pressed_modifiers_.size(), modifiers_to_be_sent_.size(),
      is_non_modifier_key_pressed_);
  return true;
}

}  // namespace ibus
}  // namespace mozc
