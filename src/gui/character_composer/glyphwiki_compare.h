// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Test-time comparison of the composer's IDS drawing with GlyphWiki's
// registered glyph for the same character. See docs/GLYPHWIKI_COMPARISON.md.
// Links //kage (GPL-3.0-or-later); anything that links this is GPL as shipped.

#ifndef MOZC_GUI_CHARACTER_COMPOSER_GLYPHWIKI_COMPARE_H_
#define MOZC_GUI_CHARACTER_COMPOSER_GLYPHWIKI_COMPARE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "composer_ids/ids_table.h"
#include "gui/character_composer/composer_kage.h"

namespace mozc {
namespace gui {

// One closed contour rasterised into a square bitmap of side |size|.
// Coordinates are in the same user units as ComposerOutlines (view_box).
struct GlyphBitmap {
  int size = 0;
  std::vector<uint8_t> pixels;  // 1 = ink, 0 = empty; row-major
  int ink = 0;
};

// Non-zero fill on a |size|×|size| grid covering [0, view_box] × [0, view_box].
GlyphBitmap RasterizeOutlines(const std::vector<Contour> &outlines,
                              int view_box = 1000, int size = 128);

struct OverlapScore {
  double iou = 0;         // |A∩B| / |A∪B|; 0 when both empty
  int intersection = 0;
  int union_ink = 0;
  int contour_delta = 0;  // |composer contours − reference contours|
};

OverlapScore ScoreOverlap(const GlyphBitmap &composer,
                          const GlyphBitmap &reference);

struct CompareResult {
  std::string character;
  std::string ids;
  std::string op;  // top-level IDS operator, or empty
  OverlapScore score;
  bool ok = false;  // false when either drawing could not be rendered
  // True when GlyphWiki's registered glyph is itself a 99: composition of
  // components (layout comparison). False when it is hand-tuned stroke data
  // (style noise in the ranked report).
  bool glyphwiki_is_composition = false;
  std::string skip_reason;
};

// Compares the composer's drawing of the first recorded IDS layout for
// |character| with GlyphWiki's registered glyph. |character| is one Unicode
// scalar.
CompareResult CompareCharacter(std::string_view character,
                               const composer_ids::IdsTable &ids,
                               const KageResources &resources,
                               int view_box = 1000, int bitmap_size = 128);

// Scores every character in |characters| that both sides can draw; sorted
// worst IoU first. Characters that cannot be scored are omitted.
std::vector<CompareResult> CompareCharacters(
    const std::vector<std::string> &characters,
    const composer_ids::IdsTable &ids, const KageResources &resources,
    int view_box = 1000, int bitmap_size = 128);

// Characters that appear as the value of an IDS layout row in |ids_tsv|
// ("⿰亻木<TAB>休"). Deduplicated, in file order. Identity rows ("休<TAB>休")
// are skipped. When |unified_ideographs_only| is true, keeps only characters
// in CJK Unified Ideographs or Extension A (what the composer usually draws).
std::vector<std::string> CharactersWithIdsLayout(
    std::string_view ids_tsv, bool unified_ideographs_only = true);

// Text report: ranked worst-first, then per-operator and per-component means.
std::string FormatCompareReport(const std::vector<CompareResult> &results);

// Baseline file: "character<TAB>iou" per line (# comments). Returns false and
// sets |message| when a listed character's IoU falls more than |tolerance|
// below the baseline, or when a listed character cannot be scored.
bool CheckBaseline(const std::vector<CompareResult> &results,
                   std::string_view baseline_tsv, double tolerance,
                   std::string *message);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_GLYPHWIKI_COMPARE_H_
