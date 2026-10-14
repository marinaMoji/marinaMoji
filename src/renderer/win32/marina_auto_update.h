// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: self-hosted auto-update check for Windows, on the same model
// as mac/marina_auto_update.mm -- poll GitHub Releases, throttled to once
// per 24h, offer a one-click download-and-launch of the installer. Nothing
// here depends on the Microsoft Store; it is a plain HTTPS GET against
// api.github.com plus a shell "open" of the downloaded file. See
// CHANGELOG.md for why the Store was ruled out for this project and why
// that has no bearing on this mechanism.
//
// Hosted in marinamoji_sync.exe (the per-user Task Scheduler logon task), and
// deliberately NOT in:
// - the renderer: it is started through WinSandbox at low integrity inside a
//   locked-down job (ActiveProcessLimit = 1), so it cannot launch the
//   downloaded installer or open a browser. The update offer used to live
//   there and its buttons could never work.
// - the TIP: that DLL loads into every application that takes text input
//   (Notepad, Chrome, banking software, sandboxed app-container processes), so
//   network I/O and dialogs there are unsafe.
// The sync daemon runs as the user at normal integrity for the whole session,
// so it can show the offer and start the MSI. Because the process has no
// comctl32 v6 manifest, the dialogs use MessageBoxW, never TaskDialogIndirect.
// The 24h throttle in base/marina_update_throttle.h keeps the periodic
// re-check cheap, and a check that cannot complete does not consume it.

#ifndef MOZC_RENDERER_WIN32_MARINA_AUTO_UPDATE_H_
#define MOZC_RENDERER_WIN32_MARINA_AUTO_UPDATE_H_

namespace mozc {
namespace renderer {
namespace win32 {

// Starts a background thread that periodically calls the throttled update
// check for as long as the hosting process lives. Reads
// auto_check_for_updates / include_unstable_updates from
// config::ConfigHandler::GetSharedConfig() itself on each wake, so it always
// sees the current setting without the caller needing to thread config
// through. Safe to call once at startup of the hosting process; idempotent
// if called again (a second call is a no-op).
void StartMarinaAutoUpdateChecker();

// Signals the background thread to stop and returns without waiting for it.
// Detaches rather than joins: the thread may be mid-flight in network I/O
// with timeouts of several tens of seconds, and process shutdown must not
// block on that. Call before exit.
void StopMarinaAutoUpdateChecker();

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_MARINA_AUTO_UPDATE_H_
