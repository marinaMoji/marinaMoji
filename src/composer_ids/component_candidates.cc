// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/component_candidates.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "base/specimen_kind.h"
#include "base/strings/unicode.h"
#include "composer_ids/ids_table.h"

namespace mozc {
namespace composer_ids {
namespace {

std::vector<std::string> SplitChars(std::string_view text) {
  std::vector<std::string> chars;
  for (const std::string_view ch : Utf8AsChars(text)) {
    if (!ch.empty()) chars.emplace_back(ch);
  }
  return chars;
}

absl::Span<const std::string_view> DefaultOpsForArity(size_t n) {
  static constexpr std::string_view kTwo[] = {"⿰", "⿱"};
  static constexpr std::string_view kThree[] = {"⿲", "⿳"};
  if (n == 2) return kTwo;
  if (n == 3) return kThree;
  return {};
}

std::string ApplyAutomaticForms(const IdsTable &table, std::string_view op,
                                const std::vector<std::string> &parts) {
  std::string ids(op);
  for (size_t i = 0; i < parts.size(); ++i) {
    if (const auto form = table.AutomaticFormFor(parts[i], op, static_cast<int>(i))) {
      absl::StrAppend(&ids, *form);
    } else {
      absl::StrAppend(&ids, parts[i]);
    }
  }
  return ids;
}

bool AddUnique(absl::flat_hash_set<std::string> *seen,
               std::vector<ComponentCandidate> *out, std::string value,
               uint8_t specimen_kind) {
  if (value.empty() || !seen->insert(value).second) return false;
  out->push_back({std::move(value), specimen_kind});
  return true;
}

}  // namespace

std::string ComposeSeedIds(absl::string_view selected) {
  const std::vector<std::string> parts = SplitChars(selected);
  if (parts.size() < 2 || parts.size() > 3) return {};
  return absl::StrCat(parts.size() == 2 ? "⿰" : "⿲", selected);
}

ComponentSearchResult BuildComponentCandidates(
    const IdsTable &table, absl::string_view selected,
    absl::Span<const std::string> dictionary_values) {
  ComponentSearchResult result;
  const std::vector<std::string> parts = SplitChars(selected);
  if (parts.empty() || parts.size() > kMaxComponentSelectionChars) {
    return result;
  }

  absl::flat_hash_set<std::string> seen;
  seen.reserve(64);

  for (const std::string &value : dictionary_values) {
    AddUnique(&seen, &result.candidates, value, /*specimen_kind=*/0);
  }

  const absl::Span<const std::string_view> ops =
      DefaultOpsForArity(parts.size());
  for (const std::string_view op : ops) {
    const std::string ids = ApplyAutomaticForms(table, op, parts);
    if (const auto encoded = table.FindEncodedCharacter(ids)) {
      if (AddUnique(&seen, &result.candidates, *encoded, /*specimen_kind=*/0)) {
        result.has_exact_encode = true;
      }
    }
  }

  absl::flat_hash_set<std::string> selection_chars(parts.begin(), parts.end());
  std::vector<std::string> containing = table.FindContaining(selected);
  if (table.has_stroke_counts()) {
    table.SortByStrokes(&containing);
  }
  for (std::string &ch : containing) {
    if (selection_chars.contains(ch)) continue;
    AddUnique(&seen, &result.candidates, std::move(ch), /*specimen_kind=*/0);
  }

  // Last: open the composer with these parts. The value is only a label (the
  // session never inserts it); the ellipsis marks it as an action.
  if (const std::string seed = ComposeSeedIds(selected); !seed.empty()) {
    result.candidates.push_back(
        {absl::StrCat(seed, "…"),
         static_cast<uint8_t>(specimen_kind::Kind::kComposeAction)});
  }

  return result;
}

}  // namespace composer_ids
}  // namespace mozc
