// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/composer_kage.h"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "base/strings/unicode.h"
#include "composer_ids/ids_tree.h"
#include "kage/kage_engine.h"

namespace mozc {
namespace gui {
namespace {

using composer_ids::ComposerSlot;

struct Box {
  int left, top, right, bottom;
};

constexpr Box kFull = {0, 0, 200, 200};

// Where each part of a layout goes in the 0..200 KAGE space. The two halves of
// ⿰ and ⿱ overlap a little, as the strokes of real GlyphWiki compositions do
// (the engine does not stretch a component to fill its box, so two clean halves
// would leave a gap). The other layouts are plain geometric defaults. These are
// the same boxes the Grognard composer uses.
std::vector<Box> BoxesFor(std::string_view op) {
  if (op == "⿰") return {{0, 0, 108, 200}, {92, 0, 200, 200}};
  if (op == "⿱") return {{0, 0, 200, 108}, {0, 92, 200, 200}};
  if (op == "⿲") return {{0, 0, 74, 200}, {63, 0, 137, 200}, {126, 0, 200, 200}};
  if (op == "⿳") return {{0, 0, 200, 74}, {0, 63, 200, 137}, {0, 126, 200, 200}};
  if (op == "⿴") return {kFull, {40, 40, 160, 160}};
  if (op == "⿵") return {kFull, {30, 60, 170, 200}};
  if (op == "⿶") return {kFull, {30, 0, 170, 140}};
  if (op == "⿷") return {kFull, {60, 30, 200, 170}};
  if (op == "⿸") return {kFull, {60, 60, 200, 200}};
  if (op == "⿹") return {kFull, {0, 60, 140, 200}};
  if (op == "⿺") return {kFull, {60, 0, 200, 140}};
  if (op == "⿻") return {kFull, kFull};
  return {{0, 0, 100, 200}, {100, 0, 200, 200}};
}

// GlyphWiki's name for a character: "u" and its code point in lower-case hex.
std::string GlyphName(std::string_view ch) {
  const std::u32string cps = strings::Utf8ToUtf32(ch);
  if (cps.size() != 1) return {};
  char buf[16];
  std::snprintf(buf, sizeof(buf), "u%04x", static_cast<unsigned>(cps[0]));
  return buf;
}

std::string ComposeLayout(std::string_view op, const std::vector<ComposerSlot> &parts,
                          kage::Buhin *nested) {
  const std::vector<Box> boxes = BoxesFor(op);
  std::vector<std::string> records;
  for (size_t i = 0; i < parts.size(); ++i) {
    const Box &box = boxes[std::min(i, boxes.size() - 1)];
    std::string name;
    if (parts[i].kind == ComposerSlot::Kind::kLeaf) {
      name = GlyphName(parts[i].input);
    } else {
      const std::string inner = ComposeLayout(parts[i].op, parts[i].parts, nested);
      if (inner.empty()) return {};
      name = absl::StrCat("__nested", nested->size());
      nested->Set(name, inner);
    }
    if (name.empty()) return {};
    records.push_back(absl::StrCat("99:0:0:", box.left, ":", box.top, ":", box.right,
                                   ":", box.bottom, ":", name, ":0:0:0"));
  }
  return absl::StrJoin(records, "$");
}

}  // namespace

std::string ComposeKageData(std::string_view op,
                            const std::vector<ComposerSlot> &parts,
                            kage::Buhin *nested) {
  if (!composer_ids::ComposeIds(op, parts)) return {};
  return ComposeLayout(op, parts, nested);
}

std::optional<std::vector<Contour>> ComposerOutlines(
    std::string_view op, const std::vector<ComposerSlot> &parts,
    const kage::Buhin &glyphs, int view_box) {
  // Nested layouts are glyphs of their own; keep them in a table on top of the
  // shared, read-only one.
  kage::Buhin scratch(&glyphs);
  const std::string data = ComposeKageData(op, parts, &scratch);
  if (data.empty()) return std::nullopt;

  const kage::Kage kage(&scratch);
  if (!kage.UnresolvedComponents(data).empty()) return std::nullopt;
  kage::Polygons polygons;
  kage.MakeGlyphFromData(&polygons, data);
  if (polygons.array().empty()) return std::nullopt;
  polygons.NormalizeWinding(/*clockwise=*/true);

  const double scale = view_box / 200.0;
  std::vector<Contour> outlines;
  for (const kage::Polygon &polygon : polygons.array()) {
    Contour contour;
    for (const kage::Point &p : polygon.Points()) {
      contour.push_back({p.x * scale, p.y * scale});
    }
    outlines.push_back(std::move(contour));
  }
  return outlines;
}

std::string OutlinesToPathData(const std::vector<Contour> &outlines) {
  std::string d;
  for (const Contour &contour : outlines) {
    for (size_t i = 0; i < contour.size(); ++i) {
      absl::StrAppend(&d, i == 0 ? "M" : "L", contour[i].first, ",",
                      contour[i].second, " ");
    }
    absl::StrAppend(&d, "Z ");
  }
  if (!d.empty()) d.pop_back();
  return d;
}

}  // namespace gui
}  // namespace mozc
