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
// Deliberately hosted in the renderer rather than the TIP:
// - The renderer is a single marinaMoji-owned process with a manifest we
//   control (mozc_renderer.exe.manifest already declares the comctl32 v6
//   dependency that TaskDialogIndirect requires). The TIP DLL loads
//   in-process into every application that takes text input -- Notepad,
//   Chrome, banking software, sandboxed app-container processes -- and we
//   control none of their manifests, so a v6-only API there would be
//   unreliable at best. See the MSIX/IME research in CHANGELOG.md for the
//   broader pattern of what a TIP can and cannot safely do.
// - It also keeps the network stack (WinHTTP) out of a component that runs
//   inside arbitrary, possibly low-integrity host processes.
//
// Because of that, the trigger is "the renderer is running" (checked once
// at WindowManager::Initialize, i.e. renderer startup) plus a periodic
// re-check on a background thread for the case where the renderer stays
// alive across a day boundary -- rather than mac's per-activation hook,
// which has no safe Windows equivalent. The 24h throttle in
// base/marina_update_throttle.h makes both call sites cheap no-ops outside
// their window, so this is not "more checking," just an adaptation of the
// trigger to how the Windows renderer process actually lives.

#ifndef MOZC_RENDERER_WIN32_MARINA_AUTO_UPDATE_H_
#define MOZC_RENDERER_WIN32_MARINA_AUTO_UPDATE_H_

namespace mozc {
namespace renderer {
namespace win32 {

// Starts a background thread that periodically calls the throttled update
// check for as long as the renderer process lives. Reads
// auto_check_for_updates / include_unstable_updates from
// config::ConfigHandler::GetSharedConfig() itself on each wake, so it always
// sees the current setting without the caller needing to thread config
// through. Safe to call once from WindowManager::Initialize(); idempotent
// if called again (a second call is a no-op).
void StartMarinaAutoUpdateChecker();

// Signals the background thread to stop and returns without waiting for it.
// Detaches rather than joins: the thread may be mid-flight in network I/O
// with timeouts of several tens of seconds, and process shutdown must not
// block on that. Call from WindowManager::DestroyAllWindows() before exit.
void StopMarinaAutoUpdateChecker();

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_MARINA_AUTO_UPDATE_H_
