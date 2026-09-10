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
  EXPECT_EQ(defaults[5].slot(), MarinaPhysicalSlot::MARINA_SLOT_0);
}

TEST(MarinaNumberRowBindingsUtilTest, EffectiveBindingsUsesDefaults) {
  config::Config config;
  const auto bindings = GetEffectiveMarinaNumberRowBindings(config);
  EXPECT_EQ(bindings.size(), 6u);
}

TEST(MarinaNumberRowBindingsUtilTest, ValidateRejectsDuplicateSlot) {
  auto bindings = GetDefaultMarinaNumberRowBindings();
  bindings[1].set_slot(MarinaPhysicalSlot::MARINA_SLOT_1);
  std::string error;
  EXPECT_FALSE(ValidateMarinaNumberRowBindings(bindings, &error));
  EXPECT_FALSE(error.empty());
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
  ASSERT_TRUE(KeyParser::ParseKey("Ctrl Shift 0", &dict_key));
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
      EXPECT_EQ(entry.first, "Ctrl Shift 0");
    }
  }
  EXPECT_EQ(word_register_rows, 1);
}

}  // namespace
}  // namespace session
}  // namespace mozc
