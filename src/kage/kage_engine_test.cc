// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The C++ engine must produce the same polygons as the JavaScript engine it
// is ported from. testdata/kage_reference.tsv holds that engine's output for
// real GlyphWiki glyphs and for composed layouts (see testdata/make_reference.mjs).

#include "kage/kage_engine.h"

#include <cmath>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "base/file_util.h"
#include "kage/kage_data.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace kage {
namespace {

using PointList = std::vector<std::pair<double, double>>;

struct Case {
  std::string data;  // empty: render the glyph named by the case
  size_t polygon_count = 0;
  std::vector<PointList> polygons;
};

std::string Read(const std::string &name) {
  const std::string path = testing::GetSourceFileOrDie({"kage", "testdata", name});
  absl::StatusOr<std::string> contents = FileUtil::GetContents(path);
  EXPECT_TRUE(contents.ok()) << contents.status();
  return contents.ok() ? *contents : "";
}

std::map<std::string, Case> LoadReference() {
  std::map<std::string, Case> cases;
  for (std::string_view line : absl::StrSplit(Read("kage_reference.tsv"), '\n')) {
    if (line.empty()) continue;
    const std::vector<std::string_view> f = absl::StrSplit(line, '\t');
    Case &c = cases[std::string(f[1])];
    if (f[0] == "D") {
      c.data = std::string(f[2]);
    } else if (f[0] == "C") {
      EXPECT_TRUE(absl::SimpleAtoi(f[2], &c.polygon_count));
    } else if (f[0] == "P") {
      PointList pts;
      for (std::string_view xy : absl::StrSplit(f[2], ';')) {
        const std::vector<std::string_view> p = absl::StrSplit(xy, ',');
        double x = 0, y = 0;
        EXPECT_TRUE(absl::SimpleAtod(p[0], &x));
        EXPECT_TRUE(absl::SimpleAtod(p[1], &y));
        pts.push_back({x, y});
      }
      c.polygons.push_back(std::move(pts));
    }
  }
  return cases;
}

TEST(KageEngineTest, MatchesTheJavaScriptEngine) {
  const Buhin buhin = ParseBuhin(Read("kage_test_data.tsv"));
  ASSERT_GT(buhin.size(), 20);
  const Kage kage(&buhin);
  const std::map<std::string, Case> reference = LoadReference();
  ASSERT_GE(reference.size(), 15);

  for (const auto &[name, expected] : reference) {
    SCOPED_TRACE(name);
    Polygons polygons;
    if (expected.data.empty()) {
      kage.MakeGlyph(&polygons, name);
    } else {
      kage.MakeGlyphFromData(&polygons, expected.data);
    }
    ASSERT_EQ(polygons.array().size(), expected.polygon_count);
    ASSERT_EQ(polygons.array().size(), expected.polygons.size());
    for (size_t i = 0; i < expected.polygons.size(); ++i) {
      SCOPED_TRACE(i);
      const std::vector<Point> got = polygons.array()[i].Points();
      ASSERT_EQ(got.size(), expected.polygons[i].size());
      for (size_t j = 0; j < got.size(); ++j) {
        EXPECT_NEAR(got[j].x, expected.polygons[i][j].first, 1e-9) << "point " << j;
        EXPECT_NEAR(got[j].y, expected.polygons[i][j].second, 1e-9) << "point " << j;
      }
    }
  }
}

TEST(KageEngineTest, UnknownGlyphDrawsNothing) {
  const Buhin buhin;
  const Kage kage(&buhin);
  Polygons polygons;
  kage.MakeGlyph(&polygons, "u4e00");
  EXPECT_TRUE(polygons.array().empty());
}

TEST(KageEngineTest, ReportsMissingComponents) {
  const Buhin buhin = ParseBuhin("u6728\t1:0:0:10:100:190:100\n");
  const Kage kage(&buhin);
  const std::vector<std::string> missing = kage.UnresolvedComponents(
      "99:0:0:0:0:100:200:u6728$99:0:0:100:0:200:200:u5dfb@3");
  ASSERT_EQ(missing.size(), 1);
  EXPECT_EQ(missing[0], "u5dfb");
}

TEST(KageEngineTest, ComponentThatIncludesItselfTerminates) {
  const Buhin buhin = ParseBuhin("ua\t99:0:0:0:0:200:200:ua$1:0:0:10:100:190:100\n");
  const Kage kage(&buhin);
  Polygons polygons;
  kage.MakeGlyph(&polygons, "ua");
  EXPECT_FALSE(polygons.array().empty());
}

TEST(KageEngineTest, ParametersOnAComponentNameAreIgnored) {
  const Buhin buhin = ParseBuhin("u6728\t1:0:0:10:100:190:100\n");
  const Kage kage(&buhin);
  Polygons a, b;
  kage.MakeGlyphFromData(&a, "99:0:0:0:0:200:200:u6728");
  kage.MakeGlyphFromData(&b, "99:0:0:0:0:200:200:u6728@4");
  ASSERT_FALSE(a.array().empty());
  EXPECT_EQ(a.array().size(), b.array().size());
}

TEST(KageEngineTest, PathDataIsOnePathOfClosedContours) {
  const Buhin buhin = ParseBuhin("u6728\t1:0:0:10:100:190:100\n");
  const Kage kage(&buhin);
  Polygons polygons;
  kage.MakeGlyph(&polygons, "u6728");
  const std::string d = PolygonsToPathData(polygons);
  EXPECT_EQ(d.substr(0, 1), "M");
  EXPECT_EQ(d.substr(d.size() - 1), "Z");
}

}  // namespace
}  // namespace kage
}  // namespace mozc
