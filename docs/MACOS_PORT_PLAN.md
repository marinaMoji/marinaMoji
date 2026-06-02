# marinaMoji macOS port plan

Planning and status for the **macOS** build of **marinaMoji** (Input Method Kit + floating toolbar). Install bundle: `marinaMoji.app` (see [MARINAMOJI.md](MARINAMOJI.md)). Linux/IBus behavior is the reference; this document tracks macOS-specific gaps and work.

### Upgrading from `marinaMozc.app` (Phase 3 branding)

1. Remove the old IME: `sudo rm -rf "/Library/Input Methods/marinaMozc.app"`
2. Install `marinaMoji.app` (steps below).
3. Remove **marinaMozc** from **System Settings → Keyboard → Input Sources**, add **marinaMoji** again.
4. Optional: migrate settings  
   `mv ~/Library/Application\ Support/marinaMozc ~/Library/Application\ Support/marinaMoji`  
   (only if you used the old trace/support paths; profile data may still be under `Mozc` from earlier builds—see logs under `~/Library/Logs/marinaMoji/` after reinstall).
5. Linux: reinstall package, `ibus write-cache && ibus restart`, re-add **marinaMoji**; config under `~/.config/marinamoji/` (legacy `marinamozc` dirs are used automatically if present).

For full setup (Xcode, Qt, Bazelisk, `.pkg` installer), see [build_mozc_in_osx.md](build_mozc_in_osx.md). For fork branding on Linux, see [MARINAMOJI.md](MARINAMOJI.md).

## Rebuild and reinstall (quick reference)

Run from your clone’s **`src/`** directory (where `MODULE.bazel` lives), for example:

```bash
cd ~/Code/marinaMoji/src
```

### Compile

Homebrew Qt is required for the macOS build (GUI tools are bundled into the IME). Set **`MOZC_QT_PATH`** in the same shell before every `bazel build` (if unset, Bazel uses `third_party/qt`, which is usually empty and fails):

```bash
export MOZC_QT_PATH=/opt/homebrew/opt/qt
bazel build --config oss_macos //mac:mozc_macos
```

If you previously built without `MOZC_QT_PATH`, reconfigure the Qt external repo once:

```bash
export MOZC_QT_PATH=/opt/homebrew/opt/qt
bazel sync --configure
```

Output (this repo’s Bazel rule packages the IME as a zip):

| Artifact | Path |
|----------|------|
| Zip | `bazel-bin/mac/mozc_macos.zip` |
| Unpacked `.app` (use for install) | `bazel-bin/mac/mozc_macos_archive-root/marinaMoji.app` |

There is no `bazel-bin/mac/marinaMoji.app` symlink; `ditto` must use the **archive-root** path (or unzip the zip first).

Optional: build the signed `.pkg` installer instead of copying the `.app` by hand:

```bash
export MOZC_QT_PATH=/opt/homebrew/opt/qt   # if using Qt tools
bazelisk build package --config oss_macos --config release_build
open bazel-bin/mac/marinaMoji.pkg
```

The `.pkg` installs `marinaMoji.app`, LaunchAgents for `marinaMojiConverter` / `marinaMojiRenderer`, and helper symlinks under `/Applications/marinaMoji/`.

### Install over an existing copy

**Run these from `src/`** (where `MODULE.bazel` and `bazel-bin/` live). If you are in the repo root (`marinaMozc/` or `marinaMoji/`), either `cd src` first or prefix paths with `src/` — otherwise `ditto` fails with “Cannot get the real path”.

From `~/Code/marinaMoji/src` (after a successful build):

```bash
cd ~/Code/marinaMoji/src   # adjust clone path; must be the directory that contains bazel-bin/
sudo rm -rf "/Library/Input Methods/marinaMoji.app"
sudo ditto "bazel-bin/mac/mozc_macos_archive-root/marinaMoji.app" \
  "/Library/Input Methods/marinaMoji.app"
./mac/install_launchagents.sh
```

**Important:** copying only the `.app` does **not** start the converter or renderer. You must run `install_launchagents.sh` (above) or install `marinaMoji.pkg`. Without LaunchAgents, Japanese conversion fails.

**After rebranding (`marinaMozc` → `marinaMoji`):** rebuild and reinstall together. An old binary still looks for `/Library/Input Methods/marinaMozc.app/...` (toolbar icons and tools vanish) while LaunchAgents may still point at `marinaMozcConverter` (converter never starts). Check:

