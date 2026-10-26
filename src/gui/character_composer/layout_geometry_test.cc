// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/layout_geometry.h"

#include "composer_ids/ids_operator.h"
#include "testing/gunit.h"

namespace mozc {
namespace gui {
namespace {

bool InsideUnitSquare(const FRect &r) {
  constexpr double kEps = 1e-9;
  return r.w > 0 && r.h > 0 && r.x >= -kEps && r.y >= -kEps &&
         r.x + r.w <= 1 + kEps && r.y + r.h <= 1 + kEps;
}

TEST(LayoutGeometryTest, EveryOperatorMatchesItsPartCountAndStaysInTheSquare) {
  for (const auto &info : composer_ids::IdsOperators()) {
    const OperatorGeometry g = GeometryFor(info.op);
    EXPECT_EQ(g.count, info.part_count) << info.name;
    for (int i = 0; i < g.count; ++i) {
      EXPECT_TRUE(InsideUnitSquare(g.cell[i])) << info.name << " cell " << i;
      EXPECT_TRUE(InsideUnitSquare(g.edit[i])) << info.name << " edit " << i;
    }
    for (const FRect &frame : g.frames) {
      EXPECT_TRUE(InsideUnitSquare(frame)) << info.name;
    }
  }
}

TEST(LayoutGeometryTest, SplitsAreOrderedAndHaveDividers) {
  const OperatorGeometry lr = GeometryFor("⿰");
  EXPECT_LT(lr.cell[0].x, lr.cell[1].x);
  EXPECT_EQ(lr.dividers.size(), 1);
  EXPECT_EQ(GeometryFor("⿲").dividers.size(), 2);
  const OperatorGeometry tb = GeometryFor("⿱");
  EXPECT_LT(tb.cell[0].y, tb.cell[1].y);
  EXPECT_EQ(GeometryFor("⿳").dividers.size(), 2);
}

TEST(LayoutGeometryTest, EnclosureFrameFillsSquareAndInnerSitsInTheOpening) {
  for (const char *op : {"⿴", "⿵", "⿶", "⿷", "⿸", "⿹", "⿺"}) {
    const OperatorGeometry g = GeometryFor(op);
    EXPECT_EQ(g.cell[0].w, 1) << op;
    EXPECT_EQ(g.cell[0].h, 1) << op;
    ASSERT_EQ(g.frames.size(), 1) << op;
    EXPECT_EQ(g.frames[0].x, g.cell[1].x) << op;
  }
  // Opening side: ⿵ opens downwards, ⿶ upwards.
  EXPECT_GT(GeometryFor("⿵").cell[1].y, GeometryFor("⿶").cell[1].y);
  // ⿸ covers the upper-left, so the inner part is to the lower right.
  EXPECT_GT(GeometryFor("⿸").cell[1].x, GeometryFor("⿹").cell[1].x);
  EXPECT_LT(GeometryFor("⿺").cell[1].y, GeometryFor("⿸").cell[1].y);
}

TEST(LayoutGeometryTest, OverlapDrawsTwoOverlappingSquaresWithDiagonalFields) {
  const OperatorGeometry g = GeometryFor("⿻");
  ASSERT_EQ(g.frames.size(), 2);
  EXPECT_TRUE(g.dividers.empty());
  const FRect &a = g.frames[0];
  const FRect &b = g.frames[1];
  EXPECT_LT(b.x, a.x + a.w);  // the squares overlap
  EXPECT_LT(b.y, a.y + a.h);
  EXPECT_LT(g.cell[0].x, g.cell[1].x);  // first part upper left
  EXPECT_LT(g.cell[0].y, g.cell[1].y);
  EXPECT_LE(g.cell[0].x + g.cell[0].w, g.cell[1].x);  // fields do not collide
}

TEST(LayoutGeometryTest, UnknownOperatorFallsBackToLeftRight) {
  EXPECT_EQ(GeometryFor("木").count, 2);
  EXPECT_EQ(GeometryFor("木").dividers.size(), 1);
}

}  // namespace
}  // namespace gui
}  // namespace mozc
