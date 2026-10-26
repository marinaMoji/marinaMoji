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

// SVG for a complete layout (every leaf filled). |view_box| is the square size
// in user units (default 1000). Empty string if the layout is incomplete.
std::string RenderComposerSvg(std::string_view op,
                              const std::vector<composer_ids::ComposerSlot> &parts,
                              int view_box = 1000);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_COMPOSER_SVG_H_
