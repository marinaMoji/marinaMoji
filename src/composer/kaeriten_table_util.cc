// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "composer/kaeriten_table_util.h"

#include <algorithm>
#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/config_file_stream.h"
#include "base/util.h"

namespace mozc {
namespace composer {
namespace {

constexpr char kKaeritenTableFile[] = "system://kaeriten.tsv";

std::string StripTrailingWhitespace(std::string s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) {
    s.pop_back();
  }
  return s;
}

std::string SuffixFromInput(absl::string_view input) {
  if (input.empty()) {
    return "";
  }
  if (input[0] == ';') {
    return std::string(input.substr(1));
  }
  return std::string(input);
}

}  // namespace

std::string GetBundledKaeritenTable() {
  std::unique_ptr<std::istream> ifs(
      ConfigFileStream::LegacyOpen(kKaeritenTableFile));
  if (ifs == nullptr) {
    return "";
  }
  std::stringstream buffer;
  buffer << ifs->rdbuf();
  return buffer.str();
}

void ParseKaeritenTsvString(absl::string_view tsv,
                            std::vector<KaeritenRow>* rows) {
  rows->clear();
  for (absl::string_view line_view :
       absl::StrSplit(tsv, '\n', absl::AllowEmpty())) {
    std::string line(line_view);
    Util::ChopReturns(&line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const std::vector<absl::string_view> fields =
        absl::StrSplit(absl::string_view(line), '\t', absl::AllowEmpty());
    if (fields.size() < 2 || fields[0].empty() || fields[1].empty()) {
      continue;
    }
    KaeritenRow row;
    row.suffix = SuffixFromInput(fields[0]);
    row.result = std::string(fields[1]);
    if (row.suffix.empty() || row.result.empty()) {
      continue;
    }
    rows->push_back(std::move(row));
  }
}

std::string SerializeKaeritenTable(const std::vector<KaeritenRow>& rows) {
  std::vector<KaeritenRow> sorted = rows;
  std::sort(sorted.begin(), sorted.end(),
            [](const KaeritenRow& a, const KaeritenRow& b) {
              return a.suffix.size() > b.suffix.size();
            });
  std::string out;
  for (const KaeritenRow& row : sorted) {
    if (row.suffix.empty() || row.result.empty()) {
      continue;
    }
    out += ';';
    out += row.suffix;
    out += '\t';
    out += row.result;
    out += "\t\tDirectInput\n";
  }
  return out;
}

std::string GetEffectiveKaeritenTable(const config::Config& config) {
  if (config.has_custom_kaeriten_table() &&
      !config.custom_kaeriten_table().empty()) {
    return config.custom_kaeriten_table();
  }
  return GetBundledKaeritenTable();
}

void LoadKaeritenShortcutEntries(
    const config::Config& config,
    std::vector<std::pair<std::string, std::string>>* entries) {
  entries->clear();
  std::vector<KaeritenRow> rows;
  ParseKaeritenTsvString(GetEffectiveKaeritenTable(config), &rows);
  for (const KaeritenRow& row : rows) {
    entries->emplace_back(';' + row.suffix, row.result);
  }
}

}  // namespace composer
}  // namespace mozc
