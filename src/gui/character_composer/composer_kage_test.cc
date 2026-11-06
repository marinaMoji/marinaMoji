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
KageResources LoadGlyphs() {
  const std::string path =
      testing::GetSourceFileOrDie({"kage", "testdata", "kage_test_data.tsv"});
  auto glyphs = kage::LoadBuhinFromFile(path);
  EXPECT_TRUE(glyphs.ok()) << glyphs.status();
  return {*std::move(glyphs), {}};
}

std::vector<ComposerSlot> Parts(std::string_view a, std::string_view b) {
  return {ComposerSlot::Leaf(a), ComposerSlot::Leaf(b)};
}

TEST(ComposerKageTest, PlacesPartsInTheirBoxes) {
  // Defaults follow GlyphWiki's median overlap (not clean halves).
  kage::Buhin nested;
  EXPECT_EQ(ComposeKageData("⿰", Parts("木", "巻"), &nested),
            "99:0:0:2:0:167:200:u6728:0:0:0$99:0:0:49:0:198:200:u5dfb:0:0:0");
  EXPECT_EQ(ComposeKageData("⿱", Parts("女", "心"), &nested),
            "99:0:0:0:4:200:130:u5973:0:0:0$99:0:0:0:50:200:195:u5fc3:0:0:0");
}

TEST(ComposerKageTest, ParsesTheLayoutTable) {
  const ComposerLayout layout = ComposerLayout::Parse(
      "# comment\n"
      "⿰\t0\t木\tu6728-01\t2 0 165 200\t442\n"
      "⿱\t1\t心\tu5fc3-04\t0 25 200 200\t487\n"
      "bad line\n");
  EXPECT_EQ(layout.size(), 2);
  const ComposerLayout::Entry *e = layout.Find("⿰", 0, "木");
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->variant, "u6728-01");
  EXPECT_EQ(e->left, 2);
  EXPECT_EQ(e->right, 165);
  EXPECT_EQ(layout.Find("⿰", 1, "木"), nullptr);  // position matters
  EXPECT_EQ(layout.Find("⿱", 0, "木"), nullptr);  // so does the operator
}

TEST(ComposerKageTest, UsesTheVariantAndBoxGlyphWikiUses) {
  KageResources r = LoadGlyphs();
  r.glyphs.Set("u6728-01", std::string(r.glyphs.Search("u6728")));
  r.layout = ComposerLayout::Parse("⿰\t0\t木\tu6728-01\t2 0 165 200\t442\n");
  kage::Buhin nested(&r.glyphs);
  // -01 expands the left box's right edge to at least 170.
  EXPECT_EQ(ComposeKageData("⿰", Parts("木", "巻"), &nested, &r.layout),
            "99:0:0:2:0:170:200:u6728-01:0:0:0$99:0:0:49:0:198:200:u5dfb:0:0:0");
}

TEST(ComposerKageTest, PrefersAPositionalVariantWhenTheTableNamesTheBareGlyph) {
  // GlyphWiki's layout table often records the bare uXXXX even when a -01
  // form exists; the composer should still draw the narrowed left form.
  KageResources r = LoadGlyphs();
  r.glyphs.Set("u6728-01", std::string(r.glyphs.Search("u6728")));
  r.layout = ComposerLayout::Parse("⿰\t0\t木\tu6728\t5 0 115 200\t5\n");
  kage::Buhin nested(&r.glyphs);
  EXPECT_EQ(ComposeKageData("⿰", Parts("木", "巻"), &nested, &r.layout),
            "99:0:0:5:0:170:200:u6728-01:0:0:0$99:0:0:49:0:198:200:u5dfb:0:0:0");
}

TEST(ComposerKageTest, DoesNotOverrideAChosenRegionVariant) {
  // A context row may name u76ee-j; PreferPositionalVariant must not smash
  // that into u76ee-02 just because -02 exists.
  KageResources r = LoadGlyphs();
  r.glyphs.Set("u76ee", "1:0:0:0:0:200:200");
  r.glyphs.Set("u76ee-02", "1:0:0:0:0:200:200");
  r.glyphs.Set("u76ee-j", "1:0:0:0:0:200:200");
  r.glyphs.Set("u6728-01", std::string(r.glyphs.Search("u6728")));
  r.layout = ComposerLayout::Parse(
      "⿰\t0\t木\tu6728-01\t0 0 208 200\t1\n"
      "⿰\t1\t目\tu76ee-02\t0 0 201 200\t12\n"
      "⿰\t1\t目\tu76ee-j\t80 0 200 200\t1\t木\n");
  kage::Buhin nested(&r.glyphs);
  const std::string data =
      ComposeKageData("⿰", Parts("木", "目"), &nested, &r.layout);
  EXPECT_NE(data.find("u76ee-j"), std::string::npos) << data;
  EXPECT_EQ(data.find("u76ee-02"), std::string::npos) << data;
}

