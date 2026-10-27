// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// See kage_engine.h for provenance and licence.

#include "kage/kage_engine.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"

namespace mozc {
namespace kage {
namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr int kMaxComponentDepth = 32;
constexpr double kPi = 3.14159265358979323846;

// The integer to switch on for a double that is integral in practice; NaN and
// huge values match no case (a cast would be undefined behaviour).
int Sw(double v) {
  return (std::isfinite(v) && std::fabs(v) < 1e9) ? static_cast<int>(v)
                                                  : INT_MIN;
}

// An adjustment table entry; out of range reads NaN, which makes the polygon
// that uses it be dropped, as with the original's undefined.
template <size_t N>
double At(const double (&table)[N], double index) {
  const int i = Sw(index);
  return (i >= 0 && static_cast<size_t>(i) < N) ? table[i] : kNaN;
}

// ---------------------------------------------------------------- util ---

// JavaScript's Math.round: halves go towards +infinity.
double JsRound(double v) { return std::floor(v + 0.5); }

double Round(double v, double rate = 1e8) { return JsRound(v * rate) / rate; }

struct Vec {
  double x;
  double y;
};

// A vector of the same direction and the given length. The zero vector gets
// the direction of atan2(y, x), which is "left" for a negative zero.
Vec Normalize(double x, double y, double magnitude = 1) {
  if (x == 0 && y == 0) {
    return {std::signbit(x) ? -magnitude : magnitude, 0};
  }
  const double k = magnitude / std::hypot(x, y);
  return {x * k, y * k};
}

double QuadraticBezier(double p1, double p2, double p3, double t) {
  const double s = 1 - t;
  return s * s * p1 + 2 * (s * t) * p2 + t * t * p3;
}

double QuadraticBezierDeriv(double p1, double p2, double p3, double t) {
  return 2 * (t * (p1 - 2 * p2 + p3) - p1 + p2);
}

double CubicBezier(double p1, double p2, double p3, double p4, double t) {
  const double s = 1 - t;
  return s * s * s * p1 + 3 * (s * s * t) * p2 + 3 * (s * t * t) * p3 +
         t * t * t * p4;
}

double CubicBezierDeriv(double p1, double p2, double p3, double p4, double t) {
  return 3 * (t * (t * (-p1 + 3 * p2 - 3 * p3 + p4) + 2 * (p1 - 2 * p2 + p3)) -
              p1 + p2);
}

// --------------------------------------------------------------- 2d.js ---

double Cross(double x1, double y1, double x2, double y2) {
  return x1 * y2 - x2 * y1;
}

bool IsCross(double x11, double y11, double x12, double y12, double x21,
             double y21, double x22, double y22) {
  const double c = Cross(x12 - x11, y12 - y11, x22 - x21, y22 - y21);
  if (std::isnan(c)) return true;  // for backward compatibility
  if (c == 0) return false;        // parallel
  const double c1121 = Cross(x12 - x11, y12 - y11, x21 - x11, y21 - y11);
  const double c1122 = Cross(x12 - x11, y12 - y11, x22 - x11, y22 - y11);
  const double c2111 = Cross(x22 - x21, y22 - y21, x11 - x21, y11 - y21);
  const double c2112 = Cross(x22 - x21, y22 - y21, x12 - x21, y12 - y21);
  return Round(c1121 * c1122, 1e5) <= 0 && Round(c2111 * c2112, 1e5) <= 0;
}

bool IsCrossBox(double x1, double y1, double x2, double y2, double bx1,
                double by1, double bx2, double by2) {
  return IsCross(x1, y1, x2, y2, bx1, by1, bx2, by1) ||
         IsCross(x1, y1, x2, y2, bx2, by1, bx2, by2) ||
         IsCross(x1, y1, x2, y2, bx1, by2, bx2, by2) ||
         IsCross(x1, y1, x2, y2, bx1, by1, bx1, by2);
}

// ----------------------------------------------------------- stroke.js ---

double Stretch(double dp, double sp, double p, double min, double max) {
  double p1, p2, p3, p4;
  if (p < sp + 100) {
    p1 = min;
    p3 = min;
    p2 = sp + 100;
    p4 = dp + 100;
  } else {
    p1 = sp + 100;
    p3 = dp + 100;
    p2 = max;
    p4 = max;
  }
  return std::floor(((p - p1) / (p2 - p1)) * (p4 - p3) + p3);
}

struct Box {
  double min_x, max_x, min_y, max_y;
};

struct Stroke {
  double a1_100, a1_opt;
  double a2_100, a2_opt, a2_opt_1, a2_opt_2, a2_opt_3;
  double a3_100, a3_opt, a3_opt_1, a3_opt_2;
  double x1, y1, x2, y2, x3, y3, x4, y4;

  explicit Stroke(const double (&d)[11]) {
    a1_100 = d[0];
    a2_100 = d[1];
    a3_100 = d[2];
    x1 = d[3];
    y1 = d[4];
    x2 = d[5];
    y2 = d[6];
    x3 = d[7];
    y3 = d[8];
    x4 = d[9];
    y4 = d[10];
    a1_opt = std::floor(a1_100 / 100);
    a1_100 = std::fmod(a1_100, 100);
    a2_opt = std::floor(a2_100 / 100);
    a2_100 = std::fmod(a2_100, 100);
    a2_opt_1 = std::fmod(a2_opt, 10);
    a2_opt_2 = std::fmod(std::floor(a2_opt / 10), 10);
    a2_opt_3 = std::floor(a2_opt / 100);
    a3_opt = std::floor(a3_100 / 100);
    a3_100 = std::fmod(a3_100, 100);
    a3_opt_1 = std::fmod(a3_opt, 10);
    a3_opt_2 = std::floor(a3_opt / 10);
  }

  // The straight segments between control points (a stroke "crosses" a box
  // when any of them does).
  std::vector<std::array<double, 4>> ControlSegments() const {
    std::vector<std::array<double, 4>> res;
    const double a1 = a1_opt == 0 ? a1_100 : 1;
    auto add_first = [&](std::array<double, 4> s) {
      res.insert(res.begin(), s);
    };
    switch (Sw(a1)) {
      case 0:
      case 8:
      case 9:
        break;
      case 6:
      case 7:
        add_first({x3, y3, x4, y4});
        [[fallthrough]];
      case 2:
      case 12:
      case 3:
      case 4:
        add_first({x2, y2, x3, y3});
        [[fallthrough]];
      default:
        add_first({x1, y1, x2, y2});
    }
    return res;
  }

  bool IsCrossSegment(double bx1, double by1, double bx2, double by2) const {
    for (const auto &s : ControlSegments()) {
      if (IsCross(s[0], s[1], s[2], s[3], bx1, by1, bx2, by2)) return true;
    }
    return false;
  }

  bool IsCrossBoxRect(double bx1, double by1, double bx2, double by2) const {
    for (const auto &s : ControlSegments()) {
      if (IsCrossBox(s[0], s[1], s[2], s[3], bx1, by1, bx2, by2)) return true;
    }
    return false;
  }

  void StretchBy(double sx, double sx2, double sy, double sy2, double bmin_x,
                 double bmax_x, double bmin_y, double bmax_y) {
    x1 = Stretch(sx, sx2, x1, bmin_x, bmax_x);
    y1 = Stretch(sy, sy2, y1, bmin_y, bmax_y);
    x2 = Stretch(sx, sx2, x2, bmin_x, bmax_x);
    y2 = Stretch(sy, sy2, y2, bmin_y, bmax_y);
    if (!(a1_100 == 99 && a1_opt == 0)) {  // always true
      x3 = Stretch(sx, sx2, x3, bmin_x, bmax_x);
      y3 = Stretch(sy, sy2, y3, bmin_y, bmax_y);
      x4 = Stretch(sx, sx2, x4, bmin_x, bmax_x);
      y4 = Stretch(sy, sy2, y4, bmin_y, bmax_y);
    }
  }

