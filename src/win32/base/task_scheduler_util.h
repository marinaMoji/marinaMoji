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

#ifndef MOZC_WIN32_BASE_TASK_SCHEDULER_UTIL_H_
#define MOZC_WIN32_BASE_TASK_SCHEDULER_UTIL_H_

#include <windows.h>

#include <string_view>

namespace mozc {
namespace win32 {

// Registers/unregisters a per-user, logon-triggered Task Scheduler task that
// runs a long-lived process (marinamoji_sync.exe --daemon). This mirrors the
// LaunchAgent (macOS) / systemd --user unit (Linux) process model for the
// sync daemon: it runs in the interactive user's own session so it can read
// %LOCALAPPDATA%\marinaMoji\sync.conf and reach whatever folder (Nextcloud,
// Syncthing, OneDrive, ...) the user picked, none of which a LocalSystem
// service could access without impersonation tricks. Unlike a Windows
// Service (see win32/cache_service/), no elevation or SCM registration is
// needed; the task is registered under the installing user's own account.
class TaskSchedulerUtil {
 public:
  TaskSchedulerUtil() = delete;
  TaskSchedulerUtil(const TaskSchedulerUtil&) = delete;
  TaskSchedulerUtil& operator=(const TaskSchedulerUtil&) = delete;

  // Registers (or, on repair/upgrade, overwrites) a task named |task_name|
  // that launches |exe_path| with |args| shortly after the user logs on, and
  // restarts it a few times if it exits with an error. The caller is
  // responsible for initializing COM (CoInitialize/CoInitializeEx) before
  // calling this function.
  static HRESULT RegisterLogonTask(std::wstring_view task_name,
                                   std::wstring_view exe_path,
                                   std::wstring_view args);

  // Removes the task named |task_name| if present. Never treats "task did
  // not exist" as an error. The caller is responsible for initializing COM
  // before calling this function.
  static HRESULT UnregisterTask(std::wstring_view task_name);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_TASK_SCHEDULER_UTIL_H_
