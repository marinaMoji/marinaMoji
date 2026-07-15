// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "win32/base/toolbar_config.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc::win32 {
namespace {

constexpr char kToolbarVisibleKey[] = "toolbar_visible";

std::string ConfigFilePath() {
  return FileUtil::JoinPath(
      {SystemUtil::GetUserProfileDirectory(), "toolbar.conf"});
}

std::vector<std::string> SplitLines(const std::string& text) {
  std::vector<std::string> lines;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t eol = text.find('\n', pos);
    std::string line = text.substr(
        pos, eol == std::string::npos ? std::string::npos : eol - pos);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(std::move(line));
    if (eol == std::string::npos) {
      break;
    }
    pos = eol + 1;
  }
  return lines;
}

}  // namespace

bool LoadToolbarVisiblePreference() {
  const auto contents = FileUtil::GetContents(ConfigFilePath());
  if (!contents.ok()) {
    return true;
  }
  for (const std::string& line : SplitLines(*contents)) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos ||
        line.substr(0, eq) != kToolbarVisibleKey) {
      continue;
    }
    const std::string value = line.substr(eq + 1);
    return value != "0" && value != "false" && value != "False";
  }
  return true;
}

bool SaveToolbarVisiblePreference(bool visible) {
  const std::string path = ConfigFilePath();
  std::vector<std::string> lines;
  if (const auto contents = FileUtil::GetContents(path); contents.ok()) {
    lines = SplitLines(*contents);
  }

  bool replaced = false;
  for (std::string& line : lines) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos ||
        line.substr(0, eq) != kToolbarVisibleKey) {
      continue;
    }
    line = absl::StrCat(kToolbarVisibleKey, "=", visible ? "1" : "0");
    replaced = true;
    break;
  }
  if (!replaced) {
    lines.push_back(absl::StrCat(kToolbarVisibleKey, "=", visible ? "1" : "0"));
  }

  std::string serialized;
  for (const std::string& line : lines) {
    absl::StrAppend(&serialized, line, "\n");
  }
  return FileUtil::SetContents(path, serialized).ok();
}

}  // namespace mozc::win32
