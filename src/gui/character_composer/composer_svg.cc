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

void AddLeaf(std::vector<PlacedGlyph> *out, std::string_view ch, const FRect &box) {
  if (ch.empty()) return;
  // Optical baseline for CJK in the cell (not the geometric centre).
  const double side = std::max(box.w, box.h);
  out->push_back({std::string(ch), box.x + box.w * 0.5, box.y + box.h * 0.78,
                  side * 0.88, std::min(1.0, box.w / side),
                  std::min(1.0, box.h / side)});
}

void AddSlot(std::vector<PlacedGlyph> *out, const ComposerSlot &slot, const FRect &box);

void AddLayout(std::vector<PlacedGlyph> *out, std::string_view op,
               const std::vector<ComposerSlot> &parts, const FRect &box) {
  const OperatorGeometry geo = GeometryFor(op);
  const int n = std::min(geo.count, static_cast<int>(parts.size()));
  for (int i = 0; i < n; ++i) {
    // Leaves use the edit strip (enclosures); nested layouts fill the cell.
    const FRect &frac =
        parts[i].kind == ComposerSlot::Kind::kNested ? geo.cell[i] : geo.edit[i];
    AddSlot(out, parts[i], ScaleRect(frac, box.x, box.y, box.w, box.h));
  }
}

void AddSlot(std::vector<PlacedGlyph> *out, const ComposerSlot &slot, const FRect &box) {
  if (slot.kind == ComposerSlot::Kind::kLeaf) {
    AddLeaf(out, slot.input, box);
    return;
  }
  AddLayout(out, slot.op, slot.parts, box);
}

}  // namespace

std::vector<PlacedGlyph> ComposerGlyphs(std::string_view op,
                                        const std::vector<ComposerSlot> &parts,
                                        int view_box) {
  std::vector<PlacedGlyph> glyphs;
  if (!composer_ids::ComposeIds(op, parts)) return glyphs;
  if (view_box < 32) view_box = 32;
  AddLayout(&glyphs, op, parts, {0, 0, static_cast<double>(view_box),
                                 static_cast<double>(view_box)});
  return glyphs;
}

std::string RenderComposerSvg(std::string_view op,
                              const std::vector<ComposerSlot> &parts,
                              int view_box) {
  if (!composer_ids::ComposeIds(op, parts)) return {};
  if (view_box < 32) view_box = 32;

  std::string body;
  for (const PlacedGlyph &g : ComposerGlyphs(op, parts, view_box)) {
    if (g.scale_x != 1 || g.scale_y != 1) {
      // Squeezed glyph: draw at the origin, then move and scale.
      absl::StrAppend(&body, "<text transform=\"translate(", g.x, " ", g.y,
                      ") scale(", g.scale_x, " ", g.scale_y, ")\" x=\"0\" y=\"0\"");
    } else {
      absl::StrAppend(&body, "<text x=\"", g.x, "\" y=\"", g.y, "\"");
    }
    absl::StrAppend(&body, " font-size=\"",
                    g.font_size,
                    "\" text-anchor=\"middle\" "
                    "font-family=\"Hiragino Mincho ProN, Yu Mincho, Songti SC, "
                    "Noto Serif CJK JP, serif\">",
                    EscapeXml(g.text), "</text>");
  }

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
