// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// C++ port of the polygon types of the KAGE engine (Kanji-glyph Automatic
// Generating Engine) as modified by kurgm,
// <https://github.com/kurgm/kage-engine>, itself derived from the original by
// Kamichi Koichi, <https://github.com/kamichikoichi/kage-engine>. Both are
// GPL-3.0; so is this port. See LICENSE and README.md in this directory.

#ifndef MOZC_KAGE_KAGE_POLYGON_H_
#define MOZC_KAGE_KAGE_POLYGON_H_

#include <vector>

namespace mozc {
namespace kage {

struct Point {
  double x = 0;
  double y = 0;
  bool off = false;  // off-curve point (only with curves; unused here)
};

// One closed contour. Like the original, coordinates are held ten times
// larger than they are reported (precision 10), and Floor() rounds that
// scaled value down, so a contour is on a 0.1 grid.
class Polygon {
 public:
  Polygon() = default;
  explicit Polygon(int zero_points);
  explicit Polygon(const std::vector<Point> &points);

  int size() const { return static_cast<int>(points_.size()); }
  Point Get(int index) const;
  std::vector<Point> Points() const;

  void Push(double x, double y, bool off = false);
  void Push(const Point &p) { Push(p.x, p.y, p.off); }
  void Set(int index, double x, double y, bool off = false);
  void Set(int index, const Point &p) { Set(index, p.x, p.y, p.off); }
  void Reverse();
  void Concat(const Polygon &other);
  void Shift();
  void Unshift(double x, double y, bool off = false);

  Polygon &Translate(double dx, double dy);
  Polygon &ReflectX();
  Polygon &ReflectY();
  Polygon &Rotate90();
  Polygon &Rotate180();
  Polygon &Rotate270();
  Polygon &Floor();

 private:
  static constexpr double kPrecision = 10;
  std::vector<Point> points_;  // scaled by kPrecision
};

// A rendered glyph: a list of contours.
class Polygons {
 public:
  // Appends |polygon| unless it is degenerate (fewer than 3 points, flat, or
  // containing NaN).
  void Push(Polygon polygon);
  const std::vector<Polygon> &array() const { return array_; }
  std::vector<Polygon> &mutable_array() { return array_; }

  // Reverses contours so that all of them wind the same way (see the
  // original's normalizeWinding); "cw" in the y-down KAGE space.
  void NormalizeWinding(bool clockwise = true);

 private:
  std::vector<Polygon> array_;
};

}  // namespace kage
}  // namespace mozc

#endif  // MOZC_KAGE_KAGE_POLYGON_H_
