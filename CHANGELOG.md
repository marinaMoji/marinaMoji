# Changelog

Notable marinaMoji-specific changes, kept by hand. This is not upstream
Mozc's release history (see [`docs/release_history.md`](docs/release_history.md)
for that) — it tracks work specific to this fork, most usefully the parts
that touch more than one file or aren't obvious from a commit subject line.

Format: newest entry first, grouped by date. Each entry should say what
changed and, where it isn't obvious, why.

## Unreleased

### Windows: verify sender of the TIP's renderer-callback window

A third review pass (Explore agent + manual verification) found that fixing
the WM_COPYDATA UIPI-filter bug two rounds back had an unintended side
effect: `TipTextServiceImpl`'s renderer-callback window
(`win32/tip/tip_text_service.cc`) now accepts `WM_COPYDATA` from *any*
process at or below the box's integrity level, not just
`marinamoji_renderer.exe`. `kMessageReceiverClassName` and the symbol-text
copy-data tag are plain public constants (`base/const.h`), so any other
process could `FindWindow` the class and send a spoofed Symbols-Palette
"commit," inserting arbitrary text into whatever the user is currently
typing into -- a password field, an elevated installer prompt, anything.
The registered-message path (candidate clicks, mode switches) has the same
lack of sender validation but a much smaller blast radius (only affects
mozc's own in-session state), and structurally can't carry a sender handle
in-band the way `WM_COPYDATA` can -- left as a known, documented residual
gap rather than redesigned this pass.

- `renderer/renderer_server.cc`: `RendererServerSendCommand::SendCommand`'s
  `WM_COPYDATA` call now puts this process's own PID in `wParam` (was
  `nullptr`) instead of leaving it empty.
