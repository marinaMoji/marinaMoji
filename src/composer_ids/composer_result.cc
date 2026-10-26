// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/composer_result.h"

#include <optional>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/strings/unicode.h"
#include "base/system_util.h"

namespace mozc {
namespace composer_ids {

std::string DefaultResultFilePath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            "character_composer_result.txt");
}

absl::Status WriteComposerResult(std::string_view path, std::string_view text) {
  return FileUtil::SetContents(path, absl::StrCat(text, "\n"));
}

std::optional<std::string> ReadComposerResult(std::string_view path) {
  const absl::StatusOr<std::string> contents = FileUtil::GetContents(path);
  if (!contents.ok()) return std::nullopt;
  const std::string_view text = absl::StripAsciiWhitespace(*contents);
  if (text.empty() || text.size() > kMaxComposerResultBytes ||
      !strings::IsValidUtf8(text)) {
    return std::nullopt;
  }
  return std::string(text);
}

}  // namespace composer_ids
}  // namespace mozc
