// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/composer_result.h"

#include <cstdint>
#include <cstdlib>
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

std::string DefaultSvgResultFilePath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            "character_composer_result.svg");
}

absl::Status WriteComposerResult(std::string_view path, std::string_view text) {
  return FileUtil::SetContents(path, absl::StrCat(text, "\n"));
}

absl::Status WriteComposerSvgResult(std::string_view path,
                                    std::string_view svg) {
  return FileUtil::SetContents(path, svg);
}

namespace {

// JSON string literal for |s| (UTF-8 passes through; the plugin loads the
// script as UTF-8). U+2028/U+2029 are legal in JSON but not in older
// JavaScript string literals, so they are escaped too.
std::string JsString(std::string_view s) {
  std::string out = "\"";
  for (size_t i = 0; i < s.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          absl::StrAppend(&out, "\\u00", c < 0x10 ? "0" : "",
                          absl::Hex(c, absl::kNoPad));
        } else if (c == 0xE2 && i + 2 < s.size() &&
                   static_cast<unsigned char>(s[i + 1]) == 0x80 &&
                   (static_cast<unsigned char>(s[i + 2]) & 0xFE) == 0xA8) {
          out += (static_cast<unsigned char>(s[i + 2]) == 0xA8) ? "\\u2028"
                                                                : "\\u2029";
          i += 2;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  out += "\"";
  return out;
}

}  // namespace

std::string OnlyOfficeHandoffScript(std::string_view svg, int64_t id_ms) {
  return absl::StrCat("window.marinaMojiComposerResult={id:", id_ms,
                      ",svg:", JsString(svg), "};\n");
}

std::string OnlyOfficePluginDirectory() {
#ifdef __APPLE__
  const char *home = std::getenv("HOME");
  if (home == nullptr || *home == '\0') return "";
  return FileUtil::JoinPath(
      home,
      "Library/Application Support/asc.onlyoffice.ONLYOFFICE/data/sdkjs-plugins/"
      "{7A9E3B2C-4D5F-6E8A-1B0C-9D3E5F7A2B1C}");
#else
  return "";
#endif  // __APPLE__
}

absl::Status WriteOnlyOfficeHandoff(std::string_view directory,
                                    std::string_view svg, int64_t id_ms) {
  if (directory.empty() || !FileUtil::DirectoryExists(directory).ok()) {
    return absl::OkStatus();
  }
  return FileUtil::SetContents(
      FileUtil::JoinPath(directory, "composer_result.js"),
      OnlyOfficeHandoffScript(svg, id_ms));
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

std::optional<std::string> ReadComposerSvgResult(std::string_view path) {
  const absl::StatusOr<std::string> contents = FileUtil::GetContents(path);
  if (!contents.ok()) return std::nullopt;
  const std::string_view text = absl::StripAsciiWhitespace(*contents);
  if (text.empty() || text.find("<svg") == std::string_view::npos) {
    return std::nullopt;
  }
  return std::string(text);
}

}  // namespace composer_ids
}  // namespace mozc
