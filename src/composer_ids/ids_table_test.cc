// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Uses small hand-written fixtures, not the GPL table. The pairs are the ones
// checked against zi.tools when the Grognard composer was designed.

#include "composer_ids/ids_table.h"

#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "base/file_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace composer_ids {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;

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

TEST(IdsTableTest, FindContainingMatchesAtAnyDepth) {
  const IdsTable table = LoadFixture();
  // 謀 = ⿰言某 and 某 = ⿱廿木, so 木 is two levels down in 謀.
  EXPECT_THAT(table.FindContaining("木"), ElementsAre("某", "相", "謀"));
}

TEST(IdsTableTest, FindContainingRequiresEveryComponent) {
  const IdsTable table = LoadFixture();
  EXPECT_THAT(table.FindContaining("木目"), ElementsAre("相"));
  EXPECT_THAT(table.FindContaining("言木"), ElementsAre("謀"));
  EXPECT_THAT(table.FindContaining("言 木 "), ElementsAre("謀"));  // blanks ignored
  EXPECT_THAT(table.FindContaining("目言"), IsEmpty());
}

TEST(IdsTableTest, FindContainingTreatsRadicalFormsAsOneComponent) {
  const IdsTable table = LoadFixture();
  EXPECT_THAT(table.FindContaining("訁"), ElementsAre("謀", "𪘙"));  // typed 訁, stored 言
  EXPECT_THAT(table.FindContaining("刀"), ElementsAre("𠟪"));         // stored 刂
  EXPECT_THAT(table.FindContaining("氵"), ElementsAre("一", "二"));  // 水 too
}

TEST(IdsTableTest, FindContainingNeverMatchesTheCharacterItself) {
  const IdsTable table = LoadFixture();
  EXPECT_THAT(table.FindContaining("謀"), IsEmpty());
  EXPECT_THAT(table.FindContaining("某"), ElementsAre("謀"));
}

TEST(IdsTableTest, FindContainingIgnoresOperatorsAndUnknownInput) {
  const IdsTable table = LoadFixture();
  EXPECT_THAT(table.FindContaining(""), IsEmpty());
  EXPECT_THAT(table.FindContaining("⿰"), IsEmpty());
  EXPECT_THAT(table.FindContaining("abc"), IsEmpty());
  EXPECT_THAT(table.FindContaining("龘"), IsEmpty());
  // An operator typed next to a component is skipped, not a filter.
  EXPECT_THAT(table.FindContaining("⿰木目"), ElementsAre("相"));
}

TEST(IdsTableTest, FindContainingSurvivesCyclicData) {
  auto table = IdsTable::FromTsv("⿰乙丙\t甲\n⿰甲丁\t乙\n", "");
  ASSERT_TRUE(table.ok());
  // Malformed data: only that it terminates and finds the direct container.
  EXPECT_THAT(table->FindContaining("丁"), Contains("乙"));
}

TEST(IdsTableTest, ParsesStrokeFilters) {
  const auto strokes = [](std::string_view text) { return ParseLookupQuery(text).strokes; };
  EXPECT_EQ(ParseLookupQuery("木12").components, "木");
  EXPECT_EQ(strokes("木12")->min, 12);
  EXPECT_EQ(strokes("木12")->max, 12);
  EXPECT_EQ(strokes("木<12")->max, 11);
  EXPECT_EQ(strokes("木<12")->min, 0);
  EXPECT_EQ(strokes("木>8")->min, 9);
  EXPECT_EQ(strokes("木>8")->max, std::numeric_limits<int>::max());
  EXPECT_EQ(strokes("木<=12")->max, 12);
  EXPECT_EQ(strokes("木>=8")->min, 8);
  EXPECT_EQ(strokes("木9-12")->min, 9);
  EXPECT_EQ(strokes("木9-12")->max, 12);
  EXPECT_EQ(strokes("木12-9")->min, 9);  // reversed range
  EXPECT_EQ(strokes("木12-9")->max, 12);
  // Full-width digits and signs, as typed with a Japanese IME.
  EXPECT_EQ(strokes("木１２")->min, 12);
  EXPECT_EQ(strokes("木＜１２")->max, 11);
  EXPECT_EQ(strokes("木９－１２")->max, 12);
  // Filter first, alone, among several components, or absent.
  EXPECT_EQ(ParseLookupQuery("12 木 言").components, "木言");
  EXPECT_EQ(strokes("12 木 言")->max, 12);
  EXPECT_EQ(strokes("9-12")->min, 9);
  EXPECT_EQ(ParseLookupQuery("9-12").components, "");
  EXPECT_FALSE(strokes("木言").has_value());
  EXPECT_FALSE(strokes("木<").has_value());
  EXPECT_FALSE(strokes("").has_value());
}

