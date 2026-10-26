// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/ids_table.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "base/file_util.h"
#include "base/strings/unicode.h"
#include "base/system_util.h"

namespace mozc {
namespace composer_ids {
namespace {

constexpr std::string_view kIdsFile = "ids_table.tsv";
constexpr std::string_view kRadicalFormsFile = "radical_forms.tsv";

std::string_view StripCarriageReturn(std::string_view line) {
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  return line;
}

}  // namespace

absl::StatusOr<IdsTable> IdsTable::FromTsv(std::string_view ids_tsv,
                                           std::string_view radical_forms_tsv) {
  IdsTable table;

  for (std::string_view line : absl::StrSplit(radical_forms_tsv, '\n')) {
    line = StripCarriageReturn(line);
    if (line.empty()) continue;
    std::vector<std::string> family;
    for (const std::string_view form : absl::StrSplit(line, '\t')) {
      if (!form.empty()) family.emplace_back(form);
    }
    if (!family.empty()) table.families_.push_back(std::move(family));
  }

  for (const std::vector<std::string> &family : table.families_) {
    for (const std::string &form : family) {
      if (table.base_form_.contains(form)) continue;
      const std::vector<std::string> forms = table.FormsOf(form);
      table.base_form_[form] = forms.front();
    }
  }

  struct Entry {
    std::string ids;
    std::string ch;
  };
  std::vector<Entry> entries;
  int line_number = 0;
  for (std::string_view line : absl::StrSplit(ids_tsv, '\n')) {
    ++line_number;
    line = StripCarriageReturn(line);
    if (line.empty()) continue;
    const size_t tab = line.find('\t');
    if (tab == std::string_view::npos || tab == 0 || tab + 1 >= line.size() ||
        line.find('\t', tab + 1) != std::string_view::npos) {
      return absl::InvalidArgumentError(
          absl::StrCat("malformed IDS table line ", line_number));
    }
    entries.push_back({std::string(line.substr(0, tab)),
                       std::string(line.substr(tab + 1))});
  }

  table.by_ids_.reserve(entries.size());
  table.by_base_radicals_.reserve(entries.size());
  for (const Entry &entry : entries) {
    // Later entries win, as in the table the data was built for.
    table.by_ids_[entry.ids] = entry.ch;

    std::string key = table.WithBaseRadicals(entry.ids);
    auto [it, inserted] = table.by_base_radicals_.try_emplace(key, entry.ch);
    if (!inserted && it->second != entry.ch) it->second.clear();

    // A character that decomposes to itself (a stroke, a radical) is atomic.
    if (entry.ids == entry.ch) continue;
    std::vector<std::string> &list = table.by_char_[entry.ch];
    if (std::find(list.begin(), list.end(), entry.ids) == list.end()) {
      list.push_back(entry.ids);
    }
  }
  return table;
}

absl::StatusOr<IdsTable> IdsTable::LoadFromDirectory(
    std::string_view directory) {
  absl::StatusOr<std::string> ids =
      FileUtil::GetContents(FileUtil::JoinPath(directory, kIdsFile));
  if (!ids.ok()) return ids.status();
  // The radical families only widen matching; the table works without them.
  absl::StatusOr<std::string> forms =
      FileUtil::GetContents(FileUtil::JoinPath(directory, kRadicalFormsFile));
  return FromTsv(*ids, forms.ok() ? std::string_view(*forms) : "");
}

std::string IdsTable::DefaultDirectory() {
  if (const char *env = std::getenv("MARINAMOJI_CHARACTER_COMPOSER_DIR");
      env != nullptr && *env != '\0') {
    return env;
  }
  return FileUtil::JoinPath(SystemUtil::GetServerDirectory(),
                            "character_composer");
}

std::vector<std::string> IdsTable::FormsOf(std::string_view ch) const {
  std::vector<std::string> forms;
  for (const std::vector<std::string> &family : families_) {
    if (std::find(family.begin(), family.end(), ch) == family.end()) continue;
    for (const std::string &form : family) {
      if (std::find(forms.begin(), forms.end(), form) == forms.end()) {
        forms.push_back(form);
      }
    }
  }
  return forms;
}

std::optional<std::string> IdsTable::NextFormOf(std::string_view ch) const {
  const std::vector<std::string> forms = FormsOf(ch);
  if (forms.size() < 2) return std::nullopt;
  const auto it = std::find(forms.begin(), forms.end(), ch);
  return forms[((it - forms.begin()) + 1) % forms.size()];
}

std::string IdsTable::WithBaseRadicals(std::string_view ids) const {
  std::string out;
  for (const std::string_view ch : Utf8AsChars(ids)) {
    const auto it = base_form_.find(ch);
    out.append(it == base_form_.end() ? ch : std::string_view(it->second));
  }
  return out;
}

std::optional<std::string> IdsTable::FindEncodedCharacter(
    std::string_view ids) const {
  if (const auto it = by_ids_.find(ids); it != by_ids_.end()) {
    return it->second;
  }
  const auto it = by_base_radicals_.find(WithBaseRadicals(ids));
  if (it == by_base_radicals_.end() || it->second.empty()) return std::nullopt;
  return it->second;
}

std::vector<std::string> IdsTable::FindDecompositions(
    std::string_view input) const {
  for (const std::string_view ch :
       Utf8AsChars(absl::StripAsciiWhitespace(input))) {
    const auto it = by_char_.find(ch);
    return it == by_char_.end() ? std::vector<std::string>() : it->second;
  }
  return {};
}

}  // namespace composer_ids
}  // namespace mozc
