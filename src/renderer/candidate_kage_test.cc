// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "renderer/candidate_kage.h"

#include <string>

#include "base/file_util.h"
#include "gui/character_composer/composer_kage.h"
#include "kage/kage_data.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace renderer {
namespace {

gui::KageResources LoadTestGlyphs() {
  const std::string path =
      testing::GetSourceFileOrDie({"kage", "testdata", "kage_test_data.tsv"});
  auto glyphs = kage::LoadBuhinFromFile(path);
  EXPECT_TRUE(glyphs.ok()) << glyphs.status();
  return {*std::move(glyphs), {}, {}};
}

TEST(CandidateKageTest, ResolvesIdsCompositionOutlines) {
  // Same components as composer_kage_test: ⿰木巻 becomes one outline set.
  const gui::KageResources resources = LoadTestGlyphs();
  const auto outlines = ResolveIdsCompositionOutlines("⿰木巻", resources);
  ASSERT_TRUE(outlines.has_value());
  EXPECT_FALSE(outlines->empty());
}

TEST(CandidateKageTest, ResolvesNamedCharacterOutlines) {
  const gui::KageResources resources = LoadTestGlyphs();
  const auto outlines = ResolveMissingFontOutlines("木", resources);
  ASSERT_TRUE(outlines.has_value());
  EXPECT_FALSE(outlines->empty());
}

TEST(CandidateKageTest, BundledDumpComposesUnencodedIds) {
  // Full dump (with layout/subtrees): ⿰亻国 is the Ctrl+2 SVG fallback case.
  const std::string kage_dir = FileUtil::Dirname(
      testing::GetSourceFileOrDie({"data", "kage", "kage_glyphs.tsv"}));
  auto resources = gui::KageResources::LoadFromDirectory(kage_dir);
  ASSERT_NE(resources, nullptr);
  const auto outlines =
      ResolveIdsCompositionOutlines("⿰亻国", *resources);
  ASSERT_TRUE(outlines.has_value());
  EXPECT_FALSE(outlines->empty());
}

}  // namespace
}  // namespace renderer
}  // namespace mozc
