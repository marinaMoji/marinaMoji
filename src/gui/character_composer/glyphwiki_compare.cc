// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/glyphwiki_compare.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "base/strings/unicode.h"
#include "composer_ids/ids_operator.h"
#include "composer_ids/ids_tree.h"
#include "kage/kage_engine.h"

namespace mozc {
namespace gui {
namespace {

using composer_ids::ComposerSlot;
using composer_ids::ParsedIds;

// Winding contribution of one closed contour at (x, y): +1 / −1 per crossing.
int ContourWinding(const Contour &contour, double x, double y) {
  if (contour.size() < 3) return 0;
  int wind = 0;
  for (size_t i = 0, n = contour.size(); i < n; ++i) {
    const auto &[x1, y1] = contour[i];
    const auto &[x2, y2] = contour[(i + 1) % n];
    if (y1 == y2) continue;
    if (y < std::min(y1, y2) || y >= std::max(y1, y2)) continue;
    const double t = (y - y1) / (y2 - y1);
    const double x_cross = x1 + t * (x2 - x1);
    if (x_cross > x) {
      wind += (y2 > y1) ? 1 : -1;
    }
  }
  return wind;
}

bool PointInOutlines(const std::vector<Contour> &outlines, double x,
                     double y) {
  int wind = 0;
  for (const Contour &c : outlines) {
    wind += ContourWinding(c, x, y);
  }
  return wind != 0;
}

// First leaf component of a parsed IDS, or empty.
std::string FirstComponent(const ParsedIds &parsed) {
  for (const ComposerSlot &slot : parsed.parts) {
    if (slot.kind == ComposerSlot::Kind::kLeaf && !slot.input.empty()) {
      return slot.input;
    }
  }
  return {};
}

std::string FormatFixed(double value, int width, int precision) {
  // Avoid depending on absl::StrFormat in this target.
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%*.*f", width, precision, value);
  return buf;
}

// True when GlyphWiki's registered glyph (following one-line 99: aliases) is a
// composition of two or more 99: component placements — i.e. comparable as
// layout, not as Mincho stroke style.
bool GlyphWikiIsComposition(std::string_view character,
                            const kage::Buhin &glyphs) {
  std::string name = GlyphWikiNameForCharacter(character);
  for (int depth = 0; depth < 5 && !name.empty(); ++depth) {
    const std::string_view data = glyphs.Search(name);
    if (data.empty()) return false;
    std::vector<std::string_view> records =
        absl::StrSplit(data, '$', absl::SkipEmpty());
    if (records.empty()) return false;
    std::string next;
    for (std::string_view record : records) {
      const std::vector<std::string_view> fields =
          absl::StrSplit(record, ':');
      if (fields.empty() || fields[0] != "99" || fields.size() < 8) {
        return false;  // a stroke record → style
      }
      if (records.size() == 1) {
        next = std::string(fields[7]);
        const size_t at = next.find('@');
        if (at != std::string::npos) next.resize(at);
      }
    }
    if (records.size() >= 2) return true;
    name = std::move(next);
  }
  return false;
}

bool IsUnifiedIdeograph(std::string_view ch) {
  const std::u32string cps = strings::Utf8ToUtf32(ch);
  if (cps.size() != 1) return false;
  const char32_t c = cps[0];
  return (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0x3400 && c <= 0x4DBF);
}

}  // namespace

GlyphBitmap RasterizeOutlines(const std::vector<Contour> &outlines,
                              int view_box, int size) {
  GlyphBitmap bitmap;
  bitmap.size = size;
  bitmap.pixels.assign(static_cast<size_t>(size) * size, 0);
  if (size <= 0 || view_box <= 0 || outlines.empty()) return bitmap;
  const double scale = static_cast<double>(view_box) / size;
  for (int row = 0; row < size; ++row) {
    for (int col = 0; col < size; ++col) {
      const double x = (col + 0.5) * scale;
      const double y = (row + 0.5) * scale;
      if (PointInOutlines(outlines, x, y)) {
        bitmap.pixels[static_cast<size_t>(row) * size + col] = 1;
        ++bitmap.ink;
      }
    }
  }
  return bitmap;
}

OverlapScore ScoreOverlap(const GlyphBitmap &composer,
                          const GlyphBitmap &reference) {
  OverlapScore score;
  if (composer.size != reference.size || composer.size <= 0) return score;
  const size_t n = composer.pixels.size();
  int intersection = 0;
  int union_ink = 0;
  for (size_t i = 0; i < n; ++i) {
    const bool a = composer.pixels[i] != 0;
    const bool b = reference.pixels[i] != 0;
    if (a && b) ++intersection;
    if (a || b) ++union_ink;
  }
  score.intersection = intersection;
  score.union_ink = union_ink;
  score.iou = union_ink == 0 ? 0.0
                             : static_cast<double>(intersection) / union_ink;
  return score;
}

CompareResult CompareCharacter(std::string_view character,
                               const composer_ids::IdsTable &ids,
                               const KageResources &resources, int view_box,
                               int bitmap_size) {
  CompareResult result;
  result.character = std::string(character);

  const std::vector<std::string> decomps = ids.FindDecompositions(character);
  if (decomps.empty()) {
    result.skip_reason = "no IDS";
    return result;
  }
  // Prefer a decomposition that starts with an IDS operator (skip identity
  // rows like "休<TAB>休").
  std::optional<ParsedIds> parsed;
  for (const std::string &ids_str : decomps) {
    parsed = composer_ids::ParseIdsToComposerTree(ids_str);
    if (parsed) {
      result.ids = ids_str;
      result.op = parsed->op;
      break;
    }
  }
  if (!parsed) {
    result.skip_reason = "IDS not a layout";
    return result;
  }

  const auto composer =
      ComposerOutlines(parsed->op, parsed->parts, resources, view_box);
  if (!composer) {
    result.skip_reason = "composer could not draw";
    return result;
  }
  const auto reference = NamedGlyphOutlines(character, resources, view_box);
  if (!reference) {
    result.skip_reason = "no registered glyph";
    return result;
  }

  const GlyphBitmap a = RasterizeOutlines(*composer, view_box, bitmap_size);
  const GlyphBitmap b = RasterizeOutlines(*reference, view_box, bitmap_size);
  result.score = ScoreOverlap(a, b);
  result.score.contour_delta = std::abs(
      static_cast<int>(composer->size()) - static_cast<int>(reference->size()));
  result.glyphwiki_is_composition =
      GlyphWikiIsComposition(character, resources.glyphs);
  result.ok = true;
  return result;
}

std::vector<std::string> CharactersWithIdsLayout(
    std::string_view ids_tsv, bool unified_ideographs_only) {
  std::vector<std::string> characters;
  absl::flat_hash_set<std::string> seen;
  for (std::string_view line :
       absl::StrSplit(ids_tsv, '\n', absl::SkipEmpty())) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || line.front() == '#') continue;
    const std::vector<std::string> cols =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    if (cols.size() < 2) continue;
    const std::string &ids = cols[0];
    const std::string &ch = cols[1];
    if (ch.empty() || ids == ch) continue;
    if (!composer_ids::IsIdsOperator(
            strings::Utf32ToUtf8(strings::Utf8ToUtf32(ids).substr(0, 1)))) {
      continue;
    }
    if (unified_ideographs_only && !IsUnifiedIdeograph(ch)) continue;
    if (seen.insert(ch).second) {
      characters.push_back(ch);
    }
  }
  return characters;
}