```bash
strings "/Library/Input Methods/marinaMoji.app/Contents/MacOS/marinaMoji" | grep "Input Methods"
plutil -p ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist | grep Program
```

Both should show `marinaMoji`, not `marinaMozc`. If not, rebuild `//mac:mozc_macos`, `ditto` again, then `./mac/install_launchagents.sh`.

**Quick workaround (old binary still looking for `marinaMozc.app`):** symlink the install name the binary expects:

```bash
sudo ln -sf "/Library/Input Methods/marinaMoji.app" \
  "/Library/Input Methods/marinaMozc.app"
killall marinaMoji 2>/dev/null
```

Permanent fix: rebuild after `GetServerDirectory()` resolves paths from the running app bundle (see `mac_util.mm`).

Alternative: unzip and copy (still from `src/`):

```bash
cd ~/Code/marinaMoji/src
cd bazel-bin/mac
unzip -o mozc_macos.zip
sudo rm -rf "/Library/Input Methods/marinaMoji.app"
sudo ditto marinaMoji.app "/Library/Input Methods/marinaMoji.app"
cd ../..   # back to src/
./mac/install_launchagents.sh
```

### Restart background services

Use LaunchAgents whose `Program` paths point at  
`marinaMoji.app/Contents/Resources/marinaMojiConverter.app` and `marinaMojiRenderer.app`.

From `src/` after install:

```bash
chmod +x mac/install_launchagents.sh
./mac/install_launchagents.sh
```

Or install `marinaMoji.pkg` (places plists under `/Library/LaunchAgents/`), then log out/in or bootstrap manually:

```bash
launchctl bootout gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist 2>/dev/null
launchctl bootout gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist 2>/dev/null
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist
```

Then toggle **marinaMoji** off and on in **System Settings → Keyboard → Input Sources**, or log out and back in.

## Goals

1. **Feature parity** with marinaMoji on Linux for historian-focused workflows: shin/kyū (OpenCC), odoriji palette, Manyōshū mode, macron vowels, floating toolbar.
2. **Install side-by-side** with stock Mozc as `marinaMoji.app` under `/Library/Input Methods/`.
3. **Do not break Linux** — macOS changes live under `src/mac/` or `__APPLE__` guards where possible.

## Architecture (short)

| Piece | Role |
|-------|------|
| `marinaMoji.app` | IMK bundle; `MozcImkInputController` handles keys and preedit |
| `marinaMojiConverter.app` | Session server (conversion, keymap, odoriji logic) |
| `marinaMojiRenderer.app` | Candidate window UI |
| `mozc_toolbar.mm` | Non-activating panel; sends session commands via `ClientInterface` |
| `KeyCodeMap.mm` | NSEvent → `KeyEvent` (modifiers + key code) |
| `system://*.tsv` in `Resources/keymap/` | Key bindings per IME state |

Toolbar actions that open the **candidate window** must route through the active `MozcImkInputController` (`sendCommand:` → `processOutput` → `updateCandidates`), not only `client_->SendCommand`. Linux does the same via `MozcEngine::SendToolbarSessionCommand` → `UpdateAll`.

## Completed (recent)

| Item | Notes |
|------|--------|
| `kProductPrefix` → `marinaMoji` on `__APPLE__` | Server/tool paths match `marinaMoji.app` layout |
| OpenCC bundled in app `Resources/opencc` | Shin/kyū conversion on macOS |
| Floating toolbar | Mode, shin/kyū, odoriji, symbols palette, dict, shortcuts popup |
| Keymap: `Ctrl Shift f` alias | macOS sends lowercase letter for Ctrl+Shift+letter; fixes shin/kyū shortcut |
| Kotoeri: shin/kyū in Composition / Precomposition | Same states as MS-IME keymap |
| Kotoeri: odoriji in Composition / Precomposition | `Ctrl+Shift+1` / `2` (and `!` / `@` on US keyboards) |
| Toolbar odoriji → IMK `sendCommand:` | Palette output reaches renderer |
| **Single visible input source: marinaMoji** | `Info.plist`: only `com.apple.inputmethod.Japanese` has `tsInputModeIsVisibleKey`; menu icon `marinamoji.tiff`; labels via `InfoPlist.strings` + `tweak_info_plist_strings.py` for `marinaMoji` branding. Katakana / half-width kana / full-width and half-width alphanumeric modes stay registered but hidden (toolbar and shortcuts still switch modes). |
| **Toolbar mode on focus** | `activateServer:` calls `GET_STATUS` so the toolbar matches the server (was stuck on Direct until first key). Toolbar mode menu routes through IMK `sendCommand:` → `processOutput`. |
| **Toolbar solid background** | Replaced `NSVisualEffectView` vibrancy with opaque white / dark gray (`#202328`) matching Linux GTK toolbar. |
| **Symbols palette (macOS)** | Toolbar symbols button opens tabbed palette (Odoriji/Kaeriten/Symbols/User), remembers last tab + pin state per device, and inserts clicked symbols; user strings editable in Preferences. |

