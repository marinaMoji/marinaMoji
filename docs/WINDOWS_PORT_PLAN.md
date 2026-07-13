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
- **Phase 2 (marina session features) landed 2026-07-13** — see checklist
  below. Most items needed zero Windows code (keymap TSV rows and composer
  tables were already in place); the two real gaps (Right/Left-Shift-alone
  modifier fidelity, and the Ctrl(+Shift)+number-row dispatcher) are now
  fixed/ported, mirroring `unix/ibus`'s reference implementation. `src/win32/`
  is no longer unmodified upstream code — `keyevent_handler.cc` has a
  targeted fix and `win32/tip/` has two new files
  (`win32_physical_slot`, `marina_number_row_dispatcher`) hooked into
  `tip_keyevent_handler.cc`'s `OnKey`. **Not yet compiled or tested on
  Windows hardware** — first signal comes from Phase 1g.
- **Phase 3 (OpenCC build/data wiring) landed 2026-07-13** — `opencc_rewriter`
  now builds and links on Windows (mirrors macOS's static-from-source
  approach) and the 4 curated marina shin/kyū data files are wired into the
  installer. **Not yet compiled** (MSVC compatibility of the vendored OpenCC
  C++ is unverified — first signal comes from Phase 1g's build attempt) and
  the `Ctrl+Shift+3` shortcut is blocked on Phase 2's Windows key-dispatch
  work (the keymap-TSV `Ctrl+Shift+F` alternative needs no Windows-specific
  code, just Phase 1g confirmation that TIP key handling works at all).

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

Turned out to be mostly-true: 4 of 6 items already worked once Phase 1
shipped, because the logic lives in shared session/composer code and keymap
TSVs. Only two genuine Windows-specific gaps existed, both closed 2026-07-13.

- [x] **Default keymap** (2026-07-13, verification only): confirmed
      `data/keymap/ms-ime.tsv` already has marina rows — `RightShift` →
      `ToggleManyoshuHiragana` (lines 23,121,252), macron vowel rows (`Ctrl
      Alt`/`Ctrl RightAlt`/`Ctrl AltGr` + letter, lines 24-53 etc.),
      `Ctrl Shift F` → `ToggleTraditionalKanji` (lines 57,58,154,286,287),
      `Ctrl Shift )` → `LaunchWordRegisterDialog` (lines 65,163,215,301,303) —
      aligned with `kotoeri.tsv` back in 2026-06, no Windows-specific work
      was ever needed here.
- [x] **Right Shift / Left Shift alone, Ctrl+Left Shift** (2026-07-13,
      **real bug, now fixed**): `Session::IsRightShiftAlone`/`IsLeftShiftAlone`/
      `IsCtrlLeftShiftAlone` ([session.cc:696-763](../src/session/session.cc))
      are fully platform-independent and already wired into `SendKey` — but
      Windows never set `RIGHT_SHIFT`/`LEFT_SHIFT` in `KeyEvent::modifier_keys`,
      only the generic `SHIFT` (upstream's own `TODO(yukawa): Distinguish left
      key from right key to fix b/2674446` at
      [keyevent_handler.cc:421](../src/win32/base/keyevent_handler.cc)), so the
      toggle silently never fired. Fixed in `ConvertToKeyEventMain`:
      `keyboard_status.IsPressed(VK_LSHIFT/VK_RSHIFT)` for the "Shift held
      while another key is pressed" case, plus scan-code-based detection
      (PC/AT scan-code-set-1: LShift=0x2A, RShift=0x36 — reliable on both
      key-down *and* key-up, unlike the keyboard-state snapshot) in the
      `case VK_SHIFT:` branch that handles the modifier-key-itself-pressed
      case. No new files; Ctrl/Alt left-right fidelity intentionally left
      generic since no marina shortcut needs it.
- [x] **Number-row shortcuts Ctrl(+Shift)+1-5/0/`** (2026-07-13, **new
      dispatcher, mirrors unix/ibus**): added
      [win32/tip/win32_physical_slot.h/.cc](../src/win32/tip/win32_physical_slot.cc)
      (scan-code → `MarinaPhysicalSlot`, reusing `unix/ibus/ibus_physical_slot.cc`'s
      table verbatim since PC/AT scan-code-set-1 and Linux evdev `KEY_1..KEY_0`/
      `KEY_GRAVE`/`KEY_BACKSPACE` share identical numeric values — including
      porting the Backspace-collision guard) and
      [win32/tip/marina_number_row_dispatcher.h/.cc](../src/win32/tip/marina_number_row_dispatcher.cc)
      (mirrors `unix/ibus/marina_number_row_dispatcher.cc`'s dispatch switch
      and 300ms autorepeat-suppression window almost line-for-line, reusing
      the shared `session::FindMarinaActionForPhysicalSlot`). No
      `PropertyHandler`/engine equivalent was needed — Windows applies the
      final `SessionCommand`'s `Output` via the existing
      `TipEditSession::OnOutputReceivedSync` call already at the end of
      `OnKey`, so `EnsureImeOn`'s intermediate `TURN_ON_IME` output is simply
      discarded (its real effect already lands server-side via the IPC call).
      Hooked into [tip_keyevent_handler.cc](../src/win32/tip/tip_keyevent_handler.cc)'s
      `OnKey`, as a new `else if` branch alongside the existing on-screen-
      keyboard prev/next-page `SendCommand` branches, gated on
      `open && is_key_down` — falls through to the normal `ImeToAsciiEx` path
      when nothing is bound, exactly like the ibus/mac reference behavior.
      Fetches `config::Config` via IPC on every key-down before checking
      Ctrl, matching (not optimizing past) the existing ibus precedent.
  - [ ] **Untested**: no Windows hardware run yet; needs Phase 1g build
        bring-up first, then manual verification of all 6 bindings plus a
        non-QWERTY layout (AZERTY/Dvorak) to confirm the physical-slot
        mapping is truly layout-independent.
- [x] Macron vowels — confirmed keymap-only (`data/keymap/ms-ime.tsv`), needs
      no Windows-specific code once Phase 1g confirms TIP key handling works.
- [x] Kaeriten `;r` `;1` … — confirmed pure composer-table data
      ([data/preedit/kaeriten.tsv](../src/data/preedit/kaeriten.tsv)), no code.
- [x] Quick dictionary injection Ctrl+Shift+0 — confirmed this is the
      keymap-TSV `Ctrl Shift )` row (a plain keystroke on US layout), not the
      number-row dispatcher; already present in `ms-ime.tsv`.
- [ ] Katakana conversion mode + `shift_R` quick switch — **not yet verified**;
      needs Phase 1g hardware access to test interaction with the Right-Shift
      fix above (right-shift-alone toggles hiragana↔Manyōshū, not katakana —
      confirm the katakana path uses a different, already-shared binding and
      doesn't collide).

# Phase 3 — OpenCC / shin-kyū

- [x] **Bazel deps (2026-07-13)**: no new `MODULE.bazel` work needed — the
      macOS `@opencc_source` `http_archive` (OpenCC 1.2.0 built from source,
      static lib, plus `@marisa_trie`/`@rapidjson`/`@darts_clone` transitive
      deps) is already declared unconditionally, just unused by Windows
      targets. Added `windows`/`oss_windows` branches to both `mozc_select()`
      calls in [rewriter/BUILD.bazel](../src/rewriter/BUILD.bazel) (`opencc_rewriter`
      target): defines `MOZC_USE_OPENCC` and links `@opencc_source//:opencc`,
      mirroring the macOS static-linking approach (Windows has no
      pkg-config/system-libopencc equivalent like Linux).
  - [ ] **Unverified**: `@opencc_source`'s vendored C++ (`src/bazel/BUILD.opencc.bazel`)
        has only ever been compiled with Clang/GCC (Linux/macOS); MSVC
        compatibility of the OpenCC/marisa-trie/rapidjson/darts-clone sources
        is untested. First real signal comes from Phase 1g's build attempt now
        that these targets are reachable on Windows.
- [x] **Data bundling & path resolution (2026-07-13)**: confirmed
      `SystemUtil::GetServerDirectory()` ([system_util.cc:503-517](../src/base/system_util.cc))
      already has a full Windows branch (registry lookup, falls back to
      `Program Files\marinaMoji`) — same function macOS/Linux use, so
      `opencc_rewriter.cc`'s `GetOpenccConfigPath()` needed **no code changes**
      for Windows. Wired the 4 curated `//data/marina_opencc:opencc_data`
      files (`marinaShin2Kyu.json` + 3 `.ocd2` dictionaries — **not** stock
      OpenCC's `jp2t.json`) into the installer:
  - [x] [win32/installer/BUILD.bazel](../src/win32/installer/BUILD.bazel): added the
        4 files to the `installer` genrule's `srcs` and as
        `--opencc_config`/`--opencc_characters`/`--opencc_phrases`/`--opencc_variants`
        args (same per-file pattern as `--icon_path`/`--credit_file`)
  - [x] [build_installer.py](../src/win32/installer/build_installer.py): added the
        4 argparse flags and threaded them through as `OpenccConfigPath` /
        `OpenccCharactersPath` / `OpenccPhrasesPath` / `OpenccVariantsPath`
        WiX `-define`s (passed unconditionally for every branding, same as
        the existing Omaha-only vars — harmless no-ops for
        `installer_64bit.wxs`/`installer_oss_64bit.wxs`, which don't
        reference them)
  - [x] [installer_marinamoji_64bit.wxs](../src/win32/installer/installer_marinamoji_64bit.wxs):
        new `opencc\` subdirectory under `MarinaMojiDir` (sibling of
        `marinamoji_server.exe`, matching `GetServerDirectory() + "/opencc/"`)
        with 4 `<Component>`/`<File>` entries + `<ComponentRef>`s added to the
        `MarinaMojiInstall` feature
- [ ] `Ctrl+Shift+3` toggle + config persistence — **blocked on Phase 2**: no
      Windows equivalent of `marina_number_row_dispatcher.cc`
      (Linux)/`KeyCodeMap.mm` (macOS) exists yet in `src/win32/` to route the
      number-row binding into `Session::ToggleTraditionalKanji()`. The
      keymap-TSV alternative (`Ctrl+Shift+F`, stock `ToggleTraditionalKanji`
      command name already in `session/keymap.cc`) should work once
      Phase 1g's TIP key-handling path is confirmed working, since keymap
      dispatch is shared code — no Windows-specific work needed for that path.
- [x] Data files added to installer file list (see bundling item above)

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
| 2026-07-13 | **Phase 3 (OpenCC) build/data wiring implemented**: added `windows`/`oss_windows` to `opencc_rewriter`'s `mozc_select()` (defines `MOZC_USE_OPENCC`, links `@opencc_source//:opencc`, mirroring macOS's static-from-source linking since Windows has no pkg-config); confirmed `SystemUtil::GetServerDirectory()` already handles Windows so `opencc_rewriter.cc` needed no code changes; wired the 4 curated `data/marina_opencc` files into `win32/installer/BUILD.bazel` genrule, `build_installer.py` args/WiX defines, and a new `opencc\` directory in `installer_marinamoji_64bit.wxs`. Not yet compiled (MSVC compatibility of vendored OpenCC C++ unverified); `Ctrl+Shift+3` dispatch blocked on Phase 2 Windows key-handling work. |
| 2026-07-13 | **Phase 2 (marina session features) implemented**: audited all 6 checklist items against shared session/composer/keymap code — 4 needed zero Windows work (keymap TSV rows, kaeriten composer table already present/aligned since 2026-06). Fixed the 2 real gaps: (1) `keyevent_handler.cc`'s `ConvertToKeyEventMain` never set `RIGHT_SHIFT`/`LEFT_SHIFT` in `modifier_keys` (upstream's own unresolved `TODO(yukawa) b/2674446`), silently breaking `Session::IsRightShiftAlone` et al. — fixed via `VK_LSHIFT`/`VK_RSHIFT` keyboard-state checks plus scan-code detection (0x2A/0x36) in the modifier-key-itself-pressed switch case, since keyboard-state snapshots are unreliable on key-up. (2) Added `win32/tip/win32_physical_slot.h/.cc` + `win32/tip/marina_number_row_dispatcher.h/.cc`, porting `unix/ibus`'s scan-code table and dispatch switch near-verbatim (PC/AT scan-code-set-1 and Linux evdev codes are numerically identical), hooked into `tip_keyevent_handler.cc`'s `OnKey` as a new branch before the normal key pipeline; reused shared `session::FindMarinaActionForPhysicalSlot` (added `//win32/tip` to its BUILD.bazel visibility). Not yet compiled/tested on Windows hardware. |
