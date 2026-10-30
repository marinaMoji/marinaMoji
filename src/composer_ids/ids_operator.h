// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// The twelve Ideographic Description Characters (U+2FF0..U+2FFB) the character
// composer offers, and how many components each one describes.

#ifndef MOZC_COMPOSER_IDS_IDS_OPERATOR_H_
#define MOZC_COMPOSER_IDS_IDS_OPERATOR_H_

#include <string_view>

#include "absl/types/span.h"

namespace mozc {
namespace composer_ids {

struct IdsOperatorInfo {
  // The operator as one UTF-8 encoded character, e.g. "⿰".
  std::string_view op;
  // English name, also the key used for the layout palette's tooltips.
  std::string_view name;
  // Two-letter code used when typing an IDS by hand, as on zi.tools
  // (LR -> ⿰, UD -> ⿱, …). ASCII, upper case.
  std::string_view abbr;
  // 2 for every operator except ⿲ and ⿳, which describe three components.
  int part_count;
};

// All twelve operators, in code point order.
absl::Span<const IdsOperatorInfo> IdsOperators();

// True if |ch| is exactly one of the twelve operators.
bool IsIdsOperator(std::string_view ch);

// How many components |op| describes. 2 for anything that is not an operator,
// so callers never see 0.
int PartCountFor(std::string_view op);

// Replaces zi.tools layout codes (LR, UD, …) with the IDS operators, ignoring
// case. Leaves every other character alone. Used by the IDS entry box.
std::string ExpandIdsAbbreviations(std::string_view ids);

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_IDS_OPERATOR_H_
