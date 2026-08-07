// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "session/marina_shortcut_list_util.h"

#include <algorithm>
#include <string>

#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

bool HasFunction(const std::vector<MarinaShortcutRow>& rows,
                 const std::string& function) {
  return std::any_of(rows.begin(), rows.end(),
                     [&function](const MarinaShortcutRow& row) {
                       return row.function == function;
                     });
}

const MarinaShortcutRow* FindFunction(
    const std::vector<MarinaShortcutRow>& rows, const std::string& function) {
  for (const MarinaShortcutRow& row : rows) {
    if (row.function == function) {
      return &row;
    }
  }
  return nullptr;
}

TEST(MarinaShortcutListUtilTest, DefaultsAreNeverEmpty) {
  const MarinaShortcutLists lists = BuildDefaultMarinaShortcutLists();
  EXPECT_FALSE(lists.script.empty());
  EXPECT_FALSE(lists.composition.empty());
  EXPECT_FALSE(lists.kaeriten.empty());
}

TEST(MarinaShortcutListUtilTest, EveryRowHasBothColumns) {
  const MarinaShortcutLists lists = BuildDefaultMarinaShortcutLists();
  for (const auto* rows : {&lists.script, &lists.composition, &lists.kaeriten}) {
    for (const MarinaShortcutRow& row : *rows) {
      EXPECT_FALSE(row.function.empty());
      EXPECT_FALSE(row.keys.empty());
    }
  }
}

TEST(MarinaShortcutListUtilTest, ScriptTabCarriesMarinaCommands) {
  const MarinaShortcutLists lists = BuildDefaultMarinaShortcutLists();
  EXPECT_TRUE(HasFunction(lists.script, "ToggleTraditionalKanji"));
  EXPECT_TRUE(HasFunction(lists.script, "ToggleManyoshuHiragana"));
}

TEST(MarinaShortcutListUtilTest, ScriptTabIsInDeclaredOrder) {
  const MarinaShortcutLists lists = BuildDefaultMarinaShortcutLists();
  // The Script tab is deliberately ordered, not alphabetical: the mode
  // toggles come before the conversion commands.
  const auto index_of = [&lists](const std::string& function) {
    for (size_t i = 0; i < lists.script.size(); ++i) {
      if (lists.script[i].function == function) {
        return static_cast<int>(i);
      }
    }
    return -1;
  };
  const int toggle_trad = index_of("ToggleTraditionalKanji");
  const int convert_hiragana = index_of("ConvertToHiragana");
  ASSERT_GE(toggle_trad, 0);
  ASSERT_GE(convert_hiragana, 0);
  EXPECT_LT(toggle_trad, convert_hiragana);
}

TEST(MarinaShortcutListUtilTest, MultipleKeysAreJoined) {
  const MarinaShortcutLists lists = BuildDefaultMarinaShortcutLists();
  // ConvertToFullKatakana is bound to both Ctrl+i and F7 in every bundled
  // keymap, so it must collapse into one row listing both.
  const MarinaShortcutRow* row =
      FindFunction(lists.script, "ConvertToFullKatakana");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->keys.find(", "), std::string::npos)
      << "expected several keys, got: " << row->keys;
}

TEST(MarinaShortcutListUtilTest, NoDuplicateFunctionRows) {
  const MarinaShortcutLists lists = BuildDefaultMarinaShortcutLists();
  for (const auto* rows : {&lists.script, &lists.composition, &lists.kaeriten}) {
    for (size_t i = 0; i < rows->size(); ++i) {
      for (size_t j = i + 1; j < rows->size(); ++j) {
        EXPECT_NE((*rows)[i].function, (*rows)[j].function)
            << "duplicate row for " << (*rows)[i].function;
      }
    }
  }
}

TEST(MarinaShortcutListUtilTest, CustomKeymapTableIsUsed) {
  config::Config config;
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(
      "status\tkey\tcommand\n"
      "Precomposition\tCtrl Shift Z\tToggleTraditionalKanji\n");

  const MarinaShortcutLists lists = BuildMarinaShortcutLists(config);
  const MarinaShortcutRow* row =
      FindFunction(lists.script, "ToggleTraditionalKanji");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->keys.find("Ctrl Shift Z"), std::string::npos)
      << "custom keymap binding missing, got: " << row->keys;
  // Not the *only* key: marinaMoji's configurable number-row shortcuts are
  // merged on top of whatever keymap table is in force, so the default
  // Ctrl+Shift+3 binding is expected alongside the custom one.
  EXPECT_NE(row->keys.find("Ctrl Shift 3"), std::string::npos)
      << "number-row binding missing, got: " << row->keys;
}

TEST(MarinaShortcutListUtilTest, EmptyCustomTableFallsBackToDefaults) {
  config::Config config;
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table("");

  const MarinaShortcutLists lists = BuildMarinaShortcutLists(config);
  EXPECT_FALSE(lists.script.empty());
  EXPECT_TRUE(HasFunction(lists.script, "ToggleTraditionalKanji"));
}

TEST(MarinaShortcutListUtilTest, KaeritenRowsMapGlyphToInput) {
  const MarinaShortcutLists lists = BuildDefaultMarinaShortcutLists();
  ASSERT_FALSE(lists.kaeriten.empty());
  // The kaeriten tab is the other way round from the other two: the left
  // column is the produced glyph, the right column the input that makes it.
  for (const MarinaShortcutRow& row : lists.kaeriten) {
    EXPECT_NE(row.keys.find(';'), std::string::npos)
        << "expected a ';'-prefixed input for " << row.function;
  }
}

}  // namespace
}  // namespace session
}  // namespace mozc
