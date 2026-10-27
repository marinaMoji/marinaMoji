// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Draw a filled IDS layout as one new glyph, from GlyphWiki's KAGE strokes: the
// components are placed in their cells (as the Grognard editor's composer does)
// and the KAGE engine turns their strokes into a single outline, so the result
// is a character and not a row of small ones.
//
// This file is BSD like the rest of the composer, but it calls //kage, which is
// GPL-3.0-or-later; whatever links this library is therefore GPL-3.0.

#ifndef MOZC_GUI_CHARACTER_COMPOSER_COMPOSER_KAGE_H_
#define MOZC_GUI_CHARACTER_COMPOSER_COMPOSER_KAGE_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "composer_ids/ids_tree.h"
#include "kage/kage_engine.h"

namespace mozc {
namespace gui {

// Closed outlines in a square of side |view_box| user units, y down. Fill them
// with the non-zero rule.
using Contour = std::vector<std::pair<double, double>>;

// KAGE data placing the parts of |op| in their boxes (the 0..200 KAGE space).
// A leaf is the glyph "u<hex>" of its character; a nested layout is composed
// first and registered in |nested| under a made-up name. Empty if the layout is
// not complete.
std::string ComposeKageData(std::string_view op,
                            const std::vector<composer_ids::ComposerSlot> &parts,
                            kage::Buhin *nested);

// The outlines of the composed glyph, or nullopt when the layout is incomplete
// or a component has no KAGE data (then the caller falls back to plain text).
std::optional<std::vector<Contour>> ComposerOutlines(
    std::string_view op, const std::vector<composer_ids::ComposerSlot> &parts,
    const kage::Buhin &glyphs, int view_box = 1000);

// "M x,y L x,y ... Z" for the outlines.
std::string OutlinesToPathData(const std::vector<Contour> &outlines);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_COMPOSER_KAGE_H_