- `win32/tip/tip_text_service.cc`: new `IsTrustedRendererSender(DWORD)`
  resolves that PID's *initial* process image path via
  `WinUtil::GetProcessInitialNtPath` (NT path, so a later same-name
  substitution doesn't fool it) and compares it against
  `SystemUtil::GetRendererPath()`. `RendererCallbackWidnowProc` now requires
  this to pass, plus a 4096-byte cap on the inserted text, before calling
  `OnRendererSymbolTextCallback`.
- This is **not** a cryptographic guarantee -- a sender can claim any PID it
  likes, and if that PID happens to belong to a real
  `marinamoji_renderer.exe` (discoverable without special privilege), the
  check passes even though the claimed PID isn't actually who sent the
  message. It raises the bar from "any process, trivially" to "a process
  that already knows how to find a running renderer," which is the
  practical ceiling reachable without redesigning this channel around a
  shared secret carried over the existing (already-authenticated) TIP<->
  renderer IPC connection -- worth doing before a wider release, not done
  this pass.
- Untested against a real Windows build -- no Windows compiler available
  here, same caveat as everything else in this file.

### Windows: auto-updater downloaded-installer integrity checks

`renderer/win32/marina_auto_update.cc` downloaded the update MSI over HTTPS
and ran it straight away: no hash check against release metadata, no
signature check (the UI copy already admits the build is unsigned), and the
download loop treated `WinHttpQueryDataAvailable`/`WinHttpReadData` returning
0 as "done" even though that's indistinguishable from a connection that
dropped mid-transfer -- a truncated download could still get executed as a
"success."

- **Truncation check**: `OpenSuccessfulGet()` now also reads the response's
  `Content-Length` header; `WriteBodyToFile()` takes it as an expected byte
  count and fails if the total actually written doesn't match. `DownloadToFile()`
  deletes the partial file on that failure rather than leaving it for the
  caller to launch.
- **Hash check**: GitHub has published a `digest` (`sha256:...`) field on
  release assets since 2024. `base/marina_github_releases.{h,cc}` now parses
  it into `MarinaGitHubAsset::digest_sha256` and exposes
  `FindMarinaMsiSha256Digest()` alongside the existing
  `FindMarinaMsiDownloadUrl()` (both now share one internal asset lookup so
  they can't disagree on which asset they mean). `marina_auto_update.cc`
  hashes the downloaded file with BCrypt (`Sha256HexOfFile()`, new `bcrypt`
  win32-lib target) and deletes it on a mismatch. When GitHub hasn't
  published a digest for a given asset (older releases), the check is
  skipped rather than treated as an error -- there's nothing to compare
  against, and refusing every pre-2024 release's installer would be worse
  than not checking.
- This is **not** a substitute for code signing -- it only guards against a
  corrupted/truncated transfer, since the digest itself is fetched over the
  same GitHub API channel as the download. The SmartScreen warning in the
  install-offer dialog (from the unsigned-build decision above) is still the
  only thing standing between the user and an unverified publisher at
  present.
- Added test coverage for the new digest parsing/lookup path
  (`marina_semver_test.cc`, `marina_github_releases_test.cc`). The WinHTTP/
  BCrypt plumbing in `marina_auto_update.cc` itself is untested here, same
  caveat as everything else in this file -- no Windows compiler available.

### Windows: indicator DPI handling and installer sync-task rollback

Two more items off the same "gaps" list as the round below.

- **Indicator DPI** (`renderer/win32/indicator_window.cc`): the mode-switch
  indicator balloon read its DPI scaling once at construction
  (`GetDPIScaling()`/`dpi_scaling_`) and never again, unlike the toolbar and
  sync overlay, which both got `WM_DPICHANGED` handling in the round below.
  Effect: drag the window (or its monitor) to a display with a different
  scale factor and the indicator's sprites stay sized for the old DPI.
  Added a `WM_DPICHANGED` handler that recomputes `dpi_scaling_` from the new
  DPI and reloads all six mode sprites, mirroring `ToolbarWindow::OnDpiChanged`
  / `LoadIcons()`.
- **Installer sync-task rollback** (`win32/installer/installer_marinamoji_64bit.wxs`,
  `win32/custom_action/custom_action.{h,cc,def}`): every other state-mutating
  install step (`RegisterTIP64`, `RestoreServiceState`, ...) has a paired
  `Execute="rollback"` action, but `RegisterSyncTask` -- which creates the
  `marinamoji_sync.exe --daemon` Task Scheduler logon task -- didn't. It also
  ran as `Execute="commit"`, which sits outside the rollback transaction
  entirely. Effect: a failed or aborted install/upgrade could leave the sync
  logon task registered with nothing else installed to back it. Switched
  `RegisterSyncTask` to `Execute="deferred"` and added
  `RegisterSyncTaskRollback` (a thin wrapper around the existing
  `UnregisterSyncTask`), scheduled `Before="RegisterSyncTask"`, matching the
  `RegisterTIP64`/`RegisterTIPRollback64` pattern. Untested against a real
  MSI build -- no Windows toolchain available here.

### Windows: docs, test coverage, sync overlay, and toolbar hide follow-ups

Second pass on the same review as the bug-fix/perf entry below, working
through the "gaps" list that pass had deliberately left untouched.

- **Docs**: `docs/build_marinamoji_on_windows.md` still opened with "marinaMoji
  does not yet ship a Windows port" and pointed every command at
  `google/mozc`; fixed the banner (now points at
  [WINDOWS_PORT_PLAN.md](docs/WINDOWS_PORT_PLAN.md)) and every clone/output/
  GitHub Actions reference to use this repo and `marinaMoji64.msi`.
  `docs/WINDOWS_PORT_PLAN.md`'s status block still said Phase 4 (toolbar)
  hadn't started and that the Symbols Palette/Shortcuts buttons were
  disabled placeholders; both were true when written but false since the
  toolbar parity pass. Corrected in place (old entries kept, with an
  "Update (2026-08-07)" note) rather than rewritten, so the doc still reads
  as a history. Same treatment for the sync-QA checklist item that claimed
  "no Windows TIP-side implementation at all" for input-blocking during
  sync -- `win32/base/sync_lock_util` implements exactly that.
- **Tests**: added `renderer/win32/marina_localized_string_test.cc`, the
  first test for any of the ~3,300 lines of new Windows UI code from this
  branch. Checks that a sample of "MM.*" keys resolve to a non-empty,
  actually-translated string in all three languages, that Japanese wording
  differs from English (catches an untranslated copy-paste), and the
  unknown-key/null-key fallback paths. Broader coverage (`PickIconSizeTier`,
  `ClampToVisibleArea`, the `toolbar.conf` round-trip) would need those
  helpers exposed from anonymous-namespace/private scope first -- left as a
  follow-up rather than done as a blind refactor with no Windows compiler
  available to verify it.
- **Sync overlay multi-monitor** (`renderer/win32/sync_overlay_window.cc`):
  the "synchronising…" overlay always centred on the *primary* monitor at
  the *primary's* DPI (`GetDpiForPoint(0, 0)` / `SPI_GETWORKAREA`), so on a
  multi-monitor setup it could appear on a screen the user isn't even
  looking at. Added `TargetScreenPoint()` (center of the foreground
  window's rect) and route both the DPI lookup and
  `GetWorkingAreaFromPoint()` through it, refreshing font/layout in
  `UpdateLayout()` if the target monitor's DPI changed -- the same pattern
  the toolbar already used for its own drag handling.
- **Toolbar delayed hide** (`renderer/win32/toolbar_window.{h,cc}`): the
  toolbar hid immediately on any focus-loss signal (`ShowToolbar` bit
  clearing in `OnUpdate()`), unlike `unix/ibus`'s 150ms grace period
  (`MozcToolbarScheduleHideDelayed`). Windows 11 focus transitions (e.g.
  Alt+Tab bouncing through an intermediate window) can flicker the toolbar
  as a result. Added a matching `SchedulePendingHide()`/
  `CancelPendingHide()` pair on a `WM_TIMER`, composing with the existing
  `menu_open_`-deferred-hide logic unchanged (a still-open context menu at
  the end of the delay still defers via the existing path). The public
  `Hide()` entry point (used for a hard hide-all, e.g. renderer shutdown)
  stays immediate, matching `unix/ibus`'s split between
  `MozcToolbarHide()` (instant) and the delayed variant.

### Windows: bug fixes and hot-path perf found in a full-code review pass

A fresh review of `src/win32/` and `src/renderer/win32/` against the mac and
Linux/ibus implementations, done after the toolbar/sync/macron/auto-update
work above had all landed. Three real bugs and three hot-path perf issues
fixed; a longer list of untouched gaps (test coverage, multi-monitor sync
overlay, stale docs, etc.) recorded in ShareDocs for later triage rather
than done here. Full writeup: ShareDocs
`documentation/Implementation/Windows_Improvement_Opportunities.md`.

**Bugs:**

- `win32/tip/tip_text_service.cc`: Symbols Palette text commits travel
  renderer→TIP via `WM_COPYDATA`. A comment claimed `WM_COPYDATA` is on
  Windows' default UIPI-allowed message list and skipped the
  `ChangeMessageFilterEx` call the neighbouring registered message gets --
  backwards; `WM_COPYDATA` is MSDN's canonical example of a message that
  *needs* the filter call to cross an integrity-level boundary. Effect: every
  Kaeriten/Symbols/User palette insertion silently failed whenever the
  focused application ran elevated (admin PowerShell, an elevated editor,
  etc.), because the medium-IL renderer's message never reached the high-IL
  TIP. Odoriji was unaffected (different, already-filtered transport). Fixed
  by adding the missing `ChangeMessageFilter(..., WM_COPYDATA)` call.
- `renderer/renderer_server.cc`: the toolbar/palette/shortcuts round-trip
  added in this branch used plain `SendMessage` (synchronous) into a window
  owned by the *focused application's* process, unlike upstream's
  `PostMessage`-only design. A stalled host app (Word mid-save, Excel
  recalculating, any temporarily-hung process) would freeze the entire
  renderer UI -- toolbar, candidate window, every palette, for every
  application -- with no way back. Switched both call sites to
  `SendMessageTimeout` with `SMTO_ABORTIFHUNG` (500ms), keeping the
  synchronous ordering these commands need while bounding the damage to one
  hung app.
- Investigated and **not a bug**: "Hide toolbar" (the toolbar's own context
  menu) looked one-way at first read, since nothing on the toolbar itself
  can turn it back on. It isn't -- the TSF language bar's existing "Tool"
  menu already has a `kToolbarVisibility` entry
  (`win32/tip/tip_lang_bar.cc`/`tip_text_service.cc`) that correctly flips
  `SaveToolbarVisiblePreference(!LoadToolbarVisiblePreference())` both ways,
  predating this review. Real open question, moved to the Windows testing
  checklist rather than code: Windows 11 hides the classic language bar by
  default (consolidated into the taskbar input indicator), so this toggle's
  practical reachability needs on-hardware confirmation, not a code fix.

**Perf (all three are per-keystroke or per-update costs that scale with how
much the user types, inside the host application's process):**

- `renderer/win32/toolbar_window.cc`: `OnUpdate()` (called on every
  `RendererCommand`, i.e. every keystroke while the toolbar is visible) was
  re-reading `AppsUseLightTheme` from the registry on every call. The value
  is already tracked correctly via `WM_SETTINGCHANGE`
  (`OnSettingChange()`), so the per-update poll was pure redundancy. Removed.
- `renderer/win32/toolbar_window.{h,cc}`: `LoadIcons()` re-read and
  WIC-decoded all seven toolbar PNGs from disk on every mode/theme change
  and every re-show, so e.g. toggling hiragana↔direct hit the disk each
  time. Added `GetOrLoadCachedIcon()` + an `icon_disk_cache_` map keyed on
  the resolved icon path, so each distinct (name, size) is decoded once for
  the life of the renderer process. `icon_cache_`/`logo_cache_` changed from
  owning `wil::unique_hbitmap` to non-owning `HBITMAP` borrowed from the
  cache (ownership now lives in `icon_disk_cache_`, which outlives any
  single `LoadIcons()` call).
- `win32/base/sync_lock_util.cc`: `IsSyncLockActive()` is polled from both
  `OnTestKey` and `OnKey` on the TSF thread with a 250ms TTL, so a sustained
  typist caused ~4 `sync.status.json` opens/second *whether or not the user
  has ever configured sync*. Added a much-longer-TTL (10s) gate on
  `sync.conf`'s existence -- free for users who never open the Sync tab,
  since that file is only ever written by explicit user action in the
  config dialog.

### Decision: Windows ships unsigned; Microsoft Store ruled out

No code change — recorded here because it determines what we release.

The Windows `.exe` ships **unsigned** for now, with the SmartScreen warning
documented in the README. Code signing comes later via
[SignPath Foundation](https://signpath.org/), which is free for open-source
projects and signs the existing MSI unchanged. SignPath require an existing
release history and don't accept brand-new projects, so shipping first and
applying later is the correct order rather than a compromise.

**The Microsoft Store was investigated and ruled out.** It cannot supply free
signing for this project, for two independent reasons:

- Microsoft only re-signs **MSIX** packages during certification. An MSI/EXE
  Store submission [must already be signed](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options)
  with a CA-chained certificate.
- MSIX cannot host a TSF text service at all. Per Microsoft's
  [MSIX preparation docs](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-prepare):
  *"Your app's modules are loaded in-process to processes that are not in your
  Windows app package. This isn't permitted, which means that in-process
  extensions, like shell extensions, aren't supported."* A TIP DLL is loaded
  in-process into every application that takes text input — architecturally the
  same shape as a shell extension. Packaged COM does not help: it exposes
  **out-of-process** servers only, and explicitly "will not work for
  application extensions that rely upon directly reading the registry", which
  is how TSF profile registration works. MSIX also blocks the HKLM writes that
  registration needs, and the Store rejects apps requiring elevation for any
  part of their functionality.

Sparse packages (`allowExternalContent`) don't rescue it either: Microsoft
signs only the MSIX package, never the external content, so the actual
binaries would remain unsigned.

A Store listing may still be worth doing later purely for discoverability, as
a plain MSI/EXE submission once a SignPath certificate exists. It is not a
route to obtaining one.

### Windows: self-hosted auto-update, on the same model as macOS

Windows previously had no update mechanism at all: `win32/installer/`
produces an unsigned `.msi` with no update path, and the "Decision: Windows
ships unsigned" entry above ruled out the Microsoft Store as a source of one
(the Store only gives free signing/auto-update to MSIX packages, and MSIX
cannot host a TSF text service). **That ruling is about the Store
specifically and has no bearing on this feature** — this mirrors macOS's own
`marina_auto_update.mm`, which has nothing to do with the App Store either:
both are a plain HTTPS poll of GitHub Releases plus a one-click
download-and-launch of the installer, entirely self-hosted, entirely free.

Reused as-is, unmodified: `base/marina_github_releases.{h,cc}` (JSON parsing,
version comparison) and `base/marina_update_throttle.{h,cc}` (24h throttle,
persisted under the user profile dir) were already platform-agnostic — mac
was simply the only caller. Added Windows-equivalent asset lookup alongside
the existing mac-only functions, without touching them:
`FindMarinaMsiDownloadUrl()` and `MarinaHostWindowsArchToken()` ("x64" /
"arm64"), matching the `marinaMoji-<tag>-<arch>.msi` naming
`.github/workflows/release.yaml` actually publishes (confirmed by reading
the workflow, not assumed — the README says ".exe" but the real artifact is
an `.msi`). Covered by a new `base/marina_github_releases_test.cc`, the
first tests this file has had; **passes on macOS**, so the parsing/lookup
logic behind this feature is verified by execution.

**Deliberately hosted in the renderer, not the TIP** — this needed a real
architectural decision, not just a port:

- `TaskDialogIndirect` (used for the update-offer prompt) requires the
  *process's* manifest to declare comctl32 v6. The renderer's own
  (`mozc_renderer.exe.manifest`) already does, because the toolbar work
  earlier in this session depends on it. The TIP DLL has no such guarantee:
  it loads in-process into every application that takes text input --
  Notepad, Chrome, banking software, sandboxed app-container processes --
  and their manifests aren't ours to control. A TaskDialog call there would
  be unreliable at best.
- It also keeps a network stack (WinHTTP) out of a component that runs
  inside arbitrary, possibly low-integrity host processes -- a materially
  larger trust footprint than putting it in a single process we own.

New `renderer/win32/marina_auto_update.{h,cc}`:

- WinHTTP for both the GitHub API call and the `.msi` download -- not the
  `curl.exe`-subprocess pattern `base/marina_curl_fetch.cc` uses on macOS.
  A TIP-hosted design would have needed subprocess invocation with untrusted
  data (a GitHub-served URL) in the arguments, and Windows command-line
  quoting is a real bug class if done by hand (unlike POSIX `popen`, where
  no shell is invoked once arguments are pre-quoted); avoided entirely by
  not shelling out. (This turned out to be moot once the design moved to
  the renderer, but WinHTTP remains the more robust choice there too: no
  dependency on `curl.exe` being on PATH, no subprocess overhead.)
- `TaskDialogIndirect` for the update offer (Download & Install… / Open
  release page / Later), mirroring mac's three-button `NSAlert`.
- Launch via `WinUtil::ShellExecuteInSystemDir(L"open", path, nullptr)`
  (already used by `Process::OpenBrowser`) -- triggers the UAC elevation
  prompt for the MSI's custom actions, the same consent step as macOS
  Installer.app's password prompt. Since the MSI is unsigned for now (see
  above), SmartScreen may also warn first; the dialog copy says so.
- Trigger: mac hooks `activateServer:`, which fires on every focus change
  into any app using the IME -- there is no safe Windows analogue given the
  TIP constraints above. Instead, a background thread starts once at
  `WindowManager::Initialize()` (renderer startup) and re-tests the 24h
  throttle every 2h for as long as the renderer stays alive, so a process
  that never restarts still notices the next day's window. The throttle
  makes both triggers cheap no-ops outside their window; this is an
  adaptation of *when* to check to Windows' process model, not "more
  checking."
- `AutoUpdateChecker::Stop()` (called from `WindowManager::DestroyAllWindows()`)
  signals and detaches rather than joins: `RunCheckOnce()` can be mid-flight
  in network I/O with timeouts up to several tens of seconds, and process
  shutdown must not wait on that. (Header comment matches this — it does not
  claim to join.)

Also closed the matching gap in the Qt Settings dialog, which is shared
cross-platform code and had partially rotted on Windows: the manual "Check
for updates…" button already detected newer releases on Windows (the
check itself, `gui/base/github_update_checker.cc`'s `CheckForUpdates()`,
already ran `curl` via `QProcess`'s safe argv-list form, cross-platform,
with no code change needed), but `DownloadAndOpenInstaller()` was `#if
defined(__APPLE__)`-only, and `onUpdateAvailable`'s auto-check-on-dialog-open
was `#ifdef __APPLE__`-only despite the check beneath it being generic --
so opening Settings on Windows never auto-checked, and a manual check that
found an update could only fall back to opening the browser. Added the
Windows branches: `QProcess` + `curl` (the same safe argv pattern already
used for the check) to download, then `QDesktopServices::openUrl` on a
`file://` URL to launch. The "Automatically check for updates (once a day)"
checkbox is now shown on Windows as well as macOS (it was previously
`#ifdef __APPLE__`-only while the renderer checker already read
`auto_check_for_updates`, which defaults to true — so Windows users would
have been prompted with no UI to turn the check off).

**Review fix (2026-08-07):** `OpenSuccessfulGet` originally returned only the
request `HINTERNET` while destroying the session and connection handles at
function exit. In WinHTTP, closing a parent handle indirectly invalidates
its children (`ERROR_INVALID_HANDLE` on later reads), so the GitHub poll and
`.msi` download would have failed silently after every successful HTTP
handshake. It now returns a `WinHttpGet` that owns the full
session → connect → request chain for the lifetime of the body read /
file write; destructors close in reverse declaration order (request,
connect, session).

Files changed: `base/marina_github_releases.{h,cc}` (additive; existing mac
functions untouched), `gui/base/github_update_checker.{h,cc}`,
`gui/config_dialog/config_dialog.cc`, `protocol/config.proto` (doc comment
only -- `auto_check_for_updates` was already a shared field, just
undocumented for Windows), `renderer/win32/window_manager.{h,cc}`, and the
`BUILD.bazel` files for `base`, `bazel/win32` (new `winhttp` target), and
`renderer/win32`.

New files: `renderer/win32/marina_auto_update.{h,cc}`,
`base/marina_github_releases_test.cc`.

macOS and Linux are untouched -- `marina_curl_fetch.cc`'s `__APPLE__` branch
and `marina_auto_update.mm` were not modified.

**Verification:** `base/marina_github_releases_test.cc` passes on macOS
(new coverage for previously-untested parsing/lookup logic).
`config_cc_proto`, `marina_semver`, and `mac:mozc_toolbar` still build
clean, confirming the proto comment edit and the additive
`marina_github_releases.cc` changes didn't disturb the mac build.
**`gui/` targets (`github_update_checker`, `config_dialog`) could not be
built even for macOS in this environment** -- the local Qt SDK checkout is
incomplete (confirmed pre-existing and unrelated to this change: the
identical failure reproduces on the pre-change tree). The
`renderer/win32/marina_auto_update.cc` WinHTTP/TaskDialog code is new API
surface for this codebase and could not be compiled at all (no MSVC
toolchain, as throughout this session) -- checked via Bazel graph queries,
a `somepath` confirmation that it links into `marinamoji_renderer.exe`, and
a declaration/definition cross-check. **Awaiting Windows CI build +
on-laptop smoke test** (update prompt, download/install, Settings
auto-check checkbox, shutdown while a check is in flight).

### Windows: uppercase macron vowels (ĀĒĪŌŪ) were unreachable

`Ctrl+Alt+Shift+`vowel produced nothing on Windows, at **every** keyboard
layout — not just the non-QWERTY layouts this was originally suspected to
affect. Lowercase `ā ē ī ō ū` worked; the uppercase forms did not.

Cause: the macron rules in the keymap TSVs are written `Ctrl Alt Shift A` ..
`Ctrl Alt Shift U`, and `KeyParser::ParseKeyVector` stores a single-glyph
token verbatim, so those rules carry an *uppercase* `key_code`. But the
alphabet path in [`keyevent_handler.cc`](src/win32/base/keyevent_handler.cc)
reports `lower_char` whenever Ctrl is held and — unlike the
Shift-without-Ctrl branch — leaves `SHIFT` in the modifier set. The resulting
event (`key_code='a'`, `{CTRL, ALT, SHIFT}`) matches no rule at all. There are
no lowercase-plus-Shift macron rows, so the chord was inert rather than firing
some other command.

The bug only bites with CapsLock **off**: with CapsLock on, the CAPS branch
emits `lower_char` and `KeyEventUtil::NormalizeModifiers` flips the case back,
which happens to produce a match.

Fixed by reporting `upper_char` for `Ctrl+Alt+Shift+`vowel, mirroring the
`macron_shift` fixup in `mac/KeyCodeMap.mm` and restricted to the same five
vowels so no other `Ctrl+Alt+Shift+`letter binding changes shape. The test is
on the produced character rather than the virtual key, because under a fixed
romaji layout (`MarinaKeyboardLayout`) the character comes from that layout's
table and it is the character, not the physical key, that the keymap matches.

No Linux-style workaround is needed: `key_translator.cc:341` remaps a Hiragana
*keysym* that X11 can produce for these chords, and Windows has no keysym
layer — it delivers virtual-key codes directly.

Two tests added:

- `session/keymap_test.cc` — `MarinaMacronVowelsRequireUppercaseKeyCode` pins
  the contract cross-platform: uppercase matches, lowercase-plus-SHIFT (the
  buggy shape) does not, lowercase-without-Shift still matches. **This one
  runs on macOS and passes**, so the premise of the fix is verified by
  execution rather than inference.
- `win32/base/keyevent_handler_test.cc` — `MarinaMacronVowelUppercaseKeyCode`
  covers the Windows handler itself across all five vowels, the no-Shift case,
  and two non-vowels (to prove the fixup is scoped). Windows CI only.

macOS and Linux are untouched.

**Verification:** the cross-platform test passes locally, along with
`session_test` and `key_parser_test`. The Windows handler and its test could
not be compiled here (no MSVC toolchain) — Bazel graph queries pass, but the
Windows test is unrun until CI.

### Windows: keystrokes no longer leak into documents during a sync

**This is a data-integrity fix, not a cosmetic one.** While marinaMojiSync
rewrites user data it takes a global lock on the session handler
(`sync/sync_runner.cc` → `Client::BeginSyncLock`), after which
`SessionHandler::SendKey`/`TestSendKey`/`SendCommand` all fail with
`Output::SYNC_LOCKED` and **no `consumed` field**
([`session_handler.cc:441`](src/session/session_handler.cc)). macOS and Linux
guard against this — macOS with a `sync.status.json` poller
(`mac/sync_overlay.mm`), Linux with both that poller *and* an explicit
`SYNC_LOCKED` check ([`mozc_engine.cc:758`](src/unix/ibus/mozc_engine.cc)).

Windows had neither. The missing `consumed` field made
[`keyevent_handler.cc:1114`](src/win32/base/keyevent_handler.cc) set
`result.succeeded = false`, which made
[`tip_keyevent_handler.cc`](src/win32/tip/tip_keyevent_handler.cc) set
`*eaten = FALSE` — handing the keystroke to the application unprocessed. The
user-visible effect: during any sync run, typing Japanese inserted raw romaji
(`nihongo` instead of にほんご) into the document, with no beep, no overlay,
and no indication why. Sync is scheduled as a logon task on Windows
(`win32/base/task_scheduler_util.cc`), so this was reachable in normal use.

Fixed with the same two-layer defence Linux uses:

- **Proactive** — new `win32/base/sync_lock_util.{h,cc}` exposes
  `IsSyncLockActive()`, wrapping `sync::IsSyncRunning()` with a 250 ms TTL
  cache (matching the mac/Linux poll interval). The cache matters: this is
  called per key event, on the TSF thread, inside the host application's
  process, so an uncached read would put disk I/O in the input hot path.
  Guards added to **both** `OnTestKey` and `OnKey` in
  `win32/tip/tip_keyevent_handler.cc` — TSF consults `OnTestKey` first and
  routes the key to the application itself if the IME declines it, so a guard
  in `OnKey` alone would never see the keystroke.
- **Reactive** — both `result.succeeded == false` paths in
  `tip_keyevent_handler.cc` now check for `Output::SYNC_LOCKED` and claim the
  key, covering a lock taken in between polls. That path also invalidates the
  cache so the proactive guard takes over from the very next key.
- **Audible feedback** — `NotifySyncBlockedInput()` issues a rate-limited
  `MessageBeep` (1 s throttle, mirroring `SyncOverlayFlashBlockedInput()` on
  mac and Linux), on key-down only.

Also added the missing visible overlay: new
`renderer/win32/sync_overlay_window.{h,cc}`, a borderless, click-through,
always-on-top window centred on the work area showing the sync message.
Hosted in the renderer rather than the TIP because the TIP DLL loads into
every application process (same reasoning as the floating toolbar), and it
polls `sync.status.json` on a 250 ms `WM_TIMER` exactly as the mac/Linux
watchers do — so it needs no IPC and stays visible for the whole sync rather
than only when a key is pressed. Deliberately excluded from
`WindowManager::HideAllWindows()`: it reports that input is being held, which
is precisely the situation where the IME has no focus. It uses a uniform
`LWA_ALPHA` translucency plus a rounded region rather than mac/Linux's opaque
label over a 55 %-alpha backdrop, since matching that exactly would require
compositing text into a premultiplied DIB and GDI text drawing does not
maintain the alpha channel.

macOS and Linux are untouched. (Noted while investigating, not changed:
macOS relies solely on its proactive poll, so it has a narrow race if a sync
begins between the poll and a keystroke — microseconds of exposure versus the
whole sync duration on Windows.)

New files: `win32/base/sync_lock_util.{h,cc}`,
`renderer/win32/sync_overlay_window.{h,cc}`. Modified:
`win32/tip/tip_keyevent_handler.cc`, `renderer/win32/window_manager.{h,cc}`,
`base/const.h`, and the `BUILD.bazel` files for `sync` (visibility),
`win32/base`, `win32/tip`, and `renderer/win32`.

**Verification:** same constraint as below — the Windows files could not be
compiled here (no MSVC toolchain). Checked via Bazel graph queries on all new
targets, `somepath` confirmation that the overlay links into
`marinamoji_renderer.exe` and that `sync_lock_util` reaches the TIP,
declaration-vs-definition cross-checks, sync API signature checks, and
dep-ordering checks. **Needs a real Windows build and a manual test against a
running sync before it is trusted.**

### Windows floating toolbar: parity pass with macOS, plus fixes

The Windows toolbar (`renderer/win32/toolbar_window.cc`) and Symbols Palette
(`renderer/win32/symbols_palette_window.cc`) were compared against the macOS
implementation (`mac/mozc_toolbar.mm`) and brought up to parity, with a
number of pre-existing bugs fixed along the way. **macOS and Linux/GTK
toolbar code is untouched** — the shared pieces below are new, Windows-only
modules, not refactors of the mac/Linux implementations.

Bugs fixed:

- Symbols Palette: window size no longer omits the non-client area
  (caption/borders), which was clipping the bottom row of symbols.
- Symbols Palette: content taller than the window now scrolls instead of
  being silently cut off (e.g. a long `user_symbols.txt`).
- Symbols Palette: child controls now get an explicit `WM_SETFONT` (shell UI
  font, enlarged for symbol glyphs) instead of falling back to the legacy
  bitmap `SYSTEM_FONT`, which rendered CJK/symbol glyphs poorly.
- Symbols Palette: full per-monitor DPI scaling (previously all metrics were
  raw, unscaled pixels).
- Symbols Palette: dark mode is now respected (background, text, and common
  controls via `DarkMode_Explorer` theming); previously always light.
- Toolbar: DPI is now re-read after a drag and on `WM_DPICHANGED`, so moving
  the toolbar to a monitor with a different scale factor rescales it.
  Previously only `WM_DISPLAYCHANGE` (a resolution change) triggered a
  refresh.
- Toolbar: icon-tier selection now rounds ties up (was rounding down),
  fixing under-scaled icons at 125% and 175% Windows scaling.
- Toolbar: `SavePosition()` no longer truncates `toolbar.conf` down to just
  `x=`/`y=`, which was silently dropping the `toolbar_visible` key written
  by the TIP from a different process.
- Toolbar: a position restored from `toolbar.conf` is now clamped back onto
  a currently-attached monitor, so unplugging the monitor the toolbar was on
  can no longer strand it off-screen with no way to drag it back.
- Toolbar: a `Hide()` suppressed while the mode menu is open is now
  re-applied once the menu closes, instead of being dropped — previously the
  toolbar could stay on screen indefinitely after a focus change during menu
  use.
- Toolbar: renamed the `LoadIcon` helper to `LoadToolbarIcon` — it was
  silently colliding with the `<winuser.h>` `LoadIcon`/`LoadIconW` macro.
- `win32/base/toolbar_config.cc`: `LoadToolbarVisiblePreference()` is now
  cached with a 1-second TTL instead of hitting disk on every renderer
  update (i.e. every keystroke, on the TSF thread, inside the host
  application's process). The TTL keeps cross-process propagation working
  (the TIP loads separately into every application).

UX/feature parity added:

- Toolbar: hover and pressed-state highlighting on all six buttons (custom
  GDI-composited, since the toolbar has no child HWNDs).
- Toolbar: tooltips on every button, via a `TOOLTIPS_CLASS` control.
- Toolbar: an `IAccessible` (MSAA) implementation
  (`renderer/win32/toolbar_accessible.{h,cc}`) exposing the toolbar as a
  `ROLE_SYSTEM_TOOLBAR` with six `ROLE_SYSTEM_PUSHBUTTON` children — the
  toolbar was previously invisible to screen readers/UI Automation, since it
  draws everything into one composited bitmap. `accDoDefaultAction` posts
  back to the toolbar's own thread (MSAA calls arrive on an RPC thread).
- Toolbar: right-click context menu (Dictionary Tool, Keyboard Shortcuts,
  Settings, Hide toolbar) — previously the Dictionary Tool and "hide
  toolbar" were unreachable from the toolbar itself.
- Toolbar and Symbols Palette: localized strings (EN/FR/JA), via new
  `renderer/win32/marina_localized_string.{h,cc}` — previously all UI text
  was hardcoded English literals. Mirrors the `MM.*` keys used by
  `mac/Resources/{en,fr,ja}.lproj/Localizable.strings` so wording stays in
  step across platforms, without sharing code (this file is Windows-only).
- **New: Keyboard Shortcuts viewer for Windows**
  (`renderer/win32/shortcuts_window.{h,cc}`), reachable from the toolbar's
  Shortcuts button and its context menu. Mirrors macOS's
  `MozcShortcutsWindowController` (Script / Composition / Kaeriten tabs).
  Supporting plumbing:
  - New `session/marina_shortcut_list_util.{h,cc}` (Windows-only,
    `visibility = ["//win32/tip:__pkg__"]`): builds the three shortcut
    lists from the effective keymap table, marina number-row bindings, and
    kaeriten table. Modelled on macOS's private helpers in
    `mac/mozc_toolbar.mm` but implemented separately — reads keymap tables
    via `ConfigFileStream`'s `system://` scheme rather than loose files,
    and de-duplicates `(command, key)` pairs. Has its own test suite
    (`marina_shortcut_list_util_test.cc`, 9 cases).
  - New `RendererCommand::ApplicationInfo::ShortcutsInfo` proto message
    (`protocol/renderer_command.proto`), populated TIP-side
    (`win32/tip/tip_ui_handler_conventional.cc`'s `FillShortcutsInfo`) since
    the renderer deliberately doesn't link config/keymap parsing.
  - New `SessionCommand` types `SHOW_SHORTCUTS_WINDOW` /
    `HIDE_SHORTCUTS_WINDOW` (local UI-visibility signals, intercepted in
    `win32/tip/tip_edit_session.cc`, same pattern as the existing
    `SHOW_SYMBOLS_PALETTE`/`HIDE_SYMBOLS_PALETTE`) and a new
    `TipPrivateContext::shortcuts_window_visible()` flag.
- New `SessionCommand::LAUNCH_DICTIONARY_TOOL` (wired into
  `session/session.cc`'s `Session::LaunchDictionaryTool`) and
  `SessionCommand::HIDE_TOOLBAR` (writes the `toolbar_visible` preference
  from the TIP side), both reachable from the toolbar's new context menu.
- `renderer/renderer_server.cc`: allowlisted and routed the four new
  `SessionCommand` types above through the existing renderer→TIP channel.

Files changed: `renderer/win32/toolbar_window.{h,cc}`,
`renderer/win32/symbols_palette_window.{h,cc}`,
`renderer/win32/window_manager.{h,cc}`, `renderer/renderer_server.cc`,
`session/session.cc`, `win32/base/toolbar_config.cc`,
`win32/tip/tip_edit_session.cc`, `win32/tip/tip_private_context.{h,cc}`,
`win32/tip/tip_ui_handler_conventional.cc`, `protocol/commands.proto`,
`protocol/renderer_command.proto`, `base/const.h`, plus the `BUILD.bazel`
files for `bazel/win32`, `composer`, `renderer/win32`, `session`, and
`win32/tip`.

New files: `renderer/win32/marina_localized_string.{h,cc}`,
`renderer/win32/toolbar_accessible.{h,cc}`,
`renderer/win32/shortcuts_window.{h,cc}`,
`session/marina_shortcut_list_util.{h,cc}`,
`session/marina_shortcut_list_util_test.cc`.

**Verification:** the cross-platform pieces (both edited protos, `session`
and `session_test`, `renderer_server`, `mac:mozc_toolbar`, and
`marina_shortcut_list_util` with its new 9-test suite) were built and
tested on macOS. The Windows-only files could not be compiled in this
environment (no MSVC toolchain available to Bazel here) — they were instead
checked via Bazel `BUILD`-graph queries on every touched target, a
`somepath` check confirming the new shortcuts window links into
`marinamoji_renderer.exe`, a declaration-vs-definition cross-check across
all three window classes, a localization-key audit, a generated-proto
accessor check against the built `.pb.h`, and a dep-ordering check across
all `BUILD.bazel` lists touched. **This is not a substitute for an actual
Windows build**, which should happen before this is trusted.
