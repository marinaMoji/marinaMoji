// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "kage/kage_data.h"

#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_split.h"
#include "base/file_util.h"
#include "kage/kage_engine.h"

namespace mozc {
namespace kage {

Buhin ParseBuhin(std::string_view contents) {
  Buhin buhin;
  for (std::string_view line : absl::StrSplit(contents, '\n')) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || line.front() == '#') continue;
    const size_t tab = line.find('\t');
    if (tab == std::string_view::npos || tab == 0) continue;
    buhin.Set(std::string(line.substr(0, tab)),
              std::string(absl::StripAsciiWhitespace(line.substr(tab + 1))));
  }
  return buhin;
}

absl::StatusOr<Buhin> LoadBuhinFromFile(std::string_view path) {
  absl::StatusOr<std::string> contents = FileUtil::GetContents(path);
  if (!contents.ok()) return contents.status();
  return ParseBuhin(*contents);
}

}  // namespace kage
}  // namespace mozc
