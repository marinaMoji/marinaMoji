// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: sync-lock awareness for the Windows TIP.
//
// While marinaMojiSync is rewriting user data it takes a global lock on the
// session handler (sync/sync_runner.cc -> Client::BeginSyncLock), after which
// SessionHandler::SendKey/TestSendKey/SendCommand all fail with
// Output::SYNC_LOCKED and, crucially, no |consumed| field. Without an
// explicit guard the Windows key path treats that as "the IME did not handle
// this key" and hands the keystroke to the application, so typing during a
// sync inserts raw romaji into the user's document.
//
// mac and Linux avoid this with a poller over sync.status.json
// (mac/sync_overlay.mm, unix/ibus/sync_overlay.cc); this is the equivalent
// for the TIP. The visible overlay lives in the renderer
// (renderer/win32/sync_overlay_window.cc) because the TIP DLL is loaded into
// every application process, so a window owned here would be created
// per-process -- the same reasoning as the floating toolbar.

#ifndef MOZC_WIN32_BASE_SYNC_LOCK_UTIL_H_
#define MOZC_WIN32_BASE_SYNC_LOCK_UTIL_H_

namespace mozc::win32 {

// True while marinaMojiSync reports state=running, i.e. while keystrokes must
// not reach the session. Result is cached briefly (see the .cc) because this
// is called on every key event, on the TSF thread, inside the host
// application's process -- reading sync.status.json each time would put
// synchronous disk I/O in the input hot path.
bool IsSyncLockActive();

// Audible feedback for a keystroke that was dropped because of the lock,
// rate-limited so holding a key down doesn't machine-gun the beeper. Mirrors
// the NSBeep / gdk_display_beep in the mac and Linux overlays.
void NotifySyncBlockedInput();

// Drops the cached value, so the next IsSyncLockActive() re-reads the status
// file. Called when the session reports SYNC_LOCKED, which means a lock was
// taken between polls.
void InvalidateSyncLockCache();

}  // namespace mozc::win32

#endif  // MOZC_WIN32_BASE_SYNC_LOCK_UTIL_H_
