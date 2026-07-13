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

#ifndef MOZC_UNIX_IBUS_KEY_EVENT_HANDLER_H_
#define MOZC_UNIX_IBUS_KEY_EVENT_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "unix/ibus/key_translator.h"

namespace mozc {
namespace ibus {

class IbusEngineWrapper;

class KeyEventHandler {
 public:
  KeyEventHandler();
  KeyEventHandler(const KeyEventHandler&) = delete;
  KeyEventHandler& operator=(const KeyEventHandler&) = delete;

  // Converts a key event came from ibus to commands::KeyEvent. This is a
  // stateful method. It stores modifier keys states since ibus doesn't send
  // an enough information about the modifier keys.
  bool GetKeyEvent(uint keyval, uint keycode, uint modifiers,
                   config::Config::PreeditMethod preedit_method,
                   bool layout_is_jp, commands::KeyEvent* key);

  // Clears states.
  void Clear();

  // Forwards IBUS_RELEASE for keys tracked as pressed (Shift/Ctrl and
  // non-modifier keys such as Return/BackSpace, e.g. before IME deactivation
  // or focus loss) so the application clears its key state.
  void ForwardTrackedReleases(IbusEngineWrapper* engine);

  // Records a physical press/release handled outside GetKeyEvent (e.g. the
  // synthesized Backspace fast paths in MozcEngine::ProcessKeyEvent) so
  // ForwardTrackedReleases still covers those keys. No-op for modifier
  // keyvals, which must go through GetKeyEvent's state machine.
  void NotifyKeyState(uint keyval, uint keycode, bool is_key_up);

 private:
  friend class KeyEventHandlerTest;

  // Returns (keyval, keycode) pairs for keys whose release should be
  // forwarded by ForwardTrackedReleases.
  std::vector<std::pair<uint, uint>> TrackedReleaseKeys() const;

  // Drops tracked Shift/Ctrl keyvals whose bit is absent from the hardware
  // modifier mask of the incoming event, i.e. keys whose release we missed
  // (focus change, grab). The mask reflects the state before a key-down and
  // still includes the key itself on its own key-up, so |current_keyval| is
  // exempt on key-up.
  void ReconcileStaleModifiers(uint current_keyval, bool is_key_up,
                               uint modifiers);

  bool HasTrackedCtrl() const;
  bool HasTrackedShift() const;

  // Resets the Left Shift / Ctrl+Left Shift toggle-chord flags only; tracked
  // pressed keys are left untouched.
  void ResetChordState();

  // Manages modifier keys. Returns false if it should not be sent to server.
  bool ProcessModifiers(bool is_key_up, uint keyval, uint keycode,
                        commands::KeyEvent* key_event);

  std::unique_ptr<KeyTranslator> key_translator_;
  // Non modifier key is pressed or not after all keys are released.
  bool is_non_modifier_key_pressed_;
  // Currently pressed modifier keys: keyval -> hardware keycode.
  std::map<uint, uint> currently_pressed_modifiers_;
  // Currently pressed non-modifier keys (Return, BackSpace, ...):
  // keyval -> hardware keycode. Only used to forward missing releases on
  // lifecycle events; takes no part in the modifier state machine.
  std::map<uint, uint> currently_pressed_non_modifiers_;
  // Pending modifier keys.
  std::set<commands::KeyEvent::ModifierKey> modifiers_to_be_sent_;
  // True when Left Shift was pressed in the current modifier chord.
  bool left_shift_in_chord_ = false;
  // Ctrl+Left Shift alone → ToggleLeftShiftModeLock (macOS/Linux parity).
  bool ctrl_left_shift_chord_armed_ = false;
  bool typed_during_ctrl_left_shift_chord_ = false;
  bool ctrl_held_during_left_shift_press_ = false;
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_KEY_EVENT_HANDLER_H_
