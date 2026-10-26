// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Checks the shipped GPL table (data/character_composer). Tagged "manual" so
// the ordinary unit tests do not depend on that data.

#include <string>

#include "base/file_util.h"
#include "composer_ids/ids_table.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace composer_ids {
namespace {

using ::testing::Contains;

IdsTable LoadRealTable() {
  const std::string ids = testing::GetSourceFileOrDie(
      {"data", "character_composer", "ids_table.tsv"});
  auto table = IdsTable::LoadFromDirectory(FileUtil::Dirname(ids));
  EXPECT_TRUE(table.ok()) << table.status();
  return *std::move(table);
}

TEST(IdsTableRealDataTest, ReproducesTheZiToolsChecks) {
  const IdsTable table = LoadRealTable();
  EXPECT_GT(table.size(), 90000);
  EXPECT_EQ(table.FindEncodedCharacter("⿰言某"), "謀");
  EXPECT_EQ(table.FindEncodedCharacter("⿰言齒"), "𪘙");
  EXPECT_EQ(table.FindEncodedCharacter("⿰牙齒"), std::nullopt);
}

TEST(IdsTableRealDataTest, RadicalFormsAndReverseLookup) {
  const IdsTable table = LoadRealTable();
  EXPECT_EQ(table.FindEncodedCharacter("⿰業刀"), "𠟪");
  EXPECT_THAT(table.FindDecompositions("謀"), Contains("⿰言某"));
}

TEST(IdsTableRealDataTest, NoStrayTagsRemain) {
  const IdsTable table = LoadRealTable();
  EXPECT_FALSE(table.FindEncodedCharacter("⿰言某[GTKV]").has_value());
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
