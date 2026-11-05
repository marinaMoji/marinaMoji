// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Display metadata for one character (readings, strokes, Unihan variants),
// read at run time from data/character_info/char_info.tsv. Missing file is
// fine: the composer still works, only the info panel stays empty.

#ifndef MOZC_COMPOSER_IDS_CHAR_INFO_H_
#define MOZC_COMPOSER_IDS_CHAR_INFO_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"

namespace mozc {
namespace composer_ids {

struct CharInfo {
  std::string cp;          // "U+8B00"
  std::string strokes;     // "16"
  std::string pinyin;      // "móu"
  std::string ja_on;       // "BOU MU"
  std::string ja_kun;      // "HAKARU …"
  std::string rs;          // "149.9"
  std::string definition;  // English gloss
  std::string variants;    // space-separated characters
  std::string ja_kana;     // "カン ケン" (on in katakana, kun in hiragana)
  std::string old_form;    // 旧字体 of this character, if this is a 新字体
  std::string new_form;    // 新字体 of this character, if this is a 旧字体
};

class CharInfoTable {
 public:
  // Parses char_info.tsv. Comment lines (#…) are ignored.
  static absl::StatusOr<CharInfoTable> FromTsv(std::string_view tsv);

  // Loads char_info.tsv from |directory| (same folder as ids_table.tsv).
  static absl::StatusOr<CharInfoTable> LoadFromDirectory(
      std::string_view directory);

  // Same directory resolution as IdsTable::DefaultDirectory():
  // MARINAMOJI_CHARACTER_COMPOSER_DIR, else <server>/character_composer.
  static std::string DefaultDirectory();

  // Lazy-loads from DefaultDirectory(). Returns nullptr if the file is
  // missing or unreadable. Thread-safe; loaded once per process.
  static const CharInfoTable *GetDefault();

  // Test hooks: replace GetDefault()'s table, or restore lazy loading.
  static void SetDefaultForTesting(std::unique_ptr<CharInfoTable> table);
  static void ClearDefaultForTesting();

  const CharInfo *Find(std::string_view ch) const;

  // Selected character first, then unique variants, old_form, new_form.
  // Empty when |ch| is unknown or has no related forms besides itself.
  std::vector<std::string> RelatedCandidates(std::string_view ch) const;

  size_t size() const { return by_char_.size(); }

 private:
  CharInfoTable() = default;

  absl::flat_hash_map<std::string, CharInfo> by_char_;
};

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_CHAR_INFO_H_
