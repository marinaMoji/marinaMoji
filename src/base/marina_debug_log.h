// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: TEMPORARY diagnostics that survive the builds we actually test.
//
// LOG() is useless in a CI artifact. .bazelrc's release_build config passes
// --compilation_mode=opt, which defines NDEBUG, and base/log_file.cc registers
// its file sink only when NDEBUG is absent -- so there is no log file. It also
// passes -DABSL_MIN_LOG_LEVEL=100, which makes absl discard every LOG
// statement at compile time, so the calls do not even exist in the binary.
//
// This writes somewhere that survives both:
//   Windows: OutputDebugString (DebugView) *and* an append-only file
//     %TEMP%\marinamoji-debug.log. DebugView's File→Save As often crashes
//     under a busy capture; the file is the reliable copy. The TIP lives
//     inside Notepad/Word and often cannot write Mozc's normal log path.
//   elsewhere: stderr.
//
// Keep the text ASCII: OutputDebugStringA is passed through unconverted.
//
// Delete this header and its call sites once the current investigation is
// closed; grep for MarinaDebugLog.

#ifndef MOZC_BASE_MARINA_DEBUG_LOG_H_
#define MOZC_BASE_MARINA_DEBUG_LOG_H_

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

#ifdef _WIN32
#include <windows.h>
#else  // _WIN32
#include <cstdio>
#endif  // _WIN32

namespace mozc {

#ifdef _WIN32
inline void MarinaDebugLogAppendFile(absl::string_view text) {
  char temp_dir[MAX_PATH] = {};
  const DWORD n = ::GetTempPathA(MAX_PATH, temp_dir);
  if (n == 0 || n >= MAX_PATH) {
    return;
  }
  const std::string path = absl::StrCat(temp_dir, "marinamoji-debug.log");
  const HANDLE file = ::CreateFileA(
      path.c_str(), FILE_APPEND_DATA,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD written = 0;
  ::WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written,
              nullptr);
  ::CloseHandle(file);
}
#endif  // _WIN32

inline void MarinaDebugLog(absl::string_view line) {
  const std::string text = absl::StrCat("[marinaMoji] ", line, "\n");
#ifdef _WIN32
  ::OutputDebugStringA(text.c_str());
  MarinaDebugLogAppendFile(text);
#else   // _WIN32
  std::fputs(text.c_str(), stderr);
#endif  // _WIN32
}

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_DEBUG_LOG_H_
