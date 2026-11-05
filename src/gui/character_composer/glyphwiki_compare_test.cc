// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Raster / IoU unit checks, plus a reviewed baseline against the bundled
// GlyphWiki tables (no network, no extra dump). See docs/GLYPHWIKI_COMPARISON.md.

#include "gui/character_composer/glyphwiki_compare.h"

#include <string>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "composer_ids/ids_table.h"
#include "gui/character_composer/composer_kage.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace gui {
namespace {

Contour Square(double x0, double y0, double x1, double y1) {
  return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

TEST(GlyphwikiCompareTest, IdenticalOutlinesHaveIoUOne) {
  const std::vector<Contour> outlines = {Square(100, 100, 400, 400)};
  const GlyphBitmap a = RasterizeOutlines(outlines, 1000, 32);
  const GlyphBitmap b = RasterizeOutlines(outlines, 1000, 32);
  const OverlapScore score = ScoreOverlap(a, b);
  EXPECT_GT(a.ink, 0);
  EXPECT_NEAR(score.iou, 1.0, 1e-9);
  EXPECT_EQ(score.intersection, score.union_ink);
}

TEST(GlyphwikiCompareTest, DisjointOutlinesHaveIoUZero) {
  const GlyphBitmap a =
      RasterizeOutlines({Square(0, 0, 200, 200)}, 1000, 32);
  const GlyphBitmap b =
      RasterizeOutlines({Square(800, 800, 1000, 1000)}, 1000, 32);
  const OverlapScore score = ScoreOverlap(a, b);
  EXPECT_GT(a.ink, 0);
  EXPECT_GT(b.ink, 0);
  EXPECT_EQ(score.intersection, 0);
  EXPECT_NEAR(score.iou, 0.0, 1e-9);
}

TEST(GlyphwikiCompareTest, PartialOverlapIsBetweenZeroAndOne) {
  const GlyphBitmap a =
      RasterizeOutlines({Square(0, 0, 600, 600)}, 1000, 40);
  const GlyphBitmap b =
      RasterizeOutlines({Square(400, 400, 1000, 1000)}, 1000, 40);
  const OverlapScore score = ScoreOverlap(a, b);
  EXPECT_GT(score.iou, 0.05);
  EXPECT_LT(score.iou, 0.95);
}

TEST(GlyphwikiCompareTest, CharactersWithIdsLayoutSkipsIdentityRows) {
  const std::vector<std::string> chars = CharactersWithIdsLayout(
      "休\t休\n"
      "⿰亻木\t休\n"
      "⿰木木\t林\n"
      "⿰木木\t林\n"      // duplicate
      "⿵⺆冫\t⺼\n",  // Kangxi radical: dropped when filtering to unified
      /*unified_ideographs_only=*/true);
  ASSERT_EQ(chars.size(), 2);
  EXPECT_EQ(chars[0], "休");
  EXPECT_EQ(chars[1], "林");
}

TEST(GlyphwikiCompareTest, CheckBaselineRejectsRegression) {
  CompareResult r;
  r.character = "休";
  r.ok = true;
  r.score.iou = 0.50;
  std::string message;
  EXPECT_FALSE(CheckBaseline({r}, "休\t0.80\n", /*tolerance=*/0.05, &message));
  EXPECT_NE(message.find("休"), std::string::npos);
  EXPECT_TRUE(CheckBaseline({r}, "休\t0.50\n", /*tolerance=*/0.05, &message));
}

class GlyphwikiCompareRealDataTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const std::string ids_path = testing::GetSourceFileOrDie(
        {"data", "character_composer", "ids_table.tsv"});
    auto table =
        composer_ids::IdsTable::LoadFromDirectory(FileUtil::Dirname(ids_path));
    ASSERT_TRUE(table.ok()) << table.status();
    ids_ = new composer_ids::IdsTable(*std::move(table));

    const std::string kage_dir = FileUtil::Dirname(
        testing::GetSourceFileOrDie({"data", "kage", "kage_glyphs.tsv"}));
    auto kage = KageResources::LoadFromDirectory(kage_dir);
    ASSERT_NE(kage, nullptr);
    kage_ = kage.release();
  }

  static void TearDownTestSuite() {
    delete ids_;
    ids_ = nullptr;
    delete kage_;
    kage_ = nullptr;
  }

  static composer_ids::IdsTable *ids_;
  static KageResources *kage_;
};

composer_ids::IdsTable *GlyphwikiCompareRealDataTest::ids_ = nullptr;
KageResources *GlyphwikiCompareRealDataTest::kage_ = nullptr;

TEST_F(GlyphwikiCompareRealDataTest, ReviewedBaselineDoesNotRegress) {
  // A small, hand-picked set spanning several IDS operators. Scores were
  // recorded once against the bundled GlyphWiki tables; style differences are
  // accepted within |kTolerance|. Re-record after intentional layout changes.
  // Prefer characters whose GlyphWiki glyph is itself a 99: composition, so
  // the score measures layout rather than Mincho stroke style (林/炎/男/尖
  // are stroke-drawn on GlyphWiki and stay low for that reason).
  const std::vector<std::string> corpus = {
      "休", "相", "明", "昌", "岩", "困", "付", "他", "仰", "位",
  };
  const std::vector<CompareResult> results =
      CompareCharacters(corpus, *ids_, *kage_, /*view_box=*/1000,
                        /*bitmap_size=*/64);
  ASSERT_GE(results.size(), 8) << "too many characters failed to draw";

  const std::string baseline_path = testing::GetSourceFileOrDie(
      {"gui", "character_composer", "testdata",
       "glyphwiki_compare_baseline.tsv"});
  const auto baseline = FileUtil::GetContents(baseline_path);
  ASSERT_TRUE(baseline.ok()) << baseline.status();

  std::string message;
  constexpr double kTolerance = 0.05;
  EXPECT_TRUE(CheckBaseline(results, *baseline, kTolerance, &message))
      << message << "\n\nCurrent scores:\n"
      << FormatCompareReport(results);
}

TEST_F(GlyphwikiCompareRealDataTest, ComposerDrawingDiffersFromRegistered) {
  // Sanity: for 休 the composed IDS and GlyphWiki's registered glyph are both
  // drawable; IoU is high but not forced to 1 (layout / variant differences).
  const CompareResult r = CompareCharacter("休", *ids_, *kage_, 1000, 64);
  ASSERT_TRUE(r.ok) << r.skip_reason;
  EXPECT_EQ(r.ids, "⿰亻木");
  EXPECT_EQ(r.op, "⿰");
  EXPECT_GT(r.score.iou, 0.2);
  EXPECT_LE(r.score.iou, 1.0);
}

}  // namespace
}  // namespace gui
}  // namespace mozc
