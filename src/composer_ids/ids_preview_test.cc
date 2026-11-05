// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/ids_preview.h"

#include "testing/gunit.h"

namespace mozc {
namespace composer_ids {
namespace {

TEST(IdsPreviewTest, LeftRight) {
  const auto p = ParseIdsPreview("⿰忄亜");
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(p->op, "⿰");
  ASSERT_EQ(p->parts.size(), 2);
  EXPECT_EQ(p->parts[0], "忄");
  EXPECT_EQ(p->parts[1], "亜");
  ASSERT_EQ(p->cells.size(), 2);
  EXPECT_DOUBLE_EQ(p->cells[0].w, 0.5);
  EXPECT_DOUBLE_EQ(p->cells[1].x, 0.5);
}

TEST(IdsPreviewTest, AboveBelowAndThreeParts) {
  const auto above = ParseIdsPreview("⿱艹白");
  ASSERT_TRUE(above.has_value());
  EXPECT_DOUBLE_EQ(above->cells[1].y, 0.5);
  const auto three = ParseIdsPreview("⿲彳山女");
  ASSERT_TRUE(three.has_value());
  ASSERT_EQ(three->parts.size(), 3);
  EXPECT_DOUBLE_EQ(three->cells[2].x, 2.0 / 3.0);
  const auto stacked = ParseIdsPreview("⿳艹日木");
  ASSERT_TRUE(stacked.has_value());
  EXPECT_DOUBLE_EQ(stacked->cells[2].y, 2.0 / 3.0);
}

TEST(IdsPreviewTest, RejectsWhatItCannotDraw) {
  EXPECT_FALSE(ParseIdsPreview("").has_value());
  EXPECT_FALSE(ParseIdsPreview("悪").has_value());
  EXPECT_FALSE(ParseIdsPreview("⿰忄").has_value());      // too few parts
  EXPECT_FALSE(ParseIdsPreview("⿰忄亜心").has_value());  // too many
  EXPECT_FALSE(ParseIdsPreview("⿲忄亜").has_value());    // ⿲ needs three
  EXPECT_FALSE(ParseIdsPreview("⿴口人").has_value());    // enclosure: not simple
  EXPECT_FALSE(ParseIdsPreview("⿰⿱艹白心").has_value()); // nested
  EXPECT_FALSE(ParseIdsPreview("abc").has_value());
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
