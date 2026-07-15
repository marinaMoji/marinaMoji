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

#include "win32/base/config_snapshot.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/key_info_util.h"

namespace mozc {
namespace win32 {
namespace {

using ::mozc::config::Config;

constexpr size_t kMaxDirectModeKeys = 128;

struct StaticConfigSnapshot {
  bool use_kana_input;
  bool use_keyboard_to_change_preedit_method;
  bool use_mode_indicator;
  size_t num_direct_mode_keys;
  KeyInformation direct_mode_keys[kMaxDirectModeKeys];
  config::MarinaKeyboardLayout marina_keyboard_layout;
};

StaticConfigSnapshot GetConfigSnapshotImpl() {
  std::shared_ptr<const Config> config =
      config::ConfigHandler::GetSharedConfig();

  StaticConfigSnapshot snapshot = {};
  snapshot.use_kana_input = (config->preedit_method() == Config::KANA);
  snapshot.use_keyboard_to_change_preedit_method =
      config->use_keyboard_to_change_preedit_method();
  snapshot.use_mode_indicator = config->use_mode_indicator();
  snapshot.marina_keyboard_layout = config->marina_keyboard_layout();

  const auto& direct_mode_keys =
      KeyInfoUtil::ExtractSortedDirectModeKeys(*config);
  const size_t size_to_be_copied =
      std::min(direct_mode_keys.size(), kMaxDirectModeKeys);
  snapshot.num_direct_mode_keys = size_to_be_copied;
  for (size_t i = 0; i < size_to_be_copied; ++i) {
    snapshot.direct_mode_keys[i] = direct_mode_keys[i];
  }

  return snapshot;
}

}  // namespace

ConfigSnapshot::Info::Info()
    : use_kana_input(false),
      use_keyboard_to_change_preedit_method(false),
      use_mode_indicator(false),
      marina_keyboard_layout(config::MARINA_KBD_OS_DEFAULT) {}

namespace {

// marinaMoji: unlike upstream, the snapshot is refreshable: it is rebuilt
// whenever config1.db's modification time changes, so a config change made in
// the config dialog takes effect in already-running applications the next
// time the snapshot is queried (TipPrivateContext::EnsureInitialized runs on
// every thread-focus event). In processes that cannot stat the config file
// (e.g. AppContainer sandboxes), the stat fails and the cached snapshot is
// kept, which matches the old load-once behavior.
struct SnapshotCache {
  absl::Mutex mutex;
  bool initialized ABSL_GUARDED_BY(mutex) = false;
  FileTimeStamp config_mtime ABSL_GUARDED_BY(mutex) = 0;
  StaticConfigSnapshot snapshot ABSL_GUARDED_BY(mutex);
};

SnapshotCache* GetSnapshotCache() {
  static SnapshotCache* cache = new SnapshotCache();
  return cache;
}

std::string GetConfigFilePath() {
  // Despite its name, GetConfigFileNameForTesting simply returns the current
  // config file name (e.g. "user://config1.db"); GetFileName resolves it to
  // an actual file path.
  return ConfigFileStream::GetFileName(
      config::ConfigHandler::GetConfigFileNameForTesting());
}

}  // namespace

// static
bool ConfigSnapshot::Get(Info* info) {
  SnapshotCache* cache = GetSnapshotCache();
  absl::MutexLock lock(&cache->mutex);

  const absl::StatusOr<FileTimeStamp> mtime =
      FileUtil::GetModificationTime(GetConfigFilePath());
  if (!cache->initialized) {
    cache->snapshot = GetConfigSnapshotImpl();
    cache->config_mtime = mtime.ok() ? *mtime : 0;
    cache->initialized = true;
  } else if (mtime.ok() && *mtime != cache->config_mtime) {
    // The config file has been rewritten since the last snapshot;
    // ConfigHandler caches the parsed config, so force a reload first.
    config::ConfigHandler::Reload();
    cache->snapshot = GetConfigSnapshotImpl();
    cache->config_mtime = *mtime;
  }

  const StaticConfigSnapshot& cached_snapshot = cache->snapshot;
  info->use_kana_input = cached_snapshot.use_kana_input;
  info->use_keyboard_to_change_preedit_method =
      cached_snapshot.use_keyboard_to_change_preedit_method;
  info->use_mode_indicator = cached_snapshot.use_mode_indicator;
  info->marina_keyboard_layout = cached_snapshot.marina_keyboard_layout;
  info->direct_mode_keys.resize(cached_snapshot.num_direct_mode_keys);
  for (size_t i = 0; i < cached_snapshot.num_direct_mode_keys; ++i) {
    info->direct_mode_keys[i] = cached_snapshot.direct_mode_keys[i];
  }
  return true;
}

}  // namespace win32
}  // namespace mozc