## Testing checklist (after each install)

1. Rebuild and reinstall (commands above).
2. Ensure LaunchAgents point at `marinaMojiConverter` / `marinaMojiRenderer` under `marinaMoji.app/Contents/Resources/` (not `Mozc.app`).
3. Reload agents or log out/in; select marinaMoji in System Settings → Keyboard → Input Sources.

### Debug IME freezes / shortcuts (`MARINA_IMK_TRACE`)

If shortcuts beep or Ctrl+Shift+5 freezes the Mac, capture a trace log:

```bash
mkdir -p ~/Library/Application\ Support/marinaMoji
touch ~/Library/Application\ Support/marinaMoji/imk_trace
killall marinaMoji    # IME stays running until killed; required after first touch
# Switch away from marinaMoji and back in Input Sources, then reproduce in TextEdit:
tail -f ~/Library/Logs/marinaMoji/marinaMoji.log | grep marinaImk
```

You should see `[marinaImk] trace enabled pid=…` when the IME loads. If `grep marinaImk` is empty but the log has other `mozc_imk_input_controller` lines, trace was off (IME started before `imk_trace`, or keys not pressed yet).

Look for repeated `processOutput depth=` (loop) or `handleEvent ... no mozc mapping` (beep).
   - If you still see old **Hiragana (Mozc)** rows or multiple mode icons, remove marinaMoji from Input Sources, reinstall, then add it again (macOS caches input-source metadata).
4. Verify:
   - [ ] Input menu shows **one** entry named **marinaMoji** (marina icon), not five Hiragana/Katakana/… rows
   - [ ] Toolbar mode icon matches composition mode **immediately** after switching to marinaMoji (not stuck on Direct until first key)
   - [ ] Japanese conversion (server running)
   - [ ] Toolbar: mode, shin/kyū, odoriji palette, symbols palette, dict, shortcuts
   - [ ] `Ctrl+Shift+3` / `#` shin/kyū while composing (Kotoeri / MS-IME / ATOK)
   - [ ] `Ctrl+Shift+1` default odoriji, `Ctrl+Shift+2` palette while composing
   - [ ] `Ctrl+Shift+4` / `$` Manyōshū toggle, `Ctrl+Shift+5` / `%` hiragana/direct
   - [ ] Candidate window F5/F6 behavior unchanged
5. Logs: `~/Library/Logs/marinaMoji/marinaMoji.log`

## Known issues / backlog

### High

| ID | Issue | Suggested fix |
|----|--------|----------------|
| M1 | **Kotoeri Conversion: `Ctrl+Shift+2` duplicate** — both `ShowOdorijiPalette` and `ToggleFullHalfWidth`; last line in TSV wins (palette blocked on keyboard) | **Resolved**: number-row mappings now use `1` odoriji default, `2` palette, `3` shin/kyū, `4` Manyōshū, `5` hiragana/direct in Kotoeri/MS-IME/ATOK keymaps. |
| M1b | **`Ctrl+Shift+5` freeze when returning from Direct** — `setValue:` / `handleConfig` / `selectInputMode` re-entry | Mitigations: no `switchDisplayMode` from keys; `setValue:` skips server + `handleConfig`; 200ms `setValue` suppress after keyboard mode change; `processOutput` depth limit. **Debug:** `MARINA_IMK_TRACE=1` → `~/Library/Logs/marinaMoji/marinaMoji.log` |
| M1c | **Ctrl+Shift+1–4 beep on Dvorak/AZERTY** | Fixed: physical number-row mapping runs before empty-`characters` check in `KeyCodeMap.mm` |
| M2 | ~~Installer LaunchAgents / `.pkg` paths~~ | **Done:** plists, postflight, `tweak_installer_files.py`, and `marinaMoji.pkg` use `marinaMoji` paths |

