// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/composer_kage.h"

#include <algorithm>
#include <memory>
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

// Where each part of a layout goes in the 0..200 KAGE space when the layout
// table has no entry. Values follow GlyphWiki's count-weighted medians from
// kage_layout.tsv (⿰ left ~2..167, right ~49..198; ⿱ top ~0..130, bottom
// ~50..195): deep overlap, not clean halves. Clean halves leave a gap because
// the engine does not stretch a component to fill its box, and they fight
// positional forms (-01/-02/-03/-04) that already encode narrowing.
std::vector<Box> BoxesFor(std::string_view op) {
  if (op == "⿰") return {{2, 0, 167, 200}, {49, 0, 198, 200}};
  if (op == "⿱") return {{0, 4, 200, 130}, {0, 50, 200, 195}};
  // ⿲/⿳: GlyphWiki medians overlap deeply (not clean thirds).
  if (op == "⿲") return {{3, 0, 133, 200}, {48, 0, 169, 200}, {74, 0, 195, 200}};
  if (op == "⿳") return {{0, 3, 200, 137}, {0, 48, 200, 130}, {0, 73, 200, 193}};
  // Enclosures: frame fills the square; inner box from GlyphWiki medians
  // (slightly roomier than the old Grognard defaults).
  if (op == "⿴") return {kFull, {30, 25, 170, 173}};
  if (op == "⿵") return {kFull, {35, 68, 165, 185}};
  if (op == "⿶") return {kFull, {32, 4, 172, 167}};
  if (op == "⿷") return {kFull, {30, 24, 185, 174}};
  if (op == "⿸") return {kFull, {35, 35, 196, 197}};
  if (op == "⿹") return {kFull, {7, 62, 154, 192}};
  if (op == "⿺") return {kFull, {55, 2, 195, 175}};
  // ⿻: both parts share the square; the second is often a mid band.
  if (op == "⿻") return {kFull, {0, 48, 200, 170}};
  return {{0, 0, 100, 200}, {100, 0, 200, 200}};
}

// True when |name| is a GlyphWiki positional form (u6728-01, u5fc3-04, …).
bool IsPositionalForm(std::string_view name) {
  const size_t dash = name.rfind('-');
  if (dash == std::string_view::npos || dash + 3 != name.size()) return false;
  const std::string_view suf = name.substr(dash + 1);
  return suf == "01" || suf == "02" || suf == "03" || suf == "04" || suf == "05";
}

// Positional forms already encode narrowing/flattening in their strokes.
// GlyphWiki then places them in wide boxes (often past 0..200). Learned median
// boxes understate that: for ⿰, the right component's left edge sits ~20 units
// too far right versus GlyphWiki's own ⿰亻X glyphs. Widen the placement so we
// do not double-squeeze.
void ExpandBoxForPositionalForm(std::string_view op, int position,
                                std::string_view name, Box *box) {
  if (box == nullptr || !IsPositionalForm(name)) return;
  if (op == "⿰") {
    if (position == 0) {
      box->right = std::max(box->right, 170);
    } else if (position == 1 && box->left > 0) {
      // Learned medians for -02 sit ~20 units too far right versus GlyphWiki
      // (measured on ⿰亻X). Skip left==0: that box is already full-bleed and
      // pulling it negative hurts characters whose registered glyph uses a
      // different variant (e.g. 相's 目).
      box->left -= 20;
    }
  } else if (op == "⿱") {
    if (position == 0) {
      box->bottom = std::max(box->bottom, 170);
    } else if (position == 1 && box->top > 0) {
      box->top = std::min(box->top, 30);
      box->top -= 15;
    }
  } else if (op == "⿲") {
    if (position == 0) {
      box->right = std::max(box->right, 130);
    } else if (position == 1) {
      box->left = std::min(box->left, 50);
      box->right = std::max(box->right, 165);
    } else if (position == 2 && box->left > 0) {
      box->left -= 15;
    }
  } else if (op == "⿳") {
    if (position == 0) {
      box->bottom = std::max(box->bottom, 130);
    } else if (position == 1) {
      box->top = std::min(box->top, 50);
      box->bottom = std::max(box->bottom, 125);
    } else if (position == 2 && box->top > 0) {
      box->top -= 15;
    }
  } else if ((op == "⿴" || op == "⿵" || op == "⿶" || op == "⿷" ||
              op == "⿸" || op == "⿹" || op == "⿺") &&
             position == 0) {
    // -05 frame forms are drawn full-square in GlyphWiki.
    box->left = std::min(box->left, 0);
    box->top = std::min(box->top, 0);
    box->right = std::max(box->right, 200);
    box->bottom = std::max(box->bottom, 200);
  }
}

