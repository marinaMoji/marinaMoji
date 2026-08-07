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
- **Phase 5 (Sync) landed 2026-07-13** — new `marinamoji_sync.exe` binary
  target, a per-user Task Scheduler logon task (`win32/base/task_scheduler_util`
  + two new custom actions) replacing the LaunchAgent/systemd-user-unit
  mechanism from mac/Linux, and confirmation that the config-dialog Sync tab
  and all sidecar file paths (`sync.conf`, `.sync_key`, etc.) already work
  cross-platform with zero Windows-specific code.
- **Update (2026-08-07): Phase 4 (floating toolbar) landed**, including the
  Symbols Palette and Keyboard Shortcuts windows this section originally
  deferred — see the Phase 4 checklist below. Sync-lock keystroke blocking
  (`win32/base/sync_lock_util`) plus a renderer sync overlay, the Windows
  self-hosted auto-updater (`renderer/win32/marina_auto_update.{h,cc}`,
  mirroring macOS), and the uppercase-macron-vowel fix
  (`Ctrl+Alt+Shift+A/E/I/O/U`) also landed the same day. All of Phases 1-6
  now have code on `feature/windows`; **none of it has a full on-hardware
  pass yet** — a manual QA pass against the internal testing checklist
  (tracked outside this public repo) is the actual blocking item now that CI
  builds green.
- **Phases 1-6 are all implemented as code — CI confirms the build itself
  compiles and packages; real-hardware functional verification of the
  features above is still outstanding.**

---

# Phase 1 — Branding & build bring-up

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
- [x] IME icon: still the stock `product_icon.ico`/`product_icon_langbar.ico`.
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

### 1g. Phase 1 acceptance test (on Windows, VM fine)  ← **current phase**

