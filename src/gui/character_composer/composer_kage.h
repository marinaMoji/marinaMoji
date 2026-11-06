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
#include <memory>
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
  // l t r b<TAB>count[<TAB>sibling]" per line; '#' lines are comments. An
  // optional sibling is the other leaf of a two-part IDS (so 目 next to 木 can
  // differ from 目 next to 金).
  static ComposerLayout Parse(std::string_view contents);

  // The entry for |component| at |position| (0-based) of |op|. When |sibling|
  // is non-empty, a context-specific row wins over the global one.
  const Entry *Find(std::string_view op, int position,
                    std::string_view component,
                    std::string_view sibling = "") const;
  size_t size() const { return entries_.size(); }

  // The box GlyphWiki most often gives whatever stands at |position| of |op|
  // (the count-weighted median over the table), for a component the table does
  // not know; nullptr if |op| has no such position in it.
  const Entry *DefaultFor(std::string_view op, int position) const;

 private:
  std::map<std::string, Entry> entries_;
  std::map<std::string, Entry> defaults_;
};

// Nested IDS fragments → a ready-made GlyphWiki glyph (data/kage/kage_subtrees.tsv).
// So ⿰亻⿱口𫶧 can place GlyphWiki's u2ff1-u53e3-… for the right slot instead of
// synthesizing a nested layout.
class ComposerSubtrees {
 public:
  // Parses "IDS<TAB>glyph name<TAB>count" lines; '#' lines are comments.
  static ComposerSubtrees Parse(std::string_view contents);

  // The GlyphWiki name for |ids|, or nullptr if unknown.
  const std::string *Find(std::string_view ids) const;
  size_t size() const { return entries_.size(); }

 private:
  std::map<std::string, std::string> entries_;
};

// What the composer needs to draw a glyph: GlyphWiki's strokes and its layouts.
struct KageResources {
  kage::Buhin glyphs;
  ComposerLayout layout;
  ComposerSubtrees subtrees;

  // Reads kage_glyphs.tsv (required) and kage_layout.tsv / kage_subtrees.tsv
  // (optional) from |directory|; nullptr if there are no glyphs.
  static std::unique_ptr<KageResources> LoadFromDirectory(
      std::string_view directory);
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
// glyph table, holds the variant); otherwise by the defaults. With |subtrees|, a
// nested slot whose IDS is known is drawn as that GlyphWiki glyph instead of a
// synthesized __nestedN.
//
// With |limit_ink_overlap| (the default), neighbouring components whose
// strokes would overlap by more than ~10% of the square are pulled apart:
// GlyphWiki's usual box overlap suits its narrowed positional forms but makes
// a full-size component (龍) collide with its neighbour.
std::string ComposeKageData(std::string_view op,
                            const std::vector<composer_ids::ComposerSlot> &parts,
                            kage::Buhin *nested,
                            const ComposerLayout *layout = nullptr,
                            const ComposerSubtrees *subtrees = nullptr,
                            bool limit_ink_overlap = true);

// The outlines of the composed glyph, or nullopt when the layout is incomplete
// or a component has no KAGE data (then the caller falls back to plain text).
std::optional<std::vector<Contour>> ComposerOutlines(
    std::string_view op, const std::vector<composer_ids::ComposerSlot> &parts,
    const KageResources &resources, int view_box = 1000);

// GlyphWiki's compound ("systematic") name for a structure: ⿰木馬 is
// u2ff0-u6728-u99ac, and nested structures nest the same way
// (u2ff1-u53e3-u2bda7). Empty if |op| or any character has no name. GlyphWiki
// registers many unencoded structures under these names, so with
// prefer_glyphwiki_drawings on they are looked up there instead of composed
// locally. Most combinations are not registered.
std::string GlyphWikiSystematicName(
    std::string_view op, const std::vector<composer_ids::ComposerSlot> &parts);

// GlyphWiki's name for a single character ("u4e00"), or empty if |ch| is not
// exactly one code point.
std::string GlyphWikiNameForCharacter(std::string_view ch);

// Outlines of GlyphWiki's registered glyph for |ch| (via uXXXX in |resources|),
// or nullopt when the character is unknown or has no strokes in the table.
// Used when prefer_glyphwiki_drawings is on.
std::optional<std::vector<Contour>> NamedGlyphOutlines(
    std::string_view ch, const KageResources &resources, int view_box = 1000);

// Same as NamedGlyphOutlines, but renders glyph |name| from |buhin| (which may
// sit on top of the bundled table with fetched/cached entries).
std::optional<std::vector<Contour>> NamedGlyphOutlinesFromBuhin(
    std::string_view name, const kage::Buhin &buhin, int view_box = 1000);

// "M x,y L x,y ... Z" for the outlines.
std::string OutlinesToPathData(const std::vector<Contour> &outlines);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_COMPOSER_KAGE_H_