  Box GetBox() const {
    const double inf = std::numeric_limits<double>::infinity();
    Box b{inf, -inf, inf, -inf};
    auto grow = [&](double x, double y) {
      b.min_x = std::min(b.min_x, x);
      b.max_x = std::max(b.max_x, x);
      b.min_y = std::min(b.min_y, y);
      b.max_y = std::max(b.max_y, y);
    };
    const double a1 = a1_opt == 0 ? a1_100 : 6;
    int stage;  // how far down the fall-through chain to go
    switch (Sw(a1)) {
      case 0:
        stage = 0;
        break;
      case 1:
      case 99:
        stage = 1;
        break;
      case 2:
      case 3:
      case 4:
        stage = 2;
        break;
      default:
        stage = 3;
    }
    if (stage >= 3) grow(x4, y4);
    if (stage >= 2) grow(x3, y3);
    if (stage >= 1) {
      grow(x1, y1);
      grow(x2, y2);
    }
    return b;
  }
};

// -------------------------------------------------------------- curve.js ---

struct FattenCurve {
  std::vector<Vec> left;
  std::vector<Vec> right;
};

FattenCurve GenerateFattenCurve(double x1, double y1, double sx1, double sy1,
                                double sx2, double sy2, double x2, double y2,
                                int k_rate,
                                const std::function<double(double)> &width) {
  FattenCurve curve;
  const bool is_quadratic = sx1 == sx2 && sy1 == sy2;
  for (int tt = 0; tt <= 1000; tt += k_rate) {
    const double t = tt / 1000.0;
    double x, y, ix, iy;
    if (is_quadratic) {
      x = QuadraticBezier(x1, sx1, x2, t);
      y = QuadraticBezier(y1, sy1, y2, t);
      ix = QuadraticBezierDeriv(x1, sx1, x2, t);
      iy = QuadraticBezierDeriv(y1, sy1, y2, t);
    } else {
      x = CubicBezier(x1, sx1, sx2, x2, t);
      y = CubicBezier(y1, sy1, sy2, y2, t);
      ix = CubicBezierDeriv(x1, sx1, sx2, x2, t);
      iy = CubicBezierDeriv(y1, sy1, sy2, y2, t);
    }
    const double w = width(t);
    Vec ia;
    if (Round(ix) == 0 && Round(iy) == 0) {
      ia = {-w, 0};
    } else {
      ia = Normalize(-iy, ix, w);
    }
    curve.left.push_back({x - ia.x, y - ia.y});
    curve.right.push_back({x + ia.x, y + ia.y});
  }
  return curve;
}

// ---------------------------------------------------------------- pen.js ---

// Local coordinates around a point, turned by a direction.
class Pen {
 public:
  Pen(double x, double y) : x_(x), y_(y) {}

  Pen &SetMatrix2(double cos_theta, double sin_theta) {
    cos_ = cos_theta;
    sin_ = sin_theta;
    return *this;
  }
  Pen &SetLeft(double ox, double oy) {
    const Vec d = Normalize(ox - x_, oy - y_);
    return SetMatrix2(-d.x, -d.y);
  }
  Pen &SetRight(double ox, double oy) {
    const Vec d = Normalize(ox - x_, oy - y_);
    return SetMatrix2(d.x, d.y);
  }
  Pen &SetDown(double ox, double oy) {
    const Vec d = Normalize(ox - x_, oy - y_);
    return SetMatrix2(d.y, -d.x);
  }

  Point GetPoint(double lx, double ly, bool off = false) const {
    return {x_ + cos_ * lx + -sin_ * ly, y_ + sin_ * lx + cos_ * ly, off};
  }

  Polygon GetPolygon(const std::vector<Point> &local) const {
    Polygon poly;
    for (const Point &p : local) poly.Push(GetPoint(p.x, p.y, p.off));
    return poly;
  }

