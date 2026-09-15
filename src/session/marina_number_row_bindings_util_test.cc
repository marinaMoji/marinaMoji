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

#include <string>
#include <utility>
#include <vector>

#include "composer/key_parser.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

using ::mozc::commands::KeyEvent;
using ::mozc::config::MarinaNumberRowAction;
using ::mozc::config::MarinaNumberRowBinding;
using ::mozc::config::MarinaPhysicalSlot;
using ::mozc::config::MarinaShortcutModifier;

#ifdef _WIN32
constexpr MarinaPhysicalSlot kDefaultDictionarySlot =
    MarinaPhysicalSlot::MARINA_SLOT_9;
constexpr const char* kDefaultDictionaryChord = "Ctrl Shift 9";
#else   // !_WIN32
constexpr MarinaPhysicalSlot kDefaultDictionarySlot =
    MarinaPhysicalSlot::MARINA_SLOT_0;
constexpr const char* kDefaultDictionaryChord = "Ctrl Shift 0";
#endif  // _WIN32

TEST(MarinaNumberRowBindingsUtilTest, DefaultBindings) {
  const auto defaults = GetDefaultMarinaNumberRowBindings();
  ASSERT_EQ(defaults.size(), 6u);
  EXPECT_EQ(defaults[0].action(),
            MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT);
  EXPECT_EQ(defaults[0].slot(), MarinaPhysicalSlot::MARINA_SLOT_1);
  EXPECT_EQ(defaults[5].action(),
            MarinaNumberRowAction::MARINA_NR_WORD_REGISTER);
  EXPECT_EQ(defaults[5].modifier(),
            MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT);
  EXPECT_EQ(defaults[5].slot(), kDefaultDictionarySlot);
}

TEST(MarinaNumberRowBindingsUtilTest, EffectiveBindingsUsesDefaults) {
  config::Config config;
  const auto bindings = GetEffectiveMarinaNumberRowBindings(config);
  EXPECT_EQ(bindings.size(), 6u);
}

// A profile saved through the Settings dialog while the shipped default for
// the dictionary action was still Ctrl+0 (2026-06-11 .. 2026-07-24), with
// other rows customised so that all six bindings are stored.
config::Config ConfigWithStaleCtrl0DictionaryBinding() {
  config::Config config;
  for (MarinaNumberRowBinding binding : GetDefaultMarinaNumberRowBindings()) {
    if (binding.action() == MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE) {
      binding.set_slot(MarinaPhysicalSlot::MARINA_SLOT_6);
    } else if (binding.action() ==
               MarinaNumberRowAction::MARINA_NR_WORD_REGISTER) {
      binding.set_modifier(MarinaShortcutModifier::MARINA_MOD_CTRL);
      binding.set_slot(MarinaPhysicalSlot::MARINA_SLOT_0);
    }
    *config.add_marina_number_row_bindings() = binding;
  }
  return config;
}

TEST(MarinaNumberRowBindingsUtilTest, StaleCtrl0DictionaryBindingMigrates) {
  const config::Config config = ConfigWithStaleCtrl0DictionaryBinding();

  KeyEvent migrated;
  ASSERT_TRUE(KeyParser::ParseKey(kDefaultDictionaryChord, &migrated));
  const auto action = FindMarinaActionForKeyEvent(config, migrated);
  ASSERT_TRUE(action.has_value());
  EXPECT_EQ(*action, MarinaNumberRowAction::MARINA_NR_WORD_REGISTER);

  // The old chord is not kept as an alias.
  KeyEvent ctrl_0;
  ASSERT_TRUE(KeyParser::ParseKey("Ctrl 0", &ctrl_0));
  EXPECT_FALSE(FindMarinaActionForKeyEvent(config, ctrl_0).has_value());

  // The customisation on another row survives.
  KeyEvent ctrl_shift_6;
  ASSERT_TRUE(KeyParser::ParseKey("Ctrl Shift 6", &ctrl_shift_6));
  const auto palette = FindMarinaActionForKeyEvent(config, ctrl_shift_6);
  ASSERT_TRUE(palette.has_value());
  EXPECT_EQ(*palette, MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE);
}

