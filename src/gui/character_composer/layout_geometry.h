// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// How each IDS operator is drawn in the composer's square, as fractions of the
// square (0..1). Shared by the editing square and the palette icons so both
// show the same shape. No Qt here, so it can be unit-tested.
//
// The square is only a picture of the layout: ink placement is not a concern
// of an IME, which just needs to know which operator sits where.

#ifndef MOZC_GUI_CHARACTER_COMPOSER_LAYOUT_GEOMETRY_H_
#define MOZC_GUI_CHARACTER_COMPOSER_LAYOUT_GEOMETRY_H_

#include <array>
#include <string_view>
#include <vector>

namespace mozc {
namespace gui {

struct FRect {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;
};

struct FLine {
  double x1 = 0;
  double y1 = 0;
  double x2 = 0;
  double y2 = 0;
};

struct OperatorGeometry {
  int count = 2;
  // The region each component occupies. A nested layout fills its cell.
  std::array<FRect, 3> cell;
  // Where a plain text field sits for each component. Equal to |cell| for
  // splits; a strip inside the frame for the outer part of an enclosure.
  std::array<FRect, 3> edit;
  // Dashed separators between siblings.
  std::vector<FLine> dividers;
  // Dashed outlines, e.g. the inner box of an enclosure.
  std::vector<FRect> frames;
};

// Geometry for one of the twelve operators; ⿰ for anything else.
OperatorGeometry GeometryFor(std::string_view op);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_LAYOUT_GEOMETRY_H_
