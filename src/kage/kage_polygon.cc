// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// See kage_polygon.h for provenance and licence.

#include "kage/kage_polygon.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace mozc {
namespace kage {

Polygon::Polygon(int zero_points) {
  for (int i = 0; i < zero_points; ++i) Push(0, 0, false);
}

Polygon::Polygon(const std::vector<Point> &points) {
  for (const Point &p : points) Push(p);
}

Point Polygon::Get(int index) const {
  const Point &p = points_[index];
  return {p.x / kPrecision, p.y / kPrecision, p.off};
}

std::vector<Point> Polygon::Points() const {
  std::vector<Point> out;
  out.reserve(points_.size());
  for (int i = 0; i < size(); ++i) out.push_back(Get(i));
  return out;
}

void Polygon::Push(double x, double y, bool off) {
  points_.push_back({x * kPrecision, y * kPrecision, off});
}

void Polygon::Set(int index, double x, double y, bool off) {
  points_[index] = {x * kPrecision, y * kPrecision, off};
}

void Polygon::Reverse() { std::reverse(points_.begin(), points_.end()); }

void Polygon::Concat(const Polygon &other) {
  points_.insert(points_.end(), other.points_.begin(), other.points_.end());
}

void Polygon::Shift() {
  if (!points_.empty()) points_.erase(points_.begin());
}

void Polygon::Unshift(double x, double y, bool off) {
  points_.insert(points_.begin(), {x * kPrecision, y * kPrecision, off});
}

Polygon &Polygon::Translate(double dx, double dy) {
  dx *= kPrecision;
  dy *= kPrecision;
  for (Point &p : points_) {
    p.x += dx;
    p.y += dy;
  }
  return *this;
}

Polygon &Polygon::ReflectX() {
  for (Point &p : points_) p.x *= -1;
  return *this;
}

Polygon &Polygon::ReflectY() {
  for (Point &p : points_) p.y *= -1;
  return *this;
}

Polygon &Polygon::Rotate90() {
  for (Point &p : points_) {
    const double x = p.x;
    p.x = -p.y;
    p.y = x;
  }
  return *this;
}

Polygon &Polygon::Rotate180() {
  for (Point &p : points_) {
    p.x *= -1;
    p.y *= -1;
  }
  return *this;
}

Polygon &Polygon::Rotate270() {
  for (Point &p : points_) {
    const double x = p.x;
    p.x = p.y;
    p.y = -x;
  }
  return *this;
}

Polygon &Polygon::Floor() {
  for (Point &p : points_) {
    p.x = std::floor(p.x);
    p.y = std::floor(p.y);
  }
  return *this;
}

void Polygons::Push(Polygon polygon) {
  // Only a simple check, as in the original.
  double minx = 200, maxx = 0, miny = 200, maxy = 0;
  if (polygon.size() < 3) return;
  polygon.Floor();
  for (const Point &p : polygon.Points()) {
    if (p.x < minx) minx = p.x;
    if (p.x > maxx) maxx = p.x;
    if (p.y < miny) miny = p.y;
    if (p.y > maxy) maxy = p.y;
    if (std::isnan(p.x) || std::isnan(p.y)) return;
  }
  if (minx != maxx && miny != maxy) array_.push_back(std::move(polygon));
}

void Polygons::NormalizeWinding(bool clockwise) {
  for (Polygon &polygon : array_) {
    const std::vector<Point> pts = polygon.Points();
    const int n = static_cast<int>(pts.size());
    if (n < 3) continue;
    // Twice the signed area by the shoelace sum, in y-down space: positive
    // means clockwise.
    double s = 0;
    for (int i = 0; i < n; ++i) {
      const Point &a = pts[i];
      const Point &b = pts[(i + 1) % n];
      s += (b.x - a.x) * (b.y + a.y);
    }
    if (s == 0) continue;
    if ((s > 0) != clockwise) polygon.Reverse();
  }
}

}  // namespace kage
}  // namespace mozc