TEST(ComposerKageTest, UnknownComponentTakesTheUsualBoxOfItsPosition) {
  KageResources r = LoadGlyphs();
  r.layout = ComposerLayout::Parse(
      "⿰\t1\t日\tu65e5\t40 0 200 200\t10\n"
      "⿰\t1\t月\tu6708\t60 0 198 200\t10\n"
      "⿰\t1\t山\tu5c71\t50 0 196 200\t10\n");
  kage::Buhin nested(&r.glyphs);
  // No entry for 木/巻: left keeps the ⿰ default; right takes the position
  // median (50..198 from the three entries above).
  EXPECT_EQ(ComposeKageData("⿰", Parts("木", "巻"), &nested, &r.layout),
            "99:0:0:2:0:167:200:u6728:0:0:0$99:0:0:50:0:198:200:u5dfb:0:0:0");
}

TEST(ComposerKageTest, IgnoresAVariantThatHasNoStrokes) {
  KageResources r = LoadGlyphs();  // has no u6728-01
  r.layout = ComposerLayout::Parse("⿰\t0\t木\tu6728-01\t2 0 165 200\t442\n");
  kage::Buhin nested(&r.glyphs);
  // Missing -01: fall back to bare u6728 with the ⿰ default boxes.
  EXPECT_EQ(ComposeKageData("⿰", Parts("木", "巻"), &nested, &r.layout),
            "99:0:0:2:0:167:200:u6728:0:0:0$99:0:0:49:0:198:200:u5dfb:0:0:0");
}

TEST(ComposerKageTest, NestedLayoutBecomesAGlyphOfItsOwn) {
  kage::Buhin nested;
  const std::string data = ComposeKageData(
      "⿰", {ComposerSlot::Leaf("木"), ComposerSlot::Nested("⿱", Parts("女", "心"))},
      &nested);
  EXPECT_EQ(data,
            "99:0:0:2:0:167:200:u6728:0:0:0$99:0:0:49:0:198:200:__nested0:0:0:0");
  EXPECT_EQ(nested.size(), 1);
}

TEST(ComposerKageTest, UsesAReadyMadeSubtreeGlyph) {
  KageResources r = LoadGlyphs();
  // Pretend GlyphWiki already drew ⿱女心 as this compound.
  r.glyphs.Set("u2ff1-u5973-u5fc3", "1:0:0:0:0:200:200");
  r.subtrees = ComposerSubtrees::Parse("⿱女心\tu2ff1-u5973-u5fc3\t3\n");
  kage::Buhin nested(&r.glyphs);
  const std::string data = ComposeKageData(
      "⿰", {ComposerSlot::Leaf("木"), ComposerSlot::Nested("⿱", Parts("女", "心"))},
      &nested, &r.layout, &r.subtrees);
  EXPECT_NE(data.find("u2ff1-u5973-u5fc3"), std::string::npos) << data;
  EXPECT_EQ(data.find("__nested"), std::string::npos) << data;
  EXPECT_EQ(nested.size(), 0);  // no synthesized child
}

