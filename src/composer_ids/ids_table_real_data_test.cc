// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Checks the shipped GPL table (data/character_composer). Tagged "manual" so
// the ordinary unit tests do not depend on that data.

#include <memory>
#include <string>
#include <utility>
#include <vector>

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

TEST(IdsTableRealDataTest, AutomaticRadicalFormsByPosition) {
  const IdsTable table = LoadRealTable();
  EXPECT_EQ(table.AutomaticFormFor("手", "⿰", 0), "扌");
  EXPECT_EQ(table.AutomaticFormFor("手", "⿰", 1), std::nullopt);
  EXPECT_EQ(table.AutomaticFormFor("水", "⿰", 0), "氵");
  EXPECT_EQ(table.AutomaticFormFor("竹", "⿱", 0), "𥫗");
}

TEST(IdsTableRealDataTest, MovesToTheHeapAndSourceCanBeDestroyed) {
  // What the composer's main() does with the loaded table.
  const std::string ids = testing::GetSourceFileOrDie(
      {"data", "character_composer", "ids_table.tsv"});
  std::unique_ptr<IdsTable> table;
  {
    auto loaded = IdsTable::LoadFromDirectory(FileUtil::Dirname(ids));
    ASSERT_TRUE(loaded.ok()) << loaded.status();
    table = std::make_unique<IdsTable>(*std::move(loaded));
  }
  EXPECT_EQ(table->AutomaticFormFor("手", "⿰", 0), "扌");
  EXPECT_EQ(table->FindEncodedCharacter("⿰言某"), "謀");
}

TEST(IdsTableRealDataTest, FindContainingWorksOnTheFullTable) {
  const IdsTable table = LoadRealTable();
  const std::vector<std::string> found = table.FindContaining("言某");
  EXPECT_THAT(found, ::testing::Contains("謀"));
  // Deeper than the top level: 謀 = ⿰言某, 某 = ⿱廿木.
  EXPECT_THAT(table.FindContaining("言木"), ::testing::Contains("謀"));
  EXPECT_GT(table.FindContaining("木").size(), found.size());
}

TEST(IdsTableRealDataTest, StrokeFilterOnTheFullTable) {
  const IdsTable table = LoadRealTable();
  ASSERT_TRUE(table.has_stroke_counts());
  const LookupQuery exact = ParseLookupQuery("言木16");
  EXPECT_THAT(table.FindContaining(exact.components, exact.strokes),
              ::testing::Contains("謀"));
  const LookupQuery fewer = ParseLookupQuery("言木<16");
  EXPECT_THAT(table.FindContaining(fewer.components, fewer.strokes),
              ::testing::Not(::testing::Contains("謀")));
  const LookupQuery range = ParseLookupQuery("木4-4");
  EXPECT_THAT(table.FindContaining(range.components, range.strokes), ::testing::IsEmpty());
}

TEST(IdsTableRealDataTest, LeadingStrokesOnTheFullTable) {
  const IdsTable table = LoadRealTable();
  EXPECT_EQ(table.LeadingStroke("木"), 1);
  EXPECT_EQ(table.LeadingStroke("呆"), 2);
  EXPECT_EQ(table.LeadingStroke("和"), 3);
  EXPECT_EQ(table.LeadingStroke("架"), 5);
  // Worked out from decompositions: 謀 = ⿰言某, 道 = ⿺辶首.
  EXPECT_EQ(table.LeadingStroke("謀"), 4);
  EXPECT_EQ(table.LeadingStroke("道"), 4);
  std::vector<std::string> found = table.FindContaining("木口");
  table.SortByStrokes(&found);
  ASSERT_FALSE(found.empty());
  for (size_t i = 1; i < found.size(); ++i) {
    const int a = table.StrokeCount(found[i - 1]).value_or(1000);
    const int b = table.StrokeCount(found[i]).value_or(1000);
    EXPECT_LE(a, b) << found[i - 1] << " before " << found[i];
  }
}

TEST(IdsTableRealDataTest, NoStrayTagsRemain) {
  const IdsTable table = LoadRealTable();
  EXPECT_FALSE(table.FindEncodedCharacter("⿰言某[GTKV]").has_value());
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
