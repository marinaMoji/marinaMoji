# marinaMoji Windows port plan

Planning and status for the **Windows** build of **marinaMoji** (TSF Text Input
Processor + floating toolbar). Linux/IBus behavior is the reference, same as
for the [macOS port](MACOS_PORT_PLAN.md). Upstream build steps:
[build_marinamoji_on_windows.md](build_marinamoji_on_windows.md).

## Goals

1. **Feature parity** with marinaMoji on Linux/macOS: shin/kyū (OpenCC),
   odoriji/symbols palette, Manyōshū mode, macron vowels, number-row
   shortcuts, floating toolbar, sync.
2. **Install side-by-side** with stock Mozc and Google Japanese Input. This
   requires distinct TSF GUIDs, CLSIDs, binary names, IPC pipe names, registry
   keys, and MSI UpgradeCode — see Phase 1.
3. **Do not break Linux/macOS** — Windows changes live under `src/win32/`,
   `_WIN32` guards, or `mozc_select()` branches.

## Architecture (short)

| Piece | Role | marinaMoji name (proposed) |
|-------|------|----------------------------|
| `mozc_tip64.dll` | TSF Text Input Processor (in-process, loaded by every app) | `marinamoji_tip64.dll` |
| `mozc_server.exe` | Converter/session server (conversion, keymap, odoriji logic) | `marinamoji_server.exe` |
| `mozc_renderer.exe` | Candidate window UI (out-of-process; Win32 GDI) | `marinamoji_renderer.exe` |
| `mozc_tool.exe` | Qt GUI tools (Preferences, Dictionary Tool, Word Register) | `marinamoji_tool.exe` |
| `mozc_broker.exe` | Elevation broker (launch tools from AppContainer processes) | `marinamoji_broker.exe` |
| `mozc_cache_service.exe` | Windows service that pre-warms dictionary cache | `marinamoji_cache_service.exe` |
| `mozc_installer_helper.dll` | WiX custom action (register/unregister IME) | `marinamoji_installer_helper.dll` |
| `Mozc64.msi` | WiX installer | `marinaMoji64.msi` |

The **toolbar has no Windows implementation** (`src/unix/ibus/mozc_toolbar.cc`
is GTK; `src/mac/mozc_toolbar.mm` is AppKit). It is the largest new-code item —
see Phase 4.

## Current status (2026-07-13)

- `src/win32/` contains upstream Mozc TSF code; session-layer features
  (Manyōshū, odoriji commands, kaeriten, number-row bindings) live in shared
  code and should come along once the client builds (Phase 2 verifies this).
- **Phase 1 (branding) landed 2026-07-13** — see checklist below, all boxes
  checked except the IME icon (needs new artwork, not just text edits).
  `bazelisk build --config oss_windows --config release_build package` has
  not yet been run on real Windows hardware/VM; next step is Phase 1g
  (build bring-up + acceptance test) on a Windows machine.
- `src/win32/` itself (TIP/TSF logic, key handling) is still **unmodified**
  upstream Mozc code — Phase 1 only touched naming/branding plumbing (Bazel
  name maps, `const.h`, GUIDs, `.rc` strings, installer `.wxs`, CI). Phases
  2–6 (session features, OpenCC, toolbar, sync, packaging) are unstarted.

---

# Phase 1 — Branding & build bring-up  ← **current phase**

Goal: `bazelisk build --config oss_windows --config release_build package`
succeeds on a Windows machine and produces a `marinaMoji64.msi` that installs
an IME named **marinaMoji**, functionally identical to stock Mozc, coexisting
with a stock Mozc install.

### 1a. Naming decisions (decided 2026-07-13)

