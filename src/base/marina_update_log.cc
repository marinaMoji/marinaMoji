// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "base/marina_update_log.h"

#include <cstddef>
#include <cstdio>
#include <ios>
#include <mutex>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace {

constexpr char kLogFileName[] = "marina_update.log";
constexpr size_t kMaxBytes = 64 * 1024;
constexpr size_t kKeepBytes = 32 * 1024;

}  // namespace

void MarinaUpdateLog(absl::string_view message) {
  static std::mutex* const mutex = new std::mutex();
  std::lock_guard<std::mutex> lock(*mutex);

  const std::string line = absl::StrCat(
      absl::FormatTime("%Y-%m-%d %H:%M:%S", absl::Now(),
                       absl::LocalTimeZone()),
      "  ", message, "\n");
#ifndef _WIN32
  // Captured by launchd for the macOS daemon; the Windows binaries that use
  // this are GUI-subsystem processes with no stderr.
  std::fputs(line.c_str(), stderr);
#endif  // !_WIN32

  const std::string path =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), kLogFileName);
  std::string body;
  if (auto existing = FileUtil::GetContents(path, std::ios::in);
      existing.ok()) {
    body = *std::move(existing);
  }
  body += line;
  if (body.size() > kMaxBytes) {
    body.erase(0, body.size() - kKeepBytes);
    // Start at a line boundary.
    const size_t newline = body.find('\n');
    if (newline != std::string::npos) {
      body.erase(0, newline + 1);
    }
  }
  FileUtil::SetContents(path, body).IgnoreError();
}

}  // namespace mozc
