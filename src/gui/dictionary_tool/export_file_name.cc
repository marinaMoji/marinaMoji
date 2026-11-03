// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/dictionary_tool/export_file_name.h"

#include <cstddef>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace gui {

std::string AddDefaultExportExtension(absl::string_view path) {
  const size_t separator = path.find_last_of("/\\");
  const absl::string_view base = separator == absl::string_view::npos
                                     ? path
                                     : path.substr(separator + 1);
  if (base.empty()) {
    // A directory, not a file name: leave it for the caller to reject.
    return std::string(path);
  }
  const size_t dot = base.rfind('.');
  if (dot == absl::string_view::npos || dot == 0) {
    return absl::StrCat(path, ".", kDictionaryExportExtension);
  }
  if (dot + 1 == base.size()) {
    // "words." -> "words.txt"
    return absl::StrCat(path, kDictionaryExportExtension);
  }
  return std::string(path);
}

}  // namespace gui
}  // namespace mozc
