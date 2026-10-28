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

#ifdef _WIN32
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include "base/win32/wide_char.h"
#endif  // _WIN32

namespace mozc {
namespace composer_ids {

#ifdef _WIN32
namespace {
// %LOCALAPPDATA%, or the shell's answer when the variable is not inherited
// (e.g. under a test runner that scrubs the environment).
std::string LocalAppDataDirectory() {
  const char *env = std::getenv("LOCALAPPDATA");
  if (env != nullptr && *env != '\0') return env;
  wchar_t *buffer = nullptr;
  const HRESULT hr =
      ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &buffer);
  std::string result;
  if (SUCCEEDED(hr) && buffer != nullptr) {
    result = win32::WideToUtf8(buffer);
  }
  if (buffer != nullptr) ::CoTaskMemFree(buffer);
  return result;
}
}  // namespace
#endif  // _WIN32

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

std::string OnlyOfficeHandoffScript(std::string_view svg, int64_t id_ms,
                                    std::string_view text) {
  std::string body = absl::StrCat("window.marinaMojiComposerResult={id:", id_ms);
  if (!svg.empty()) {
    absl::StrAppend(&body, ",svg:", JsString(svg));
  }
  if (!text.empty()) {
    absl::StrAppend(&body, ",text:", JsString(text));
  }
  absl::StrAppend(&body, "};\n");
  return body;
}

std::string OnlyOfficePluginDirectory() {
  // Folder name matches onlyoffice/config.json guid without the "asc." prefix.
  static constexpr char kPluginGuid[] =
      "{7A9E3B2C-4D5F-6E8A-1B0C-9D3E5F7A2B1C}";
#if defined(_WIN32)
  // Official Desktop Editors path:
  // %LOCALAPPDATA%\ONLYOFFICE\DesktopEditors\data\sdkjs-plugins\{GUID}
  const std::string local_app_data = LocalAppDataDirectory();
  if (local_app_data.empty()) return "";
  return FileUtil::JoinPath(
      {local_app_data, "ONLYOFFICE", "DesktopEditors", "data", "sdkjs-plugins",
       kPluginGuid});
#else
  const char *home = std::getenv("HOME");
  if (home == nullptr || *home == '\0') return "";
#ifdef __APPLE__
  return FileUtil::JoinPath(
      home,
      FileUtil::JoinPath(
          "Library/Application Support/asc.onlyoffice.ONLYOFFICE/data/"
          "sdkjs-plugins",
          kPluginGuid));
#elif defined(__linux__)
  // Deb/RPM: ~/.local/share/onlyoffice/desktopeditors/sdkjs-plugins/{GUID}
  // Flatpak (newer): ~/.var/app/org.onlyoffice.desktopeditors/data/onlyoffice/
  //   desktopeditors/data/sdkjs-plugins/{GUID}
  // Flatpak (older): …/desktopeditors/sdkjs-plugins/{GUID}
  // Ubuntu ARM often ships Flatpak/Snap rather than the .deb; write into an
  // installed plugin folder if we can find one.
  const char *xdg_data = std::getenv("XDG_DATA_HOME");
  const std::string data_home =
      (xdg_data != nullptr && *xdg_data != '\0')
          ? std::string(xdg_data)
          : FileUtil::JoinPath(home, ".local/share");
  const std::string candidates[] = {
      FileUtil::JoinPath(
          data_home,
          FileUtil::JoinPath("onlyoffice/desktopeditors/sdkjs-plugins",
                             kPluginGuid)),
      FileUtil::JoinPath(
          home,
          FileUtil::JoinPath(
              ".var/app/org.onlyoffice.desktopeditors/data/onlyoffice/"
              "desktopeditors/data/sdkjs-plugins",
              kPluginGuid)),
      FileUtil::JoinPath(
          home,
          FileUtil::JoinPath(
              ".var/app/org.onlyoffice.desktopeditors/data/onlyoffice/"
              "desktopeditors/sdkjs-plugins",
              kPluginGuid)),
  };
  std::string fallback;
  for (const std::string &candidate : candidates) {
    if (!FileUtil::DirectoryExists(candidate).ok()) continue;
    if (FileUtil::FileExists(FileUtil::JoinPath(candidate, "config.json"))
            .ok() ||
        FileUtil::FileExists(FileUtil::JoinPath(candidate, "index.html"))
            .ok()) {
      return candidate;
    }
    if (fallback.empty()) fallback = candidate;
  }
  return fallback.empty() ? candidates[0] : fallback;
#else
  return "";
#endif
#endif  // _WIN32
}

namespace {

void AppendOnlyOfficeHandoffLog(std::string_view line) {
  const std::string path = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), "composer_onlyoffice.log");
  const absl::StatusOr<std::string> previous = FileUtil::GetContents(path);
  const std::string body =
      previous.ok() ? absl::StrCat(*previous, line, "\n")
                    : absl::StrCat(line, "\n");
  // Keep the log short so it stays readable while debugging Desktop hand-off.
  constexpr size_t kMaxLogBytes = 32 * 1024;
  const std::string trimmed =
      body.size() > kMaxLogBytes ? body.substr(body.size() - kMaxLogBytes)
                                 : body;
  (void)FileUtil::SetContents(path, trimmed);
}

}  // namespace

absl::Status WriteOnlyOfficeHandoff(std::string_view directory,
                                    std::string_view svg, int64_t id_ms,
                                    std::string_view text) {
  if (directory.empty()) {
    AppendOnlyOfficeHandoffLog(
        absl::StrCat("skip id=", id_ms, " reason=empty_plugin_directory"));
    return absl::OkStatus();
  }
  if (!FileUtil::DirectoryExists(directory).ok()) {
    AppendOnlyOfficeHandoffLog(absl::StrCat(
        "skip id=", id_ms, " reason=missing_plugin_directory path=", directory));
    return absl::OkStatus();
  }
  if (svg.empty() && text.empty()) {
    AppendOnlyOfficeHandoffLog(
        absl::StrCat("skip id=", id_ms, " reason=empty_svg_and_text"));
    return absl::OkStatus();
  }
  const std::string out =
      FileUtil::JoinPath(directory, "composer_result.js");
  const absl::Status status =
      FileUtil::SetContents(out, OnlyOfficeHandoffScript(svg, id_ms, text));
  if (status.ok()) {
    const std::string line = absl::StrCat(
        "wrote id=", id_ms, " svg_bytes=", svg.size(),
        " text_bytes=", text.size(), " path=", out);
    AppendOnlyOfficeHandoffLog(line);
    // Also drop a one-line sidecar next to the plugin so Desktop debugging
    // does not depend on the IME profile directory.
    (void)FileUtil::SetContents(FileUtil::JoinPath(directory, "composer_handoff_last.txt"),
                                absl::StrCat(line, "\n"));
  } else {
    AppendOnlyOfficeHandoffLog(absl::StrCat("write_failed id=", id_ms,
                                            " path=", out, " status=",
                                            status.ToString()));
  }
  return status;
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