// GlyphWiki's name for a character: "u" and its code point in lower-case hex.
std::string GlyphName(std::string_view ch) {
  const std::u32string cps = strings::Utf8ToUtf32(ch);
  if (cps.size() != 1) return {};
  char buf[16];
  std::snprintf(buf, sizeof(buf), "u%04x", static_cast<unsigned>(cps[0]));
  return buf;
}

std::optional<std::vector<Contour>> OutlinesFromData(const kage::Kage &engine,
                                                     std::string_view data,
                                                     int view_box) {
  if (data.empty() || !engine.UnresolvedComponents(data).empty()) {
    return std::nullopt;
  }
  kage::Polygons polygons;
  engine.MakeGlyphFromData(&polygons, data);
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

// Bare "uXXXX" from a GlyphWiki name that may already carry a suffix.
std::string CanonicalGlyphBase(std::string_view name) {
  if (name.size() < 5 || name[0] != 'u') return {};
  size_t i = 1;
  while (i < name.size() &&
         ((name[i] >= '0' && name[i] <= '9') ||
          (name[i] >= 'a' && name[i] <= 'f'))) {
    ++i;
  }
  if (i < 5) return {};
  return std::string(name.substr(0, i));
}

// Preferred GlyphWiki *positional* suffixes for a part of an IDS operator
// (from kage_layout.tsv: ⿰ left -01, right -02, ⿱ top -03, bottom -04,
// enclosing frames often -05). Region tags like -j are not listed: bare
// uXXXX already points at them, and substituting -j only renames the glyph.
std::vector<std::string_view> PreferredSuffixes(std::string_view op, int position) {
  if (op == "⿰") {
    if (position == 0) return {"01"};
    return {"02"};
  }
  if (op == "⿱") {
    if (position == 0) return {"03"};
    return {"04"};
  }
  if (op == "⿲") {
    if (position <= 1) return {"01"};
    return {"02"};
  }
  if (op == "⿳") {
    if (position <= 1) return {"03"};
    return {"04"};
  }
  if (op == "⿴" || op == "⿵" || op == "⿶" || op == "⿷" || op == "⿸" ||
      op == "⿹" || op == "⿺") {
    if (position == 0) return {"05"};
    return {};
  }
  return {};
}

// Pick a positional form when the dump has one (u6728 -> u6728-01 on the left
// of ⿰). Only upgrades a *bare* uXXXX: if the layout table already chose
// u76ee-j or u6728-02, leave that choice alone.
std::string PreferPositionalVariant(std::string_view op, int position,
                                    std::string name, const kage::Buhin &glyphs) {
  const std::string base = CanonicalGlyphBase(name);
  if (base.empty() || name != base) return name;
  for (std::string_view suffix : PreferredSuffixes(op, position)) {
    const std::string candidate = absl::StrCat(base, "-", suffix);
    if (!glyphs.Search(candidate).empty()) return candidate;
  }
  return name;
}

bool HasGlyph(const kage::Buhin &glyphs, std::string_view name) {
  return !glyphs.Search(name).empty();
}

// The other leaf of a flat two-part layout, else empty (no context).
std::string SiblingLeaf(const std::vector<ComposerSlot> &parts, size_t index) {
  if (parts.size() != 2) return {};
  const size_t other = 1 - index;
  if (parts[other].kind != ComposerSlot::Kind::kLeaf) return {};
  return parts[other].input;
}

// After both sides of ⿰ are placed, pull the right box left so its overhang
// matches how narrow the left is (GlyphWiki: 亻 → right.left≈2; wider lefts
// keep a gentler overlap).
void HarmonizeHorizontalOverlap(Box *left, Box *right) {
  if (left == nullptr || right == nullptr) return;
  // Desired overlap grows as the left stays narrower.
  const int overlap = left->right <= 155 ? 55 : left->right <= 170 ? 40 : 25;
  const int target = left->right - overlap;
  if (right->left > target) right->left = target;
}

// Same idea for ⿱ / ⿳: pull the lower part up so bands overlap as GlyphWiki
// does (clean stacked strips leave a gap; medians overlap by ~40–80).
void HarmonizeVerticalOverlap(Box *upper, Box *lower) {
  if (upper == nullptr || lower == nullptr) return;
  const int overlap = upper->bottom <= 120 ? 50 : upper->bottom <= 150 ? 40 : 25;
  const int target = upper->bottom - overlap;
  if (lower->top > target) lower->top = target;
}

// GlyphWiki's compound name for a filled slot: ⿱口𫶧 → u2ff1-u53e3-u2bda7.
std::string SystematicGlyphName(const ComposerSlot &slot) {
  if (slot.kind == ComposerSlot::Kind::kLeaf) {
    return GlyphName(slot.input);
  }
  std::string name = GlyphName(slot.op);
  if (name.empty()) return {};
  for (const ComposerSlot &part : slot.parts) {
    const std::string piece = SystematicGlyphName(part);
    if (piece.empty()) return {};
    absl::StrAppend(&name, "-", piece);
  }
  return name;
}

// A ready-made GlyphWiki glyph for a nested slot, else empty (synthesize).
std::string ReadyMadeNestedGlyph(const ComposerSlot &slot, const kage::Buhin &glyphs,
                                 const ComposerSubtrees *subtrees) {
  if (const auto ids = composer_ids::ComposeIds(slot.op, slot.parts)) {
    if (subtrees != nullptr) {
      if (const std::string *known = subtrees->Find(*ids);
          known != nullptr && HasGlyph(glyphs, *known)) {
        return *known;
      }
    }
  }
  const std::string systematic = SystematicGlyphName(slot);
  if (!systematic.empty() && HasGlyph(glyphs, systematic)) return systematic;
  return {};
}

std::string ComposeLayout(std::string_view op, const std::vector<ComposerSlot> &parts,
                          kage::Buhin *nested, const ComposerLayout *layout,
                          const ComposerSubtrees *subtrees) {
  const std::vector<Box> boxes = BoxesFor(op);
  struct Placement {
    std::string name;
    Box box;
  };
  std::vector<Placement> placements;
  placements.reserve(parts.size());
  for (size_t i = 0; i < parts.size(); ++i) {
    Box box = boxes[std::min(i, boxes.size() - 1)];
    std::string name;
    if (parts[i].kind == ComposerSlot::Kind::kLeaf) {
      name = GlyphName(parts[i].input);
      if (layout != nullptr) {
        const std::string sibling = SiblingLeaf(parts, i);
        const ComposerLayout::Entry *e = layout->Find(
            op, static_cast<int>(i), parts[i].input, sibling);
        if (e != nullptr && HasGlyph(*nested, e->variant)) {
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
      // Bare uXXXX only: prefer a positional form if the dump has one.
      name = PreferPositionalVariant(op, static_cast<int>(i), std::move(name),
                                     *nested);
      ExpandBoxForPositionalForm(op, static_cast<int>(i), name, &box);
    } else {
      name = ReadyMadeNestedGlyph(parts[i], *nested, subtrees);
      if (name.empty()) {
        const std::string inner = ComposeLayout(parts[i].op, parts[i].parts,
                                                nested, layout, subtrees);
        if (inner.empty()) return {};
        name = absl::StrCat("__nested", nested->size());
        nested->Set(name, inner);
      }
      // Nested slots have no leaf character in the layout table. Keep the
      // operator default box (BoxesFor); DefaultFor's median can be a near-
      // full frame that then fights HarmonizeHorizontalOverlap.
    }
    if (name.empty()) return {};
    placements.push_back({std::move(name), box});
  }
  if (op == "⿰" && placements.size() >= 2) {
    HarmonizeHorizontalOverlap(&placements[0].box, &placements[1].box);
  } else if (op == "⿱" && placements.size() >= 2) {
    HarmonizeVerticalOverlap(&placements[0].box, &placements[1].box);
  } else if (op == "⿲" && placements.size() >= 3) {
    HarmonizeHorizontalOverlap(&placements[0].box, &placements[1].box);
    HarmonizeHorizontalOverlap(&placements[1].box, &placements[2].box);
  } else if (op == "⿳" && placements.size() >= 3) {
    HarmonizeVerticalOverlap(&placements[0].box, &placements[1].box);
    HarmonizeVerticalOverlap(&placements[1].box, &placements[2].box);
  }
  std::vector<std::string> records;
  records.reserve(placements.size());
  for (const Placement &p : placements) {
    records.push_back(absl::StrCat("99:0:0:", p.box.left, ":", p.box.top, ":",
                                   p.box.right, ":", p.box.bottom, ":", p.name,
                                   ":0:0:0"));
  }
  return absl::StrJoin(records, "$");
}

}  // namespace

ComposerLayout ComposerLayout::Parse(std::string_view contents) {
  ComposerLayout layout;
  // (box, count) of every entry, by operator and position.
  // Context rows (with a sibling) do not feed the position defaults: those
  // stay the global median.
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
    const std::string_view sibling = f.size() > 6 ? f[6] : "";
    if (sibling.empty()) {
      by_position[absl::StrCat(f[0], position)].push_back(
          {{e.left, e.top, e.right, e.bottom}, std::max(count, 1)});
      layout.entries_[absl::StrCat(f[0], position, f[2])] = std::move(e);
    } else {
      layout.entries_[absl::StrCat(f[0], position, f[2], "\t", sibling)] =
          std::move(e);
    }
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
                                                  std::string_view component,
                                                  std::string_view sibling) const {
  if (!sibling.empty()) {
    const auto ctx =
        entries_.find(absl::StrCat(op, position, component, "\t", sibling));
    if (ctx != entries_.end()) return &ctx->second;
  }
  const auto it = entries_.find(absl::StrCat(op, position, component));
  return it == entries_.end() ? nullptr : &it->second;
}

const ComposerLayout::Entry *ComposerLayout::DefaultFor(std::string_view op,
                                                        int position) const {
  const auto it = defaults_.find(absl::StrCat(op, position));
  return it == defaults_.end() ? nullptr : &it->second;
}

ComposerSubtrees ComposerSubtrees::Parse(std::string_view contents) {
  ComposerSubtrees table;
  for (std::string_view line : absl::StrSplit(contents, '\n')) {
    if (line.empty() || line.front() == '#') continue;
    const std::vector<std::string_view> f = absl::StrSplit(line, '\t');
    if (f.size() < 2 || f[0].empty() || f[1].empty()) continue;
    table.entries_.emplace(std::string(f[0]), std::string(f[1]));
  }
  return table;
}

const std::string *ComposerSubtrees::Find(std::string_view ids) const {
  const auto it = entries_.find(std::string(ids));
  return it == entries_.end() ? nullptr : &it->second;
}

std::unique_ptr<KageResources> KageResources::LoadFromDirectory(
    std::string_view directory) {
  absl::StatusOr<kage::Buhin> glyphs = kage::LoadBuhinFromFile(
      FileUtil::JoinPath(directory, "kage_glyphs.tsv"));
  if (!glyphs.ok() || glyphs->size() == 0) return nullptr;
  auto resources = std::make_unique<KageResources>();
  resources->glyphs = *std::move(glyphs);
  if (absl::StatusOr<std::string> layout = FileUtil::GetContents(
          FileUtil::JoinPath(directory, "kage_layout.tsv"));
      layout.ok()) {
    resources->layout = ComposerLayout::Parse(*layout);
  }
  if (absl::StatusOr<std::string> subtrees = FileUtil::GetContents(
          FileUtil::JoinPath(directory, "kage_subtrees.tsv"));
      subtrees.ok()) {
    resources->subtrees = ComposerSubtrees::Parse(*subtrees);
  }
  return resources;
}

std::string ComposeKageData(std::string_view op,
                            const std::vector<ComposerSlot> &parts,
                            kage::Buhin *nested, const ComposerLayout *layout,
                            const ComposerSubtrees *subtrees) {
  if (!composer_ids::ComposeIds(op, parts)) return {};
  return ComposeLayout(op, parts, nested, layout, subtrees);
}

std::optional<std::vector<Contour>> ComposerOutlines(
    std::string_view op, const std::vector<ComposerSlot> &parts,
    const KageResources &resources, int view_box) {
  // Nested layouts are glyphs of their own; keep them in a table on top of the
  // shared, read-only one.
  kage::Buhin scratch(&resources.glyphs);
  const std::string data = ComposeKageData(op, parts, &scratch, &resources.layout,
                                           &resources.subtrees);
  if (data.empty()) return std::nullopt;
  return OutlinesFromData(kage::Kage(&scratch), data, view_box);
}

std::string GlyphWikiNameForCharacter(std::string_view ch) {
  return GlyphName(ch);
}

std::optional<std::vector<Contour>> NamedGlyphOutlinesFromBuhin(
    std::string_view name, const kage::Buhin &buhin, int view_box) {
  if (name.empty()) return std::nullopt;
  const kage::Kage engine(&buhin);
  const std::string_view data = buhin.Search(name);
  if (data.empty()) return std::nullopt;
  return OutlinesFromData(engine, data, view_box);
}

std::optional<std::vector<Contour>> NamedGlyphOutlines(
    std::string_view ch, const KageResources &resources, int view_box) {
  return NamedGlyphOutlinesFromBuhin(GlyphName(ch), resources.glyphs, view_box);
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