 private:
  double x_, y_;
  double cos_ = 1, sin_ = 0;
};

// ---------------------------------------------------------- mincho font ---

struct Mincho {
  double kMinWidthY = 2;
  double kMinWidthU = 2;
  double kMinWidthT = 6;
  double kWidth = 5;
  double kKakato = 3;
  double kL2RDfatten = 1.1;
  double kMage = 10;
  int kRate = 100;  // must divide 1000
  double kAdjustKakatoL[5] = {14, 9, 5, 2, 0};
  double kAdjustKakatoR[4] = {8, 6, 4, 2};
  double kAdjustKakatoRangeX = 20;
  double kAdjustKakatoRangeY[4] = {1, 19, 24, 30};
  int kAdjustKakatoStep = 3;
  double kAdjustUrokoX[4] = {24, 20, 16, 12};
  double kAdjustUrokoY[4] = {12, 11, 9, 8};
  double kAdjustUrokoLength[3] = {22, 36, 50};
  int kAdjustUrokoLengthStep = 3;
  double kAdjustUrokoLine[3] = {22, 26, 30};
  int kAdjustUroko2Step = 3;
  double kAdjustUroko2Length = 40;
  int kAdjustTateStep = 4;
  int kAdjustMageStep = 5;
};

// A stroke plus the adjustments the other strokes of the glyph imply.
struct AdjStroke {
  Stroke stroke;
  double kirikuchiAdjustment, tateAdjustment, haneAdjustment, urokoAdjustment,
      kakatoAdjustment, mageAdjustment;
};

// ----- cd.js: how one stroke becomes polygons -----

void DrawCurveBody(const Mincho &font, Polygons *polygons, double x1, double y1,
                   double sx1, double sy1, double sx2, double sy2, double x2,
                   double y2, double a1, double a2, double kMinWidthT,
                   double opt3, double opt4) {
  const bool is_quadratic = sx1 == sx2 && sy1 == sy2;
  double hosomi = 0.5;
  if (std::hypot(x2 - x1, y2 - y1) < 50) {
    hosomi += 0.4 * (1 - std::hypot(x2 - x1, y2 - y1) / 50);
  }
  std::function<double(double)> deltad_func;
  if ((a1 == 7 || a1 == 27) && a2 == 0) {  // L2RD: fatten
    deltad_func = [&](double t) { return std::pow(t, hosomi) * font.kL2RDfatten; };
  } else if (a1 == 7 || a1 == 27) {
    if (is_quadratic) {
      deltad_func = [&](double t) { return std::pow(t, hosomi); };
    } else {
      deltad_func = [&](double t) {
        return std::pow(std::pow(t, hosomi), 0.7);  // make fatten
      };
    }
  } else if (a2 == 7) {
    deltad_func = [&](double t) { return std::pow(1 - t, hosomi); };
  } else if (is_quadratic && (opt3 > 0 || opt4 > 0)) {
    deltad_func = [&](double t) {
      return ((font.kMinWidthT - opt3 / 2) - (opt4 - opt3) / 2 * t) /
             font.kMinWidthT;
    };
  } else {
    deltad_func = [](double) { return 1.0; };
  }
  const FattenCurve fat = GenerateFattenCurve(
      x1, y1, sx1, sy1, sx2, sy2, x2, y2, font.kRate, [&](double t) {
        double deltad = deltad_func(t);
        if (deltad < 0.15) deltad = 0.15;
        return kMinWidthT * deltad;
      });
  Polygon poly, poly2;
  for (const Vec &p : fat.left) poly.Push(p.x, p.y);
  for (const Vec &p : fat.right) poly2.Push(p.x, p.y);

  // Join the stroke to a horizontal line (the box's top-right corner).
  if (a1 == 132 || (a1 == 22 && (is_quadratic ? (y1 > y2) : (x1 > sx1)))) {
    poly.Floor();
    poly2.Floor();
    for (int index = 0, length = poly2.size(); index + 1 < length; ++index) {
      const Point point1 = poly2.Get(index);
      const Point point2 = poly2.Get(index + 1);
      if (point1.y <= y1 && y1 <= point2.y) {
        const double newx1 = point2.x + (point1.x - point2.x) * (y1 - point2.y) /
                                            (point1.y - point2.y);
        const double newy1 = y1;
        const Point point3 = poly.Get(0);
        const Point point4 = poly.Get(1);
        const double newx2 =
            (a1 == 132)
                ? point3.x + (point4.x - point3.x) * (y1 - point3.y) /
                                 (point4.y - point3.y)
                : point3.x + (point4.x - point3.x + 1) * (y1 - point3.y) /
                                 (point4.y - point3.y);
        const double newy2 = (a1 == 132) ? y1 : y1 + 1;
        for (int i = 0; i < index; ++i) poly2.Shift();
        poly2.Set(0, newx1, newy1);
        poly.Unshift(newx2, newy2);
        break;
      }
    }
  }
  poly2.Reverse();
  poly.Concat(poly2);
  polygons->Push(std::move(poly));
}

void DrawCurveHead(Polygons *polygons, const Mincho &font, double x1, double y1,
                   double sx1, double sy1, double a1, double kMinWidthT,
                   bool is_up_to_bottom, double corner_offset) {
  switch (Sw(a1)) {
    case 12: {
      Pen pen1(x1, y1);
      if (x1 != sx1) pen1.SetDown(sx1, sy1);
      polygons->Push(pen1.GetPolygon({{-kMinWidthT, 0},
                                      {+kMinWidthT, 0},
                                      {-kMinWidthT, -kMinWidthT}}));
      break;
    }
    case 0: {
      if (is_up_to_bottom) {
        Pen pen1(x1, y1);
        if (x1 != sx1) pen1.SetDown(sx1, sy1);
        double type = std::atan2(std::fabs(y1 - sy1), std::fabs(x1 - sx1)) /
                          kPi * 2 -
                      0.4;
        if (type > 0) {
          type *= 2;
        } else {
          type *= 16;
        }
        const double pm = type < 0 ? -1 : 1;
        polygons->Push(pen1.GetPolygon(
            {{-kMinWidthT, 1},
             {+kMinWidthT, 0},
             {-pm * kMinWidthT, -font.kMinWidthY * std::fabs(type)}}));
        // beginning of the stroke
        const double move = type < 0 ? -type * font.kMinWidthY : 0;
        if (x1 == sx1 && y1 == sy1) {
          polygons->Push(pen1.GetPolygon(
              {{kMinWidthT, -move},
               {kMinWidthT * 1.5, font.kMinWidthY - move},
               {kMinWidthT - 2, font.kMinWidthY * 2 + 1}}));
        } else {
          polygons->Push(pen1.GetPolygon(
              {{kMinWidthT, -move},
               {kMinWidthT * 1.5, font.kMinWidthY - move * 1.2},
               {kMinWidthT - 2, font.kMinWidthY * 2 - move * 0.8 + 1}}));
        }
      } else {  // bottom to up
        Pen pen1(x1, y1);
        if (x1 == sx1) {
          pen1.SetMatrix2(0, 1);
        } else {
          pen1.SetRight(sx1, sy1);
        }
        polygons->Push(pen1.GetPolygon({{0, +kMinWidthT},
                                        {0, -kMinWidthT},
                                        {-font.kMinWidthY, -kMinWidthT}}));
        polygons->Push(pen1.GetPolygon({{0, +kMinWidthT},
                                        {+font.kMinWidthY, +kMinWidthT * 1.5},
                                        {+font.kMinWidthY * 3, +kMinWidthT * 0.5}}));
      }
      break;
    }
    case 22:
    case 27: {  // box's up-right corner, any time same degree
      std::vector<Point> local = {{-kMinWidthT, -font.kMinWidthY},
                                  {0, -font.kMinWidthY - font.kWidth},
                                  {+kMinWidthT + font.kWidth, +font.kMinWidthY},
                                  {+kMinWidthT, +kMinWidthT - 1}};
      if (a1 == 27) {
        local.push_back({0, +kMinWidthT + 2});
        local.push_back({0, 0.5});
      } else {
        local.push_back({-kMinWidthT, +kMinWidthT + 4});
      }
      polygons->Push(Pen(x1 - corner_offset, y1).GetPolygon(local));
      break;
    }
    default:
      break;
  }
}

void DrawCurveTail(Polygons *polygons, const Mincho &font, double sx2,
                   double sy2, double x2, double y2, double a1, double a2,
                   double kMinWidthT, double hane_adjustment, double opt4,
                   bool is_bottom_to_up) {
  switch (Sw(a2)) {
    case 1:
    case 8:
    case 15: {  // the last filled circle ... it can change 15->5
      const double w = font.kMinWidthT - opt4 / 2;
      Pen pen2(x2, y2);
      if (sx2 == x2) {
        pen2.SetMatrix2(0, 1);
      } else if (sy2 != y2) {
        pen2.SetLeft(sx2, sy2);
      }
      Polygon poly = pen2.GetPolygon({{0, -w},
                                      {+w * 0.7, -w * 0.7},
                                      {+w, 0},
                                      {+w * 0.7, +w * 0.7},
                                      {0, +w}});
      if (sx2 == x2) poly.Reverse();
      polygons->Push(std::move(poly));
      if (a2 == 15) {  // jump up
        Pen pen2_r(x2, y2);
        if (is_bottom_to_up) pen2_r.SetMatrix2(-1, 0);
        polygons->Push(pen2_r.GetPolygon({{0, -kMinWidthT + 1},
                                          {+2, -kMinWidthT - font.kWidth * 5},
                                          {0, -kMinWidthT - font.kWidth * 5},
                                          {-kMinWidthT, -kMinWidthT + 1}}));
      }
      break;
    }
    case 0:
      if (!(a1 == 7 || a1 == 27)) break;
      [[fallthrough]];
    case 9: {  // L2RD harai
      double type = std::atan2(std::fabs(y2 - sy2), std::fabs(x2 - sx2)) /
                        kPi * 2 -
                    0.6;
      if (type > 0) {
        type *= 8;
      } else {
        type *= 3;
      }
      const double pm = type < 0 ? -1 : 1;
      Pen pen2(x2, y2);
      if (sy2 == y2) {
        pen2.SetMatrix2(1, 0);
      } else if (sx2 == x2) {
        pen2.SetMatrix2(0, y2 > sy2 ? -1 : 1);
      } else {
        pen2.SetLeft(sx2, sy2);
      }
      const double f = kMinWidthT * font.kL2RDfatten;
      polygons->Push(pen2.GetPolygon(
          {{0, +f}, {0, -f}, {std::fabs(type) * f, pm * f}}));
      break;
    }
    case 14: {  // jump to left, always go left
      const double jump_factor = kMinWidthT > 6 ? 6.0 / kMinWidthT : 1.0;
      const double hane_length =
          font.kWidth * 4 *
          std::min(1 - hane_adjustment / 10,
                   std::pow(kMinWidthT / font.kMinWidthT, 3)) *
          jump_factor;
      polygons->Push(Pen(x2, y2).GetPolygon({{0, 0},
                                             {0, -kMinWidthT},
                                             {-hane_length, -kMinWidthT},
                                             {-hane_length, -kMinWidthT * 0.5}}));
      break;
    }
    default:
      break;
  }
}

void CdDrawCurveU(const Mincho &font, Polygons *polygons, double x1_,
                  double y1_, double sx1, double sy1, double sx2, double sy2,
                  double x2_, double y2_, double a1, double a2, double opt1,
                  double hane_adjustment, double opt3, double opt4) {
  const double kMinWidthT = font.kMinWidthT - opt1 / 2;
  double x1 = x1_, y1 = y1_;
  std::optional<double> delta1;
  switch (Sw(std::fmod(a1, 100))) {
    case 0:
    case 7:
    case 27:
      delta1 = -1 * font.kMinWidthY * 0.5;
      break;
    case 1:
    case 2:
    case 6:
    case 22:
    case 32:
      delta1 = 0;
      break;
    case 12:
      delta1 = font.kMinWidthY;
      break;
    default:
      break;
  }
  bool has1 = delta1.has_value();
  if (has1 && *delta1 != 0) {
    Vec d = (x1 == sx1 && y1 == sy1) ? Vec{0, *delta1}
                                      : Normalize(x1 - sx1, y1 - sy1, *delta1);
    x1 += d.x;
    y1 += d.y;
  }
  double corner_offset = 0;
  if (has1 && (a1 == 22 || a1 == 27) && a2 == 7 && kMinWidthT > 6) {
    const double contour_length = std::hypot(sx1 - x1, sy1 - y1) +
                                  std::hypot(sx2 - sx1, sy2 - sy1) +
                                  std::hypot(x2_ - sx2, y2_ - sy2);
    if (contour_length < 100) {
      corner_offset = (kMinWidthT - 6) * ((100 - contour_length) / 100);
      x1 += corner_offset;
    }
  }
  double x2 = x2_, y2 = y2_;
  std::optional<double> delta2;
  switch (Sw(std::fmod(a2, 100))) {
    case 0:
    case 1:
    case 7:
    case 9:
    case 15:
    case 14:
    case 17:
    case 5:
      delta2 = 0;
      break;
    case 8:  // get shorten for tail's circle
      delta2 = -1 * kMinWidthT * 0.5;
      break;
    default:
      delta2 = delta1;
      break;
  }
  bool has2 = delta2.has_value();
  if (has2 && *delta2 != 0) {
    Vec d = (sx2 == x2 && sy2 == y2) ? Vec{0, -*delta2}
                                      : Normalize(x2 - sx2, y2 - sy2, *delta2);
    x2 += d.x;
    y2 += d.y;
  }
  if (has1 && has2) {
    DrawCurveBody(font, polygons, x1, y1, sx1, sy1, sx2, sy2, x2, y2, a1, a2,
                  kMinWidthT, opt3, opt4);
  }
  if (has1) {
    const bool is_up_to_bottom = has2 ? y1 <= y2 : false;
    DrawCurveHead(polygons, font, x1, y1, sx1, sy1, a1, kMinWidthT,
                  is_up_to_bottom, corner_offset);
  }
  if (has2) {
    const bool is_bottom_to_up = has1 ? y2 <= y1 : false;
    DrawCurveTail(polygons, font, sx2, sy2, x2, y2, a1, a2, kMinWidthT,
                  hane_adjustment, opt4, is_bottom_to_up);
  }
}

void CdDrawBezier(const Mincho &font, Polygons *polygons, double x1, double y1,
                  double x2, double y2, double x3, double y3, double x4,
                  double y4, double a1, double a2, double opt1,
                  double hane_adjustment, double opt3, double opt4) {
  CdDrawCurveU(font, polygons, x1, y1, x2, y2, x3, y3, x4, y4, a1, a2, opt1,
               hane_adjustment, opt3, opt4);
}

void CdDrawCurve(const Mincho &font, Polygons *polygons, double x1, double y1,
                 double x2, double y2, double x3, double y3, double a1,
                 double a2, double opt1, double hane_adjustment, double opt3,
                 double opt4) {
  CdDrawCurveU(font, polygons, x1, y1, x2, y2, x2, y2, x3, y3, a1, a2, opt1,
               hane_adjustment, opt3, opt4);
}

void CdDrawLine(const Mincho &font, Polygons *polygons, double x1, double y1,
                double x2, double y2, double a1, double a2, double opt1,
                double uroko_adjustment, double kakato_adjustment) {
  const double kMinWidthT = font.kMinWidthT - opt1 / 2;
  if (x1 == x2 ||
      (y1 != y2 && (x1 > x2 || std::fabs(y2 - y1) >= std::fabs(x2 - x1) ||
                    a1 == 6 || a2 == 6))) {
    // A tate stroke uses the y axis; steep angle or a hook's horizontal bar.
    double cosrad, sinrad;
    if (x1 == x2) {
      cosrad = 0;
      sinrad = 1;
    } else {
      const Vec n = Normalize(x2 - x1, y2 - y1);
      cosrad = n.x;
      sinrad = n.y;
    }
    Pen pen1(x1, y1);
    Pen pen2(x2, y2);
    pen1.SetMatrix2(sinrad, -cosrad);
    pen2.SetMatrix2(sinrad, -cosrad);
    Polygon poly0(4);
    switch (Sw(a1)) {
      case 0:
        poly0.Set(0, pen1.GetPoint(kMinWidthT, font.kMinWidthY / 2));
        poly0.Set(3, pen1.GetPoint(-kMinWidthT, -font.kMinWidthY / 2));
        break;
      case 1:
      case 6:
        poly0.Set(0, pen1.GetPoint(kMinWidthT, 0));
        poly0.Set(3, pen1.GetPoint(-kMinWidthT, 0));
        break;
      case 12:
        poly0.Set(0, pen1.GetPoint(kMinWidthT, -font.kMinWidthY));
        poly0.Set(3, pen1.GetPoint(-kMinWidthT, -font.kMinWidthY - kMinWidthT));
        break;
      case 22:
        if (x1 == x2) {
          poly0.Set(0, x1 + kMinWidthT, y1);
          poly0.Set(3, x1 - kMinWidthT, y1);
        } else {
          const double v = x1 > x2 ? -1 : 1;
          poly0.Set(0, x1 + (kMinWidthT + v) / sinrad, y1 + 1);
          poly0.Set(3, x1 - kMinWidthT / sinrad, y1);
        }
        break;
      case 32:
        if (x1 == x2) {
          poly0.Set(0, x1 + kMinWidthT, y1 - font.kMinWidthY);
          poly0.Set(3, x1 - kMinWidthT, y1 - font.kMinWidthY);
        } else {
          poly0.Set(0, x1 + kMinWidthT / sinrad, y1);
          poly0.Set(3, x1 - kMinWidthT / sinrad, y1);
        }
        break;
      default:
        break;
    }
    switch (Sw(a2)) {
      case 0:
        if (a1 == 6) {  // kagi's tail
          poly0.Set(1, pen2.GetPoint(kMinWidthT, 0));
          poly0.Set(2, pen2.GetPoint(-kMinWidthT, 0));
        } else {
          poly0.Set(1, pen2.GetPoint(kMinWidthT, -kMinWidthT / 2));
          poly0.Set(2, pen2.GetPoint(-kMinWidthT, kMinWidthT / 2));
        }
        break;
      case 5:
        if (x1 == x2) break;
        [[fallthrough]];
      case 1:
        poly0.Set(1, pen2.GetPoint(kMinWidthT, 0));
        poly0.Set(2, pen2.GetPoint(-kMinWidthT, 0));
        break;
      case 13: {
        const double k =
            At(font.kAdjustKakatoL, kakato_adjustment);
        poly0.Set(1, pen2.GetPoint(kMinWidthT, k));
        poly0.Set(2, pen2.GetPoint(-kMinWidthT, k + kMinWidthT));
        break;
      }
      case 23: {
        const double k =
            At(font.kAdjustKakatoR, kakato_adjustment);
        poly0.Set(1, pen2.GetPoint(kMinWidthT, k));
        poly0.Set(2, pen2.GetPoint(-kMinWidthT, k + kMinWidthT));
        break;
      }
      case 24:
      case 32:
        if (x1 == x2) {
          poly0.Set(1, x2 + kMinWidthT, y2 + font.kMinWidthY);
          poly0.Set(2, x2 - kMinWidthT, y2 + font.kMinWidthY);
        } else {
          poly0.Set(1, x2 + kMinWidthT / sinrad, y2);
          poly0.Set(2, x2 - kMinWidthT / sinrad, y2);
        }
        break;
      default:
        break;
    }
    polygons->Push(poly0);

    switch (Sw(a2)) {
      case 24: {  // for T design
        polygons->Push(Pen(x2, y2).GetPolygon(
            {{0, +font.kMinWidthY},
             (x1 == x2) ? Point{+kMinWidthT, -font.kMinWidthY * 3}
                        : Point{+kMinWidthT * 0.5, -font.kMinWidthY * 4},
             {+kMinWidthT * 2, -font.kMinWidthY},
             {+kMinWidthT * 2, +font.kMinWidthY}}));
        break;
      }
      case 13:
        if (kakato_adjustment == 4) {  // box's left bottom corner
          if (x1 == x2) {
            polygons->Push(Pen(x2, y2).GetPolygon(
                {{-kMinWidthT, -font.kMinWidthY * 3},
                 {-kMinWidthT * 2, 0},
                 {-font.kMinWidthY, +font.kMinWidthY * 5},
                 {+kMinWidthT, +font.kMinWidthY}}));
          } else {
            const double m = (x1 > x2 && y1 != y2)
                                 ? std::floor((x1 - x2) / (y2 - y1) * 3)
                                 : 0;
            polygons->Push(Pen(x2 + m, y2).GetPolygon(
                {{0, -font.kMinWidthY * 5},
                 {-kMinWidthT * 2, 0},
                 {-font.kMinWidthY, +font.kMinWidthY * 5},
                 {+kMinWidthT, +font.kMinWidthY},
                 {0, 0}}));
          }
        }
        break;
      default:
        break;
    }

    switch (Sw(a1)) {
      case 22:
      case 27: {  // box's right top corner
        std::vector<Point> local = {
            {-kMinWidthT, -font.kMinWidthY},
            {0, -font.kMinWidthY - font.kWidth},
            {+kMinWidthT + font.kWidth, +font.kMinWidthY}};
        if (x1 == x2) {
          local.push_back({+kMinWidthT, +kMinWidthT});
          local.push_back({-kMinWidthT, 0});
        } else if (a1 == 27) {
          local.push_back({+kMinWidthT, +kMinWidthT - 1});
          local.push_back({0, +kMinWidthT + 2});
          local.push_back({0, 0});
        } else {
          local.push_back({+kMinWidthT, +kMinWidthT - 1});
          local.push_back({-kMinWidthT, +kMinWidthT + 4});
        }
        polygons->Push(Pen(x1, y1).GetPolygon(local));
        break;
      }
      case 0: {  // beginning of the stroke
        Polygon poly = pen1.GetPolygon(
            {{kMinWidthT, font.kMinWidthY * 0.5},
             {kMinWidthT + kMinWidthT * 0.5,
              font.kMinWidthY * 0.5 + font.kMinWidthY},
             {kMinWidthT - 2, font.kMinWidthY * 0.5 + font.kMinWidthY * 2 + 1}});
        if (x1 != x2) {
          poly.Set(2,
                   x1 + (kMinWidthT - 2) * sinrad +
                       (font.kMinWidthY * 0.5 + font.kMinWidthY * 2) * cosrad,
                   y1 + (kMinWidthT + 1) * -cosrad +
                       (font.kMinWidthY * 0.5 + font.kMinWidthY * 2) * sinrad);
        }
        polygons->Push(std::move(poly));
        break;
      }
      default:
        break;
    }

    if ((x1 == x2 && a2 == 1) ||
        (a1 == 6 && (a2 == 0 || (x1 != x2 && a2 == 5)))) {
      // The round end of a hook's bar.
      Polygon poly;
      const double r =
          (x1 == x2 && ((a1 == 6 && a2 == 0) || a2 == 1)) ? 0.6 : 0.8;
      poly.Push(pen2.GetPoint(kMinWidthT, 0));
      poly.Push(pen2.GetPoint(kMinWidthT * 0.6, kMinWidthT * r));
      poly.Push(pen2.GetPoint(0, kMinWidthT));
      poly.Push(pen2.GetPoint(-kMinWidthT * 0.6, kMinWidthT * r));
      poly.Push(pen2.GetPoint(-kMinWidthT, 0));
      if (x1 == x2 && ((a1 == 6 && a2 == 0) || a2 == 1)) poly.Reverse();
      polygons->Push(std::move(poly));
      if (x1 != x2 && a1 == 6 && a2 == 5) {  // the hook's flick
        const double hane_length = font.kWidth * 5;
        const double rv = x1 < x2 ? 1 : -1;
        polygons->Push(pen2.GetPolygon(
            {{rv * (kMinWidthT - 1), 0},
             {rv * (kMinWidthT + hane_length), 2},
             {rv * (kMinWidthT + hane_length), 0},
             {kMinWidthT - 1, -kMinWidthT}}));
      }
    }
  } else if (y1 == y2 && a1 == 6) {
    // A kagi's horizontal stroke: use the x axis, and get bold.
    Pen pen1_r(x1, y1);
    Pen pen2_r(x2, y2);
    Polygon poly0;
    poly0.Push(pen1_r.GetPoint(0, -kMinWidthT));
    poly0.Push(pen2_r.GetPoint(0, -kMinWidthT));
    poly0.Push(pen2_r.GetPoint(0, +kMinWidthT));
    poly0.Push(pen1_r.GetPoint(0, +kMinWidthT));
    polygons->Push(std::move(poly0));
    switch (Sw(a2)) {
      case 1:
      case 0:
      case 5: {
        Pen pen2(x2, y2);
        if (x1 > x2) pen2.SetMatrix2(-1, 0);
        const double r = 0.6;
        Polygon poly = pen2.GetPolygon({{0, -kMinWidthT},
                                        {+kMinWidthT * r, -kMinWidthT * 0.6},
                                        {+kMinWidthT, 0},
                                        {+kMinWidthT * r, +kMinWidthT * 0.6},
                                        {0, +kMinWidthT}});
        if (x1 >= x2) poly.Reverse();
        polygons->Push(std::move(poly));
        if (a2 == 5) {
          const double hane_length =
              font.kWidth * (4 * (1 - opt1 / font.kAdjustMageStep) + 1);
          const double rv = x1 < x2 ? 1 : -1;
          polygons->Push(pen2.GetPolygon(
              {{0, rv * -kMinWidthT},
               {2, rv * (-kMinWidthT - hane_length)},
               {0, rv * (-kMinWidthT - hane_length)},
               {-kMinWidthT, rv * -kMinWidthT}}));
        }
        break;
      }
      default:
        break;
    }
  } else {
    // Shallow angle: use the x axis.
    double cosrad, sinrad;
    if (y1 == y2) {
      cosrad = 1;
      sinrad = 0;
    } else {
      const Vec n = Normalize(x2 - x1, y2 - y1);
      cosrad = n.x;
      sinrad = n.y;
    }
    Pen pen1(x1, y1);
    Pen pen2(x2, y2);
    pen1.SetMatrix2(cosrad, sinrad);
    pen2.SetMatrix2(cosrad, sinrad);
    Polygon poly;
    poly.Push(pen1.GetPoint(0, -font.kMinWidthY));
    poly.Push(pen2.GetPoint(0, -font.kMinWidthY));
    poly.Push(pen2.GetPoint(0, font.kMinWidthY));
    poly.Push(pen1.GetPoint(0, font.kMinWidthY));
    polygons->Push(std::move(poly));
    if (Sw(a2) == 0) {  // uroko
      const double uroko_scale =
          (font.kMinWidthU / font.kMinWidthY - 1.0) / 4.0 + 1.0;
      Polygon poly2 = pen2.GetPolygon(
          {{0, -font.kMinWidthY}, {-At(font.kAdjustUrokoX, uroko_adjustment) * uroko_scale, 0}});
      poly2.Push(x2 - (cosrad - sinrad) * At(font.kAdjustUrokoX, uroko_adjustment) * uroko_scale / 2,
                 y2 - (sinrad + cosrad) * At(font.kAdjustUrokoY, uroko_adjustment) * uroko_scale);
      polygons->Push(std::move(poly2));
    }
  }
}

// ----- mincho/index.js: transforms, per-type stroke drawing, adjustments -----

std::vector<int> SelectPolygonsRect(const Polygons &polygons, double x1,
                                    double y1, double x2, double y2) {
  std::vector<int> selected;
  const auto &array = polygons.array();
  for (int i = 0; i < static_cast<int>(array.size()); ++i) {
    bool inside = true;
    for (const Point &p : array[i].Points()) {
      if (!(x1 <= p.x && p.x <= x2 && y1 <= p.y && p.y <= y2)) {
        inside = false;
        break;
      }
    }
    if (inside) selected.push_back(i);
  }
  return selected;
}

// Type 0 strokes: reflect / rotate what has been drawn inside a rectangle.
void DfTransform(Polygons *polygons, double x1, double y1, double x2,
                 double y2, double a2_100, double a3_100, double a2_opt,
                 double a3_opt) {
  auto apply = [&](double dx, double dy, auto &&op) {
    for (int i : SelectPolygonsRect(*polygons, x1, y1, x2, y2)) {
      Polygon &p = polygons->mutable_array()[i];
      op(p);
      p.Translate(dx, dy).Floor();
    }
  };
  if (a2_100 == 98 && a2_opt == 0) {
    apply(x1 + x2, 0, [](Polygon &p) { p.ReflectX(); });
  } else if (a2_100 == 97 && a2_opt == 0) {
    apply(0, y1 + y2, [](Polygon &p) { p.ReflectY(); });
  } else if (a2_100 == 99 && a2_opt == 0) {
    if (a3_100 == 1 && a3_opt == 0) {
      apply(x1 + y2, y1 - x1, [](Polygon &p) { p.Rotate90(); });
    } else if (a3_100 == 2 && a3_opt == 0) {
      apply(x1 + x2, y1 + y2, [](Polygon &p) { p.Rotate180(); });
    } else if (a3_100 == 3 && a3_opt == 0) {
      apply(x1 - y1, y2 + x1, [](Polygon &p) { p.Rotate270(); });
    }
  }
}

void DfDrawFont(const Mincho &font, Polygons *polygons, const AdjStroke &adj) {
  const Stroke &s = adj.stroke;
  const double a2_100 = s.a2_100, a2_opt_1 = s.a2_opt_1, a2_opt_2 = s.a2_opt_2,
               a2_opt_3 = s.a2_opt_3, a3_100 = s.a3_100, a3_opt = s.a3_opt,
               a3_opt_1 = s.a3_opt_1, a3_opt_2 = s.a3_opt_2;
  const double x1 = s.x1, y1 = s.y1, x2 = s.x2, y2 = s.y2, x3 = s.x3, y3 = s.y3,
               x4 = s.x4, y4 = s.y4;
  const double tate = adj.tateAdjustment, hane = adj.haneAdjustment,
               uroko = adj.urokoAdjustment, kakato = adj.kakatoAdjustment,
               mage = adj.mageAdjustment, kirikuchi = adj.kirikuchiAdjustment;

  switch (Sw(s.a1_100)) {
    case 0:
      DfTransform(polygons, x1, y1, x2, y2, a2_100, a3_100, s.a2_opt, a3_opt);
      break;
    case 1: {
      if (a3_100 == 4) {
        const Vec d = (x1 == x2 && y1 == y2) ? Vec{0, font.kMage}
                                              : Normalize(x1 - x2, y1 - y2, font.kMage);
        const double tx1 = x2 + d.x, ty1 = y2 + d.y;
        CdDrawLine(font, polygons, x1, y1, tx1, ty1, a2_100 + a2_opt_1 * 100, 1,
                   tate, 0, 0);
        CdDrawCurve(font, polygons, tx1, ty1, x2, y2,
                    x2 - font.kMage *
                             (((font.kAdjustTateStep + 4) - tate) /
                              (font.kAdjustTateStep + 4)),
                    y2, 1, 14, std::fmod(tate, 10), hane, std::floor(tate / 10),
                    a3_opt_2);
      } else {
        CdDrawLine(font, polygons, x1, y1, x2, y2, a2_100 + a2_opt_1 * 100,
                   a3_100, tate, uroko, kakato);
      }
      break;
    }
    case 2: {
      if (a3_100 == 4) {
        Vec d;
        if (x2 == x3) {
          d = {0, -font.kMage};
        } else if (y2 == y3) {
          d = {-font.kMage, 0};
        } else {
          d = Normalize(x2 - x3, y2 - y3, font.kMage);
        }
        const double tx1 = x3 + d.x, ty1 = y3 + d.y;
        CdDrawCurve(font, polygons, x1, y1, x2, y2, tx1, ty1,
                    a2_100 + kirikuchi * 100, 0, a2_opt_2, 0, a2_opt_3, 0);
        CdDrawCurve(font, polygons, tx1, ty1, x3, y3, x3 - font.kMage, y3, 2, 14,
                    a2_opt_2, hane, 0, a3_opt_2);
      } else {
        CdDrawCurve(font, polygons, x1, y1, x2, y2, x3, y3,
                    a2_100 + kirikuchi * 100,
                    (a3_100 == 5 && a3_opt == 0) ? 15 : a3_100, a2_opt_2,
                    a3_opt_1, a2_opt_3, a3_opt_2);
      }
      break;
    }
    case 3: {
      const Vec d1 = (x1 == x2 && y1 == y2) ? Vec{0, font.kMage}
                                             : Normalize(x1 - x2, y1 - y2, font.kMage);
      const double tx1 = x2 + d1.x, ty1 = y2 + d1.y;
      const Vec d2 = (x2 == x3 && y2 == y3) ? Vec{0, -font.kMage}
                                             : Normalize(x3 - x2, y3 - y2, font.kMage);
      const double tx2 = x2 + d2.x, ty2 = y2 + d2.y;
      CdDrawLine(font, polygons, x1, y1, tx1, ty1, a2_100 + a2_opt_1 * 100, 1,
                 tate, 0, 0);
      CdDrawCurve(font, polygons, tx1, ty1, x2, y2, tx2, ty2, 1, 1, 0, 0, tate,
                  mage);
      if (!(a3_100 == 5 && a3_opt_1 == 0 &&
            !((x2 < x3 && x3 - tx2 > 0) || (x2 > x3 && tx2 - x3 > 0)))) {
        const double opt2 =
            (a3_100 == 5 && a3_opt_1 == 0) ? 0 : a3_opt_1 + mage * 10;
        CdDrawLine(font, polygons, tx2, ty2, x3, y3, 6, a3_100, mage, opt2, opt2);
      }
      break;
    }
    case 12: {
      CdDrawCurve(font, polygons, x1, y1, x2, y2, x3, y3,
                  a2_100 + a2_opt_1 * 100, 1, a2_opt_2, 0, a2_opt_3, 0);
      CdDrawLine(font, polygons, x3, y3, x4, y4, 6, a3_100, 0, a3_opt, a3_opt);
      break;
    }
    case 4: {
      double rate = std::hypot(x3 - x2, y3 - y2) / 120 * 6;
      if (rate > 6) rate = 6;
      const Vec d1 = (x1 == x2 && y1 == y2)
                         ? Vec{0, font.kMage * rate}
                         : Normalize(x1 - x2, y1 - y2, font.kMage * rate);
      const double tx1 = x2 + d1.x, ty1 = y2 + d1.y;
      const Vec d2 = (x2 == x3 && y2 == y3)
                         ? Vec{0, -font.kMage * rate}
                         : Normalize(x3 - x2, y3 - y2, font.kMage * rate);
      const double tx2 = x2 + d2.x, ty2 = y2 + d2.y;
      CdDrawLine(font, polygons, x1, y1, tx1, ty1, a2_100 + a2_opt_1 * 100, 1,
                 a2_opt_2 + a2_opt_3 * 10, 0, 0);
      CdDrawCurve(font, polygons, tx1, ty1, x2, y2, tx2, ty2, 1, 1, 0, 0, 0, 0);
      if (!(a3_100 == 5 && a3_opt == 0 && x3 - tx2 <= 0)) {
        CdDrawLine(font, polygons, tx2, ty2, x3, y3, 6, a3_100, 0, a3_opt, a3_opt);
      }
      break;
    }
    case 6: {
      if (a3_100 == 4) {
        Vec d;
        if (x3 == x4) {
          d = {0, -font.kMage};
        } else if (y3 == y4) {
          d = {-font.kMage, 0};
        } else {
          d = Normalize(x3 - x4, y3 - y4, font.kMage);
        }
        const double tx1 = x4 + d.x, ty1 = y4 + d.y;
        CdDrawBezier(font, polygons, x1, y1, x2, y2, x3, y3, tx1, ty1,
                     a2_100 + a2_opt_1 * 100, 1, a2_opt_2, 0, a2_opt_3, 0);
        CdDrawCurve(font, polygons, tx1, ty1, x4, y4, x4 - font.kMage, y4, 1, 14,
                    0, hane, 0, a3_opt_2);
      } else {
        CdDrawBezier(font, polygons, x1, y1, x2, y2, x3, y3, x4, y4,
                     a2_100 + a2_opt_1 * 100,
                     (a3_100 == 5 && a3_opt == 0) ? 15 : a3_100, a2_opt_2,
                     a3_opt_1, a2_opt_3, a3_opt_2);
      }
      break;
    }
    case 7: {
      CdDrawLine(font, polygons, x1, y1, x2, y2, a2_100 + a2_opt_1 * 100, 1,
                 tate, 0, 0);
      CdDrawCurve(font, polygons, x2, y2, x3, y3, x4, y4, 1, a3_100,
                  std::fmod(tate, 10), a3_opt_1, std::floor(tate / 10), a3_opt_2);
      break;
    }
    default:
      break;
  }
}

// How a stroke is changed by its neighbours (thinner where crowded, etc.).
void AdjustHane(std::vector<AdjStroke> *adj) {
  struct Seg {
    int index;
    double x, y1, y2;
  };
  std::vector<Seg> vert;
  for (int i = 0; i < static_cast<int>(adj->size()); ++i) {
    const Stroke &s = (*adj)[i].stroke;
    if (s.a1_100 == 1 && s.a1_opt == 0 && s.x1 == s.x2) {
      vert.push_back({i, s.x1, s.y1, s.y2});
    }
  }
  for (int i = 0; i < static_cast<int>(adj->size()); ++i) {
    AdjStroke &a = (*adj)[i];
    const Stroke &s = a.stroke;
    if ((s.a1_100 == 1 || s.a1_100 == 2 || s.a1_100 == 6) && s.a1_opt == 0 &&
        s.a3_100 == 4 && s.a3_opt == 0) {
      double lpx, lpy;
      if (s.a1_100 == 1) {
        lpx = s.x2;
        lpy = s.y2;
      } else if (s.a1_100 == 2) {
        lpx = s.x3;
        lpy = s.y3;
      } else {
        lpx = s.x4;
        lpy = s.y4;
      }
      double mn = std::numeric_limits<double>::infinity();
      if (lpx + 18 < 100) mn = lpx + 18;
      for (const Seg &v : vert) {
        if (v.index != i && lpx - v.x < 100 && v.x < lpx && v.y1 <= lpy &&
            v.y2 >= lpy) {
          mn = std::min(mn, lpx - v.x);
        }
      }
      if (mn != std::numeric_limits<double>::infinity()) {
        a.haneAdjustment += 7 - std::floor(mn / 15);
      }
    }
  }
}

void AdjustMage(const Mincho &font, std::vector<AdjStroke> *adj) {
  struct Seg {
    int index;
    bool is_target;
    double y, x1, x2;
  };
  std::vector<Seg> hori;
  for (int i = 0; i < static_cast<int>(adj->size()); ++i) {
    const Stroke &s = (*adj)[i].stroke;
    if (s.a1_100 == 1 && s.a1_opt == 0 && s.y1 == s.y2) {
      hori.push_back({i, false, s.y2, s.x1, s.x2});
    } else if (s.a1_100 == 3 && s.a1_opt == 0 && s.y2 == s.y3) {
      hori.push_back({i, true, s.y2, s.x2, s.x3});
    }
  }
  for (const Seg &seg : hori) {
    if (!seg.is_target) continue;
    AdjStroke &a = (*adj)[seg.index];
    for (const Seg &o : hori) {
      if (o.index != seg.index && !(seg.x1 + 1 > o.x2 || seg.x2 - 1 < o.x1) &&
          Round(std::fabs(seg.y - o.y)) < font.kMinWidthT * font.kAdjustMageStep) {
        a.mageAdjustment +=
            font.kAdjustMageStep - std::floor(std::fabs(seg.y - o.y) / font.kMinWidthT);
        if (a.mageAdjustment > font.kAdjustMageStep) {
          a.mageAdjustment = font.kAdjustMageStep;
        }
      }
    }
  }
}

void AdjustTate(const Mincho &font, std::vector<AdjStroke> *adj) {
  struct Seg {
    int index;
    double x, y1, y2;
  };
  std::vector<Seg> vert;
  for (int i = 0; i < static_cast<int>(adj->size()); ++i) {
    const Stroke &s = (*adj)[i].stroke;
    if ((s.a1_100 == 1 || s.a1_100 == 3 || s.a1_100 == 7) && s.a1_opt == 0 &&
        s.x1 == s.x2) {
      vert.push_back({i, s.x1, s.y1, s.y2});
    }
  }
  for (const Seg &seg : vert) {
    AdjStroke &a = (*adj)[seg.index];
    const Stroke &stroke = a.stroke;
    for (const Seg &o : vert) {
      if (o.index != seg.index && !(seg.y1 + 1 > o.y2 || seg.y2 - 1 < o.y1) &&
          Round(std::fabs(seg.x - o.x)) < font.kMinWidthT * font.kAdjustTateStep) {
        a.tateAdjustment +=
            font.kAdjustTateStep - std::floor(std::fabs(seg.x - o.x) / font.kMinWidthT);
        if (a.tateAdjustment > font.kAdjustTateStep ||
            (a.tateAdjustment == font.kAdjustTateStep &&
             (stroke.a2_opt_1 != 0 || stroke.a2_100 != 0))) {
          a.tateAdjustment = font.kAdjustTateStep;
        }
      }
    }
  }
}

void AdjustKakato(const Mincho &font, std::vector<AdjStroke> *adj) {
  for (int i = 0; i < static_cast<int>(adj->size()); ++i) {
    AdjStroke &a = (*adj)[i];
    const Stroke &s = a.stroke;
    if (s.a1_100 == 1 && s.a1_opt == 0 && (s.a3_100 == 13 || s.a3_100 == 23) &&
        s.a3_opt == 0) {
      for (int k = 0; k < font.kAdjustKakatoStep; ++k) {
        bool crossed = false;
        for (int j = 0; j < static_cast<int>(adj->size()); ++j) {
          if (j != i &&
              (*adj)[j].stroke.IsCrossBoxRect(
                  s.x2 - font.kAdjustKakatoRangeX / 2,
                  s.y2 + font.kAdjustKakatoRangeY[k],
                  s.x2 + font.kAdjustKakatoRangeX / 2,
                  s.y2 + font.kAdjustKakatoRangeY[k + 1])) {
            crossed = true;
            break;
          }
        }
        if (crossed || Round(s.y2 + font.kAdjustKakatoRangeY[k + 1]) > 200 ||
            Round(s.y2 - s.y1) < font.kAdjustKakatoRangeY[k + 1]) {
          a.kakatoAdjustment = 3 - k;
          break;
        }
      }
    }
  }
}

void AdjustUroko(const Mincho &font, std::vector<AdjStroke> *adj) {
  for (int i = 0; i < static_cast<int>(adj->size()); ++i) {
    AdjStroke &a = (*adj)[i];
    const Stroke &s = a.stroke;
    if (s.a1_100 == 1 && s.a1_opt == 0 && s.a3_100 == 0 && s.a3_opt == 0) {
      for (int k = 0; k < font.kAdjustUrokoLengthStep; ++k) {
        double cosrad, sinrad;
        if (s.y1 == s.y2) {
          cosrad = 1;
          sinrad = 0;
        } else if (s.x2 - s.x1 < 0) {
          const Vec n = Normalize(s.x1 - s.x2, s.y1 - s.y2);
          cosrad = n.x;
          sinrad = n.y;
        } else {
          const Vec n = Normalize(s.x2 - s.x1, s.y2 - s.y1);
          cosrad = n.x;
          sinrad = n.y;
        }
        const double tx = s.x2 - font.kAdjustUrokoLine[k] * cosrad - 0.5 * sinrad;
        const double ty = s.y2 - font.kAdjustUrokoLine[k] * sinrad - 0.5 * cosrad;
        const double tlen = (s.y1 == s.y2) ? s.x2 - s.x1
                                           : std::hypot(s.y2 - s.y1, s.x2 - s.x1);
        bool crossed = false;
        for (int j = 0; j < static_cast<int>(adj->size()); ++j) {
          if (j != i && (*adj)[j].stroke.IsCrossSegment(tx, ty, s.x2, s.y2)) {
            crossed = true;
            break;
          }
        }
        if (Round(tlen) < font.kAdjustUrokoLength[k] || crossed) {
          a.urokoAdjustment = font.kAdjustUrokoLengthStep - k;
          break;
        }
      }
    }
  }
}

void AdjustUroko2(const Mincho &font, std::vector<AdjStroke> *adj) {
  struct Seg {
    int index;
    bool is_target;
    double y, x1, x2;
  };
  std::vector<Seg> hori;
  for (int i = 0; i < static_cast<int>(adj->size()); ++i) {
    const AdjStroke &a = (*adj)[i];
    const Stroke &s = a.stroke;
    if (s.a1_100 == 1 && s.a1_opt == 0 && s.y1 == s.y2) {
      hori.push_back({i,
                      s.a3_100 == 0 && s.a3_opt == 0 && a.urokoAdjustment == 0,
                      s.y1, s.x1, s.x2});
    } else if (s.a1_100 == 3 && s.a1_opt == 0 && s.y2 == s.y3) {
      hori.push_back({i, false, s.y2, s.x2, s.x3});
    }
  }
  for (const Seg &seg : hori) {
    if (!seg.is_target) continue;
    double pressure = 0;
    for (const Seg &o : hori) {
      if (o.index != seg.index && !(seg.x1 + 1 > o.x2 || seg.x2 - 1 < o.x1) &&
          Round(std::fabs(seg.y - o.y)) < font.kAdjustUroko2Length) {
        pressure += std::pow(font.kAdjustUroko2Length - std::fabs(seg.y - o.y), 1.1);
      }
    }
    (*adj)[seg.index].urokoAdjustment =
        std::min(std::floor(pressure / font.kAdjustUroko2Length),
                 static_cast<double>(font.kAdjustUroko2Step));
  }
}

void AdjustKirikuchi(std::vector<AdjStroke> *adj) {
  struct Seg {
    double y, x1, x2;
  };
  std::vector<Seg> hori;
  for (const AdjStroke &a : *adj) {
    const Stroke &s = a.stroke;
    if (s.a1_100 == 1 && s.a1_opt == 0 && s.y1 == s.y2) {
      hori.push_back({s.y1, s.x1, s.x2});
    }
  }
  for (AdjStroke &a : *adj) {
    const Stroke &s = a.stroke;
    if (s.a1_100 == 2 && s.a1_opt == 0 && s.a2_100 == 32 && s.a2_opt == 0 &&
        s.x1 > s.x2 && s.y1 < s.y2) {
      for (const Seg &h : hori) {
        if (h.x1 < s.x1 && h.x2 > s.x1 && h.y == s.y1) {
          a.kirikuchiAdjustment = 1;
          break;
        }
      }
    }
  }
}

std::vector<AdjStroke> AdjustStrokes(const Mincho &font,
                                     const std::vector<Stroke> &strokes) {
  std::vector<AdjStroke> adj;
  adj.reserve(strokes.size());
  for (const Stroke &s : strokes) {
    adj.push_back({s,
                   s.a2_opt_1,                      // kirikuchi
                   s.a2_opt_2 + s.a2_opt_3 * 10,    // tate
                   s.a3_opt_1,                      // hane
                   s.a3_opt,                        // uroko
                   s.a3_opt,                        // kakato
                   s.a3_opt_2});                    // mage
  }
  AdjustHane(&adj);
  AdjustMage(font, &adj);
  AdjustTate(font, &adj);
  AdjustKakato(font, &adj);
  AdjustUroko(font, &adj);
  AdjustUroko2(font, &adj);
  AdjustKirikuchi(&adj);
  return adj;
}

// ------------------------------------------------------------- kage.js ---

// JavaScript's +"..." then Math.floor: "" is 0, a missing column or text that
// is not a number is NaN.
double Column(const std::vector<std::string_view> &cols, size_t i) {
  if (i >= cols.size()) return kNaN;
  std::string s(cols[i]);
  const size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return 0;
  const size_t e = s.find_last_not_of(" \t\r\n");
  s = s.substr(b, e - b + 1);
  char *end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str() || *end != '\0') return kNaN;
  return std::floor(v);
}

Box GetBox(const std::vector<Stroke> &strokes) {
  Box b{200, 0, 200, 0};
  for (const Stroke &s : strokes) {
    const Box sb = s.GetBox();
    b.min_x = std::min(b.min_x, sb.min_x);
    b.max_x = std::max(b.max_x, sb.max_x);
    b.min_y = std::min(b.min_y, sb.min_y);
    b.max_y = std::max(b.max_y, sb.max_y);
  }
  return b;
}

void CollectStrokes(const Buhin &buhin, std::string_view data, int depth,
                    std::vector<Stroke> *out);

void CollectStrokesOfBuhin(const Buhin &buhin, std::string_view data, double x1,
                           double y1, double x2, double y2, double sx, double sy,
                           double sx2, double sy2, int depth,
                           std::vector<Stroke> *out) {
  std::vector<Stroke> strokes;
  CollectStrokes(buhin, data, depth, &strokes);
  const Box box = GetBox(strokes);
  if (sx != 0 || sy != 0) {
    if (sx > 100) {
      sx -= 200;
    } else {
      sx2 = 0;
      sy2 = 0;
    }
  }
  for (Stroke &s : strokes) {
    if (sx != 0 || sy != 0) {
      s.StretchBy(sx, sx2, sy, sy2, box.min_x, box.max_x, box.min_y, box.max_y);
    }
    s.x1 = x1 + s.x1 * (x2 - x1) / 200;
    s.y1 = y1 + s.y1 * (y2 - y1) / 200;
    s.x2 = x1 + s.x2 * (x2 - x1) / 200;
    s.y2 = y1 + s.y2 * (y2 - y1) / 200;
    s.x3 = x1 + s.x3 * (x2 - x1) / 200;
    s.y3 = y1 + s.y3 * (y2 - y1) / 200;
    s.x4 = x1 + s.x4 * (x2 - x1) / 200;
    s.y4 = y1 + s.y4 * (y2 - y1) / 200;
    out->push_back(s);
  }
}

void CollectStrokes(const Buhin &buhin, std::string_view data, int depth,
                    std::vector<Stroke> *out) {
  if (depth > kMaxComponentDepth) return;  // a glyph that includes itself
  for (std::string_view record : absl::StrSplit(data, '$')) {
    const std::vector<std::string_view> cols = absl::StrSplit(record, ':');
    const double type = Column(cols, 0);
    if (type != 99) {
      double d[11];
      for (int i = 0; i < 11; ++i) d[i] = Column(cols, i);
      out->push_back(Stroke(d));
    } else {
      const std::string_view sub =
          buhin.Search(cols.size() > 7 ? cols[7] : std::string_view());
      if (!sub.empty()) {
        CollectStrokesOfBuhin(buhin, sub, Column(cols, 3), Column(cols, 4),
                              Column(cols, 5), Column(cols, 6), Column(cols, 1),
                              Column(cols, 2), Column(cols, 9), Column(cols, 10),
                              depth + 1, out);
      }
    }
  }
}

void CollectUnresolved(const Buhin &buhin, std::string_view data, int depth,
                       std::vector<std::string> *missing) {
  if (depth > kMaxComponentDepth) return;
  for (std::string_view record : absl::StrSplit(data, '$')) {
    const std::vector<std::string_view> cols = absl::StrSplit(record, ':');
    if (cols.size() < 8 || cols[0] != "99" || cols[7].empty()) continue;
    const std::string_view sub = buhin.Search(cols[7]);
    if (sub.empty()) {
      std::string name(cols[7].substr(0, cols[7].find('@')));
      if (std::find(missing->begin(), missing->end(), name) == missing->end()) {
        missing->push_back(std::move(name));
      }
    } else {
      CollectUnresolved(buhin, sub, depth + 1, missing);
    }
  }
}

}  // namespace