TEST(MarinaNumberRowBindingsUtilTest,
     StaleCtrl0DictionaryBindingKeptWhenTargetIsTaken) {
  // If the user has deliberately put another action on the current default
  // dictionary chord, the dictionary binding stays on Ctrl+0 rather than
  // colliding with it.
  config::Config config = ConfigWithStaleCtrl0DictionaryBinding();
  for (auto& binding : *config.mutable_marina_number_row_bindings()) {
    if (binding.action() == MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT) {
      binding.set_slot(kDefaultDictionarySlot);
    }
  }

  KeyEvent ctrl_0;
  ASSERT_TRUE(KeyParser::ParseKey("Ctrl 0", &ctrl_0));
  const auto action = FindMarinaActionForKeyEvent(config, ctrl_0);
  ASSERT_TRUE(action.has_value());
  EXPECT_EQ(*action, MarinaNumberRowAction::MARINA_NR_WORD_REGISTER);

  KeyEvent taken;
  ASSERT_TRUE(KeyParser::ParseKey(kDefaultDictionaryChord, &taken));
  const auto other = FindMarinaActionForKeyEvent(config, taken);
  ASSERT_TRUE(other.has_value());
  EXPECT_EQ(*other, MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT);
}

#ifdef _WIN32
TEST(MarinaNumberRowBindingsUtilTest,
     WindowsStaleCtrlShift0DictionaryBindingMigratesTo9) {
  // Profiles that stored the old Windows default Ctrl+Shift+0 should move to
  // Ctrl+Shift+9, since the OS swallows the former chord.
  config::Config config;
  for (MarinaNumberRowBinding binding : GetDefaultMarinaNumberRowBindings()) {
    if (binding.action() == MarinaNumberRowAction::MARINA_NR_WORD_REGISTER) {
      binding.set_slot(MarinaPhysicalSlot::MARINA_SLOT_0);
    }
    *config.add_marina_number_row_bindings() = binding;
  }

  KeyEvent ctrl_shift_9;
  ASSERT_TRUE(KeyParser::ParseKey("Ctrl Shift 9", &ctrl_shift_9));
  const auto action = FindMarinaActionForKeyEvent(config, ctrl_shift_9);
  ASSERT_TRUE(action.has_value());
  EXPECT_EQ(*action, MarinaNumberRowAction::MARINA_NR_WORD_REGISTER);

  KeyEvent ctrl_shift_0;
  ASSERT_TRUE(KeyParser::ParseKey("Ctrl Shift 0", &ctrl_shift_0));
  EXPECT_FALSE(FindMarinaActionForKeyEvent(config, ctrl_shift_0).has_value());
}
#endif  // _WIN32

TEST(MarinaNumberRowBindingsUtilTest, ValidateRejectsDuplicateSlot) {
  auto bindings = GetDefaultMarinaNumberRowBindings();
  bindings[1].set_slot(MarinaPhysicalSlot::MARINA_SLOT_1);
  std::string error;
  EXPECT_FALSE(ValidateMarinaNumberRowBindings(bindings, &error));
  EXPECT_FALSE(error.empty());
}

TEST(MarinaNumberRowBindingsUtilTest, CtrlShift0BlockedOnlyOnWindows) {
#ifdef _WIN32
  EXPECT_TRUE(IsMarinaNumberRowChordBlocked(
      MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT,
      MarinaPhysicalSlot::MARINA_SLOT_0));
  EXPECT_FALSE(IsMarinaNumberRowChordBlocked(
      MarinaShortcutModifier::MARINA_MOD_CTRL,
      MarinaPhysicalSlot::MARINA_SLOT_0));
  EXPECT_FALSE(IsMarinaNumberRowChordBlocked(
      MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT,
      MarinaPhysicalSlot::MARINA_SLOT_9));

  auto bindings = GetDefaultMarinaNumberRowBindings();
  for (auto& binding : bindings) {
    if (binding.action() == MarinaNumberRowAction::MARINA_NR_WORD_REGISTER) {
      binding.set_modifier(MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT);
      binding.set_slot(MarinaPhysicalSlot::MARINA_SLOT_0);
    }
  }
  std::string error;
  EXPECT_FALSE(ValidateMarinaNumberRowBindings(bindings, &error));
  EXPECT_NE(error.find("Ctrl+Shift+0"), std::string::npos);
#else   // !_WIN32
  EXPECT_FALSE(IsMarinaNumberRowChordBlocked(
      MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT,
      MarinaPhysicalSlot::MARINA_SLOT_0));
  EXPECT_TRUE(ValidateMarinaNumberRowBindings(
      GetDefaultMarinaNumberRowBindings(), nullptr));
#endif  // _WIN32
}

TEST(MarinaNumberRowBindingsUtilTest, FormatLabel) {
  MarinaNumberRowBinding binding;
  binding.set_action(MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT);
  binding.set_modifier(MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT);
  binding.set_slot(MarinaPhysicalSlot::MARINA_SLOT_4);
  EXPECT_EQ(FormatMarinaBindingLabel(binding), "Ctrl Shift 4");

  binding.set_modifier(MarinaShortcutModifier::MARINA_MOD_CTRL);
  binding.set_slot(MarinaPhysicalSlot::MARINA_SLOT_0);
  EXPECT_EQ(FormatMarinaBindingLabel(binding), "Ctrl 0");
}

