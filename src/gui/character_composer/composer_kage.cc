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

#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "base/file_util.h"
#include "kage/kage_data.h"
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
                          kage::Buhin *nested, const ComposerLayout *layout) {
  const std::vector<Box> boxes = BoxesFor(op);
  std::vector<std::string> records;
  for (size_t i = 0; i < parts.size(); ++i) {
    Box box = boxes[std::min(i, boxes.size() - 1)];
    std::string name;
    if (parts[i].kind == ComposerSlot::Kind::kLeaf) {
      name = GlyphName(parts[i].input);
      if (layout != nullptr) {
        const ComposerLayout::Entry *e =
            layout->Find(op, static_cast<int>(i), parts[i].input);
        if (e != nullptr && !nested->Search(e->variant).empty()) {
          name = e->variant;
          box = {e->left, e->top, e->right, e->bottom};
        } else if (const ComposerLayout::Entry *d =
                       e == nullptr ? layout->DefaultFor(op, static_cast<int>(i))
                                    : nullptr) {
          // Not in the table: GlyphWiki's usual box for this position, with
          // the component's own glyph.
          box = {d->left, d->top, d->right, d->bottom};
        }
      }
    } else {
      const std::string inner =
          ComposeLayout(parts[i].op, parts[i].parts, nested, layout);
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

ComposerLayout ComposerLayout::Parse(std::string_view contents) {
  ComposerLayout layout;
  // (box, count) of every entry, by operator and position.
  std::map<std::string, std::vector<std::pair<Box, int>>> by_position;
  for (std::string_view line : absl::StrSplit(contents, '\n')) {
    if (line.empty() || line.front() == '#') continue;
    const std::vector<std::string_view> f = absl::StrSplit(line, '\t');
    if (f.size() < 5) continue;
    int position = 0;
    const std::vector<std::string_view> box = absl::StrSplit(f[4], ' ');
    Entry e;
    if (!absl::SimpleAtoi(f[1], &position) || box.size() != 4 ||
        !absl::SimpleAtoi(box[0], &e.left) || !absl::SimpleAtoi(box[1], &e.top) ||
        !absl::SimpleAtoi(box[2], &e.right) || !absl::SimpleAtoi(box[3], &e.bottom)) {
      continue;
    }
    e.variant = std::string(f[3]);
    int count = 1;
    if (f.size() > 5 && !absl::SimpleAtoi(f[5], &count)) count = 1;
    by_position[absl::StrCat(f[0], position)].push_back(
        {{e.left, e.top, e.right, e.bottom}, std::max(count, 1)});
    layout.entries_[absl::StrCat(f[0], position, f[2])] = std::move(e);
  }
  // A few very common components must not decide it alone: cap each at 20.
  for (const auto &[key, boxes] : by_position) {
    int edge[4];
    for (int k = 0; k < 4; ++k) {
      std::vector<std::pair<int, int>> values;  // (coordinate, weight)
      long total = 0;
      for (const auto &[box, count] : boxes) {
        const int coordinate = k == 0 ? box.left : k == 1 ? box.top
                               : k == 2 ? box.right : box.bottom;
        const int weight = std::min(count, 20);
        values.push_back({coordinate, weight});
        total += weight;
      }
      std::sort(values.begin(), values.end());
      long seen = 0;
      edge[k] = values.back().first;
      for (const auto &[coordinate, weight] : values) {
        seen += weight;
        if (2 * seen >= total) {
          edge[k] = coordinate;
          break;
        }
      }
    }
    Entry d;
    d.left = edge[0], d.top = edge[1], d.right = edge[2], d.bottom = edge[3];
    layout.defaults_[key] = std::move(d);
  }
  return layout;
}

const ComposerLayout::Entry *ComposerLayout::Find(std::string_view op, int position,
                                                  std::string_view component) const {
  const auto it = entries_.find(absl::StrCat(op, position, component));
  return it == entries_.end() ? nullptr : &it->second;
}

const ComposerLayout::Entry *ComposerLayout::DefaultFor(std::string_view op,
                                                        int position) const {
  const auto it = defaults_.find(absl::StrCat(op, position));
  return it == defaults_.end() ? nullptr : &it->second;
}

std::optional<KageResources> KageResources::LoadFromDirectory(
    std::string_view directory) {
  absl::StatusOr<kage::Buhin> glyphs = kage::LoadBuhinFromFile(
      FileUtil::JoinPath(directory, "kage_glyphs.tsv"));
  if (!glyphs.ok() || glyphs->size() == 0) return std::nullopt;
  KageResources resources{*std::move(glyphs), {}};
  if (absl::StatusOr<std::string> layout = FileUtil::GetContents(
          FileUtil::JoinPath(directory, "kage_layout.tsv"));
      layout.ok()) {
    resources.layout = ComposerLayout::Parse(*layout);
  }
  return resources;
}

std::string ComposeKageData(std::string_view op,
                            const std::vector<ComposerSlot> &parts,
                            kage::Buhin *nested, const ComposerLayout *layout) {
  if (!composer_ids::ComposeIds(op, parts)) return {};
  return ComposeLayout(op, parts, nested, layout);
}

std::optional<std::vector<Contour>> ComposerOutlines(
    std::string_view op, const std::vector<ComposerSlot> &parts,
    const KageResources &resources, int view_box) {
  // Nested layouts are glyphs of their own; keep them in a table on top of the
  // shared, read-only one.
  kage::Buhin scratch(&resources.glyphs);
  const std::string data = ComposeKageData(op, parts, &scratch, &resources.layout);
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