std::vector<CompareResult> CompareCharacters(
    const std::vector<std::string> &characters,
    const composer_ids::IdsTable &ids, const KageResources &resources,
    int view_box, int bitmap_size) {
  std::vector<CompareResult> results;
  results.reserve(characters.size());
  for (const std::string &ch : characters) {
    CompareResult r =
        CompareCharacter(ch, ids, resources, view_box, bitmap_size);
    if (r.ok) results.push_back(std::move(r));
  }
  std::sort(results.begin(), results.end(),
            [](const CompareResult &a, const CompareResult &b) {
              if (a.score.iou != b.score.iou) return a.score.iou < b.score.iou;
              return a.character < b.character;
            });
  return results;
}

std::string FormatCompareReport(const std::vector<CompareResult> &results) {
  std::string out;
  int n_layout = 0, n_style = 0;
  for (const CompareResult &r : results) {
    (r.glyphwiki_is_composition ? n_layout : n_style) += 1;
  }
  absl::StrAppend(&out, "GlyphWiki comparison: ", results.size(),
                  " characters scored (worst IoU first); ", n_layout,
                  " layout (GlyphWiki is a 99: composition), ", n_style,
                  " style (stroke-drawn)\n\n");
  absl::StrAppend(&out, "char  IDS           IoU    Δctr  op   kind\n");
  for (const CompareResult &r : results) {
    absl::StrAppend(&out, r.character, "  ", r.ids, "  ",
                    FormatFixed(r.score.iou, 6, 3), "  ",
                    r.score.contour_delta, "  ", r.op, "  ",
                    r.glyphwiki_is_composition ? "layout" : "style", "\n");
  }

  struct Agg {
    double sum = 0;
    int n = 0;
  };
  auto fill_groups = [&](bool layout_only, std::map<std::string, Agg> *by_op,
                         std::map<std::string, Agg> *by_component) {
    for (const CompareResult &r : results) {
      if (layout_only && !r.glyphwiki_is_composition) continue;
      (*by_op)[r.op].sum += r.score.iou;
      (*by_op)[r.op].n += 1;
      const auto parsed = composer_ids::ParseIdsToComposerTree(r.ids);
      if (!parsed) continue;
      const std::string comp = FirstComponent(*parsed);
      if (!comp.empty()) {
        (*by_component)[comp].sum += r.score.iou;
        (*by_component)[comp].n += 1;
      }
    }
  };

  auto append_ranked = [&](std::string_view title,
                           const std::map<std::string, Agg> &groups) {
    std::vector<std::pair<std::string, double>> ranked;
    for (const auto &[key, agg] : groups) {
      ranked.push_back({key, agg.sum / agg.n});
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto &a, const auto &b) { return a.second < b.second; });
    absl::StrAppend(&out, "\n", title, " (mean IoU, worst first)\n");
    for (const auto &[key, mean] : ranked) {
      absl::StrAppend(&out, "  ", key, "  ", FormatFixed(mean, 6, 3), "  (n=",
                      groups.at(key).n, ")\n");
    }
  };

  std::map<std::string, Agg> by_op, by_component, by_op_layout, by_comp_layout;
  fill_groups(/*layout_only=*/false, &by_op, &by_component);
  fill_groups(/*layout_only=*/true, &by_op_layout, &by_comp_layout);
  append_ranked("By IDS operator (all)", by_op);
  append_ranked("By IDS operator (layout only)", by_op_layout);
  append_ranked("By first component (layout only)", by_comp_layout);
  return out;
}

