// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// IDS -> encoded character lookup, and the reverse.
//
// The table is data, not code. It is read at run time from the files in
// data/character_composer/ (ids_table.tsv, radical_forms.tsv), which are
// GPL-2.0-or-later (CHISE IDS) and are deliberately never compiled or linked
// into a marinaMoji binary. See data/character_composer/README.md.

#ifndef MOZC_COMPOSER_IDS_IDS_TABLE_H_
#define MOZC_COMPOSER_IDS_IDS_TABLE_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace mozc {
namespace composer_ids {

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

  // Every form of the radical |ch| belongs to, |ch| included, base form
  // first. 阝 belongs to two families (阜 and 邑) and gets both. Empty when
  // |ch| is not a known radical form.
  std::vector<std::string> FormsOf(std::string_view ch) const;

  // The next form after |ch| in its family, wrapping round, or nullopt when
  // |ch| has no other form.
  std::optional<std::string> NextFormOf(std::string_view ch) const;

  size_t size() const { return by_ids_.size(); }

 private:
  IdsTable() = default;

  std::string WithBaseRadicals(std::string_view ids) const;

  absl::flat_hash_map<std::string, std::string> by_ids_;
  // Reduced (base-radical) IDS -> character; empty string when ambiguous.
  absl::flat_hash_map<std::string, std::string> by_base_radicals_;
  absl::flat_hash_map<std::string, std::vector<std::string>> by_char_;
  std::vector<std::vector<std::string>> families_;
  // Any form of a radical -> the base form of its (first) family.
  absl::flat_hash_map<std::string, std::string> base_form_;
};

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_IDS_TABLE_H_
