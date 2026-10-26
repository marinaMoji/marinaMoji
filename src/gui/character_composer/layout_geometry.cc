// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/layout_geometry.h"

#include <string_view>

namespace mozc {
namespace gui {
namespace {

OperatorGeometry Columns(int n) {
  OperatorGeometry g;
  g.count = n;
  for (int i = 0; i < n; ++i) {
    g.cell[i] = g.edit[i] = {static_cast<double>(i) / n, 0, 1.0 / n, 1};
    if (i > 0) {
      const double x = static_cast<double>(i) / n;
      g.dividers.push_back({x, 0, x, 1});
    }
  }
  return g;
}

OperatorGeometry Rows(int n) {
  OperatorGeometry g;
  g.count = n;
  for (int i = 0; i < n; ++i) {
    g.cell[i] = g.edit[i] = {0, static_cast<double>(i) / n, 1, 1.0 / n};
    if (i > 0) {
      const double y = static_cast<double>(i) / n;
      g.dividers.push_back({0, y, 1, y});
    }
  }
  return g;
}

// An enclosure: part 0 is the frame (it fills the square; its text field is a
// small strip |frame_edit|), part 1 sits inside it at |inner|.
OperatorGeometry Enclosure(FRect frame_edit, FRect inner) {
  OperatorGeometry g;
  g.count = 2;
  g.cell[0] = {0, 0, 1, 1};
  g.edit[0] = frame_edit;
  g.cell[1] = g.edit[1] = inner;
  g.frames.push_back(inner);
  return g;
}

}  // namespace

OperatorGeometry GeometryFor(std::string_view op) {
  if (op == "⿱") return Rows(2);
  if (op == "⿲") return Columns(3);
  if (op == "⿳") return Rows(3);
  if (op == "⿴") return Enclosure({0.06, 0.04, 0.5, 0.18}, {0.28, 0.28, 0.44, 0.44});
  if (op == "⿵") return Enclosure({0.06, 0.04, 0.5, 0.18}, {0.25, 0.42, 0.5, 0.5});
  if (op == "⿶") return Enclosure({0.06, 0.78, 0.5, 0.18}, {0.25, 0.08, 0.5, 0.5});
  if (op == "⿷") return Enclosure({0.04, 0.04, 0.4, 0.18}, {0.42, 0.25, 0.54, 0.5});
  if (op == "⿸") return Enclosure({0.06, 0.04, 0.5, 0.18}, {0.38, 0.38, 0.58, 0.58});
  if (op == "⿹") return Enclosure({0.44, 0.04, 0.5, 0.18}, {0.04, 0.38, 0.58, 0.58});
  if (op == "⿺") return Enclosure({0.06, 0.78, 0.5, 0.18}, {0.38, 0.04, 0.58, 0.58});
  if (op == "⿻") {
    // Two squares offset diagonally so they overlap; each field sits in the
    // corner of its own square that the other one does not cover.
    OperatorGeometry g;
    g.count = 2;
    g.cell[0] = g.edit[0] = {0.07, 0.07, 0.4, 0.4};
    g.cell[1] = g.edit[1] = {0.53, 0.53, 0.4, 0.4};
    g.frames.push_back({0.05, 0.05, 0.6, 0.6});
    g.frames.push_back({0.35, 0.35, 0.6, 0.6});
    return g;
  }
  return Columns(2);
}

}  // namespace gui
}  // namespace mozc
