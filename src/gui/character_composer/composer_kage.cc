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
using composer_ids::SlotPath;

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

// Positional forms already encode narrowing/flattening in their strokes, so
// the learned box is used as it stands. Only the enclosing operators keep a
// rule: -05 frame forms are drawn full-square in GlyphWiki.
//
// Earlier versions also widened or shifted the boxes of ⿰ ⿱ ⿲ ⿳ positional
// forms by hand-picked amounts. Measured with glyphwiki_compare_main
// (--stride=15 --limit=0, layout-only rows) those rules lowered the mean IoU
// for every operator; see docs/GLYPHWIKI_COMPARISON.md.
constexpr int kNarrowLeftRight = 165;
constexpr int kNarrowLeftShift = 20;

void ExpandBoxForPositionalForm(std::string_view op, int position,
                                std::string_view name, const Box *left_box,
                                Box *box) {
  if (box == nullptr || !IsPositionalForm(name)) return;
  if (op == "⿰" && position == 1 && left_box != nullptr && box->left > 0 &&
      left_box->right <= kNarrowLeftRight) {
    // Next to a narrow left component (亻 and its kin) GlyphWiki sets the
    // right component further left than the learned median.
    box->left -= kNarrowLeftShift;
  }
  if ((op == "⿴" || op == "⿵" || op == "⿶" || op == "⿷" || op == "⿸" ||
       op == "⿹" || op == "⿺") &&
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

// A coarse bitmap of the ink a placed component draws (its strokes, not its
// box) over the 0..200 KAGE square; empty if it cannot be drawn.
constexpr int kInkGrid = 50;
using InkMask = std::vector<bool>;

InkMask InkMaskOf(const kage::Buhin &glyphs, const Box &box,
                  std::string_view name) {
  const std::string record = absl::StrCat("99:0:0:", box.left, ":", box.top,
                                          ":", box.right, ":", box.bottom, ":",
                                          name, ":0:0:0");
  const kage::Kage engine(&glyphs);
  if (!engine.UnresolvedComponents(record).empty()) return {};
  kage::Polygons polygons;
  engine.MakeGlyphFromData(&polygons, record);
  InkMask mask(kInkGrid * kInkGrid, false);
  bool any = false;
  for (const kage::Polygon &polygon : polygons.array()) {
    const auto &pts = polygon.Points();
    if (pts.size() < 3) continue;
    double min_y = pts[0].y, max_y = pts[0].y;
    for (const kage::Point &p : pts) {
      min_y = std::min<double>(min_y, p.y);
      max_y = std::max<double>(max_y, p.y);
    }
    const double cell = 200.0 / kInkGrid;
    for (int row = 0; row < kInkGrid; ++row) {
      const double y = (row + 0.5) * cell;
      if (y < min_y || y > max_y) continue;
      // Even-odd scanline: x where the polygon's edges cross this row.
      std::vector<double> xs;
      for (size_t i = 0; i < pts.size(); ++i) {
        const kage::Point &p = pts[i];
        const kage::Point &q = pts[(i + 1) % pts.size()];
        if ((p.y <= y) == (q.y <= y)) continue;
        xs.push_back(p.x + (y - p.y) / (q.y - p.y) * (q.x - p.x));
      }
      std::sort(xs.begin(), xs.end());
      for (size_t i = 0; i + 1 < xs.size(); i += 2) {
        const int from = std::max(0, static_cast<int>(xs[i] / cell));
        const int to = std::min(kInkGrid - 1, static_cast<int>(xs[i + 1] / cell));
        for (int col = from; col <= to; ++col) {
          mask[row * kInkGrid + col] = true;
          any = true;
        }
      }
    }
  }
  if (!any) return {};
  return mask;
}

// |mask| grown by one cell (4 units) in every direction, so that strokes that
// merely touch count as colliding: dense neighbours need a little air.
InkMask Dilated(const InkMask &mask) {
  InkMask out = mask;
  for (int row = 0; row < kInkGrid; ++row) {
    for (int col = 0; col < kInkGrid; ++col) {
      if (!mask[row * kInkGrid + col]) continue;
      for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
          const int r = row + dr, c = col + dc;
          if (r >= 0 && r < kInkGrid && c >= 0 && c < kInkGrid) {
            out[r * kInkGrid + c] = true;
          }
        }
      }
    }
  }
  return out;
}

// Fraction of the smaller component's ink that the other's (slightly grown)
// strokes cover.
double InkCollision(const InkMask &a, const InkMask &b_raw) {
  const InkMask b = Dilated(b_raw);
  long both = 0, na = 0, nb = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    na += a[i];
    nb += b_raw[i];
    both += a[i] && b[i];
  }
  const long smaller = std::min(na, nb);
  return smaller == 0 ? 0.0 : static_cast<double>(both) / smaller;
}

