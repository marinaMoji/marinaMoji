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

#ifndef MOZC_SESSION_MARINA_NUMBER_ROW_BINDINGS_UTIL_H_
#define MOZC_SESSION_MARINA_NUMBER_ROW_BINDINGS_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace session {

// Bundled default bindings (Ctrl+Shift+1..5, Ctrl+0 dictionary).
std::vector<config::MarinaNumberRowBinding> GetDefaultMarinaNumberRowBindings();

// Effective bindings: config repeated field or defaults when empty.
std::vector<config::MarinaNumberRowBinding> GetEffectiveMarinaNumberRowBindings(
    const config::Config& config);

// Returns true when |bindings| has no duplicate (modifier, slot) pairs and
// includes all six actions.
bool ValidateMarinaNumberRowBindings(
    const std::vector<config::MarinaNumberRowBinding>& bindings,
    std::string* error_message);

// Human-readable shortcut label, e.g. "Ctrl Shift 4".
std::string FormatMarinaBindingLabel(
    const config::MarinaNumberRowBinding& binding);

// Keymap command name for toolbar grouping, e.g. "ToggleManyoshuHiragana".
const char* MarinaActionToKeymapCommandName(
    config::MarinaNumberRowAction action);

// True for marina number-row keymap rows that the config dispatcher replaces.
bool IsMarinaNumberRowKeymapBinding(const std::string& command_name,
                                    const std::string& key_event_name);

// Match a binding against modifiers and physical slot.
bool BindingMatchesPhysicalSlot(const config::MarinaNumberRowBinding& binding,
                                config::MarinaShortcutModifier modifier,
                                config::MarinaPhysicalSlot slot);

// Find action for physical slot + modifier from config.
std::optional<config::MarinaNumberRowAction> FindMarinaActionForPhysicalSlot(
    const config::Config& config, config::MarinaShortcutModifier modifier,
    config::MarinaPhysicalSlot slot);

// Infer physical slot from normalized Mozc key code ('1'..'0', '`').
std::optional<config::MarinaPhysicalSlot> PhysicalSlotFromKeyCode(
    uint32_t key_code);

// Find action from a Mozc KeyEvent (after platform physical-key normalization).
std::optional<config::MarinaNumberRowAction> FindMarinaActionForKeyEvent(
    const config::Config& config, const commands::KeyEvent& key_event);

// Modifier extraction from Mozc KeyEvent.
bool KeyEventHasCtrlOnly(const commands::KeyEvent& key_event);
bool KeyEventHasCtrlShiftOnly(const commands::KeyEvent& key_event);

config::MarinaShortcutModifier ModifierFromKeyEvent(
    const commands::KeyEvent& key_event);

// QWERTY shifted symbol shown in slot dropdown, e.g. SLOT_2 -> "@".
const char* PhysicalSlotShiftedLabel(config::MarinaPhysicalSlot slot);

// Action display name for settings UI.
const char* MarinaActionDisplayName(config::MarinaNumberRowAction action);

// Replace marina number-row rows in toolbar shortcut lists with config bindings.
void ApplyMarinaNumberRowShortcutEntries(
    const config::Config& config,
    std::vector<std::pair<std::string, std::string>>* script_shortcuts,
    std::vector<std::pair<std::string, std::string>>* composition_shortcuts);

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_MARINA_NUMBER_ROW_BINDINGS_UTIL_H_