- [x] Executable naming scheme: lowercase `marinamoji_*.exe` / `.dll`
      (mirrors OSS Mozc's `mozc_*`; matches Linux `marinamoji` dirs), product
      display name **marinaMoji**.
- [x] GUIDs (freshly generated; **never reuse Mozc's** — identical GUIDs make
      Windows treat marinaMoji and Mozc as the same IME and the MSIs as
      upgrades of each other):
  - TSF **text service CLSID**: `{8D513EAE-75C6-4CCA-A307-90F97E573706}`
    (replaces Mozc `{10A67BC8-…}`)
  - TSF **profile GUID**: `{499B197C-3E88-428C-99E1-D1118B8A3734}`
    (replaces Mozc `{186F700C-…}`)
  - MSI **UpgradeCode**: `F01BE4B5-4749-46C1-B714-DFF9FE9744A0`
    (replaces Mozc `DD94B570-B5E2-4100-9D42-61930C611D8A`)
- [x] Vendor / Manufacturer: **CRCAO**. Registry root:
      `Software\CRCAO\marinaMoji` (config) and
      `Software\Policies\CRCAO\marinaMoji\Preferences` (policy), replacing
      `Software\Mozc Project\Mozc`.

### 1b. Bazel plumbing (makes targets exist) — done 2026-07-13

Added `"marinaMoji": "<name>"` to `executable_name_map` in each of:

- [x] [src/server/BUILD.bazel](../src/server/BUILD.bazel) → `marinamoji_server.exe`
- [x] [src/renderer/win32/BUILD.bazel](../src/renderer/win32/BUILD.bazel) → `marinamoji_renderer.exe`
- [x] [src/gui/tool/BUILD.bazel](../src/gui/tool/BUILD.bazel) → `marinamoji_tool.exe`
- [x] [src/win32/broker/BUILD.bazel](../src/win32/broker/BUILD.bazel) → `marinamoji_broker.exe`
- [x] [src/win32/cache_service/BUILD.bazel](../src/win32/cache_service/BUILD.bazel) → `marinamoji_cache_service.exe`
- [x] [src/win32/custom_action/BUILD.bazel](../src/win32/custom_action/BUILD.bazel) → `marinamoji_installer_helper.dll`
- [x] [src/win32/tip/BUILD.bazel](../src/win32/tip/BUILD.bazel) — all four TIP
      targets (`mozc_tip32`, `mozc_tip64`, `mozc_tip64arm`) →
      `marinamoji_tip32/64/64arm.dll`; the `mozc_tip64x` forwarder genrule's
      `outs` and [build_tip_forwarder_dll.py](../src/win32/tip/build_tip_forwarder_dll.py)
      (name maps + `--branding` choices) also updated for the arm64x pure
      forwarder path.
- [x] `mozc_win32_resource_from_template`
      ([build_defs.bzl](../src/build_defs.bzl)): `_rc_defines` map now has
      `"marinaMoji": ["MOZC_BUILD", "MARINAMOJI"]` (keeps `MOZC_BUILD` so OSS
      `#else` branches still apply; `MARINAMOJI` gates our overrides).
- [x] Audited other `BRANDING ==` call sites — only the installer's
      `_WXS_FILE`/`_MSI_FILE` (handled in 1f) and the `mozc_tip64x` genrule
      `outs` (handled above) compare BRANDING directly.

### 1c. `src/base/const.h` — Windows constants — done 2026-07-13

The `_WIN32` non-Google branch now has marinaMoji names throughout (folded
into the existing `#ifdef GOOGLE_JAPANESE_INPUT_BUILD / #else` structure —
no separate `MARINAMOJI` branch needed here since this file has no
third "OSS Mozc" case to preserve on Windows):

- [x] `kProductNameInEnglish` → `"marinaMoji"` (drives the profile dir:
      `%LOCALAPPDATA%\marinaMoji`, see
      [system_util.cc](../src/base/system_util.cc)) — extended the existing
      `__APPLE__` branch to `__APPLE__ || _WIN32`
- [x] `kCompanyNameInEnglish` → `"CRCAO"`
- [x] `kEventPathPrefix` / `kMutexPathPrefix` → `Local\\marinaMoji.event.` / `.mutex.`
- [x] `kMozcServerName`, `kMozcTIP32/64/64X`, `kMozcBroker`, `kMozcTool`,
      `kMozcRenderer`, `kMozcCacheServiceExeName`, `kMozcCacheServiceName` →
      match 1b names exactly
- [x] `kMessageReceiverMessageName` / `kMessageReceiverClassName` →
      `marinamoji.renderer.message` / `.window`
- [x] Window class names (`kCandidateWindowClassName`, `kCompositionWindowClassName`,
      `kIndicatorWindowClassName`, `kInfolistWindowClassName`, `kIMEUIWndClassName`)
      → `marinaMoji…` (`kIMEUIWndClassName` = `marinaMojiUIWnd`, 15 chars + NUL,
      fits the 16-TCHAR limit)
- [x] `kIPCPrefix` → `\\\\.\\pipe\\marinamoji.` — **critical for
      side-by-side**: with the stock prefix, marinaMoji clients would talk to
      a running stock Mozc server (protocol-version chaos)
- [x] `kCandidateUIDescription`, `kConfigurationDisplayname`
- [x] `kMozcRegKey`, `kElevatedProcessDisabledKey` → `Software\CRCAO\marinaMoji`
      / `Software\Policies\CRCAO\marinaMoji\Preferences`
- [ ] Sync binary equivalent (`kMozcSyncName`/`kMozcSyncExecutable` pattern) —
      deferred to Phase 5, no Windows sync binary exists yet

### 1d. TSF identity — `src/win32/base/tsf_profile.cc` — done 2026-07-13

- [x] Added marinaMoji **text service GUID**
      (`{8D513EAE-75C6-4CCA-A307-90F97E573706}`) and **profile GUID**
      (`{499B197C-3E88-428C-99E1-D1118B8A3734}`) in the non-Google branch
- [x] Found and fixed one hard-coded copy of the old text-service GUID in
      [system_util.cc](../src/base/system_util.cc) (`kMozcTipClsid`, used to
      read the TIP install dir from the registry) — would otherwise have kept
      looking up stock Mozc's registry key
- [x] Also rotated the two **TSF display-attribute GUIDs** in
      [tip_display_attributes.cc](../src/win32/tip/tip_display_attributes.cc)
      (input/converted underline styles) — same side-by-side-registration
      reasoning, not listed in the original plan but same category of risk
- [ ] `tsf_profile_test.cc` — no such test file exists; nothing to update

### 1e. Display names & resources — done 2026-07-13 (except icon art)

- [x] [tip_resource.rc](../src/win32/tip/tip_resource.rc): `IDS_IME_DISPLAYNAME`
      / `IDS_TEXTSERVICE_DISPLAYNAME_SYNONYM` → **marinaMoji** (both JA/EN
      string tables); `MOZC_RES_FILE_DESCRIPTION`/internal/original-filename →
      marinaMoji; window description strings (`IDS_CANDIDATE_WINDOW` etc.)
- [x] Also rebranded the four sibling `.rc` files that share the same
      `#ifdef GOOGLE_JAPANESE_INPUT_BUILD / #elif MOZC_BUILD` pattern:
      [mozc_broker.rc](../src/win32/broker/mozc_broker.rc),
      [mozc_cache_service.rc](../src/win32/cache_service/mozc_cache_service.rc)
      (incl. the service display-name/description string tables),
      [custom_action.rc](../src/win32/custom_action/custom_action.rc),
      [mozc_renderer.rc](../src/renderer/win32/mozc_renderer.rc),
      [mozc_tool.rc](../src/gui/tool/mozc_tool.rc)
- [x] [mozc_win32_resource_template.rc](../src/build_tools/mozc_win32_resource_template.rc):
      `MOZC_RES_COMPANY_NAME` → `"CRCAO"`, `MOZC_RES_PRODUCT_NAME` →
      `"marinaMoji"` under `MARINAMOJI` — this is the shared template every
      `.rc` above includes, so it drives the CompanyName/ProductName shown on
      the Properties → Details tab of every marinaMoji Windows binary
- [ ] Version-info resources — **not yet verified on a real build**; needs a
      Windows machine (Phase 1g)
- [ ] IME icon: still the stock `product_icon.ico`/`product_icon_langbar.ico`.
      Swapping requires new `.ico` artwork (SVG→ICO conversion), not a text
      edit — tracked separately, not blocking Phase 1g functional testing

### 1f. Installer — done 2026-07-13

- [x] [src/win32/installer/BUILD.bazel](../src/win32/installer/BUILD.bazel):
      `_WXS_FILE`/`_MSI_FILE` changed from ternary to dict lookup with an
      explicit `"marinaMoji"` entry (previously any non-`"Mozc"` branding,
      including ours, silently fell through to the **Google/Omaha** wxs)
- [x] Created
      [installer_marinamoji_64bit.wxs](../src/win32/installer/installer_marinamoji_64bit.wxs)
      from `installer_oss_64bit.wxs`: `Package Name="marinaMoji"`,
      `Manufacturer="CRCAO"`, install dir `marinaMoji`, every `<File>` Id/Name
      renamed to the 1b `marinamoji_*` names, cache service name
      `marinaMojiCacheService` (matches `kMozcCacheServiceName`), prelauncher
      registry value `marinaMoji Prelauncher`
- [x] [build_installer.py](../src/win32/installer/build_installer.py): added
      `branding == 'marinaMoji'` branch with UpgradeCode
      `F01BE4B5-4749-46C1-B714-DFF9FE9744A0` (no Omaha update-key wiring,
      matching the plain-`Mozc` branch — marinaMoji has no auto-update service)
- [x] `_MSI_FILE` → `marinaMoji64.msi`
- [x] Custom action verified — derives all names from `base/const.h` via
      `kMozc*` constants, no hard-coded `"Mozc"`/`"mozc"` literals found
- [x] `.github/workflows/windows.yaml` — all three build jobs (x64, universal,
      arm64) now upload/reference `marinaMoji64.msi` /
      `marinaMoji64_{x64,universal,arm64}.msi`

### 1g. Phase 1 acceptance test (on Windows, VM fine)

- [ ] `bazelisk build --config oss_windows --config release_build package`
      from `src/` succeeds
- [ ] Install `marinaMoji64.msi` on a machine that **already has stock Mozc**:
      both appear separately in Settings → Time & Language → Language →
      Japanese → keyboard options
- [ ] Language bar shows **marinaMoji** with our icon
- [ ] Basic kana→kanji conversion works in Notepad (server pipe OK)
- [ ] Preferences and Dictionary Tool open (`marinamoji_tool.exe`, Qt)
- [ ] Profile dir created at `%LOCALAPPDATA%\marinaMoji` (not `Mozc`)
- [ ] Stock Mozc still converts normally with marinaMoji installed (no pipe /
      window-class / mutex collisions); uninstall marinaMoji → Mozc unaffected
- [ ] Windows tests: `bazelisk test ... --config oss_windows --build_tests_only -c dbg`

---

# Phase 2 — marina session features on Windows

Mostly verification: these features live in shared session/composer code and
keymap TSVs, so they should light up once Phase 1 ships. Reference:
"Keymap notes" in [MACOS_PORT_PLAN.md](MACOS_PORT_PLAN.md) — Windows has its
own key-translation quirks (`win32/base/keyboard.cc`), expect a similar batch
of fixes.

- [ ] Default keymap on Windows is MS-IME (`config_handler.cc`) — verify
      marina rows exist in `ms-ime.tsv` (they do; aligned 2026-06)
- [ ] Number-row shortcuts Ctrl+Shift+1–5 (odoriji, palette, shin/kyū,
      Manyōshū, hiragana/direct) — verify `marina_number_row_bindings_util`
      dispatch works from the TIP key handler; physical-scancode mapping for
      non-QWERTY layouts (macOS needed `KeyCodeMap` work; Windows analog is
      scancode→VK handling)
- [ ] Right Shift alone toggles hiragana ↔ Manyōshū (Linux `IBUS_Shift_R` /
      macOS `kVK_RightShift` parity — Windows: `VK_RSHIFT` release tracking;
      mind the existing stuck-Shift fixes in direct mode, see
      [SHIFT_RETURN_STUCK_MODIFIER_STOPGAP.md](SHIFT_RETURN_STUCK_MODIFIER_STOPGAP.md))
- [ ] Macron vowels (Ctrl+Alt+Shift+letter) in ASCII mode
- [ ] Kaeriten `;r` `;1` … input
- [ ] Quick dictionary injection Ctrl+Shift+0 (`LAUNCH_WORD_REGISTER_DIALOG`
      session command → broker → `marinamoji_tool.exe --mode=word_register_dialog`)
- [ ] Katakana conversion mode + `shift_R` quick switch

# Phase 3 — OpenCC / shin-kyū

- [ ] Build OpenCC for Windows (MSVC) or add to Bazel deps; link into server
- [ ] Bundle marina conversion tables; install under the server dir; path
      resolution in `oss`/Windows layout (macOS uses `Resources/opencc`,
      Linux `opencc/` — Windows: alongside `marinamoji_server.exe`)
- [ ] `Ctrl+Shift+3` toggle + config persistence
- [ ] Add tables + dictionaries to the `.wxs` file list

# Phase 4 — Floating toolbar (the big block)

New Windows implementation; no upstream analog. Reference behavior:
[GTK_TOOLBAR.md](GTK_TOOLBAR.md) (Linux) and `src/mac/mozc_toolbar.mm` (macOS).

**Architecture decision needed:** the TIP DLL runs **inside every
application's process** — a toolbar window owned by the DLL would be
created/destroyed per-process and can't outlive focus changes cleanly. The
natural host is **`marinamoji_renderer.exe`** (already an out-of-process,
always-running UI process that receives per-keystroke state over IPC — this is
exactly how stock Mozc draws its mode **indicator**, see
`win32/base/indicator_visibility_tracker` and `renderer/win32/`).

- [ ] Decide host process (proposal: renderer) and UI stack (proposal: raw
      Win32/GDI+ or Direct2D like the rest of the renderer; **not** Qt — the
      renderer must stay lightweight and Qt there would be a new dependency)
- [ ] Extend renderer IPC (`renderer/renderer_command.proto` usage) to carry
      toolbar state: composition mode, shin/kyū flag, half/full — mirror
      `MozcToolbarUpdate(output)` fields
- [ ] Toolbar window: `WS_POPUP` + `WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
      WS_EX_TOPMOST` (no taskbar entry, never steals focus — equivalent of
      GTK UTILITY hint + accept_focus false), rounded corners
      (`DwmSetWindowAttribute` on Win11, fallback region on Win10), light/dark
      from `AppsUseLightTheme` registry key + `WM_SETTINGCHANGE`
- [ ] Bottom-right of primary work area (`SHAppBarMessage`/
      `SystemParametersInfo(SPI_GETWORKAREA)`), draggable on background
      (`WM_NCHITTEST` → `HTCAPTION`), position persisted
- [ ] Buttons: mode indicator, shin/kyū toggle, symbols palette, dict, settings,
      shortcuts — icons from `toolbar_icons/*.svg` pre-rendered to PNG at
      build time (no librsvg on Windows; add a genrule, or ship multi-size PNGs)
- [ ] Button actions → session commands: toolbar (renderer) sends
      `SessionCommand` back over IPC; **route through the focused TIP context**
      so candidate-window-opening commands work (Windows analog of the macOS
      `sendCommand:` → `processOutput` lesson and Linux
      `SendToolbarSessionCommand` → `UpdateAll`)
- [ ] Symbols palette (tabbed Odoriji/Kaeriten/Symbols/User) — same window
      infrastructure; insert on click commits via session command
- [ ] Show on IME focus-in, delayed hide on focus-out (150 ms, matching GTK),
      visibility preference persisted (`toolbar.conf` analog under
      `%LOCALAPPDATA%\marinaMoji`)
- [ ] Multi-monitor + per-monitor DPI (`PROCESS_PER_MONITOR_DPI_AWARE`)

# Phase 5 — Sync

- [ ] Port `mozc_sync` binary/service to Windows (libsodium + miniz already in
      third_party; both build with MSVC)
- [ ] Decide process model: scheduled task vs on-demand from server (macOS
      uses a LaunchAgent; Windows analog is Task Scheduler or the existing
      cache-service pattern)
- [ ] `sync.conf` sidecar path under `%LOCALAPPDATA%\marinaMoji`
- [ ] Config dialog sync tab paths (folder picker → Nextcloud/Syncthing/
      OneDrive folders); [SYNC_MANUAL_QA.md](SYNC_MANUAL_QA.md) two-device run
      with one Windows device

# Phase 6 — Polish, packaging, QA

- [ ] Qt deployment for `marinamoji_tool.exe` verified on a clean VM (upstream
      `build_qt.py` handles this; confirm our extra dialogs/resources)
- [ ] About dialog branding (`gui/about_dialog`, `MARINAMOJI` define — note it
      is set via `local_defines` per-target; ensure Windows targets get it)
- [ ] Code signing decision (unsigned MSI → SmartScreen warning; document the
      "More info → Run anyway" path for colleagues, or budget for a cert)
- [ ] CI: windows.yaml green, artifact renamed, add to README build status
- [ ] Update [build_marinamoji_on_windows.md](build_marinamoji_on_windows.md)
      from upstream-reference to real instructions
- [ ] Full manual QA: macOS checklist in
      [MACOS_PORT_PLAN.md](MACOS_PORT_PLAN.md) § Testing, adapted (Notepad,
      Word, Edge, terminal; Dvorak/AZERTY layouts; RDP session behavior)

---

## Known risks

| Risk | Notes |
|------|-------|
| TIP runs in-process | Any crash in marina code inside the DLL kills the host app (Word, browser). Keep marina logic in the server where possible; TIP changes minimal. Same lesson as macOS M1d toolbar crash. |
| Toolbar host = renderer | Renderer lifecycle is driven by candidate-window needs; toolbar needs it alive whenever IME has focus. May need to adjust renderer launch/keepalive policy. |
| Pipe/mutex collisions | Missing one `kIPCPrefix`/mutex rename breaks side-by-side in ways that only show with both IMEs installed. Phase 1g explicitly tests with stock Mozc present. |
| ARM64 | Upstream builds ARM64 but dev machines are x64; defer ARM64 artifact until x64 is stable (universal installer needs extra VS components). |
| GYP leftovers | None — GYP path is deprecated; Bazel only. |

## Revision log

| Date | Change |
|------|--------|
| 2026-07-13 | Initial plan. Identified `executable_name_map`/BRANDING gate as the reason the Windows build is a silent no-op; phased checklist with branding first. |
| 2026-07-13 | **Phase 1 (branding) implemented**: naming decided (`marinamoji_*`, vendor CRCAO); 7 `executable_name_map` entries + rc-defines fix; `const.h` Windows constants; new TSF/display-attribute GUIDs (+ one stray hard-coded GUID found in `system_util.cc`); 6 `.rc` files + shared resource template rebranded; `installer_marinamoji_64bit.wxs` created and wired into `BUILD.bazel`/`build_installer.py`; CI artifact names updated. Not yet built/tested on a Windows machine (Phase 1g); IME icon art not yet swapped. |
