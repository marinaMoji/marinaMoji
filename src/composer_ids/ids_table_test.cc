// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Uses small hand-written fixtures, not the GPL table. The pairs are the ones
// checked against zi.tools when the Grognard composer was designed.

#include "composer_ids/ids_table.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace composer_ids {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

IdsTable LoadFixture() {
  const std::string ids = testing::GetSourceFileOrDie(
      {"composer_ids", "testdata", "ids_table.tsv"});
  const std::string dir = FileUtil::Dirname(ids);
  auto table = IdsTable::LoadFromDirectory(dir);
  EXPECT_TRUE(table.ok()) << table.status();
  return *std::move(table);
}

TEST(IdsTableTest, FindsAssignedCharacterForExactIds) {
  EXPECT_EQ(LoadFixture().FindEncodedCharacter("⿰言某"), "謀");
}

TEST(IdsTableTest, NoAnswerForUnrecordedStructure) {
  const IdsTable table = LoadFixture();
  EXPECT_EQ(table.FindEncodedCharacter("⿰牙齒"), std::nullopt);
  // Exact operator only.
  EXPECT_EQ(table.FindEncodedCharacter("⿱言某"), std::nullopt);
  EXPECT_EQ(table.FindEncodedCharacter(""), std::nullopt);
}

TEST(IdsTableTest, MatchesAcrossTheFormsOfARadical) {
  const IdsTable table = LoadFixture();
  EXPECT_EQ(table.FindEncodedCharacter("⿰業刀"), "𠟪");
  EXPECT_EQ(table.FindEncodedCharacter("⿰訁某"), "謀");
}

TEST(IdsTableTest, PrefersExactOverRadicalFormMatch) {
  const IdsTable table = LoadFixture();
  EXPECT_EQ(table.FindEncodedCharacter("⿰氵甲"), "一");
  EXPECT_EQ(table.FindEncodedCharacter("⿰水甲"), "二");
}

TEST(IdsTableTest, AutomaticFormDependsOnOperatorAndPosition) {
  const IdsTable table = LoadFixture();
  EXPECT_EQ(table.AutomaticFormFor("言", "⿰", 0), "訁");
  EXPECT_EQ(table.AutomaticFormFor("言", "⿰", 1), std::nullopt);  // right side keeps 言
  EXPECT_EQ(table.AutomaticFormFor("言", "⿱", 0), std::nullopt);
  EXPECT_EQ(table.AutomaticFormFor("人", "⿱", 0), "𠆢");
  EXPECT_EQ(table.AutomaticFormFor("人", "⿰", 0), "亻");
  EXPECT_EQ(table.AutomaticFormFor("木", "⿰", 0), std::nullopt);
}

TEST(IdsTableTest, AutomaticFormLinesAreNotFamilies) {
  EXPECT_EQ(LoadFixture().FormsOf("@"), std::vector<std::string>());
  EXPECT_EQ(LoadFixture().FormsOf("言"), (std::vector<std::string>{"言", "訁"}));
}

TEST(IdsTableTest, NoAnswerWhenRadicalFormsAreAmbiguous) {
  EXPECT_EQ(LoadFixture().FindEncodedCharacter("⿰⺡甲"), std::nullopt);
}

TEST(IdsTableTest, FindsDecompositions) {
  const IdsTable table = LoadFixture();
  EXPECT_THAT(table.FindDecompositions("謀"), ElementsAre("⿰言某"));
  // Only the first character counts; surrounding blanks are ignored.
  EXPECT_THAT(table.FindDecompositions(" 謀謀"), ElementsAre("⿰言某"));
  EXPECT_THAT(table.FindDecompositions("⺸"), ElementsAre("⿱艹干"));
}

TEST(IdsTableTest, AtomicAndUnknownCharactersHaveNoDecomposition) {
  const IdsTable table = LoadFixture();
  EXPECT_THAT(table.FindDecompositions("言"), IsEmpty());
  EXPECT_THAT(table.FindDecompositions(""), IsEmpty());
}

TEST(IdsTableTest, LaterEntryWinsAndDecompositionsAreDeduplicated) {
  auto table = IdsTable::FromTsv("⿰一二\tA\n⿰一二\tB\n⿰三四\tB\n⿰三四\tB\n", "");
  ASSERT_TRUE(table.ok());
  EXPECT_EQ(table->FindEncodedCharacter("⿰一二"), "B");
  EXPECT_THAT(table->FindDecompositions("B"), ElementsAre("⿰一二", "⿰三四"));
}

TEST(IdsTableTest, FormsOfAndNextFormOf) {
  const IdsTable table = LoadFixture();
  EXPECT_THAT(table.FormsOf("亻"), ElementsAre("人", "亻", "⺅", "𠆢"));
  EXPECT_THAT(table.FormsOf("木"), IsEmpty());
  EXPECT_EQ(table.NextFormOf("人"), "亻");
  EXPECT_EQ(table.NextFormOf("𠆢"), "人");  // wraps round
  EXPECT_EQ(table.NextFormOf("木"), std::nullopt);
  // 阝 belongs to two families and gets both.
  EXPECT_THAT(table.FormsOf("阝"), ElementsAre("阜", "阝", "邑"));
}

TEST(IdsTableTest, WorksWithoutRadicalForms) {
  auto table = IdsTable::FromTsv("⿰言某\t謀\r\n\n", "");
  ASSERT_TRUE(table.ok());
  EXPECT_EQ(table->FindEncodedCharacter("⿰言某"), "謀");
  EXPECT_EQ(table->size(), 1);
}

TEST(IdsTableTest, MalformedLineIsAnError) {
  EXPECT_FALSE(IdsTable::FromTsv("⿰言某\n", "").ok());
  EXPECT_FALSE(IdsTable::FromTsv("a\tb\tc\n", "").ok());
  EXPECT_FALSE(IdsTable::FromTsv("\tb\n", "").ok());
}

TEST(IdsTableTest, MissingDirectoryIsAnError) {
  EXPECT_FALSE(IdsTable::LoadFromDirectory("/nonexistent/dir").ok());
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
