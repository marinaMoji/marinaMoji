// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/ids_operator.h"

#include <string>
#include <string_view>

#include "absl/strings/ascii.h"
#include "absl/types/span.h"

namespace mozc {
namespace composer_ids {
namespace {

// Abbreviations match zi.tools's action_abbr_map (the twelve Unicode IDCs we
// offer). Newer IDCs ⿼..⿿ and ㇯ are not in the composer.
constexpr IdsOperatorInfo kOperators[] = {
    {"⿰", "Left / right", "LR", 2},
    {"⿱", "Above / below", "UD", 2},
    {"⿲", "Left / middle / right", "LL", 3},
    {"⿳", "Top / middle / bottom", "UU", 3},
    {"⿴", "Full enclosure", "OC", 2},
    {"⿵", "Surround from above", "OD", 2},
    {"⿶", "Surround from below", "OU", 2},
    {"⿷", "Surround from left", "OR", 2},
    {"⿸", "Upper-left enclosure", "RD", 2},
    {"⿹", "Upper-right enclosure", "LD", 2},
    {"⿺", "Lower-left enclosure", "RU", 2},
    {"⿻", "Overlap", "XX", 2},
};

}  // namespace

absl::Span<const IdsOperatorInfo> IdsOperators() { return kOperators; }

bool IsIdsOperator(std::string_view ch) {
  for (const IdsOperatorInfo &info : kOperators) {
    if (info.op == ch) return true;
  }
  return false;
}

int PartCountFor(std::string_view op) {
  for (const IdsOperatorInfo &info : kOperators) {
    if (info.op == op) return info.part_count;
  }
  return 2;
}

std::string ExpandIdsAbbreviations(std::string_view ids) {
  std::string out(ids);
  // Every code is two ASCII letters; scan left to right and rewrite matches.
  // Case-insensitive, like zi.tools (`gi` replace).
  for (size_t i = 0; i + 1 < out.size();) {
    if (!absl::ascii_isalpha(out[i]) || !absl::ascii_isalpha(out[i + 1])) {
      ++i;
      continue;
    }
    const char pair[3] = {static_cast<char>(absl::ascii_toupper(out[i])),
                          static_cast<char>(absl::ascii_toupper(out[i + 1])),
                          '\0'};
    bool replaced = false;
    for (const IdsOperatorInfo &info : kOperators) {
      if (pair == info.abbr) {
        out.replace(i, 2, info.op);
        i += info.op.size();
        replaced = true;
        break;
      }
    }
    if (!replaced) ++i;
  }
  return out;
}

}  // namespace composer_ids
}  // namespace mozc
