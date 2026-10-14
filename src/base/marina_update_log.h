// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// A tiny diagnostic log for the update checkers, independent of absl logging.
//
// Release builds compile with ABSL_MIN_LOG_LEVEL=100 (see .bazelrc), which turns
// every LOG(...) statement into a no-op, and RegisterLogFileSink() is compiled
// out under NDEBUG. So no release binary writes any log at all, and an update
// check that silently fails leaves nothing to debug. This writes explicit lines
// to <profile dir>/marina_update.log (and to stderr where something captures it,
// e.g. the macOS LaunchDaemon's StandardErrorPath).
//
// Deliberately minimal and low volume: one line per decision or failure, never
// user text. The file is capped at 64 KB (oldest lines are dropped).

#ifndef MOZC_BASE_MARINA_UPDATE_LOG_H_
#define MOZC_BASE_MARINA_UPDATE_LOG_H_

#include "absl/strings/string_view.h"

namespace mozc {

// Appends "YYYY-MM-DD HH:MM:SS  <message>" to marina_update.log. Thread-safe;
// never throws and never fails the caller.
void MarinaUpdateLog(absl::string_view message);

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_UPDATE_LOG_H_