### Medium

| ID | Issue | Suggested fix |
|----|--------|----------------|
| M3 | **Right Shift → Manyōshū** — keymap has `RightShift`; Mac `KeyCodeMap` does not set `RIGHT_SHIFT` | Map right shift key in `KeyCodeMap.mm` (see Linux `IBUS_Shift_R`) |
| M4 | **Toolbar mode menu** uses `client_->SendCommand` only | Route through active controller `sendCommand:` for full `processOutput` sync |
| M5 | **Macron `Ctrl+Alt+Shift+Letter`** — TSV may use uppercase; Mac sends lowercase with modifiers | Add lowercase aliases (same pattern as `Ctrl Shift f`) |

### Low / by design

| ID | Issue | Notes |
|----|--------|--------|
| M6 | **⌘ Command** combos ignored | `KeyCodeMap.mm` returns NO when Command is held |
| M7 | **Candidate window position** | Some apps return bad cursor rects; same class of issue as stock Mozc |
| M8 | **Input menu icon vs composition mode** | With one visible TIS mode (or only “Hiragana” installed), the menu bar icon stays on that mode’s TIFF (orange hiragana on stock Mozc). Mode changes are shown on the **toolbar**, not by swapping the system menu icon. Hidden sub-modes + `selectInputMode:` may not update the visible icon on recent macOS. |

## Keymap notes (macOS)

- Default session keymap on Mac: **Kotoeri** (`config_handler.cc`).
- **Ctrl+letter** shortcuts use lowercase in TSV (`Ctrl j`); they work on Mac.
- **Ctrl+Shift+letter** on Mac sends **lowercase** key code + SHIFT modifier; TSV entries with uppercase letters need a **lowercase alias** (e.g. `Ctrl Shift F` and `Ctrl Shift f`).
- **Digits with Shift** on US layout: bind both `Ctrl Shift 1` and `Ctrl Shift !` (and `2` / `@`) so IBus-style and Mac-style key codes match.
- **Number-row shortcuts (macOS):** `KeyCodeMap` maps **physical** `kVK_ANSI_1`..`0` + Ctrl+Shift to digit `1`..`0` so Dvorak / AZERTY / custom layouts match QWERTY keymap rows (`Ctrl Shift 1` = odoriji, `3` = shin/kyū, `4` = Manyōshū, `5` = hiragana/direct).
- **marinaMoji (Kotoeri):** `Ctrl+Shift+3` / `#` → shin/kyū (`ToggleTraditionalKanji`); `Ctrl+Shift+4` / `$` → hiragana/Manyōshū (`ToggleManyoshuHiragana`). `Ctrl+Shift+5` / `%` → hiragana/direct toggle.
- **Aligned keymaps:** `ms-ime.tsv` and `atok.tsv` now follow the same number-row mapping (`1` odoriji default, `2` palette, `3` shin/kyū, `4` Manyōshū, `5` hiragana/direct, with shifted symbol variants).

## File map (macOS-specific)

| Path | Purpose |
|------|---------|
| `src/mac/mozc_imk_input_controller.mm` | IMK controller, `processOutput`, renderer |
| `src/mac/mozc_toolbar.mm` | Floating toolbar |
| `src/mac/KeyCodeMap.mm` | Keyboard translation |
| `src/mac/BUILD.bazel` | `marinaMoji` bundle, toolbar, resources |
| `src/data/keymap/kotoeri.tsv` | Default Mac keymap (marinaMoji extensions) |
| `src/base/const.h` | `kProductPrefix` on Apple |

## Related docs

- [ODORIJI_PALETTE.md](ODORIJI_PALETTE.md) — palette behavior (all platforms)
- [SYMBOLS_PALETTE.md](SYMBOLS_PALETTE.md) — macOS tabbed symbols palette
- [SHIN_KYU_TOOLBAR.md](SHIN_KYU_TOOLBAR.md) — shin/kyū UI
- [OPENCC_INTEGRATION.md](OPENCC_INTEGRATION.md) — OpenCC / traditional kanji
- [GTK_TOOLBAR.md](GTK_TOOLBAR.md) — Linux toolbar reference implementation

## Revision log

| Date | Change |
|------|--------|
| 2026-05-27 | Initial plan; Kotoeri odoriji in Composition/Precomposition; document M1–M7 |
| 2026-05-27 | Single visible IME **marinaMoji** in input menu (hidden secondary TIS modes) |
