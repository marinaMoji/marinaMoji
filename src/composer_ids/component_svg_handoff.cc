// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/component_svg_handoff.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/types/span.h"
#include "base/strings/unicode.h"
#include "composer_ids/composer_result.h"
#include "composer_ids/ids_tree.h"

namespace mozc {
namespace composer_ids {
namespace {

struct FRect {
  double x = 0, y = 0, w = 0, h = 0;
};

// Same cell fractions as gui/character_composer/layout_geometry for the four
// default Ctrl+2 operators (kept local so the IME server need not link GUI).
std::vector<FRect> CellsFor(std::string_view op, int view_box) {
  const double s = static_cast<double>(view_box);
  if (op == "⿱") {
    return {{0, 0, s, s * 0.5}, {0, s * 0.5, s, s * 0.5}};
  }
  if (op == "⿲") {
    return {{0, 0, s / 3.0, s}, {s / 3.0, 0, s / 3.0, s}, {2.0 * s / 3.0, 0, s / 3.0, s}};
  }
  if (op == "⿳") {
    return {{0, 0, s, s / 3.0}, {0, s / 3.0, s, s / 3.0}, {0, 2.0 * s / 3.0, s, s / 3.0}};
  }
  // ⿰ (default)
  return {{0, 0, s * 0.5, s}, {s * 0.5, 0, s * 0.5, s}};
}

std::string EscapeXml(std::string_view text) {
  return absl::StrReplaceAll(text, {{"&", "&amp;"},
                                    {"<", "&lt;"},
                                    {">", "&gt;"},
                                    {"\"", "&quot;"},
                                    {"'", "&apos;"}});
}

std::vector<std::string> SplitChars(std::string_view text) {
  std::vector<std::string> chars;
  for (const std::string_view ch : Utf8AsChars(text)) {
    if (!ch.empty()) chars.emplace_back(ch);
  }
  return chars;
}

}  // namespace

std::string RenderComponentSelectionSvg(std::string_view op,
                                        absl::Span<const std::string> parts,
                                        int view_box) {
  if (parts.empty()) return {};
  std::string use_op(op);
  if (use_op.empty()) {
    use_op = parts.size() == 3 ? "⿲" : "⿰";
  }
  std::vector<ComposerSlot> slots;
  slots.reserve(parts.size());
  for (const std::string &p : parts) {
    slots.push_back(ComposerSlot::Leaf(p));
  }
  const auto ids = ComposeIds(use_op, slots);
  if (!ids) return {};
  if (view_box < 32) view_box = 32;

  const std::vector<FRect> cells = CellsFor(use_op, view_box);
  std::string body;
  const size_t n = std::min(parts.size(), cells.size());
  for (size_t i = 0; i < n; ++i) {
    const FRect &box = cells[i];
    const double side = std::max(box.w, box.h);
    const double x = box.x + box.w * 0.5;
    const double y = box.y + box.h * 0.78;
    const double font_size = side * 0.88;
    const double scale_x = std::min(1.0, box.w / side);
    const double scale_y = std::min(1.0, box.h / side);
    if (scale_x != 1.0 || scale_y != 1.0) {
      absl::StrAppend(&body, "<text transform=\"translate(", x, " ", y,
                      ") scale(", scale_x, " ", scale_y, ")\" x=\"0\" y=\"0\"");
    } else {
      absl::StrAppend(&body, "<text x=\"", x, "\" y=\"", y, "\"");
    }
    absl::StrAppend(&body, " font-size=\"", font_size,
                    "\" text-anchor=\"middle\" "
                    "font-family=\"Hiragino Mincho ProN, Yu Mincho, Songti SC, "
                    "Noto Serif CJK JP, serif\">",
                    EscapeXml(parts[i]), "</text>");
  }

  return absl::StrCat(
      "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"", view_box,
      "\" height=\"", view_box, "\" viewBox=\"0 0 ", view_box, " ", view_box,
      "\" role=\"img\" aria-label=\"", EscapeXml(*ids), "\">",
      "<title>", EscapeXml(*ids), "</title>",
      "<rect width=\"100%\" height=\"100%\" fill=\"none\"/>", body, "</svg>");
}

absl::Status HandOffComponentSvg(std::string_view op,
                                 absl::Span<const std::string> parts) {
  const std::string svg = RenderComponentSelectionSvg(op, parts);
  if (svg.empty()) {
    return absl::InvalidArgumentError("cannot render component SVG");
  }
  if (absl::Status status =
          WriteComposerSvgResult(DefaultSvgResultFilePath(), svg);
      !status.ok()) {
    return status;
  }
  const int64_t now_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  if (absl::Status status =
          WriteOnlyOfficeHandoff(OnlyOfficePluginDirectory(), svg, now_ms);
      !status.ok()) {
    return status;
  }
  return absl::OkStatus();
}

absl::Status HandOffComponentSvgFromSelection(std::string_view op,
                                              std::string_view selected) {
  const std::vector<std::string> parts = SplitChars(selected);
  return HandOffComponentSvg(op, parts);
}

}  // namespace composer_ids
}  // namespace mozc