TEST(MarinaNumberRowBindingsUtilTest, KeymapBindingDetection) {
  EXPECT_TRUE(IsMarinaNumberRowKeymapBinding("InsertOdorijiDefault",
                                             "Ctrl Shift 1"));
  EXPECT_TRUE(IsMarinaNumberRowKeymapBinding("LaunchWordRegisterDialog",
                                             "Ctrl Shift 0"));
  EXPECT_TRUE(IsMarinaNumberRowKeymapBinding("LaunchWordRegisterDialog",
                                             "Ctrl 0"));
  // How every shipped keymap TSV actually spells Ctrl+Shift+0. If this row
  // survives, the number-row dispatcher and the keymap both fire for the same
  // physical chord on US layouts, and the Shortcuts window lists the command
  // twice -- once as "Ctrl Shift )" and once as "Ctrl Shift 0".
  EXPECT_TRUE(IsMarinaNumberRowKeymapBinding("LaunchWordRegisterDialog",
                                             "Ctrl Shift )"));
  EXPECT_TRUE(IsMarinaNumberRowKeymapBinding("LaunchWordRegisterDialog",
                                             "Ctrl )"));
  // Windows dictionary default is slot 9; drop matching keymap spellings too.
  EXPECT_TRUE(IsMarinaNumberRowKeymapBinding("LaunchWordRegisterDialog",
                                             "Ctrl Shift 9"));
  EXPECT_TRUE(IsMarinaNumberRowKeymapBinding("LaunchWordRegisterDialog",
                                             "Ctrl Shift ("));
  // Not a number-row chord: ATOK's own binding stays in the keymap.
  EXPECT_FALSE(IsMarinaNumberRowKeymapBinding("LaunchWordRegisterDialog",
                                              "Ctrl F7"));
  EXPECT_TRUE(IsMarinaNumberRowKeymapBinding("IMEOn", "Ctrl Shift 5"));
  EXPECT_FALSE(IsMarinaNumberRowKeymapBinding("ToggleTraditionalKanji",
                                              "Ctrl Shift F"));
  EXPECT_FALSE(IsMarinaNumberRowKeymapBinding("InsertOdorijiDefault", "Ctrl j"));
}

TEST(MarinaNumberRowBindingsUtilTest, FindActionForKeyEvent) {
  config::Config config;
  KeyEvent key;
  ASSERT_TRUE(KeyParser::ParseKey("Ctrl Shift 3", &key));
  const auto action = FindMarinaActionForKeyEvent(config, key);
  ASSERT_TRUE(action.has_value());
  EXPECT_EQ(*action, MarinaNumberRowAction::MARINA_NR_TRADITIONAL_KANJI);

  KeyEvent dict_key;
  ASSERT_TRUE(KeyParser::ParseKey(kDefaultDictionaryChord, &dict_key));
  const auto dict_action = FindMarinaActionForKeyEvent(config, dict_key);
  ASSERT_TRUE(dict_action.has_value());
  EXPECT_EQ(*dict_action, MarinaNumberRowAction::MARINA_NR_WORD_REGISTER);

  KeyEvent ctrl_only_dict_key;
  ASSERT_TRUE(KeyParser::ParseKey("Ctrl 0", &ctrl_only_dict_key));
  EXPECT_FALSE(
      FindMarinaActionForKeyEvent(config, ctrl_only_dict_key).has_value());
}

TEST(MarinaNumberRowBindingsUtilTest, ShortcutEntriesListDictionaryEntryOnce) {
  // The keymap TSV row and the configured binding describe the same chord;
  // only the binding's label should reach the Shortcuts window.
  std::vector<std::pair<std::string, std::string>> script;
  std::vector<std::pair<std::string, std::string>> composition = {
      {"Enter", "Commit"},
      {"Ctrl Shift )", "LaunchWordRegisterDialog"},
  };
  ApplyMarinaNumberRowShortcutEntries(config::Config(), &script, &composition);

  int word_register_rows = 0;
  for (const auto& entry : composition) {
    if (entry.second == "LaunchWordRegisterDialog") {
      ++word_register_rows;
      EXPECT_EQ(entry.first, kDefaultDictionaryChord);
    }
  }
  EXPECT_EQ(word_register_rows, 1);
}

}  // namespace
}  // namespace session
}  // namespace mozc
