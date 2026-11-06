// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Shared GlyphWiki/KAGE outline resolution for candidate windows (Linux/Qt,
// macOS, Windows). Contours are filled by each platform's paint code.
// Linking this pulls //kage (GPL-3.0-or-later).

#ifndef MOZC_RENDERER_CANDIDATE_KAGE_H_
#define MOZC_RENDERER_CANDIDATE_KAGE_H_

#include <optional>
#include <vector>

#include "absl/strings/string_view.h"
#include "gui/character_composer/composer_kage.h"

namespace mozc {
namespace renderer {

// Resolves outlines for one Unicode character (amber-plate drawings):
//   1. bundled GlyphWiki dump (uXXXX)
//   2. on-disk GlyphWiki cache
//   3. IDS decomposition composed from parts (dump + disk-cached leaves)
//   4. GlyphWiki network fetch for the character (cached to disk)
std::optional<std::vector<gui::Contour>> ResolveMissingFontOutlines(
    absl::string_view ch, const gui::KageResources& resources);

// GlyphWiki composition for an unencoded IDS string (⿰亻国, …) — same path
// the character composer uses, not the naive half/half font preview.
std::optional<std::vector<gui::Contour>> ResolveIdsCompositionOutlines(
    absl::string_view ids, const gui::KageResources& resources);

// Cached outlines keyed by a Unicode character or a full IDS string. Loads
// bundled KAGE data from GetServerDirectory()/character_composer once.
// Returns nullptr if |key| is empty or the table is missing.
const std::optional<std::vector<gui::Contour>>* CachedKageOutlines(
    absl::string_view key);

bool HasKageOutlineForKey(absl::string_view key);
bool HasKageOutlineForCharacter(absl::string_view ch);

}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_CANDIDATE_KAGE_H_
