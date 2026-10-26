// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/ids_tree.h"

#include <optional>
#include <string>
#include <vector>

#include "testing/gunit.h"

namespace mozc {
namespace composer_ids {
namespace {

using Slots = std::vector<ComposerSlot>;

TEST(IsSlotFilledTest, Leaves) {
  EXPECT_FALSE(IsSlotFilled(ComposerSlot::Leaf()));
  EXPECT_FALSE(IsSlotFilled(ComposerSlot::Leaf("  \xE3\x80\x80")));
  EXPECT_TRUE(IsSlotFilled(ComposerSlot::Leaf("言")));
}

TEST(IsSlotFilledTest, NestedNeedsEveryPart) {
  ComposerSlot partial = ComposerSlot::Nested(
      "⿱", {ComposerSlot::Leaf("攴"), ComposerSlot::Leaf()});
  EXPECT_FALSE(IsSlotFilled(partial));
  partial.parts[1] = ComposerSlot::Leaf("女");
  EXPECT_TRUE(IsSlotFilled(partial));
}

TEST(IsSlotFilledTest, ChecksAllThreePartsOfAThreePartOperator) {
  ComposerSlot slot =
      ComposerSlot::Nested("⿲", {ComposerSlot::Leaf("一"), ComposerSlot::Leaf(),
                                 ComposerSlot::Leaf("三")});
  EXPECT_FALSE(IsSlotFilled(slot));
  slot.parts[1] = ComposerSlot::Leaf("二");
  EXPECT_TRUE(IsSlotFilled(slot));
}

TEST(IsSlotFilledTest, RecursesThroughDeepNesting) {
  const ComposerSlot deep = ComposerSlot::Nested(
      "⿰",
      {ComposerSlot::Leaf("言"),
       ComposerSlot::Nested(
           "⿱", {ComposerSlot::Leaf("攴"), ComposerSlot::Leaf()})});
  EXPECT_FALSE(IsSlotFilled(deep));
}

TEST(ResizePartsForOperatorTest, KeepsBinaryToBinary) {
  const Slots parts = {ComposerSlot::Leaf("言"), ComposerSlot::Leaf()};
  EXPECT_EQ(ResizePartsForOperator("⿱", parts), parts);
}

TEST(ResizePartsForOperatorTest, PadsForThreePartOperator) {
  const Slots parts = {ComposerSlot::Leaf("一"), ComposerSlot::Leaf("二")};
  const Slots resized = ResizePartsForOperator("⿲", parts);
  ASSERT_EQ(resized.size(), 3);
  EXPECT_EQ(resized[0], parts[0]);
  EXPECT_EQ(resized[1], parts[1]);
  EXPECT_EQ(resized[2], ComposerSlot::Leaf());
}

TEST(ResizePartsForOperatorTest, TruncatesBackToBinary) {
  const Slots parts = {ComposerSlot::Leaf("一"), ComposerSlot::Leaf("二"),
                       ComposerSlot::Leaf("三")};
  EXPECT_EQ(ResizePartsForOperator("⿰", parts),
            (Slots{ComposerSlot::Leaf("一"), ComposerSlot::Leaf("二")}));
}

TEST(DraftIdsTest, EmptyFieldIsQuestionMarkAndLayoutsNest) {
  const Slots parts = {
      ComposerSlot::Leaf("木"),
      ComposerSlot::Nested("⿱", {ComposerSlot::Leaf(), ComposerSlot::Leaf()}),
      ComposerSlot::Leaf()};
  EXPECT_EQ(DraftIds("⿲", parts), "⿲木⿱???");
}

TEST(ComposeIdsTest, OnlyWhenEverythingIsFilled) {
  Slots parts = {ComposerSlot::Leaf("言"), ComposerSlot::Leaf()};
  EXPECT_EQ(ComposeIds("⿰", parts), std::nullopt);
  parts[1] = ComposerSlot::Leaf(" 某 ");
  EXPECT_EQ(ComposeIds("⿰", parts), "⿰言某");
}

TEST(ApplyPaletteOperatorTest, OuterSquareChangesLayoutAndKeepsText) {
  const Slots binary = {ComposerSlot::Leaf("木"), ComposerSlot::Leaf()};
  const Layout next = ApplyPaletteOperator("⿱", binary, {}, "⿲");
  EXPECT_EQ(next.op, "⿲");
  ASSERT_EQ(next.parts.size(), 3);
  EXPECT_EQ(next.parts[0], ComposerSlot::Leaf("木"));
}

TEST(ApplyPaletteOperatorTest, SelectedFieldBecomesLayoutKeepingItsText) {
  const Slots binary = {ComposerSlot::Leaf("木"), ComposerSlot::Leaf()};
  const Layout next = ApplyPaletteOperator("⿰", binary, {0}, "⿱");
  EXPECT_EQ(next.op, "⿰");
  EXPECT_EQ(next.parts[0],
            ComposerSlot::Nested(
                "⿱", {ComposerSlot::Leaf("木"), ComposerSlot::Leaf()}));
  EXPECT_EQ(next.parts[1], ComposerSlot::Leaf());
}

TEST(ApplyPaletteOperatorTest, SelectedInnerLayoutChangesItsOwnShape) {
  const Slots binary = {ComposerSlot::Leaf("木"), ComposerSlot::Leaf()};
  const Layout nested = ApplyPaletteOperator("⿰", binary, {1}, "⿱");
  const Layout changed = ApplyPaletteOperator("⿰", nested.parts, {1}, "⿴");
  EXPECT_EQ(changed.op, "⿰");
  EXPECT_EQ(changed.parts[1].kind, ComposerSlot::Kind::kNested);
  EXPECT_EQ(changed.parts[1].op, "⿴");
  EXPECT_EQ(changed.parts[0], ComposerSlot::Leaf("木"));
}

TEST(ApplyPaletteOperatorTest, InvalidSelectionChangesNothing) {
  const Slots binary = {ComposerSlot::Leaf("木"), ComposerSlot::Leaf()};
  const Layout next = ApplyPaletteOperator("⿰", binary, {5}, "⿱");
  EXPECT_EQ(next.op, "⿰");
  EXPECT_EQ(next.parts, binary);
}

TEST(FlattenNestedSlotTest, EmptyLayoutBecomesAnEmptyField) {
  const auto flat = FlattenNestedSlot(ComposerSlot::Nested(
      "⿰", {ComposerSlot::Leaf(), ComposerSlot::Leaf()}));
  ASSERT_TRUE(flat.has_value());
  EXPECT_EQ(*flat, ComposerSlot::Leaf());
}

TEST(FlattenNestedSlotTest, KeepsTheOnePartWithContent) {
  const auto flat = FlattenNestedSlot(ComposerSlot::Nested(
      "⿱", {ComposerSlot::Leaf("木"), ComposerSlot::Leaf()}));
  ASSERT_TRUE(flat.has_value());
  EXPECT_EQ(*flat, ComposerSlot::Leaf("木"));
}

TEST(FlattenNestedSlotTest, RefusesWhenTwoPartsHaveContent) {
  EXPECT_EQ(FlattenNestedSlot(ComposerSlot::Nested(
                "⿰", {ComposerSlot::Leaf("木"), ComposerSlot::Leaf("口")})),
            std::nullopt);
}

TEST(SlotPathTest, SlotAtAndReplaceSlotAt) {
  const Slots parts = {
      ComposerSlot::Leaf("木"),
      ComposerSlot::Nested("⿱", {ComposerSlot::Leaf("日"),
                                 ComposerSlot::Leaf()})};
  EXPECT_EQ(SlotAt(parts, {}), nullptr);
  ASSERT_NE(SlotAt(parts, {1, 0}), nullptr);
  EXPECT_EQ(SlotAt(parts, {1, 0})->input, "日");
  EXPECT_EQ(SlotAt(parts, {0, 0}), nullptr);  // cannot descend into a leaf
  EXPECT_EQ(SlotAt(parts, {2}), nullptr);
  EXPECT_EQ(SlotAt(parts, {-1}), nullptr);

  const Slots replaced = ReplaceSlotAt(parts, {1, 1}, ComposerSlot::Leaf("月"));
  EXPECT_EQ(SlotAt(replaced, {1, 1})->input, "月");
  EXPECT_EQ(SlotAt(parts, {1, 1})->input, "");  // the original is untouched
  EXPECT_EQ(ReplaceSlotAt(parts, {0, 0}, ComposerSlot::Leaf("x")), parts);
}

TEST(ParseIdsToComposerTreeTest, FlatIds) {
  const auto parsed = ParseIdsToComposerTree("⿰言某");
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->op, "⿰");
  EXPECT_EQ(parsed->parts,
            (Slots{ComposerSlot::Leaf("言"), ComposerSlot::Leaf("某")}));
  EXPECT_EQ(parsed->unencoded_count, 0);
}

TEST(ParseIdsToComposerTreeTest, NestedAndThreePart) {
  const auto parsed = ParseIdsToComposerTree("⿲木⿱日月口");
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->op, "⿲");
  ASSERT_EQ(parsed->parts.size(), 3);
  EXPECT_EQ(parsed->parts[1],
            ComposerSlot::Nested(
                "⿱", {ComposerSlot::Leaf("日"), ComposerSlot::Leaf("月")}));
  EXPECT_EQ(parsed->parts[2], ComposerSlot::Leaf("口"));
}

TEST(ParseIdsToComposerTreeTest, UnencodedPlaceholdersBecomeEmptySlots) {
  const auto parsed = ParseIdsToComposerTree("⿰言⑤");
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->parts[1], ComposerSlot::Leaf());
  EXPECT_EQ(parsed->unencoded_count, 1);
}

TEST(ParseIdsToComposerTreeTest, RejectsMalformedInput) {
  EXPECT_EQ(ParseIdsToComposerTree(""), std::nullopt);
  EXPECT_EQ(ParseIdsToComposerTree("木"), std::nullopt);       // no operator
  EXPECT_EQ(ParseIdsToComposerTree("⿰木"), std::nullopt);      // too short
  EXPECT_EQ(ParseIdsToComposerTree("⿰木口日"), std::nullopt);  // trailing
}

TEST(ParseIdsToComposerTreeTest, DeepNestingDoesNotOverflow) {
  std::string deep;
  for (int i = 0; i < 100000; ++i) deep += "⿰木";
  EXPECT_EQ(ParseIdsToComposerTree(deep), std::nullopt);
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