bool CheckBaseline(const std::vector<CompareResult> &results,
                   std::string_view baseline_tsv, double tolerance,
                   std::string *message) {
  absl::flat_hash_map<std::string, double> got;
  for (const CompareResult &r : results) {
    if (r.ok) got[r.character] = r.score.iou;
  }

  std::vector<std::string> failures;
  for (std::string_view line :
       absl::StrSplit(baseline_tsv, '\n', absl::SkipEmpty())) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || line.front() == '#') continue;
    const std::vector<std::string> cols =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    if (cols.size() < 2) continue;
    const std::string &ch = cols[0];
    double expected = 0;
    if (!absl::SimpleAtod(cols[1], &expected)) {
      failures.push_back(absl::StrCat(ch, ": bad baseline value"));
      continue;
    }
    const auto it = got.find(ch);
    if (it == got.end()) {
      failures.push_back(absl::StrCat(ch, ": not scored"));
      continue;
    }
    if (it->second + tolerance < expected) {
      failures.push_back(absl::StrCat(
          ch, ": IoU ", FormatFixed(it->second, 0, 4),
          " is more than ", FormatFixed(tolerance, 0, 4),
          " below baseline ", FormatFixed(expected, 0, 4)));
    }
  }
  if (failures.empty()) return true;
  if (message != nullptr) {
    *message = absl::StrJoin(failures, "\n");
  }
  return false;
}

}  // namespace gui
}  // namespace mozc
