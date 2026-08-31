// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "unix/ibus/ibus_debug_log.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

#include <unistd.h>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/version.h"

namespace mozc {
namespace ibus {
namespace {

constexpr char kDebugLogEnv[] = "MARINAMOJI_IBUS_DEBUG_LOG";
constexpr char kEchoBackShiftLEnv[] = "MARINAMOJI_IBUS_ECHO_BACK_SHIFT_L";

const char* DebugLogPath() {
  const char* path = ::getenv(kDebugLogEnv);
  if (path == nullptr || path[0] == '\0') {
    return nullptr;
  }
  return path;
}

std::mutex& LogMutex() {
  static auto* mutex = new std::mutex;
  return *mutex;
}

std::string NowForLog() {
  return absl::FormatTime("%Y-%m-%dT%H:%M:%E*S%Ez", absl::Now(),
                          absl::LocalTimeZone());
}

bool EqualsIgnoreCase(const char* value, const char* literal) {
  while (*value != '\0' && *literal != '\0') {
    if (std::tolower(static_cast<unsigned char>(*value)) !=
        std::tolower(static_cast<unsigned char>(*literal))) {
      return false;
    }
    ++value;
    ++literal;
  }
  return *value == '\0' && *literal == '\0';
}

bool EnvVarIsTruthy(const char* name) {
  const char* value = ::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  if (value[0] == '0' && value[1] == '\0') {
    return false;
  }
  if (EqualsIgnoreCase(value, "false") || EqualsIgnoreCase(value, "no") ||
      EqualsIgnoreCase(value, "off")) {
    return false;
  }
  return true;
}

const char* EnvOrDash(const char* name) {
  const char* value = ::getenv(name);
  return (value != nullptr && value[0] != '\0') ? value : "-";
}

void AppendLogLineUnlocked(const char* path, const char* tag,
                           const char* message) {
  FILE* file = std::fopen(path, "a");
  if (file == nullptr) {
    return;
  }
  std::fprintf(file, "%s\tpid=%ld\t%s\t%s\n", NowForLog().c_str(),
               static_cast<long>(::getpid()), tag != nullptr ? tag : "-",
               message);
  std::fclose(file);
}

void MaybeWriteSessionBannerUnlocked(const char* path) {
  static bool banner_written = false;
  if (banner_written) {
    return;
  }
  banner_written = true;

  char message[1024];
  std::snprintf(
      message, sizeof(message),
      "session_start version=%s echo_back_shift_l=%d "
      "XDG_SESSION_TYPE=%s WAYLAND_DISPLAY=%s DISPLAY=%s GDK_BACKEND=%s",
      Version::GetMozcVersion().c_str(),
      ShouldForwardEchoBackShiftLRelease() ? 1 : 0, EnvOrDash("XDG_SESSION_TYPE"),
      EnvOrDash("WAYLAND_DISPLAY"), EnvOrDash("DISPLAY"),
      EnvOrDash("GDK_BACKEND"));
  AppendLogLineUnlocked(path, "engine.lifecycle", message);
}

}  // namespace

bool IsIbusDebugLogEnabled() { return DebugLogPath() != nullptr; }

bool ShouldForwardEchoBackShiftLRelease() {
  return EnvVarIsTruthy(kEchoBackShiftLEnv);
}

void MaybeLogIbusDebug(const char* tag, const char* format, ...) {
  const char* path = DebugLogPath();
  if (path == nullptr) {
    return;
  }

  char message[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  std::lock_guard<std::mutex> lock(LogMutex());
  MaybeWriteSessionBannerUnlocked(path);
  AppendLogLineUnlocked(path, tag, message);
}

}  // namespace ibus
}  // namespace mozc
