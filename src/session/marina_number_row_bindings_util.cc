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

#include "session/marina_number_row_bindings_util.h"

#include <cstdint>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

#include <cstring>

#include "composer/key_event_util.h"

namespace mozc {
namespace session {
namespace {

using ::mozc::commands::KeyEvent;
using ::mozc::config::MarinaNumberRowAction;
using ::mozc::config::MarinaNumberRowBinding;
using ::mozc::config::MarinaPhysicalSlot;
using ::mozc::config::MarinaShortcutModifier;

MarinaNumberRowBinding MakeBinding(MarinaNumberRowAction action,
                                   MarinaShortcutModifier modifier,
                                   MarinaPhysicalSlot slot) {
  MarinaNumberRowBinding binding;
  binding.set_action(action);
  binding.set_modifier(modifier);
  binding.set_slot(slot);
  return binding;
}

bool IsCtrlShiftNumberRowKeyName(const std::string& key) {
  static const char* const kKeys[] = {
      "Ctrl Shift 1",  "Ctrl Shift !",  "Ctrl Shift 2",  "Ctrl Shift @",
      "Ctrl Shift 3",  "Ctrl Shift #",  "Ctrl Shift 4",  "Ctrl Shift $",
      "Ctrl Shift 5",  "Ctrl Shift %",  "Ctrl Shift ²",
  };
  for (const char* k : kKeys) {
    if (key == k) {
      return true;
    }
  }
  return false;
}

bool IsMarinaNumberRowCommandName(const std::string& command_name) {
  return command_name == "InsertOdorijiDefault" ||
         command_name == "ShowOdorijiPalette" ||
         command_name == "ToggleTraditionalKanji" ||
         command_name == "ToggleManyoshuHiragana" ||
         command_name == "ToggleHiraganaDirect";
}

}  // namespace

std::vector<MarinaNumberRowBinding> GetDefaultMarinaNumberRowBindings() {
  return {
      MakeBinding(MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT,
                  MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT,
                  MarinaPhysicalSlot::MARINA_SLOT_1),
      MakeBinding(MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE,
                  MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT,
                  MarinaPhysicalSlot::MARINA_SLOT_2),
      MakeBinding(MarinaNumberRowAction::MARINA_NR_TRADITIONAL_KANJI,
                  MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT,
                  MarinaPhysicalSlot::MARINA_SLOT_3),
      MakeBinding(MarinaNumberRowAction::MARINA_NR_MANYOSHU_HIRAGANA,
                  MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT,
                  MarinaPhysicalSlot::MARINA_SLOT_4),
      MakeBinding(MarinaNumberRowAction::MARINA_NR_HIRAGANA_DIRECT,
                  MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT,
                  MarinaPhysicalSlot::MARINA_SLOT_5),
      MakeBinding(MarinaNumberRowAction::MARINA_NR_WORD_REGISTER,
                  MarinaShortcutModifier::MARINA_MOD_CTRL,
                  MarinaPhysicalSlot::MARINA_SLOT_0),
  };
}

std::vector<MarinaNumberRowBinding> GetEffectiveMarinaNumberRowBindings(
    const config::Config& config) {
  if (config.marina_number_row_bindings_size() == 0) {
    return GetDefaultMarinaNumberRowBindings();
  }
  std::vector<MarinaNumberRowBinding> bindings;
  bindings.reserve(config.marina_number_row_bindings_size());
  for (const auto& binding : config.marina_number_row_bindings()) {
    bindings.push_back(binding);
  }
  return bindings;
}

bool ValidateMarinaNumberRowBindings(
    const std::vector<MarinaNumberRowBinding>& bindings,
    std::string* error_message) {
  if (bindings.size() != 6) {
    if (error_message != nullptr) {
      *error_message = "Expected exactly six shortcut bindings.";
    }
    return false;
  }

  std::set<MarinaNumberRowAction> actions;
  std::set<std::pair<MarinaShortcutModifier, MarinaPhysicalSlot>> slots;
  for (const auto& binding : bindings) {
    if (!binding.has_action() || !binding.has_slot()) {
      if (error_message != nullptr) {
        *error_message = "Each binding must specify an action and a slot.";
      }
      return false;
    }
    actions.insert(binding.action());
    const MarinaShortcutModifier modifier =
        binding.has_modifier() ? binding.modifier()
                               : MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT;
    const auto slot_key = std::make_pair(modifier, binding.slot());
    if (!slots.insert(slot_key).second) {
      if (error_message != nullptr) {
        *error_message =
            "Duplicate shortcut: " + FormatMarinaBindingLabel(binding);
      }
      return false;
    }
  }

  static const MarinaNumberRowAction kRequired[] = {
      MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT,
      MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE,
      MarinaNumberRowAction::MARINA_NR_TRADITIONAL_KANJI,
      MarinaNumberRowAction::MARINA_NR_MANYOSHU_HIRAGANA,
      MarinaNumberRowAction::MARINA_NR_HIRAGANA_DIRECT,
      MarinaNumberRowAction::MARINA_NR_WORD_REGISTER,
  };
  for (const MarinaNumberRowAction required : kRequired) {
    if (!actions.contains(required)) {
      if (error_message != nullptr) {
        *error_message = "Missing binding for a required action.";
      }
      return false;
    }
  }
  return true;
}

std::string FormatMarinaBindingLabel(const MarinaNumberRowBinding& binding) {
  std::string label;
  const MarinaShortcutModifier modifier =
      binding.has_modifier() ? binding.modifier()
                             : MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT;
  if (modifier == MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT) {
    label = "Ctrl Shift ";
  } else {
    label = "Ctrl ";
  }

  switch (binding.slot()) {
    case MarinaPhysicalSlot::MARINA_SLOT_1:
      label += '1';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_2:
      label += '2';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_3:
      label += '3';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_4:
      label += '4';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_5:
      label += '5';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_6:
      label += '6';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_7:
      label += '7';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_8:
      label += '8';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_9:
      label += '9';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_0:
      label += '0';
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_GRAVE:
      label += '`';
      break;
    default:
      label += '?';
      break;
  }
  return label;
}

const char* MarinaActionToKeymapCommandName(MarinaNumberRowAction action) {
  switch (action) {
    case MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT:
      return "InsertOdorijiDefault";
    case MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE:
      return "ShowOdorijiPalette";
    case MarinaNumberRowAction::MARINA_NR_TRADITIONAL_KANJI:
      return "ToggleTraditionalKanji";
    case MarinaNumberRowAction::MARINA_NR_MANYOSHU_HIRAGANA:
      return "ToggleManyoshuHiragana";
    case MarinaNumberRowAction::MARINA_NR_HIRAGANA_DIRECT:
      return "ToggleHiraganaDirect";
    case MarinaNumberRowAction::MARINA_NR_WORD_REGISTER:
      return "LaunchWordRegisterDialog";
    default:
      return nullptr;
  }
}

bool IsMarinaNumberRowKeymapBinding(const std::string& command_name,
                                    const std::string& key_event_name) {
  if (command_name == "LaunchWordRegisterDialog") {
    return key_event_name == "Ctrl 0" || key_event_name == "Ctrl Shift 0";
  }
  if (command_name == "IMEOn") {
    return IsCtrlShiftNumberRowKeyName(key_event_name) &&
           (key_event_name == "Ctrl Shift 5" || key_event_name == "Ctrl Shift %" ||
            key_event_name == "Ctrl Shift ²");
  }
  if (!IsMarinaNumberRowCommandName(command_name)) {
    return false;
  }
  return IsCtrlShiftNumberRowKeyName(key_event_name);
}

bool BindingMatchesPhysicalSlot(const MarinaNumberRowBinding& binding,
                                MarinaShortcutModifier modifier,
                                MarinaPhysicalSlot slot) {
  const MarinaShortcutModifier binding_modifier =
      binding.has_modifier() ? binding.modifier()
                             : MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT;
  return binding.has_slot() && binding_modifier == modifier &&
         binding.slot() == slot;
}

std::optional<MarinaNumberRowAction> FindMarinaActionForPhysicalSlot(
    const config::Config& config, MarinaShortcutModifier modifier,
    MarinaPhysicalSlot slot) {
  for (const auto& binding : GetEffectiveMarinaNumberRowBindings(config)) {
    if (BindingMatchesPhysicalSlot(binding, modifier, slot)) {
      return binding.action();
    }
  }
  return std::nullopt;
}

std::optional<MarinaPhysicalSlot> PhysicalSlotFromKeyCode(uint32_t key_code) {
  if (key_code >= '1' && key_code <= '9') {
    return static_cast<MarinaPhysicalSlot>(
        static_cast<int>(MarinaPhysicalSlot::MARINA_SLOT_1) + (key_code - '1'));
  }
  if (key_code == '0') {
    return MarinaPhysicalSlot::MARINA_SLOT_0;
  }
  if (key_code == '`' || key_code == '~') {
    return MarinaPhysicalSlot::MARINA_SLOT_GRAVE;
  }
  return std::nullopt;
}

bool KeyEventHasCtrlOnly(const KeyEvent& key_event) {
  if (!key_event.has_key_code()) {
    return false;
  }
  const uint32_t mods = KeyEventUtil::GetModifiers(key_event);
  constexpr uint32_t kCtrl = KeyEvent::CTRL | KeyEvent::LEFT_CTRL |
                             KeyEvent::RIGHT_CTRL;
  constexpr uint32_t kShift = KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT |
                              KeyEvent::RIGHT_SHIFT;
  constexpr uint32_t kAlt = KeyEvent::ALT | KeyEvent::LEFT_ALT |
                            KeyEvent::RIGHT_ALT;
  return (mods & kCtrl) != 0 && (mods & kShift) == 0 && (mods & kAlt) == 0;
}

bool KeyEventHasCtrlShiftOnly(const KeyEvent& key_event) {
  if (!key_event.has_key_code()) {
    return false;
  }
  const uint32_t mods = KeyEventUtil::GetModifiers(key_event);
  constexpr uint32_t kCtrl = KeyEvent::CTRL | KeyEvent::LEFT_CTRL |
                             KeyEvent::RIGHT_CTRL;
  constexpr uint32_t kShift = KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT |
                              KeyEvent::RIGHT_SHIFT;
  constexpr uint32_t kAlt = KeyEvent::ALT | KeyEvent::LEFT_ALT |
                            KeyEvent::RIGHT_ALT;
  return (mods & kCtrl) != 0 && (mods & kShift) != 0 && (mods & kAlt) == 0;
}

MarinaShortcutModifier ModifierFromKeyEvent(const KeyEvent& key_event) {
  if (KeyEventHasCtrlShiftOnly(key_event)) {
    return MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT;
  }
  if (KeyEventHasCtrlOnly(key_event)) {
    return MarinaShortcutModifier::MARINA_MOD_CTRL;
  }
  return MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT;
}

std::optional<MarinaNumberRowAction> FindMarinaActionForKeyEvent(
    const config::Config& config, const KeyEvent& key_event) {
  if (!key_event.has_key_code()) {
    return std::nullopt;
  }
  const std::optional<MarinaPhysicalSlot> slot =
      PhysicalSlotFromKeyCode(key_event.key_code());
  if (!slot.has_value()) {
    return std::nullopt;
  }
  const MarinaShortcutModifier modifier = ModifierFromKeyEvent(key_event);
  if (!KeyEventHasCtrlOnly(key_event) && !KeyEventHasCtrlShiftOnly(key_event)) {
    return std::nullopt;
  }
  return FindMarinaActionForPhysicalSlot(config, modifier, *slot);
}

const char* PhysicalSlotShiftedLabel(MarinaPhysicalSlot slot) {
  switch (slot) {
    case MarinaPhysicalSlot::MARINA_SLOT_1:
      return "!";
    case MarinaPhysicalSlot::MARINA_SLOT_2:
      return "@";
    case MarinaPhysicalSlot::MARINA_SLOT_3:
      return "#";
    case MarinaPhysicalSlot::MARINA_SLOT_4:
      return "$";
    case MarinaPhysicalSlot::MARINA_SLOT_5:
      return "%";
    case MarinaPhysicalSlot::MARINA_SLOT_6:
      return "^";
    case MarinaPhysicalSlot::MARINA_SLOT_7:
      return "&";
    case MarinaPhysicalSlot::MARINA_SLOT_8:
      return "*";
    case MarinaPhysicalSlot::MARINA_SLOT_9:
      return "(";
    case MarinaPhysicalSlot::MARINA_SLOT_0:
      return ")";
    case MarinaPhysicalSlot::MARINA_SLOT_GRAVE:
      return "~";
    default:
      return "";
  }
}

bool IsMarinaToolbarScriptCommand(const char* command_name) {
  return command_name != nullptr &&
         (strcmp(command_name, "ToggleHiraganaDirect") == 0 ||
          strcmp(command_name, "ToggleTraditionalKanji") == 0 ||
          strcmp(command_name, "ToggleManyoshuHiragana") == 0);
}

bool IsMarinaToolbarCompositionCommand(const char* command_name) {
  return command_name != nullptr &&
         (strcmp(command_name, "LaunchWordRegisterDialog") == 0 ||
          strcmp(command_name, "InsertOdorijiDefault") == 0 ||
          strcmp(command_name, "ShowOdorijiPalette") == 0);
}

void ApplyMarinaNumberRowShortcutEntries(
    const config::Config& config,
    std::vector<std::pair<std::string, std::string>>* script_shortcuts,
    std::vector<std::pair<std::string, std::string>>* composition_shortcuts) {
  if (script_shortcuts == nullptr || composition_shortcuts == nullptr) {
    return;
  }

  const auto remove_marina_rows =
      [](std::vector<std::pair<std::string, std::string>>* entries) {
        entries->erase(
            std::remove_if(entries->begin(), entries->end(),
                           [](const std::pair<std::string, std::string>& e) {
                             return IsMarinaNumberRowKeymapBinding(e.second,
                                                                   e.first);
                           }),
            entries->end());
      };
  remove_marina_rows(script_shortcuts);
  remove_marina_rows(composition_shortcuts);

  for (const auto& binding : GetEffectiveMarinaNumberRowBindings(config)) {
    const char* command_name = MarinaActionToKeymapCommandName(binding.action());
    if (command_name == nullptr) {
      continue;
    }
    const std::string label = FormatMarinaBindingLabel(binding);
    if (IsMarinaToolbarScriptCommand(command_name)) {
      script_shortcuts->emplace_back(label, command_name);
    } else if (IsMarinaToolbarCompositionCommand(command_name)) {
      composition_shortcuts->emplace_back(label, command_name);
    }
  }
}

const char* MarinaActionDisplayName(MarinaNumberRowAction action) {
  switch (action) {
    case MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT:
      return "Insert default odoriji";
    case MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE:
      return "Odoriji palette";
    case MarinaNumberRowAction::MARINA_NR_TRADITIONAL_KANJI:
      return "Shin / kyū kanji";
    case MarinaNumberRowAction::MARINA_NR_MANYOSHU_HIRAGANA:
      return "Hiragana ↔ Manyōshū";
    case MarinaNumberRowAction::MARINA_NR_HIRAGANA_DIRECT:
      return "Japanese ↔ direct input";
    case MarinaNumberRowAction::MARINA_NR_WORD_REGISTER:
      return "Dictionary entry";
    default:
      return "";
  }
}

}  // namespace session
}  // namespace mozc
