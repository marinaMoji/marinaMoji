// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Display metadata for one character (readings, strokes, Unihan variants),
// read at run time from data/character_info/char_info.tsv. Missing file is
// fine: the composer still works, only the info panel stays empty.

#ifndef MOZC_COMPOSER_IDS_CHAR_INFO_H_
#define MOZC_COMPOSER_IDS_CHAR_INFO_H_

#include <optional>
#include <string>
#include <string_view>

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
};

class CharInfoTable {
 public:
  // Parses char_info.tsv. Comment lines (#…) are ignored.
  static absl::StatusOr<CharInfoTable> FromTsv(std::string_view tsv);

  // Loads char_info.tsv from |directory| (same folder as ids_table.tsv).
  static absl::StatusOr<CharInfoTable> LoadFromDirectory(
      std::string_view directory);

  const CharInfo *Find(std::string_view ch) const;

  size_t size() const { return by_char_.size(); }

 private:
  CharInfoTable() = default;

  absl::flat_hash_map<std::string, CharInfo> by_char_;
};

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_CHAR_INFO_H_
