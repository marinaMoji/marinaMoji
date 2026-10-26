// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/composer_svg.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "composer_ids/ids_tree.h"
#include "gui/character_composer/layout_geometry.h"

namespace mozc {
namespace gui {
namespace {

using composer_ids::ComposerSlot;

std::string EscapeXml(std::string_view text) {
  std::string out(text);
  out = absl::StrReplaceAll(out, {{"&", "&amp;"},
                                  {"<", "&lt;"},
                                  {">", "&gt;"},
                                  {"\"", "&quot;"},
                                  {"'", "&apos;"}});
  return out;
}

FRect ScaleRect(const FRect &frac, double x, double y, double w, double h) {
  return {x + frac.x * w, y + frac.y * h, frac.w * w, frac.h * h};
}

void AppendLeaf(std::string *out, std::string_view ch, const FRect &box) {
  if (ch.empty()) return;
  const double cx = box.x + box.w * 0.5;
  // Optical baseline for CJK in the cell (not the geometric centre).
  const double cy = box.y + box.h * 0.78;
  const double font = std::min(box.w, box.h) * 0.88;
  absl::StrAppend(out, "<text x=\"", cx, "\" y=\"", cy, "\" font-size=\"", font,
                  "\" text-anchor=\"middle\" "
                  "font-family=\"Hiragino Mincho ProN, Yu Mincho, Songti SC, "
                  "Noto Serif CJK JP, serif\">",
                  EscapeXml(ch), "</text>");
}

void AppendSlot(std::string *out, const ComposerSlot &slot, const FRect &box);

void AppendLayout(std::string *out, std::string_view op,
                  const std::vector<ComposerSlot> &parts, const FRect &box) {
  const OperatorGeometry geo = GeometryFor(op);
  const int n = std::min(geo.count, static_cast<int>(parts.size()));
  for (int i = 0; i < n; ++i) {
    // Leaves use the edit strip (enclosures); nested layouts fill the cell.
    const FRect &frac =
        parts[i].kind == ComposerSlot::Kind::kNested ? geo.cell[i] : geo.edit[i];
    AppendSlot(out, parts[i], ScaleRect(frac, box.x, box.y, box.w, box.h));
  }
}

void AppendSlot(std::string *out, const ComposerSlot &slot, const FRect &box) {
  if (slot.kind == ComposerSlot::Kind::kLeaf) {
    AppendLeaf(out, slot.input, box);
    return;
  }
  AppendLayout(out, slot.op, slot.parts, box);
}

}  // namespace

std::string RenderComposerSvg(std::string_view op,
                              const std::vector<ComposerSlot> &parts,
                              int view_box) {
  if (!composer_ids::ComposeIds(op, parts)) return {};
  if (view_box < 32) view_box = 32;

  std::string body;
  AppendLayout(&body, op, parts, {0, 0, static_cast<double>(view_box),
                                  static_cast<double>(view_box)});

  const std::string ids = *composer_ids::ComposeIds(op, parts);
  return absl::StrCat(
      "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"", view_box,
      "\" height=\"", view_box, "\" viewBox=\"0 0 ", view_box, " ", view_box,
      "\" role=\"img\" aria-label=\"", EscapeXml(ids), "\">",
      "<title>", EscapeXml(ids), "</title>",
      "<rect width=\"100%\" height=\"100%\" fill=\"none\"/>", body, "</svg>");
}

}  // namespace gui
}  // namespace mozc
