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

#include "client/marina_setup_wizard.h"

#include <atomic>
#include <string>

#include "absl/log/log.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "client/client_interface.h"

namespace mozc::client {
namespace {

constexpr char kShownMarkerFileName[] = "setup_wizard_shown";
constexpr char kConfigFileUri[] = "user://config1.db";

std::atomic<bool> g_launch_in_flight{false};

std::string ShownMarkerPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kShownMarkerFileName);
}

bool MarkerPresent() { return FileUtil::FileExists(ShownMarkerPath()).ok(); }

void WriteMarker() {
  FileUtil::SetContents(ShownMarkerPath(), "1").IgnoreError();
}

bool ConfigFilePresent() {
  const std::string path = ConfigFileStream::GetFileName(kConfigFileUri);
  if (path.empty()) {
    return false;
  }
  return FileUtil::FileExists(path).ok();
}

}  // namespace

void MaybeLaunchSetupWizard(ClientInterface* client) {
  if (client == nullptr) {
    return;
  }
  if (MarkerPresent()) {
    return;
  }
  if (ConfigFilePresent()) {
    // Existing install / profile: never show the first-run wizard.
    WriteMarker();
    return;
  }

  // One attempt at a time in this process so rapid activate/Enable storms do
  // not spawn multiple wizards before the marker is written.
  bool expected = false;
  if (!g_launch_in_flight.compare_exchange_strong(expected, true)) {
    return;
  }

  if (MarkerPresent()) {
    g_launch_in_flight.store(false);
    return;
  }

  if (!client->LaunchTool("setup_wizard", "")) {
    LOG(WARNING) << "Failed to launch setup_wizard; will retry later";
    g_launch_in_flight.store(false);
    return;
  }

  WriteMarker();
  // Leave g_launch_in_flight set: marker is the durable guard; this stops
  // another LaunchTool before the filesystem write is visible.
}

void ResetSetupWizardLaunchStateForTesting() {
  g_launch_in_flight.store(false);
  FileUtil::UnlinkIfExists(ShownMarkerPath()).IgnoreError();
}

bool IsSetupWizardShownMarkerPresentForTesting() { return MarkerPresent(); }

}  // namespace mozc::client
