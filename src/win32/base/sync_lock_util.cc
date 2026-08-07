// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "win32/base/sync_lock_util.h"

#include <windows.h>

#include <atomic>
#include <cstdint>

#include "base/file_util.h"
#include "sync/sync_config.h"
#include "sync/sync_status.h"

namespace mozc::win32 {
namespace {

// Matches the 250 ms poll interval the mac and Linux overlays use, so the
// worst-case delay before keys start being blocked is the same on all three
// platforms. Short enough that a lock taken mid-typing is caught almost
// immediately; long enough that a fast typist causes ~4 file reads/second
// rather than one per keystroke.
constexpr uint64_t kCacheTtlMsec = 250;

// Users who have never opened the Sync tab in Preferences have no
// sync.conf, so IsSyncLockActive() can never be true for them -- but
// without this gate every one of them would still pay the same ~4
// sync.status.json opens/second while typing, on the TSF thread, inside
// every application. sync.conf is only written by explicit user action in
// the config dialog, so a much longer TTL is fine here; it just needs to
// notice within a few seconds of the user enabling sync for the first time.
constexpr uint64_t kConfiguredCacheTtlMsec = 10000;

// Minimum gap between beeps, mirroring the 1 s throttle in
// SyncOverlayFlashBlockedInput() on mac and Linux.
constexpr uint64_t kBeepThrottleMsec = 1000;

std::atomic<uint64_t> g_cache_expiry_tick{0};
std::atomic<bool> g_cached_active{false};
std::atomic<uint64_t> g_last_beep_tick{0};
std::atomic<uint64_t> g_configured_cache_expiry_tick{0};
std::atomic<bool> g_cached_configured{false};

bool IsSyncConfigured() {
  const uint64_t now = ::GetTickCount64();
  if (now < g_configured_cache_expiry_tick.load(std::memory_order_acquire)) {
    return g_cached_configured.load(std::memory_order_relaxed);
  }
  const bool configured =
      mozc::FileUtil::FileExists(mozc::sync::GetSyncConfigPath()).ok();
  g_cached_configured.store(configured, std::memory_order_relaxed);
  g_configured_cache_expiry_tick.store(now + kConfiguredCacheTtlMsec,
                                       std::memory_order_release);
  return configured;
}

}  // namespace

bool IsSyncLockActive() {
  if (!IsSyncConfigured()) {
    return false;
  }
  const uint64_t now = ::GetTickCount64();
  if (now < g_cache_expiry_tick.load(std::memory_order_acquire)) {
    return g_cached_active.load(std::memory_order_relaxed);
  }
  // IsSyncRunning() also treats a status file older than 10 minutes as stale,
  // so a crashed sync process can't wedge input permanently.
  const bool active = mozc::sync::IsSyncRunning();
  g_cached_active.store(active, std::memory_order_relaxed);
  g_cache_expiry_tick.store(now + kCacheTtlMsec, std::memory_order_release);
  return active;
}

void NotifySyncBlockedInput() {
  const uint64_t now = ::GetTickCount64();
  const uint64_t last = g_last_beep_tick.load(std::memory_order_relaxed);
  if (last != 0 && now - last < kBeepThrottleMsec) {
    return;
  }
  g_last_beep_tick.store(now, std::memory_order_relaxed);
  // Asynchronous: never block the TSF thread on the audio stack.
  ::MessageBeep(MB_ICONWARNING);
}

void InvalidateSyncLockCache() {
  g_cache_expiry_tick.store(0, std::memory_order_release);
}

}  // namespace mozc::win32
