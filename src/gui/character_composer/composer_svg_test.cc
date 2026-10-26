// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/composer_svg.h"

#include "composer_ids/ids_tree.h"
#include "testing/gunit.h"

namespace mozc {
namespace gui {
namespace {

TEST(ComposerSvgTest, EmptyWhenIncomplete) {
  EXPECT_TRUE(RenderComposerSvg("⿰", {composer_ids::ComposerSlot::Leaf("言"),
                                       composer_ids::ComposerSlot::Leaf("")})
                  .empty());
}

TEST(ComposerSvgTest, LeftRightContainsBothParts) {
  const std::string svg = RenderComposerSvg(
      "⿰", {composer_ids::ComposerSlot::Leaf("言"),
             composer_ids::ComposerSlot::Leaf("某")});
  ASSERT_FALSE(svg.empty());
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find("言"), std::string::npos);
  EXPECT_NE(svg.find("某"), std::string::npos);
  EXPECT_NE(svg.find("⿰言某"), std::string::npos);
}

TEST(ComposerSvgTest, NestedLayout) {
  auto inner = composer_ids::ComposerSlot::Nested(
      "⿱", {composer_ids::ComposerSlot::Leaf("木"),
             composer_ids::ComposerSlot::Leaf("日")});
  const std::string svg = RenderComposerSvg(
      "⿰", {composer_ids::ComposerSlot::Leaf("言"), inner});
  ASSERT_FALSE(svg.empty());
  EXPECT_NE(svg.find("木"), std::string::npos);
  EXPECT_NE(svg.find("日"), std::string::npos);
  EXPECT_NE(svg.find("言"), std::string::npos);
}

}  // namespace
}  // namespace gui
}  // namespace mozc