- [x] `bazelisk build --config oss_windows --config release_build package`
      from `src/` succeeds (2026-07-14) — produces
      `bazel-bin/win32/installer/marinaMoji64.msi`. Needed `update_deps.py`
      to actually finish (LLVM archive was cached but never extracted;
      `dotnet tool restore` needs `dotnet` on `PATH`) and `BAZEL_VC` set in
      the invoking shell to the VS install's `VC` dir (`--action_env=BAZEL_VC`
      only passes it through, doesn't supply a value). Along the way, fixed
      several real bugs this was the first build to ever exercise: a missing
      `//win32/tip` visibility grant on `composer:kaeriten_table_util`;
      libsodium's runtime-dispatched SIMD sources needing per-file
      target-feature flags (`.bazelrc`) plus a required `-DSODIUM_STATIC`
      (its Windows headers default to `dllimport`, which appears to have
      crashed `lld-link` at libsodium's scale); a non-existent
      `WICCreateImagingFactory_Proxy` API (renderer already initializes COM
      in `main()`, so switched to plain `CoCreateInstance`); a vendored
      rapidjson v1.1.0 bug (assignment through two `const` members, fixed via
      placement-new `patch_cmds`); `SetEnvironmentVariableA` called with a
      stray 3rd arg copied from the POSIX `setenv()` branch; a private
      `ButtonId` enum used at file scope; `<shlwapi.h>` (pulled in via
      `atlbase.h`) `#define`-ing `StrCat` to `StrCatW`, silently breaking
      every `absl::StrCat` call in `toolbar_window.cc` (fixed with `#undef
      StrCat`); a wrong nested-type qualifier (`RendererCommand::
      ApplicationInfo::SymbolsPaletteInfo` instead of `RendererCommand::
      SymbolsPaletteInfo`); `KeyEventHandler::MaybeSpawnTool` left `protected`
      after being wired into an external caller; and a literal `--daemon` in
      an XML comment (illegal per the XML spec) in the installer `.wxs`.
      **Not yet installed/run** — next is actually installing the MSI and
      working through the rest of this checklist.
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

- [x] **Host process and UI stack (2026-07-13)**: renderer
      (`marinamoji_renderer.exe`), raw Win32/GDI (no Qt/WinUI/MSIX — see
      new `ToolbarWindow` class, `src/renderer/win32/toolbar_window.{h,cc}`),
      matching this doc's original proposal and the rest of the renderer.
- [x] **Renderer IPC (2026-07-13)**: no new `ToolbarInfo` message needed —
      `RendererCommand` already carries the full `commands::Output` at top
      level (`command.output()`, set in `tip_ui_handler_conventional.cc`'s
      `UpdateCommand()`), so `ToolbarWindow::OnUpdate` reads composition
      mode/shin-kyū directly from it, mirroring mac's `MozcToolbarUpdate
      (output)`. Added a `ShowToolbar` bit to `ApplicationInfo::UIVisibility`
      (`protocol/renderer_command.proto`) for visibility gating, set in
      `FillVisibility()` and cleared on thread-focus-loss in
      `tip_ui_handler_conventional.cc`'s `UpdateCommand()`.
- [x] **Toolbar window (2026-07-13)**: `WS_POPUP` + `WS_EX_LAYERED |
      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST` (layered rather
      than a DWM region/attribute, so rounded corners are antialiased in the
      composited bitmap itself — consistent across Win10/11 with no version
      branch needed). Light/dark from `AppsUseLightTheme` +
      `WM_SETTINGCHANGE`.
- [x] **Positioning (2026-07-13)**: bottom-right of primary work area via
      `GetWorkingAreaFromPoint` (existing `win32_renderer_util` helper),
      draggable via `WM_NCHITTEST` → `HTCAPTION` on background pixels,
      position persisted to `toolbar.conf` under
      `SystemUtil::GetUserProfileDirectory()` (plain `x=`/`y=` text, own
      tiny parser — no plist/GKeyFile dependency).
- [x] **Icons (2026-07-13)**: `toolbar_icons/*.svg` pre-rendered to PNG at
      24/36/48px by `src/data/images/win/generate_toolbar_icons.py` (using
      `resvg-py`, a pure-Python/Rust-wheel SVG renderer — no system rsvg-
      convert/cairo needed), committed to `src/data/images/win/toolbar_icons/`
      and shipped as loose files (matching the OpenCC data convention, not
      `.rc` resource-embedding) so they load from a directory at runtime like
      mac/Linux. New `LoadPngFileToHBitmap` WIC-based loader in
      `win32_image_util.{h,cc}` (no PNG/SVG decoding existed on Windows
      before this). Buttons: mode indicator, shin/kyū toggle, dict, settings
      are fully wired; symbols palette and shortcuts viewer buttons were
      **rendered visually disabled** at this point (reduced opacity, inert)
      since those windows didn't exist on Windows yet. **Update
      (2026-08-07): both now exist and both buttons are live** —
      `SymbolsPaletteWindow` and `ShortcutsWindow` shipped later in this same
      phase (see their own checklist items below and in the toolbar parity
      pass), so this is no longer a deferred gap.
- [x] **Button actions → session commands (2026-07-13)**: reused the
      existing candidate-click round-trip (`RendererServerSendCommand` →
      `PostMessage` to the TIP's renderer-callback window → `TipEditSession::
      OnRendererCallbackAsync` → `client::Client::SendCommand`). Extended
      `RendererServerSendCommand`'s allowlist (`renderer/renderer_server.cc`)
      and `OnRendererCallbackAsync` (`win32/tip/tip_edit_session.cc`) to also
      carry `SWITCH_COMPOSITION_MODE` (mode packed into the `id` field, since
      this channel only carries `(type, id)`), `TURN_OFF_IME`,
      `TOGGLE_TRADITIONAL_KANJI`, `LAUNCH_WORD_REGISTER_DIALOG`, and a new
      `LAUNCH_CONFIG_DIALOG` `SessionCommand` type (added since Windows only
      had a keymap-triggered path to `Session::LaunchConfigDialog`, no
      `SessionCommand` entry point — needed since the toolbar can only send
      `SessionCommand`s, not raw `KeyEvent`s). Also wired
      `KeyEventHandler::MaybeSpawnTool` into the `SessionCommand` response
      path (`AsyncSessionCommandEditSessionImpl::DoEditSession`), which
      previously only ran after `SendKey` — so `Output::launch_tool_mode`
      now actually spawns tools for *any* `SessionCommand`, not just the
      toolbar's.
- [x] **Symbols palette (2026-07-13)**: new `SymbolsPaletteWindow`
      (`renderer/win32/symbols_palette_window.{h,cc}`) with a native
      `SysTabControl32` (4 tabs) + native `BUTTON` children per symbol —
      unlike the toolbar, no custom GDI compositing needed since symbol
      buttons are just system-font glyphs, not icons. `WS_EX_TOOLWINDOW |
      WS_EX_NOACTIVATE` so it never steals focus, matching mac's
      `NSWindowStyleMaskNonactivatingPanel`. Odoriji tab reuses the existing
      `SHOW_ODORIJI_PALETTE` + `SUBMIT_CANDIDATE(id)` session round-trip
      unchanged; Kaeriten/Symbols/User tabs needed a new transport since the
      renderer→TIP channel only carried `(type, id)`, no arbitrary text:
  - Added `SessionCommand::INSERT_SYMBOL_TEXT` (`protocol/commands.proto`) →
    `Session::InsertSymbolText` → the already-generic
    `CommitStringDirectly` (no new validation, unlike `InsertMacronVowel`).
  - Sent renderer→TIP via `WM_COPYDATA` (`RendererServerSendCommand::
    SendCommand` in `renderer/renderer_server.cc`, tagged with
    `kSymbolTextCopyDataTag` in `base/const.h`) rather than the existing
    `PostMessage`-based `(type, id)` channel, which can't carry a string.
    `WM_COPYDATA` is on Windows' default UIPI-allowed message list, so
    (unlike the custom registered message) no `ChangeMessageFilter` call
    was needed. Received in `TipTextService`'s `RendererCallbackWidnowProc`
    → new `TipEditSession::OnRendererSymbolTextCallbackAsync`.
  - Palette *content* stays TIP-side (keeps the renderer a "dumb" UI
    process, no `composer`/config-parsing pulled into it): a new
    `SymbolsPaletteInfo` message on `RendererCommand.ApplicationInfo`
    (`protocol/renderer_command.proto`) carries Kaeriten symbols (via the
    already-shared `composer::LoadKaeritenShortcutEntries`) and User
    symbols (`user_symbols.txt` under `SystemUtil::GetUserProfileDirectory
    ()`, mirroring mac's `LoadUserSymbolsFromFile()`); Odoriji/general-
    symbols lists are static and hardcoded renderer-side instead. Populated
    only while `TipPrivateContext::symbols_palette_visible()` is true, a
    pure local UI flag (mirrors mac's `g_symbols_palette_visible`) toggled
    by two new local-only `SessionCommand`s, `SHOW_SYMBOLS_PALETTE`/
    `HIDE_SYMBOLS_PALETTE`, intercepted in `OnRendererCallbackAsync`
    *before* reaching the session/converter at all.
  - Toolbar's Symbols button enabled (`toolbar_window.cc`), sends
    `SHOW_SYMBOLS_PALETTE`. Shortcuts button remains disabled (separate,
    smaller follow-up).
  - Also fixed a pre-existing bug while in this area: `ToolbarWindow`
    referenced `kToolbarWindowClassName` (`DECLARE_WND_CLASS_EX`) without
    it ever being defined anywhere — added it (and the new
    `kSymbolsPaletteWindowClassName`) to `base/const.h` alongside the
    other window class name constants, and added the missing
    `//base:const` dep to `toolbar_window`'s `BUILD.bazel` target.
- [x] **Show/hide (2026-07-13)**: shown whenever the TSF thread has focus
      (`ShowToolbar` bit), hidden immediately on focus loss — no 150 ms
      delayed-hide like GTK's (not yet ported; low priority since Windows
      focus transitions are typically less bouncy than X11's).
- [x] **Multi-monitor + DPI (2026-07-13)**: icon tier (24/36/48px) and layout
      chosen from `GetDpiForPoint`, refreshed on `WM_DISPLAYCHANGE` /
      monitor moves — not yet stress-tested across mixed-DPI multi-monitor
      setups on real hardware.

# Phase 5 — Sync

Sync is a self-contained encrypted-file mechanism (libsodium + vendored
miniz; no transport of its own — the user points it at a folder already
synced by Nextcloud/Syncthing/OneDrive/etc.), the same binary/code on every
platform, just run under a different per-user scheduling mechanism.

- [x] **Windows sync binary (2026-07-13)**: added `kMozcSyncExecutable =
      "marinamoji_sync.exe"` to [const.h](../src/base/const.h)'s `_WIN32`
      branch (`sync_util.cc`'s existing generic
      `SystemUtil::GetServerDirectory() + kMozcSyncExecutable` path needed no
      code changes — it already handles Windows). Added
      `marinaMojiSync_win` to [sync/BUILD.bazel](../src/sync/BUILD.bazel) via
      `mozc_win32_cc_prod_binary`, deliberately named `marinamoji_sync.exe`
      for **every** `BRANDING` value (matching how `marinaMojiSync_bin`/
      `_macos` are already unbranded fixed names on Linux/macOS — sync has no
      stock-Mozc equivalent to collide with). No Windows-specific `.rc`
      version resource yet (matches Linux's `marinaMojiSync_bin`, which also
      has none) — deferred, same category as the Phase 1 IME icon.
- [x] **Process model decision (2026-07-13): per-user Task Scheduler logon
      task, not a Windows Service.** `cache_service` is LocalSystem-context
      and machine-wide — wrong fit, since sync needs the *logged-on user's*
      profile (`%LOCALAPPDATA%\marinaMoji\sync.conf`, `.sync_key`, and
      whatever folder the user mapped) exactly like the LaunchAgent (macOS,
      `gui/<uid>` domain) and systemd **user** unit (Linux) it mirrors — both
      explicitly refuse to run as root/sudo for the same reason. Kept the
      existing `--daemon` sleep-loop/poll model (no rewrite of
      `sync_poll.cc`/`sync_activity.cc`, both already fully portable) rather
      than re-triggering `--now` every N minutes via the scheduler, for
      maximum code/behavior parity with mac/Linux.
  - [x] [win32/base/task_scheduler_util.h/.cc](../src/win32/base/task_scheduler_util.cc):
        new COM wrapper around `ITaskService`/`ITaskFolder`/`ITaskDefinition`
        (`taskschd.h`) — `RegisterLogonTask` creates a `TASK_TRIGGER_LOGON`
        trigger (30s delay so the user's sync client has time to mount the
        folder) + `TASK_ACTION_EXEC` action running
        `marinamoji_sync.exe --daemon`, `TASK_LOGON_INTERACTIVE_TOKEN` (no
        stored password, runs only while logged on — the direct Windows
        analog of LaunchAgent/systemd-user's session-scoping), 3 restarts on
        failure; `UnregisterTask` removes it, tolerating "task doesn't
        exist". `linkopts = ["/DEFAULTLIB:taskschd.lib"]` in
        [win32/base/BUILD.bazel](../src/win32/base/BUILD.bazel).
  - [x] New custom actions `RegisterSyncTask`/`UnregisterSyncTask` in
        [custom_action.cc](../src/win32/custom_action/custom_action.cc) +
        `.def` exports, `Impersonate="yes"` (registers/removes the task under
        the installing user's own account, matching the existing
        `EnableTipProfile`/`RestoreUserIMEEnvironment` pattern — not a new
        WiX extension). Wired into
        [installer_marinamoji_64bit.wxs](../src/win32/installer/installer_marinamoji_64bit.wxs)'s
        `InstallExecuteSequence`: `RegisterSyncTask` before `InstallFinalize`
        on every non-removal install (including upgrades, unlike
        `EnableTipProfile` — registration is idempotent via
        `TASK_CREATE_OR_UPDATE`); `UnregisterSyncTask` after
        `RestoreUserIMEEnvironment` on uninstall. New `MarinaMojiSync`
        `<Component>`/`<File>` alongside `marinamoji_broker.exe` under
        `MarinaMojiDir`, threaded through
        [installer/BUILD.bazel](../src/win32/installer/BUILD.bazel) and
        [build_installer.py](../src/win32/installer/build_installer.py) as
        `MarinaMojiSyncPath` (same per-file pattern as the OpenCC data files).
  - [ ] **Untested**: chose `TASK_LOGON_INTERACTIVE_TOKEN` with no explicit
        `IPrincipal::UserId` (documented to mean "the user calling
        `RegisterTaskDefinition`") based on MSDN docs, not a real install —
        first real signal is Phase 1g. Also flagged: RDP/Fast-User-Switching
        sessions may behave differently for logon triggers (noted as a
        manual-QA follow-up, not a regression vs. mac/Linux's own
        GUI-session-scoping caveats).
- [x] `sync.conf`/`.sync_key`/`sync.status.json`/`sync.activity.json` sidecar
      paths — confirmed all resolve via
      `SystemUtil::GetUserProfileDirectory()`, already Windows-aware; no
      changes needed. `sync_key.cc`'s only platform conditional (POSIX
      `chmod(0600)`) was already guarded `#if !defined(_WIN32)`.
- [x] **Config dialog sync tab — confirmed zero Windows-specific work
      needed.** [gui/config_dialog/BUILD.bazel](../src/gui/config_dialog/BUILD.bazel)'s
      main `config_dialog` target already unconditionally builds
      `config_dialog_sync_tab.cc` and depends on `//sync:sync_config`,
      `//sync:sync_key`, `//sync:sync_status`, `//sync:sync_util` for every
      platform (no `mozc_select` gating on these) — the folder-picker/enable
      toggle/generate-key UI in `marinamoji_tool.exe`'s Preferences should
      "just work" once `sync/` compiles with MSVC.
- [x] **Input-blocking during active sync (2026-08-07): implemented.**
      `win32/base/sync_lock_util.{h,cc}` guards both `OnTestKey` and `OnKey`
      in `win32/tip/tip_keyevent_handler.cc` (proactive poll of
      `sync.status.json` plus a reactive `Output::SYNC_LOCKED` catch),
      `NotifySyncBlockedInput()` beeps on a blocked keystroke, and
      `renderer/win32/sync_overlay_window.cc` shows the "synchronising…"
      overlay — mirroring mac/Linux's Tests 7-8. Not yet exercised on real
      Windows hardware.
- [ ] [SYNC_MANUAL_QA.md](SYNC_MANUAL_QA.md) needs a Windows section added
      (mirroring its existing per-platform checklists) covering the above,
      once real-hardware testing (not just CI) is available.

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
| 2026-07-13 | **Phase 5 (Sync) implemented**: `kMozcSyncExecutable = "marinamoji_sync.exe"` added to `const.h` (Windows' generic `sync_util.cc` path resolution needed no changes); new `marinaMojiSync_win` binary target in `sync/BUILD.bazel`, deliberately unbranded (same name for every `BRANDING`) matching Linux/macOS precedent. Process-model decision: per-user Task Scheduler logon task, not a Windows Service — mirrors the LaunchAgent/systemd-user-unit per-user-session model (sync needs the logged-on user's profile/mapped folders, which a LocalSystem service can't reach cleanly). New `win32/base/task_scheduler_util.h/.cc` (COM `ITaskService`/`ITaskFolder`/`ITaskDefinition` wrapper: `TASK_TRIGGER_LOGON` + `TASK_ACTION_EXEC` running `--daemon`, `TASK_LOGON_INTERACTIVE_TOKEN`) plus two new `Impersonate="yes"` custom actions (`RegisterSyncTask`/`UnregisterSyncTask`, mirroring the existing `EnableTipProfile` pattern — no new WiX extension needed) wired into `installer_marinamoji_64bit.wxs`'s `InstallExecuteSequence` and a new `MarinaMojiSync` file component. Confirmed the config-dialog Sync tab and all sync sidecar files (`sync.conf`, `.sync_key`, etc.) already work cross-platform with zero additional Windows code. Not yet compiled/tested on Windows hardware; Task Scheduler registration semantics (`TASK_LOGON_INTERACTIVE_TOKEN` + unset `UserId`) are based on MSDN docs, not a real install. |
