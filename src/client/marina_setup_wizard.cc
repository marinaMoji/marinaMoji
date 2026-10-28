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
#include <filesystem>  // NOLINT(build/c++17)
#include <string>
#include <system_error>  // NOLINT(build/c++11)

#include "absl/log/log.h"
#include "base/config_file_stream.h"
#include "base/system_util.h"
#include "client/client_interface.h"

namespace mozc::client {
namespace {

// The marker is a directory, not a file: creating a directory is an atomic
// "create if absent" on every platform, so exactly one process (the IME is
// loaded into many processes at once on Windows) wins the right to show the
// wizard.
constexpr char kShownMarkerName[] = "setup_wizard_shown";

// Files that only exist once the IME has actually been used. Any of them marks
// a profile that predates the wizard. config1.db alone is not enough: it is
// written only when Preferences are saved.
constexpr const char* kExistingProfileFiles[] = {
    "user://config1.db",
    "user://user_dictionary.db",
    "user://history.db",
    "user://cform.db",
};

// True once this process knows the wizard is decided (shown, grandfathered or
// claimed by another process), so later activations do no file I/O.
std::atomic<bool> g_decided{false};

std::filesystem::path Utf8Path(const std::string& s) {
  return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

std::filesystem::path MarkerPath() {
  return Utf8Path(SystemUtil::GetUserProfileDirectory()) / kShownMarkerName;
}

bool MarkerPresent() {
  std::error_code ec;
  return std::filesystem::exists(MarkerPath(), ec);
}

bool ExistingProfile() {
  for (const char* uri : kExistingProfileFiles) {
    const std::string path = ConfigFileStream::GetFileName(uri);
    if (path.empty()) continue;
    std::error_code ec;
    if (std::filesystem::exists(Utf8Path(path), ec)) return true;
  }
  return false;
}

enum class Claim { kWon, kLost, kError };

Claim ClaimMarker() {
  std::error_code ec;
  const bool created = std::filesystem::create_directory(MarkerPath(), ec);
  if (ec) return Claim::kError;
  return created ? Claim::kWon : Claim::kLost;
}

void ReleaseMarker() {
  std::error_code ec;
  std::filesystem::remove(MarkerPath(), ec);
}

}  // namespace

bool IsSetupWizardPending() {
  if (g_decided.load(std::memory_order_relaxed)) {
    return false;
  }
  if (MarkerPresent()) {
    g_decided.store(true, std::memory_order_relaxed);
    return false;
  }
  return true;
}

void MaybeLaunchSetupWizard(ClientInterface* client) {
  if (client == nullptr || !IsSetupWizardPending()) {
    return;
  }

  const bool existing_profile = ExistingProfile();
  switch (ClaimMarker()) {
    case Claim::kLost:
      g_decided.store(true, std::memory_order_relaxed);
      return;
    case Claim::kError:
      // Cannot record the decision, so never show the wizard: better to skip
      // it than to show it on every activation.
      return;
    case Claim::kWon:
      break;
  }
  g_decided.store(true, std::memory_order_relaxed);

  if (existing_profile) {
    // Existing install / profile: never show the first-run wizard.
    return;
  }
  if (!client->LaunchTool("setup_wizard", "")) {
    LOG(WARNING) << "Failed to launch setup_wizard; will retry later";
    ReleaseMarker();
    g_decided.store(false, std::memory_order_relaxed);
  }
}

void ResetSetupWizardLaunchStateForTesting() {
  g_decided.store(false, std::memory_order_relaxed);
  ReleaseMarker();
}

bool IsSetupWizardShownMarkerPresentForTesting() { return MarkerPresent(); }

}  // namespace mozc::client