std::string_view Buhin::Search(std::string_view name) const {
  const auto it = map_.find(name);
  if (it != map_.end()) return it->second;
  const size_t at = name.find('@');
  if (at != std::string_view::npos) {
    const auto base = map_.find(name.substr(0, at));
    if (base != map_.end()) return base->second;
  }
  return parent_ != nullptr ? parent_->Search(name) : std::string_view();
}

void Kage::MakeGlyph(Polygons *polygons, std::string_view name) const {
  MakeGlyphFromData(polygons, buhin_->Search(name));
}

void Kage::MakeGlyphFromData(Polygons *polygons, std::string_view data) const {
  if (data.empty()) return;
  std::vector<Stroke> strokes;
  CollectStrokes(*buhin_, data, 0, &strokes);
  const Mincho font;
  for (const AdjStroke &adj : AdjustStrokes(font, strokes)) {
    DfDrawFont(font, polygons, adj);
  }
}

std::vector<std::string> Kage::UnresolvedComponents(
    std::string_view data) const {
  std::vector<std::string> missing;
  CollectUnresolved(*buhin_, data, 0, &missing);
  return missing;
}

std::string PolygonsToPathData(Polygons polygons) {
  polygons.NormalizeWinding(/*clockwise=*/true);
  std::string d;
  for (const Polygon &poly : polygons.array()) {
    const std::vector<Point> pts = poly.Points();
    for (size_t i = 0; i < pts.size(); ++i) {
      absl::StrAppend(&d, i == 0 ? "M" : "L", pts[i].x, ",", pts[i].y, " ");
    }
    absl::StrAppend(&d, "Z ");
  }
  if (!d.empty()) d.pop_back();
  return d;
}

}  // namespace kage
}  // namespace mozc
