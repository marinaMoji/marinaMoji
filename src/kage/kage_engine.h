// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// C++ port of the KAGE engine (Kanji-glyph Automatic Generating Engine) as
// modified by kurgm, <https://github.com/kurgm/kage-engine> (itself derived
// from Kamichi Koichi's original, <https://github.com/kamichikoichi/kage-engine>),
// Mincho font only. Both are GPL-3.0; so is this port. See LICENSE and
// README.md in this directory.
//
// KAGE data is the stroke format of GlyphWiki: records joined by "$", each
// "type:a2:a3:x1:y1:x2:y2:x3:y3:x4:y4" in a 200x200 box, where type 99 places
// another glyph (a "buhin", component) into a rectangle.

#ifndef MOZC_KAGE_KAGE_ENGINE_H_
#define MOZC_KAGE_KAGE_ENGINE_H_

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "kage/kage_polygon.h"

namespace mozc {
namespace kage {

// Glyph name -> KAGE data. A table may sit on top of a read-only |parent| (not
// owned), so a few extra glyphs can be added without copying a large table.
class Buhin {
 public:
  Buhin() = default;
  explicit Buhin(const Buhin *parent) : parent_(parent) {}

  void Set(std::string name, std::string data) {
    map_[std::move(name)] = std::move(data);
  }
  // The data, or "" if |name| is unknown. A "@N" render-parameter suffix on
  // |name| is ignored (GlyphWiki's default form is used).
  std::string_view Search(std::string_view name) const;
  // Glyphs held by this table itself, not by its parent.
  size_t size() const { return map_.size(); }

 private:
  const Buhin *parent_ = nullptr;
  absl::flat_hash_map<std::string, std::string> map_;
};

class Kage {
 public:
  explicit Kage(const Buhin *buhin) : buhin_(buhin) {}

  // Renders glyph |name| (or raw KAGE |data|) on top of |polygons|.
  void MakeGlyph(Polygons *polygons, std::string_view name) const;
  void MakeGlyphFromData(Polygons *polygons, std::string_view data) const;

  // Names of components that |data| (and the glyphs it places, recursively)
  // refers to but that |buhin| does not hold.
  std::vector<std::string> UnresolvedComponents(std::string_view data) const;

 private:
  const Buhin *buhin_;
};

// Renders |polygons| as one SVG path "d" string in the 0..200 box: each
// contour is "M x,y L x,y ... Z". Contours are wound the same way, so the
// path fills correctly with the default non-zero rule.
std::string PolygonsToPathData(Polygons polygons);

}  // namespace kage
}  // namespace mozc

#endif  // MOZC_KAGE_KAGE_ENGINE_H_
