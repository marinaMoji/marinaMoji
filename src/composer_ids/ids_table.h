// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// IDS -> encoded character lookup, and the reverse.
//
// The table is data, not code. It is read at run time from the files in
// data/character_composer/ (ids_table.tsv, radical_forms.tsv), which are
// GPL-2.0-or-later (CHISE IDS) and are deliberately never compiled or linked
// into a marinaMoji binary; stroke_counts.tsv (Unihan) is optional and read
// the same way. See data/character_composer/README.md.

#ifndef MOZC_COMPOSER_IDS_IDS_TABLE_H_
#define MOZC_COMPOSER_IDS_IDS_TABLE_H_

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace mozc {
namespace composer_ids {

// An inclusive range of total stroke counts.
struct StrokeRange {
  int min = 0;
  int max = std::numeric_limits<int>::max();

  bool Contains(int strokes) const { return strokes >= min && strokes <= max; }
};

// What the "find characters containing" field holds: components, and
// optionally a stroke filter written among them.
struct LookupQuery {
  // The component characters, in order, as one UTF-8 string.
  std::string components;
  std::optional<StrokeRange> strokes;
};

// Reads "木12" (exactly 12 strokes), "木<12" (fewer than 12), "木>8" (more than
// 8), "木9-12" (9 to 12), and "<=" / ">=" ("木>=8") for inclusive bounds.
// Full-width digits and signs work too. The filter may stand anywhere, and
// alone ("9-12"). Anything else that is ASCII is ignored.
LookupQuery ParseLookupQuery(std::string_view text);

class IdsTable {
 public:
  // Builds a table from the contents of ids_table.tsv ("IDS<TAB>char" per
  // line) and radical_forms.tsv (one family per line, tab-separated, base
  // form first). Blank lines are ignored; a malformed ids line is an error.
  // |radical_forms_tsv| may be empty.
  static absl::StatusOr<IdsTable> FromTsv(std::string_view ids_tsv,
                                          std::string_view radical_forms_tsv);

  // Loads the two files from |directory|.
  static absl::StatusOr<IdsTable> LoadFromDirectory(std::string_view directory);

  // Where the installed data lives: $MARINAMOJI_CHARACTER_COMPOSER_DIR if set,
  // otherwise <server directory>/character_composer.
  static std::string DefaultDirectory();

  // The encoded character this IDS describes ("⿰言某" -> "謀"), or nullopt
  // when the structure has no standard decomposition on record. An exact match
  // wins; failing that the radicals are compared by family, so "⿰業刀" finds
  // the character recorded as "⿰業刂". Nullopt too when family-level matching
  // would be ambiguous (two different characters share the reduced form).
  std::optional<std::string> FindEncodedCharacter(std::string_view ids) const;

  // Every IDS recorded for the first character of |input| ("謀" -> {"⿰言某"}),
  // in the data's order. Empty for an atomic component, an unknown character
  // or empty input.
  std::vector<std::string> FindDecompositions(std::string_view input) const;

  // Every encoded character whose decomposition contains every component in
  // |components| (one character each; whitespace and duplicates are ignored),
  // at any depth: with 某 = ⿱廿木 and 謀 = ⿰言某, "木" finds both. Radical
  // forms count as the same component (扌 finds what 手 builds), and a
  // component never matches itself. Sorted by code point; empty when
  // |components| holds no component or nothing matches. The index behind this
  // is built on first use, so the table is not safe to share across threads.
  std::vector<std::string> FindContaining(std::string_view components) const;

  // As above, keeping only characters with a total stroke count in |strokes|
  // (a character with several recorded counts matches if any does; one with
  // none never matches). With no components, every character in the range.
  // Without stroke data (see LoadStrokeCounts) a range matches nothing.
  std::vector<std::string> FindContaining(
      std::string_view components, const std::optional<StrokeRange> &strokes) const;

  // Reads stroke_counts.tsv ("character<TAB>count[,count...]" per line; bad
  // lines are skipped). The table works without it.
  void LoadStrokeCounts(std::string_view tsv);
  bool has_stroke_counts() const { return !stroke_counts_.empty(); }

  // The first recorded total stroke count of |ch| (Unihan lists the main
  // count first), or nullopt when there is none.
  std::optional<int> StrokeCount(std::string_view ch) const;

  // Overrides the built-in leading strokes: "character<TAB>class" per line,
  // class 1-5; "#" lines and bad lines are skipped. Not read from the data
  // directory; for tests, or for a data file of one's own.
  void LoadLeadingStrokes(std::string_view tsv);

  // The class of the first stroke written in |ch|: 1 一 horizontal, 2 丨
  // vertical, 3 丿 left-falling, 4 丶 dot, 5 乛 turning; 0 when unknown.
  // Looked up in LoadLeadingStrokes' entries, then in a hand-written table of
  // strokes, radical forms and common components, then taken from the part
  // written first in the character's decomposition (the second part of ⿺
  // 辶/廴 and ⿶ 凵, which are written last).
  int LeadingStroke(std::string_view ch) const;

  // Orders |chars| for display: by stroke count, then leading stroke class,
  // then code point. Characters with no stroke count go last, and an unknown
  // leading stroke sorts after the five classes.
  void SortByStrokes(std::vector<std::string> *chars) const;

  // Every form of the radical |ch| belongs to, |ch| included, base form
  // first. 阝 belongs to two families (阜 and 邑) and gets both. Empty when
  // |ch| is not a known radical form.
  std::vector<std::string> FormsOf(std::string_view ch) const;

  // The next form after |ch| in its family, wrapping round, or nullopt when
  // |ch| has no other form.
  std::optional<std::string> NextFormOf(std::string_view ch) const;

  // The form a radical takes at a position: 手 is 扌 as part 0 of ⿰, 竹 is 𥫗
  // as part 0 of ⿱. nullopt when |ch|'s own shape is right there (or nothing is
  // known about it). Read from the "@" lines of radical_forms.tsv.
  std::optional<std::string> AutomaticFormFor(std::string_view ch,
                                              std::string_view op,
                                              int part_index) const;

  size_t size() const { return by_ids_.size(); }

 private:
  IdsTable() = default;

  std::string WithBaseRadicals(std::string_view ids) const;
  void BuildContainingIndex() const;
  int LeadingStrokeOfIds(std::string_view ids, int depth) const;
  int LeadingStrokeAt(std::string_view ch, int depth) const;

  absl::flat_hash_map<std::string, std::string> by_ids_;
  // Reduced (base-radical) IDS -> character; empty string when ambiguous.
  absl::flat_hash_map<std::string, std::string> by_base_radicals_;
  absl::flat_hash_map<std::string, std::vector<std::string>> by_char_;
  std::vector<std::vector<std::string>> families_;
  // "<char>\t<op><part index>" -> form drawn there.
  absl::flat_hash_map<std::string, std::string> automatic_forms_;
  // Any form of a radical -> the base form of its (first) family.
  absl::flat_hash_map<std::string, std::string> base_form_;
  absl::flat_hash_map<std::string, std::vector<int>> stroke_counts_;
  absl::flat_hash_map<std::string, int> leading_strokes_;
  // LeadingStroke results, including the ones worked out from decompositions.
  mutable absl::flat_hash_map<std::string, int> leading_cache_;
  // Base form of a component -> sorted characters containing it at any depth.
  // Built by FindContaining.
  mutable std::optional<
      absl::flat_hash_map<std::string, std::vector<std::string>>>
      containing_;
};

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_IDS_TABLE_H_