// GlyphWiki's medians let neighbouring boxes overlap by 25..110 units, which
// suits its narrowed positional forms and interleaved strokes (亻 and 木) but
// makes dense components (木 and 巻, 龍 and 龍) run into each other. Compare
// the strokes themselves and, while more than a sliver collide, squeeze the
// facing edges of the two boxes closer together.
constexpr double kMaxInkCollision = 0.015;
constexpr int kInkStep = 3;
constexpr int kMinBoxSpan = 60;

template <typename Placement>
void LimitInkOverlap(bool horizontal, const kage::Buhin &glyphs,
                     Placement *first, Placement *second) {
  for (int round = 0; round < 30; ++round) {
    int &a_end = horizontal ? first->box.right : first->box.bottom;
    int &b_start = horizontal ? second->box.left : second->box.top;
    if (b_start >= a_end) return;  // boxes no longer overlap
    const InkMask a = InkMaskOf(glyphs, first->box, first->name);
    const InkMask b = InkMaskOf(glyphs, second->box, second->name);
    if (a.empty() || b.empty()) return;
    if (InkCollision(a, b) <= kMaxInkCollision) return;
    const int a_span = horizontal ? first->box.right - first->box.left
                                  : first->box.bottom - first->box.top;
    const int b_span = horizontal ? second->box.right - second->box.left
                                  : second->box.bottom - second->box.top;
    if (a_span - kInkStep < kMinBoxSpan || b_span - kInkStep < kMinBoxSpan) {
      return;
    }
    a_end -= kInkStep;
    b_start += kInkStep;
  }
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

// A box given in the space of a cell (0..200) as seen from the whole glyph.
BoxRect ToFrame(const BoxRect &frame, const BoxRect &box) {
  const double sx = (frame.right - frame.left) / 200.0;
  const double sy = (frame.bottom - frame.top) / 200.0;
  return {frame.left + box.left * sx, frame.top + box.top * sy,
          frame.left + box.right * sx, frame.top + box.bottom * sy};
}

// |adjust| holds the user's boxes for slots of this layout (keys relative to
// it), |frame| is where this layout sits in the whole glyph's 0..200 space and
// |prefix| its path; when |report| is set, every slot that is drawn as one
// piece (a leaf, or a ready-made nested glyph) is listed there.
std::string ComposeLayout(std::string_view op, const std::vector<ComposerSlot> &parts,
                          kage::Buhin *nested, const ComposerLayout *layout,
                          const ComposerSubtrees *subtrees,
                          bool limit_ink_overlap,
                          const LayoutAdjustments *adjust = nullptr,
                          const BoxRect &frame = {0, 0, 200, 200},
                          const SlotPath &prefix = {},
                          std::vector<PlacedBox> *report = nullptr) {
  const std::vector<Box> boxes = BoxesFor(op);
  struct Placement {
    std::string name;
    Box box;
    bool piece = true;  // drawn as one glyph (not a layout composed here)
    std::string text;
  };
  std::vector<Placement> placements;
  placements.reserve(parts.size());
  // The user's boxes: [i] for slot i itself, deeper ones for its inside. They
  // are applied last, so they win over every automatic adjustment below.
  std::vector<LayoutAdjustments> inner(parts.size());
  std::vector<const BoxRect *> own(parts.size(), nullptr);
  if (adjust != nullptr) {
    for (const auto &[path, box] : *adjust) {
      if (path.empty() || path[0] < 0 || path[0] >= static_cast<int>(parts.size())) {
        continue;
      }
      if (path.size() == 1) {
        own[path[0]] = &box;
      } else {
        inner[path[0]][SlotPath(path.begin() + 1, path.end())] = box;
      }
    }
  }
  for (size_t i = 0; i < parts.size(); ++i) {
    Box box = boxes[std::min(i, boxes.size() - 1)];
    std::string name;
    bool piece = true;
    std::string text;
    if (parts[i].kind == ComposerSlot::Kind::kLeaf) {
      text = parts[i].input;
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
      ExpandBoxForPositionalForm(op, static_cast<int>(i), name,
                                 placements.empty() ? nullptr
                                                    : &placements.back().box,
                                 &box);
    } else {
      name = ReadyMadeNestedGlyph(parts[i], *nested, subtrees);
      if (name.empty()) {
        const std::string inner_data = ComposeLayout(
            parts[i].op, parts[i].parts, nested, layout, subtrees,
            limit_ink_overlap, &inner[i]);
        if (inner_data.empty()) return {};
        name = absl::StrCat("__nested", nested->size());
        nested->Set(name, inner_data);
        piece = false;
      }
      // Nested slots have no leaf character in the layout table. Keep the
      // operator default box (BoxesFor); DefaultFor's median can be a near-
      // full frame that then fights HarmonizeHorizontalOverlap.
    }
    if (name.empty()) return {};
    placements.push_back({std::move(name), box, piece, std::move(text)});
  }
  if (op == "⿰" && placements.size() >= 2) {
    HarmonizeHorizontalOverlap(&placements[0].box, &placements[1].box);
  } else if (op == "⿲" && placements.size() >= 3) {
    HarmonizeHorizontalOverlap(&placements[0].box, &placements[1].box);
    HarmonizeHorizontalOverlap(&placements[1].box, &placements[2].box);
  }
  const bool horizontal = op == "⿰" || op == "⿲";
  if (limit_ink_overlap && (horizontal || op == "⿱" || op == "⿳")) {
    for (size_t i = 0; i + 1 < placements.size(); ++i) {
      LimitInkOverlap(horizontal, *nested, &placements[i], &placements[i + 1]);
    }
  }
  // Automatic boxes are final here; report them, then let the user's win.
  std::vector<BoxRect> auto_boxes;
  for (const Placement &p : placements) {
    auto_boxes.push_back({double(p.box.left), double(p.box.top),
                          double(p.box.right), double(p.box.bottom)});
  }
  for (size_t i = 0; i < placements.size(); ++i) {
    if (own[i] == nullptr) continue;
    placements[i].box = {static_cast<int>(std::lround(own[i]->left)),
                         static_cast<int>(std::lround(own[i]->top)),
                         static_cast<int>(std::lround(own[i]->right)),
                         static_cast<int>(std::lround(own[i]->bottom))};
  }
  if (report != nullptr) {
    for (size_t i = 0; i < placements.size(); ++i) {
      if (placements[i].piece) {
        PlacedBox pb;
        pb.path = prefix;
        pb.path.push_back(static_cast<int>(i));
        pb.text = placements[i].text;
        pb.auto_box = auto_boxes[i];
        const Box &b = placements[i].box;
        pb.box = {double(b.left), double(b.top), double(b.right), double(b.bottom)};
        pb.parent = frame;
        report->push_back(std::move(pb));
      } else {
        // A composed nested layout: its pieces are the handles. Compose it
        // again against its final box to learn where they sit.
        SlotPath child_prefix = prefix;
        child_prefix.push_back(static_cast<int>(i));
        const Box &b = placements[i].box;
        kage::Buhin scratch(nested);
        ComposeLayout(parts[i].op, parts[i].parts, &scratch, layout, subtrees,
                      limit_ink_overlap, &inner[i],
                      ToFrame(frame, {double(b.left), double(b.top),
                                      double(b.right), double(b.bottom)}),
                      child_prefix, report);
      }
    }
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
                            const ComposerSubtrees *subtrees,
                            bool limit_ink_overlap,
                            const LayoutAdjustments *adjust) {
  if (!composer_ids::ComposeIds(op, parts)) return {};
  return ComposeLayout(op, parts, nested, layout, subtrees, limit_ink_overlap,
                       adjust);
}

std::vector<PlacedBox> ComposerBoxes(
    std::string_view op, const std::vector<ComposerSlot> &parts,
    const KageResources &resources, const LayoutAdjustments *adjust) {
  std::vector<PlacedBox> boxes;
  if (!composer_ids::ComposeIds(op, parts)) return boxes;
  kage::Buhin scratch(&resources.glyphs);
  ComposeLayout(op, parts, &scratch, &resources.layout, &resources.subtrees,
                /*limit_ink_overlap=*/true, adjust, {0, 0, 200, 200}, {}, &boxes);
  return boxes;
}

std::optional<std::vector<Contour>> ComposerOutlines(
    std::string_view op, const std::vector<ComposerSlot> &parts,
    const KageResources &resources, int view_box,
    const LayoutAdjustments *adjust) {
  // Nested layouts are glyphs of their own; keep them in a table on top of the
  // shared, read-only one.
  kage::Buhin scratch(&resources.glyphs);
  const std::string data = ComposeKageData(op, parts, &scratch, &resources.layout,
                                           &resources.subtrees, true, adjust);
  if (data.empty()) return std::nullopt;
  return OutlinesFromData(kage::Kage(&scratch), data, view_box);
}

std::string GlyphWikiNameForCharacter(std::string_view ch) {
  return GlyphName(ch);
}

std::string GlyphWikiSystematicName(
    std::string_view op, const std::vector<composer_ids::ComposerSlot> &parts) {
  return SystematicGlyphName(composer_ids::ComposerSlot::Nested(op, parts));
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
