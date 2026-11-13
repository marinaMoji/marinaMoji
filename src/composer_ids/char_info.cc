// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/char_info.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace composer_ids {
namespace {

constexpr std::string_view kFile = "char_info.tsv";
constexpr std::string_view kMjFile = "char_info_mj.tsv";

std::string Cell(const std::vector<std::string_view> &cols, size_t i) {
  return i < cols.size() ? std::string(cols[i]) : std::string();
}

struct DefaultHolder {
  std::mutex mutex;
  std::unique_ptr<CharInfoTable> table;
  bool attempted = false;
  bool testing_override = false;
};

DefaultHolder *GetDefaultHolder() {
  static DefaultHolder *holder = new DefaultHolder();
  return holder;
}

}  // namespace

absl::StatusOr<CharInfoTable> CharInfoTable::FromTsv(std::string_view tsv) {
  CharInfoTable table;
  for (std::string_view line : absl::StrSplit(tsv, '\n')) {
    if (line.empty() || line[0] == '#') continue;
    const std::vector<std::string_view> cols =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    // char cp strokes pinyin ja_on ja_kun rs definition variants ja_kana old_form
    // new_form region_forms ivs_forms
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
    info.ja_kana = Cell(cols, 9);
    info.old_form = Cell(cols, 10);
    info.new_form = Cell(cols, 11);
    info.region_forms = Cell(cols, 12);
    info.ivs_forms = Cell(cols, 13);
    table.by_char_[std::string(cols[0])] = std::move(info);
  }
  return table;
}

absl::StatusOr<CharInfoTable> CharInfoTable::LoadFromDirectory(
    std::string_view directory) {
  absl::StatusOr<std::string> contents =
      FileUtil::GetContents(FileUtil::JoinPath(directory, kFile));
  if (!contents.ok()) return contents.status();
  absl::StatusOr<CharInfoTable> table = FromTsv(*contents);
  if (!table.ok()) return table;
  // Optional: the MJ links ship as a separate file under their own licence.
  if (absl::StatusOr<std::string> mj =
          FileUtil::GetContents(FileUtil::JoinPath(directory, kMjFile));
      mj.ok()) {
    table->AddMjLinks(*mj);
  }
  return table;
}

void CharInfoTable::AddMjLinks(std::string_view tsv) {
  auto add = [this](const std::string &from, const std::string &to) {
    std::vector<std::string> &links = mj_links_[from];
    if (std::find(links.begin(), links.end(), to) == links.end()) {
      links.push_back(to);
    }
  };
  for (std::string_view line : absl::StrSplit(tsv, '\n')) {
    if (line.empty() || line[0] == '#') continue;
    const std::vector<std::string_view> cols =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    if (cols.size() < 2 || cols[0].empty() || cols[1].empty() ||
        cols[0] == cols[1]) {
      continue;
    }
    add(std::string(cols[0]), std::string(cols[1]));
    add(std::string(cols[1]), std::string(cols[0]));
  }
}

std::string CharInfoTable::DefaultDirectory() {
  if (const char *env = std::getenv("MARINAMOJI_CHARACTER_COMPOSER_DIR");
      env != nullptr && *env != '\0') {
    return env;
  }
  return FileUtil::JoinPath(SystemUtil::GetServerDirectory(),
                            "character_composer");
}

const CharInfoTable *CharInfoTable::GetDefault() {
  DefaultHolder *holder = GetDefaultHolder();
  std::lock_guard<std::mutex> lock(holder->mutex);
  if (holder->testing_override) {
    return holder->table.get();
  }
  if (!holder->attempted) {
    holder->attempted = true;
    if (auto loaded = LoadFromDirectory(DefaultDirectory()); loaded.ok()) {
      holder->table =
          std::make_unique<CharInfoTable>(*std::move(loaded));
    }
  }
  return holder->table.get();
}

void CharInfoTable::SetDefaultForTesting(std::unique_ptr<CharInfoTable> table) {
  DefaultHolder *holder = GetDefaultHolder();
  std::lock_guard<std::mutex> lock(holder->mutex);
  holder->testing_override = true;
  holder->table = std::move(table);
  holder->attempted = true;
}

void CharInfoTable::ClearDefaultForTesting() {
  DefaultHolder *holder = GetDefaultHolder();
  std::lock_guard<std::mutex> lock(holder->mutex);
  holder->testing_override = false;
  holder->table.reset();
  holder->attempted = false;
}

const CharInfo *CharInfoTable::Find(std::string_view ch) const {
  const auto it = by_char_.find(ch);
  return it == by_char_.end() ? nullptr : &it->second;
}

std::vector<std::string> CharInfoTable::RelatedCandidates(
    std::string_view ch) const {
  const CharInfo *info = Find(ch);
  if (info == nullptr) {
    return {};
  }

  std::vector<std::string> out;
  absl::flat_hash_set<std::string> seen;
  auto add = [&](std::string_view form) {
    if (form.empty() || !seen.insert(std::string(form)).second) {
      return;
    }
    out.emplace_back(form);
  };

  add(ch);
  for (std::string_view token :
       absl::StrSplit(info->variants, absl::ByAnyChar(" \t"),
                      absl::SkipEmpty())) {
    add(token);
  }
  add(info->old_form);
  add(info->new_form);
  if (const auto it = mj_links_.find(ch); it != mj_links_.end()) {
    for (const std::string &link : it->second) add(link);
  }

  // Only the selected character: nothing related to offer.
  if (out.size() <= 1) {
    return {};
  }
  return out;
}

}  // namespace composer_ids
}  // namespace mozc
