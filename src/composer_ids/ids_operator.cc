// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/ids_operator.h"

#include <string_view>

#include "absl/types/span.h"

namespace mozc {
namespace composer_ids {
namespace {

constexpr IdsOperatorInfo kOperators[] = {
    {"⿰", "Left / right", 2},
    {"⿱", "Above / below", 2},
    {"⿲", "Left / middle / right", 3},
    {"⿳", "Top / middle / bottom", 3},
    {"⿴", "Full enclosure", 2},
    {"⿵", "Surround from above", 2},
    {"⿶", "Surround from below", 2},
    {"⿷", "Surround from left", 2},
    {"⿸", "Upper-left enclosure", 2},
    {"⿹", "Upper-right enclosure", 2},
    {"⿺", "Lower-left enclosure", 2},
    {"⿻", "Overlap", 2},
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

}  // namespace composer_ids
}  // namespace mozc
