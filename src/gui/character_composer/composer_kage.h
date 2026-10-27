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

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "composer_ids/ids_tree.h"
#include "kage/kage_engine.h"

namespace mozc {
namespace gui {

// How GlyphWiki places each component in each position of each IDS operator,
// learned from its own glyphs (data/kage/kage_layout.tsv). A component the
// table does not know is placed by the plain defaults.
class ComposerLayout {
 public:
  struct Entry {
    std::string variant;  // the glyph to draw: u6728-01 is 木 on the left
    int left = 0, top = 0, right = 200, bottom = 200;
  };

  // Parses the table: "operator<TAB>position<TAB>component<TAB>variant<TAB>
  // l t r b<TAB>count" per line; '#' lines are comments.
  static ComposerLayout Parse(std::string_view contents);

  // The entry for |component| at |position| (0-based) of |op|, or nullptr.
  const Entry *Find(std::string_view op, int position,
                    std::string_view component) const;
  size_t size() const { return entries_.size(); }

  // The box GlyphWiki most often gives whatever stands at |position| of |op|
  // (the count-weighted median over the table), for a component the table does
  // not know; nullptr if |op| has no such position in it.
  const Entry *DefaultFor(std::string_view op, int position) const;

 private:
  std::map<std::string, Entry> entries_;
  std::map<std::string, Entry> defaults_;
};

// What the composer needs to draw a glyph: GlyphWiki's strokes and its layouts.
struct KageResources {
  kage::Buhin glyphs;
  ComposerLayout layout;

  // Reads kage_glyphs.tsv (required) and kage_layout.tsv (optional) from
  // |directory|; nullopt if there are no glyphs.
  static std::optional<KageResources> LoadFromDirectory(std::string_view directory);
};

// Closed outlines in a square of side |view_box| user units, y down. Fill them
// with the non-zero rule.
using Contour = std::vector<std::pair<double, double>>;

// KAGE data placing the parts of |op| in their boxes (the 0..200 KAGE space).
// A leaf is the glyph "u<hex>" of its character; a nested layout is composed
// first and registered in |nested| under a made-up name. Empty if the layout is
// not complete.
//
// With |layout|, a component is drawn in the variant and box GlyphWiki uses for
// it in that position when the table knows one (and |nested|, which sits on the
// glyph table, holds the variant); otherwise by the defaults.
std::string ComposeKageData(std::string_view op,
                            const std::vector<composer_ids::ComposerSlot> &parts,
                            kage::Buhin *nested,
                            const ComposerLayout *layout = nullptr);

// The outlines of the composed glyph, or nullopt when the layout is incomplete
// or a component has no KAGE data (then the caller falls back to plain text).
std::optional<std::vector<Contour>> ComposerOutlines(
    std::string_view op, const std::vector<composer_ids::ComposerSlot> &parts,
    const KageResources &resources, int view_box = 1000);

// "M x,y L x,y ... Z" for the outlines.
std::string OutlinesToPathData(const std::vector<Contour> &outlines);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_COMPOSER_KAGE_H_
