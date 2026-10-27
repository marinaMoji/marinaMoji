// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Draw a filled IDS layout as an SVG (for unencoded structures the composer
// cannot insert as Unicode). Pure string building; no Qt.

#ifndef MOZC_GUI_CHARACTER_COMPOSER_COMPOSER_SVG_H_
#define MOZC_GUI_CHARACTER_COMPOSER_COMPOSER_SVG_H_

#include <string>
#include <string_view>
#include <vector>

#include "composer_ids/ids_tree.h"

namespace mozc {
namespace gui {

// One glyph of a laid-out structure, in user units of a |view_box| square.
// |y| is the baseline; the text is centred horizontally on |x|.
struct PlacedGlyph {
  std::string text;
  double x = 0;
  double y = 0;
  double font_size = 0;
};

// Where each filled leaf is drawn, or empty if the layout is incomplete. The
// SVG below and the dialog's preview are both drawn from this.
std::vector<PlacedGlyph> ComposerGlyphs(
    std::string_view op, const std::vector<composer_ids::ComposerSlot> &parts,
    int view_box = 1000);

// SVG for a complete layout (every leaf filled). |view_box| is the square size
// in user units (default 1000). Empty string if the layout is incomplete.
std::string RenderComposerSvg(std::string_view op,
                              const std::vector<composer_ids::ComposerSlot> &parts,
                              int view_box = 1000);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_COMPOSER_SVG_H_
