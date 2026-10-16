// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "base/marina_update_cleanup.h"

#include <filesystem>
#include <string>
#include <system_error>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "base/marina_semver.h"

namespace mozc {
namespace {

constexpr absl::string_view kPrefix = "marinaMoji-update-";

}  // namespace

bool IsStaleMarinaUpdateInstaller(absl::string_view filename,
                                  absl::string_view running_version) {
  if (!absl::StartsWith(filename, kPrefix)) {
    return false;
  }
  absl::string_view tag = filename.substr(kPrefix.size());
  if (absl::EndsWith(tag, ".msi") || absl::EndsWith(tag, ".pkg")) {
    tag.remove_suffix(4);
  } else {
    return false;
  }
  if (!ParseMarinaSemVer(tag).has_value() ||
      !ParseMarinaSemVer(running_version).has_value()) {
    return false;
  }
  // Stale unless the file's release is newer than what is running.
  return !MarinaSemVerLess(running_version, tag);
}

int RemoveStaleMarinaUpdateInstallers(const std::string& dir,
                                      absl::string_view running_version) {
  std::error_code ec;
  // path(std::u8string) so a non-ASCII profile directory works on Windows,
  // where a narrow std::string path would use the ANSI code page.
  const std::filesystem::path root(std::u8string(
      reinterpret_cast<const char8_t*>(dir.data()), dir.size()));
  std::filesystem::directory_iterator it(root, ec);
  if (ec) {
    return 0;
  }
  int removed = 0;
  for (const std::filesystem::directory_iterator end; it != end;
       it.increment(ec)) {
    if (ec) {
      break;
    }
    const std::u8string name = it->path().filename().u8string();
    const std::string utf8_name(reinterpret_cast<const char*>(name.data()),
                                name.size());
    if (!IsStaleMarinaUpdateInstaller(utf8_name, running_version)) {
      continue;
    }
    if (FileUtil::UnlinkIfExists(FileUtil::JoinPath(dir, utf8_name)).ok()) {
      ++removed;
    }
  }
  return removed;
}

}  // namespace mozc