TEST(IdsTableTest, SeveralStrokeFiltersAllApply) {
  const auto range = [](std::string_view text) {
    return *ParseLookupQuery(text).strokes;
  };
  EXPECT_EQ(range("木>3 <9").min, 4);
  EXPECT_EQ(range("木>3 <9").max, 8);
  EXPECT_EQ(range("木 4-12 8-20").min, 8);
  EXPECT_EQ(range("木 4-12 8-20").max, 12);
  // Disjoint filters leave an empty range, which matches nothing.
  EXPECT_FALSE(range("<3 >8").Contains(5));
  EXPECT_FALSE(range("<3 >8").Contains(1));
}

TEST(IdsTableTest, LeadingStrokeIsNotPoisonedByACycle) {
  // ㄅ and ㄆ contain each other; ㄆ also has an honest decomposition.
  auto table = IdsTable::FromTsv(
      "⿰ㄆ丁\tㄅ\n⿰ㄅ丁\tㄆ\n⿰一ㄅ\tㄆ\n", "");
  ASSERT_TRUE(table.ok());
  // Asking for ㄆ first walks into ㄅ while ㄆ is in progress; ㄅ's answer
  // there is a dead end that must not stick.
  EXPECT_EQ(table->LeadingStroke("ㄆ"), 1);
  EXPECT_EQ(table->LeadingStroke("ㄅ"), 1);
}

TEST(IdsTableTest, FindContainingFiltersByStrokeCount) {
  IdsTable table = LoadFixture();
  table.LoadStrokeCounts("某\t9\n相\t9\n謀\t16\n言\t7\n木\t4\n齒\t12,15\nbad line\n");
  EXPECT_TRUE(table.has_stroke_counts());
  const auto find = [&](std::string_view q) {
    const LookupQuery query = ParseLookupQuery(q);
    return table.FindContaining(query.components, query.strokes);
  };
  EXPECT_THAT(find("木"), ElementsAre("某", "相", "謀"));
  EXPECT_THAT(find("木9"), ElementsAre("某", "相"));
  EXPECT_THAT(find("木<10"), ElementsAre("某", "相"));
  EXPECT_THAT(find("木>9"), ElementsAre("謀"));
  EXPECT_THAT(find("木10-20"), ElementsAre("謀"));
  EXPECT_THAT(find("木>=9"), ElementsAre("某", "相", "謀"));
  EXPECT_THAT(find("木言16"), ElementsAre("謀"));
  EXPECT_THAT(find("木言9"), IsEmpty());
  // No components: every character with that many strokes (齒 has two counts).
  EXPECT_THAT(find("12"), ElementsAre("齒"));
  EXPECT_THAT(find("15"), ElementsAre("齒"));
  EXPECT_THAT(find("<5"), ElementsAre("木"));
}

TEST(IdsTableTest, StrokeFilterMatchesNothingWithoutStrokeData) {
  const IdsTable table = LoadFixture();
  EXPECT_FALSE(table.has_stroke_counts());
  const LookupQuery query = ParseLookupQuery("木9");
  EXPECT_THAT(table.FindContaining(query.components, query.strokes), IsEmpty());
  EXPECT_THAT(table.FindContaining("木"), Not(IsEmpty()));
}

TEST(IdsTableTest, StrokeCountIsTheFirstRecordedOne) {
  IdsTable table = LoadFixture();
  table.LoadStrokeCounts("謀\t16\n齒\t12,15\n");
  EXPECT_EQ(table.StrokeCount("謀"), 16);
  EXPECT_EQ(table.StrokeCount("齒"), 12);
  EXPECT_EQ(table.StrokeCount("某"), std::nullopt);
}

