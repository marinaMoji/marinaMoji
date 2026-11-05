// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Where the components of a simple IDS string sit inside one character cell,
// for drawing a preview of an unencoded composition (the Ctrl+2 "drawing"
// candidate) in a candidate window. No UI here: each renderer draws the
// components itself in the cells given.

#ifndef MOZC_COMPOSER_IDS_IDS_PREVIEW_H_
#define MOZC_COMPOSER_IDS_IDS_PREVIEW_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mozc {
namespace composer_ids {

// A rectangle as fractions (0..1) of the character cell.
struct PreviewCell {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;
};

struct IdsPreview {
  std::string op;                  // one of ⿰ ⿱ ⿲ ⿳
  std::vector<std::string> parts;  // one component per cell
  std::vector<PreviewCell> cells;  // same size as |parts|
};

// The cells of |op| (⿰ ⿱ ⿲ ⿳; ⿰ for anything else).
std::vector<PreviewCell> PreviewCellsFor(std::string_view op);

// Parses "⿰忄亜": one of the four operators followed by exactly as many
// single characters as it has cells. Anything else (nested layouts, other
// operators, plain text) returns nullopt, and the caller draws the text.
std::optional<IdsPreview> ParseIdsPreview(std::string_view ids);

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_IDS_PREVIEW_H_
