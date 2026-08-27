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
//   Windows: OutputDebugString. View with Sysinternals DebugView, with
//     "Capture > Capture Win32" enabled ("Capture Global Win32" too if the
//     target application runs elevated). Works in every process, including
//     marinamoji_tip64.dll, which is loaded into arbitrary host applications
//     that never call InitMozc and so has no log file even in a debug build.
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

inline void MarinaDebugLog(absl::string_view line) {
  const std::string text = absl::StrCat("[marinaMoji] ", line, "\n");
#ifdef _WIN32
  ::OutputDebugStringA(text.c_str());
#else   // _WIN32
  std::fputs(text.c_str(), stderr);
#endif  // _WIN32
}

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_DEBUG_LOG_H_