TEST(ComposerKageTest, BundledSubtreesCoverKan) {
  // 侃 = ⿰亻⿱口𫶧; the bundled table maps the nested slot to GlyphWiki's
  // compound so we do not synthesize __nested0.
  const std::string kage_dir = FileUtil::Dirname(
      testing::GetSourceFileOrDie({"data", "kage", "kage_glyphs.tsv"}));
  auto resources = KageResources::LoadFromDirectory(kage_dir);
  ASSERT_NE(resources, nullptr);
  ASSERT_GT(resources->subtrees.size(), 0u);
  const std::string *ready = resources->subtrees.Find("⿱口𫶧");
  ASSERT_NE(ready, nullptr);
  EXPECT_EQ(*ready, "u2ff1-u53e3-u2bda7");
  kage::Buhin nested(&resources->glyphs);
  const std::string data = ComposeKageData(
      "⿰",
      {ComposerSlot::Leaf("亻"),
       ComposerSlot::Nested("⿱", Parts("口", "𫶧"))},
      &nested, &resources->layout, &resources->subtrees);
  EXPECT_NE(data.find("u2ff1-u53e3-u2bda7"), std::string::npos) << data;
  EXPECT_EQ(data.find("__nested"), std::string::npos) << data;
  // GlyphWiki's u4f83-j places the compound at ~52:0:197:200; our ⿰ default
  // + harmonize should land in the same neighbourhood, not a near-full box.
  EXPECT_EQ(data.find(":17:0:200:200:u2ff1"), std::string::npos) << data;
}

TEST(ComposerKageTest, FallsBackToSystematicCompoundName) {
  // No subtrees table: still use u2ff1-… when that glyph is in the dump.
  KageResources r = LoadGlyphs();
  r.glyphs.Set("u2ff1-u5973-u5fc3", "1:0:0:0:0:200:200");
  kage::Buhin nested(&r.glyphs);
  const std::string data = ComposeKageData(
      "⿰", {ComposerSlot::Leaf("木"), ComposerSlot::Nested("⿱", Parts("女", "心"))},
      &nested, nullptr, nullptr);
  EXPECT_NE(data.find("u2ff1-u5973-u5fc3"), std::string::npos) << data;
  EXPECT_EQ(data.find("__nested"), std::string::npos) << data;
}

TEST(ComposerKageTest, IncompleteLayoutHasNoData) {
  kage::Buhin nested;
  EXPECT_EQ(ComposeKageData("⿰", Parts("木", ""), &nested), "");
}

TEST(ComposerKageTest, DrawsOneOutlineFromBothComponents) {
  const KageResources glyphs = LoadGlyphs();
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
  const KageResources glyphs = LoadGlyphs();
  EXPECT_FALSE(ComposerOutlines("⿰", Parts("木", "𠀀"), glyphs, 1000).has_value());
}

TEST(ComposerKageTest, SvgIsAPathWhenGlyphsAreGivenAndTextWhenNot) {
  const KageResources glyphs = LoadGlyphs();
  const std::string drawn = RenderComposerSvg("⿰", Parts("木", "巻"), 1000, &glyphs);
  EXPECT_NE(drawn.find("<path"), std::string::npos);
  EXPECT_EQ(drawn.find("<text"), std::string::npos);

  const std::string text = RenderComposerSvg("⿰", Parts("木", "巻"), 1000, nullptr);
  EXPECT_NE(text.find("<text"), std::string::npos);
  EXPECT_EQ(text.find("<path"), std::string::npos);
}

TEST(ComposerKageTest, NamedGlyphOutlinesUsesRegisteredUnicodeGlyph) {
  const KageResources glyphs = LoadGlyphs();
  EXPECT_EQ(GlyphWikiNameForCharacter("木"), "u6728");
  const auto outlines = NamedGlyphOutlines("木", glyphs, 1000);
  ASSERT_TRUE(outlines.has_value());
  EXPECT_FALSE(outlines->empty());
  EXPECT_FALSE(NamedGlyphOutlines("木巻", glyphs, 1000).has_value());
}

TEST(ComposerKageTest, GlyphWikiSystematicName) {
  using composer_ids::ComposerSlot;
  EXPECT_EQ(GlyphWikiSystematicName(
                "⿰", {ComposerSlot::Leaf("木"), ComposerSlot::Leaf("馬")}),
            "u2ff0-u6728-u99ac");
  // Nested structures nest the same way GlyphWiki names them.
  EXPECT_EQ(GlyphWikiSystematicName(
                "⿱", {ComposerSlot::Leaf("口"),
                       ComposerSlot::Nested("⿰", {ComposerSlot::Leaf("木"),
                                                   ComposerSlot::Leaf("馬")})}),
            "u2ff1-u53e3-u2ff0-u6728-u99ac");
  // No name when a slot is empty.
  EXPECT_EQ(GlyphWikiSystematicName(
                "⿰", {ComposerSlot::Leaf("木"), ComposerSlot::Leaf("")}),
            "");
}

}  // namespace
}  // namespace gui
}  // namespace mozc
