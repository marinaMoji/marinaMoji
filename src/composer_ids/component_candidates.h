// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Candidate list for Ctrl+2: dictionary hits, IDS exact encode and
// FindContaining, then a final "Compose…" row for two or three parts. Pure merge
// logic; no UI. There is deliberately no drawing of an unencoded layout: the
// selection says nothing about how the parts combine, so any single layout
// would be a guess. The "Compose…" row opens the character composer, where the
// user chooses one.

#ifndef MOZC_COMPOSER_IDS_COMPONENT_CANDIDATES_H_
#define MOZC_COMPOSER_IDS_COMPONENT_CANDIDATES_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/types/span.h"
#include "composer_ids/ids_table.h"

namespace mozc {
namespace composer_ids {

inline constexpr size_t kMaxComponentSelectionChars = 4;

struct ComponentCandidate {
  std::string value;
  // 0 = plain Unicode, 3 = the "Compose…" action row (see base/specimen_kind.h).
  uint8_t specimen_kind = 0;
};

struct ComponentSearchResult {
  std::vector<ComponentCandidate> candidates;
  // True when IDS exact encode found at least one Unicode character.
  bool has_exact_encode = false;
};

// The IDS expression the character composer should open with for the UTF-8
// selection |selected|: ⿰ for two characters, ⿲ for three (the layout is only
// the composer's starting point; the user changes it there). Empty unless
// |selected| is two or three characters.
std::string ComposeSeedIds(absl::string_view selected);

// Merges candidates for the UTF-8 selection |selected| (1–4 characters).
// |dictionary_values| are LookupExact hits for the full selected string (may
// be empty). Empty result means the caller should no-op.
ComponentSearchResult BuildComponentCandidates(
    const IdsTable &table, absl::string_view selected,
    absl::Span<const std::string> dictionary_values);

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_COMPONENT_CANDIDATES_H_
