// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/char_info.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "base/file_util.h"

namespace mozc {
namespace composer_ids {
namespace {

constexpr std::string_view kFile = "char_info.tsv";

std::string Cell(const std::vector<std::string_view> &cols, size_t i) {
  return i < cols.size() ? std::string(cols[i]) : std::string();
}

}  // namespace

absl::StatusOr<CharInfoTable> CharInfoTable::FromTsv(std::string_view tsv) {
  CharInfoTable table;
  for (std::string_view line : absl::StrSplit(tsv, '\n')) {
    if (line.empty() || line[0] == '#') continue;
    const std::vector<std::string_view> cols =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    // char cp strokes pinyin ja_on ja_kun rs definition variants
    if (cols.size() < 2 || cols[0].empty()) {
      return absl::InvalidArgumentError(
          "char_info.tsv line needs at least char and cp");
    }
    CharInfo info;
    info.cp = Cell(cols, 1);
    info.strokes = Cell(cols, 2);
    info.pinyin = Cell(cols, 3);
    info.ja_on = Cell(cols, 4);
    info.ja_kun = Cell(cols, 5);
    info.rs = Cell(cols, 6);
    info.definition = Cell(cols, 7);
    info.variants = Cell(cols, 8);
    table.by_char_[std::string(cols[0])] = std::move(info);
  }
  return table;
}

absl::StatusOr<CharInfoTable> CharInfoTable::LoadFromDirectory(
    std::string_view directory) {
  absl::StatusOr<std::string> contents =
      FileUtil::GetContents(FileUtil::JoinPath(directory, kFile));
  if (!contents.ok()) return contents.status();
  return FromTsv(*contents);
}

const CharInfo *CharInfoTable::Find(std::string_view ch) const {
  const auto it = by_char_.find(ch);
  return it == by_char_.end() ? nullptr : &it->second;
}

}  // namespace composer_ids
}  // namespace mozc
