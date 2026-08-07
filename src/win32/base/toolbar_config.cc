// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "win32/base/toolbar_config.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc::win32 {
namespace {

constexpr char kToolbarVisibleKey[] = "toolbar_visible";

// LoadToolbarVisiblePreference() is called from FillVisibility() on every
// renderer update -- i.e. per keystroke, on the TSF thread, inside the host
// application's process (Word, a browser, ...). Reading the file each time
// puts synchronous disk I/O in the input hot path.
//
// The value is therefore cached, but only briefly: the TIP DLL is loaded into
// every application separately, so toggling the toolbar in one application
// must still reach the others. A time-to-live keeps that propagation (at
// worst kCacheTtlMsec late, which is imperceptible for a visibility toggle)
// while cutting the reads from one per keystroke to at most one per second.
// A write through SaveToolbarVisiblePreference() below updates the cache
// immediately, so the process the user actually toggled it in never waits.
//
// std::atomic because TSF creates one UI thread per application UI thread,
// each with its own text service instance.
constexpr uint64_t kCacheTtlMsec = 1000;
std::atomic<uint64_t> g_cache_expiry_tick{0};
std::atomic<bool> g_cached_visible{true};

void PublishToCache(bool visible) {
  g_cached_visible.store(visible, std::memory_order_relaxed);
  g_cache_expiry_tick.store(::GetTickCount64() + kCacheTtlMsec,
                            std::memory_order_release);
}

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

namespace {

bool ReadToolbarVisiblePreference() {
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

}  // namespace

bool LoadToolbarVisiblePreference() {
  if (::GetTickCount64() <
      g_cache_expiry_tick.load(std::memory_order_acquire)) {
    return g_cached_visible.load(std::memory_order_relaxed);
  }
  const bool visible = ReadToolbarVisiblePreference();
  PublishToCache(visible);
  return visible;
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
  const bool saved = FileUtil::SetContents(path, serialized).ok();

  // Publish the new value even when the write failed: the user asked for it,
  // and a stale cache would ignore the request for a further second.
  PublishToCache(visible);
  return saved;
}

}  // namespace mozc::win32