TEST(IdsTableTest, LeadingStrokeFromTheTableAndDecompositions) {
  IdsTable table = LoadFixture();
  // Strokes, radical forms and common components are in the built-in table.
  EXPECT_EQ(table.LeadingStroke("言"), 4);
  EXPECT_EQ(table.LeadingStroke("木"), 1);
  EXPECT_EQ(table.LeadingStroke("丿"), 3);
  EXPECT_EQ(table.LeadingStroke("氵"), 4);
  EXPECT_EQ(table.LeadingStroke("亻"), 3);
  EXPECT_EQ(table.LeadingStroke("目"), 2);
  EXPECT_EQ(table.LeadingStroke("水"), 5);  // 竖钩 is a turning stroke
  // Not in the table: the part written first decides.
  EXPECT_EQ(table.LeadingStroke("謀"), 4);  // ⿰言某
  EXPECT_EQ(table.LeadingStroke("相"), 1);  // ⿰木目
  EXPECT_EQ(table.LeadingStroke("某"), 1);  // ⿱廿木
  EXPECT_EQ(table.LeadingStroke("龘"), 0);
  EXPECT_EQ(table.LeadingStroke(""), 0);
}

TEST(IdsTableTest, LoadedLeadingStrokesOverrideTheTable) {
  IdsTable table = LoadFixture();
  EXPECT_EQ(table.LeadingStroke("謀"), 4);  // computed, and cached
  table.LoadLeadingStrokes("# comment\n謀\t2\nbad line\n某\t9\n");
  EXPECT_EQ(table.LeadingStroke("謀"), 2);
  EXPECT_EQ(table.LeadingStroke("某"), 1);  // class 9 is not a class: ignored
}

TEST(IdsTableTest, LeadingStrokeSkipsEnclosuresWrittenLast) {
  auto table = IdsTable::FromTsv("⿺辶首\t道\n⿶凵乂\t凶\n⿺走己\t起\n⿰⿱首首木\t𣖕\n", "");
  ASSERT_TRUE(table.ok());
  table->LoadLeadingStrokes("首\t4\n乂\t3\n走\t1\n");
  EXPECT_EQ(table->LeadingStroke("道"), 4);  // 首 before 辶
  EXPECT_EQ(table->LeadingStroke("凶"), 3);  // 乂 before 凵
  EXPECT_EQ(table->LeadingStroke("起"), 1);  // 走 is written first
  EXPECT_EQ(table->LeadingStroke("𣖕"), 4);  // nested layout: its first part
}

TEST(IdsTableTest, LeadingStrokeSurvivesCyclicData) {
  // Rare characters that are not in the built-in table, decomposing into each
  // other: only that it terminates (with "unknown").
  auto table = IdsTable::FromTsv("⿰㐁㐂\t㐀\n⿰㐀丁\t㐁\n", "");
  ASSERT_TRUE(table.ok());
  EXPECT_EQ(table->LeadingStroke("㐀"), 0);
  EXPECT_EQ(table->LeadingStroke("㐁"), 0);
}

TEST(IdsTableTest, SortsByStrokesThenLeadingStroke) {
  auto table = IdsTable::FromTsv("", "");
  ASSERT_TRUE(table.ok());
  // 㐀 has no leading stroke (not in the table, no decomposition).
  table->LoadStrokeCounts("甲\t5\n乙\t1\n丙\t5\n丁\t2\n㐀\t5\n己\t3\n");
  std::vector<std::string> chars = {"甲", "庚", "乙", "㐀", "丙", "丁", "己"};
  table->SortByStrokes(&chars);
  // From the table: 乙 5, 丁 1, 己 5, 丙 1 (一), 甲 2 (丨). 㐀 goes after the
  // five classes; 庚 has no stroke count, so it goes last.
  EXPECT_THAT(chars, ElementsAre("乙", "丁", "己", "丙", "甲", "㐀", "庚"));
}

TEST(IdsTableTest, SortByStrokesBreaksTiesByCodePoint) {
  IdsTable table = LoadFixture();
  table.LoadStrokeCounts("某\t9\n相\t9\n謀\t16\n");
  // 某 (⿱廿木) and 相 (⿰木目) both start with 一.
  std::vector<std::string> chars = {"謀", "相", "某"};
  table.SortByStrokes(&chars);
  EXPECT_THAT(chars, ElementsAre("某", "相", "謀"));
}

TEST(IdsTableTest, MissingDirectoryIsAnError) {
  EXPECT_FALSE(IdsTable::LoadFromDirectory("/nonexistent/dir").ok());
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
