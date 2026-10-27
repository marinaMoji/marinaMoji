// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/composer_kage.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "composer_ids/ids_tree.h"
#include "gui/character_composer/composer_svg.h"
#include "kage/kage_data.h"
#include "kage/kage_engine.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace gui {
namespace {

using composer_ids::ComposerSlot;

// A few real GlyphWiki glyphs (木 巻 女 心 国 玉 ...), see kage/testdata.
kage::Buhin LoadGlyphs() {
  const std::string path =
      testing::GetSourceFileOrDie({"kage", "testdata", "kage_test_data.tsv"});
  auto glyphs = kage::LoadBuhinFromFile(path);
  EXPECT_TRUE(glyphs.ok()) << glyphs.status();
  return *std::move(glyphs);
}

std::vector<ComposerSlot> Parts(std::string_view a, std::string_view b) {
  return {ComposerSlot::Leaf(a), ComposerSlot::Leaf(b)};
}

TEST(ComposerKageTest, PlacesPartsInTheirBoxes) {
  kage::Buhin nested;
  EXPECT_EQ(ComposeKageData("⿰", Parts("木", "巻"), &nested),
            "99:0:0:0:0:108:200:u6728:0:0:0$99:0:0:92:0:200:200:u5dfb:0:0:0");
  EXPECT_EQ(ComposeKageData("⿱", Parts("女", "心"), &nested),
            "99:0:0:0:0:200:108:u5973:0:0:0$99:0:0:0:92:200:200:u5fc3:0:0:0");
}

TEST(ComposerKageTest, NestedLayoutBecomesAGlyphOfItsOwn) {
  kage::Buhin nested;
  const std::string data = ComposeKageData(
      "⿰", {ComposerSlot::Leaf("木"), ComposerSlot::Nested("⿱", Parts("女", "心"))},
      &nested);
  EXPECT_EQ(data,
            "99:0:0:0:0:108:200:u6728:0:0:0$99:0:0:92:0:200:200:__nested0:0:0:0");
  EXPECT_EQ(nested.size(), 1);
}

TEST(ComposerKageTest, IncompleteLayoutHasNoData) {
  kage::Buhin nested;
  EXPECT_EQ(ComposeKageData("⿰", Parts("木", ""), &nested), "");
}

TEST(ComposerKageTest, DrawsOneOutlineFromBothComponents) {
  const kage::Buhin glyphs = LoadGlyphs();
  const auto outlines = ComposerOutlines("⿰", Parts("木", "巻"), glyphs, 1000);
  ASSERT_TRUE(outlines.has_value());
  EXPECT_GT(outlines->size(), 10);  // many strokes
  double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
  for (const Contour &c : *outlines) {
    for (const auto &[x, y] : c) {
      min_x = std::min(min_x, x);
      max_x = std::max(max_x, x);
      min_y = std::min(min_y, y);
      max_y = std::max(max_y, y);
    }
  }
  // It fills the whole square, as one character does: not two small glyphs.
  EXPECT_LT(min_x, 150);
  EXPECT_GT(max_x, 850);
  EXPECT_LT(min_y, 150);
  EXPECT_GT(max_y, 850);
}

TEST(ComposerKageTest, ComponentWithoutDataFallsBack) {
  const kage::Buhin glyphs = LoadGlyphs();
  EXPECT_FALSE(ComposerOutlines("⿰", Parts("木", "𠀀"), glyphs, 1000).has_value());
}

TEST(ComposerKageTest, SvgIsAPathWhenGlyphsAreGivenAndTextWhenNot) {
  const kage::Buhin glyphs = LoadGlyphs();
  const std::string drawn = RenderComposerSvg("⿰", Parts("木", "巻"), 1000, &glyphs);
  EXPECT_NE(drawn.find("<path"), std::string::npos);
  EXPECT_EQ(drawn.find("<text"), std::string::npos);

  const std::string text = RenderComposerSvg("⿰", Parts("木", "巻"), 1000, nullptr);
  EXPECT_NE(text.find("<text"), std::string::npos);
  EXPECT_EQ(text.find("<path"), std::string::npos);
}

}  // namespace
}  // namespace gui
}  // namespace mozc
