// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Prints a ranked GlyphWiki-vs-composer overlap report. Developer / test-time
// only; not shipped to users. See docs/GLYPHWIKI_COMPARISON.md.
//
//   bazel run //gui/character_composer:glyphwiki_compare -- \
//       [--limit=N] [--bitmap=128]

#include <iostream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "base/file_util.h"
#include "base/init_mozc.h"
#include "composer_ids/ids_table.h"
#include "gui/character_composer/composer_kage.h"
#include "gui/character_composer/glyphwiki_compare.h"
#include "testing/mozctest.h"

ABSL_FLAG(int32_t, limit, 200,
          "Attempt at most this many IDS-layout characters from the table "
          "(0 = all). Successful drawings are ranked; raise for a fuller "
          "report.");
ABSL_FLAG(int32_t, bitmap, 64,
          "Raster grid size (smaller is faster; 64 is enough for a report).");
ABSL_FLAG(bool, all_blocks, false,
          "Include radicals and non-unified characters; default keeps CJK "
          "Unified Ideographs and Extension A only.");

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  const std::string ids_path = mozc::testing::GetSourceFileOrDie(
      {"data", "character_composer", "ids_table.tsv"});
  const std::string kage_dir = mozc::FileUtil::Dirname(
      mozc::testing::GetSourceFileOrDie({"data", "kage", "kage_glyphs.tsv"}));

  auto table =
      mozc::composer_ids::IdsTable::LoadFromDirectory(
          mozc::FileUtil::Dirname(ids_path));
  if (!table.ok()) {
    std::cerr << "Failed to load IDS table: " << table.status() << "\n";
    return 1;
  }
  auto kage = mozc::gui::KageResources::LoadFromDirectory(kage_dir);
  if (kage == nullptr) {
    std::cerr << "Failed to load KAGE data from " << kage_dir << "\n";
    return 1;
  }

  const auto ids_tsv = mozc::FileUtil::GetContents(ids_path);
  if (!ids_tsv.ok()) {
    std::cerr << "Failed to read " << ids_path << ": " << ids_tsv.status()
              << "\n";
    return 1;
  }

  std::vector<std::string> characters = mozc::gui::CharactersWithIdsLayout(
      *ids_tsv, /*unified_ideographs_only=*/!absl::GetFlag(FLAGS_all_blocks));
  if (absl::GetFlag(FLAGS_limit) > 0 &&
      static_cast<int>(characters.size()) > absl::GetFlag(FLAGS_limit)) {
    characters.resize(absl::GetFlag(FLAGS_limit));
  }

  const std::vector<mozc::gui::CompareResult> results =
      mozc::gui::CompareCharacters(characters, *table, *kage, /*view_box=*/1000,
                                   absl::GetFlag(FLAGS_bitmap));
  std::cout << mozc::gui::FormatCompareReport(results);
  return 0;
}
