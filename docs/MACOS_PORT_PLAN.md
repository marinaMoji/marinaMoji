# marinaMozc macOS port plan

Planning and status for the **macOS** build of marinaMozc (Input Method Kit + floating toolbar). Linux/IBus behavior is the reference; this document tracks macOS-specific gaps and work.

For full setup (Xcode, Qt, Bazelisk, `.pkg` installer), see [build_mozc_in_osx.md](build_mozc_in_osx.md). For fork branding on Linux, see [MARINAMOZC.md](MARINAMOZC.md).

## Rebuild and reinstall (quick reference)

Run from your clone’s **`src/`** directory (where `MODULE.bazel` lives), for example:

```bash
cd ~/Code/marinaMozc/src
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
| Unpacked `.app` (use for install) | `bazel-bin/mac/mozc_macos_archive-root/marinaMozc.app` |

There is no `bazel-bin/mac/marinaMozc.app` symlink; `ditto` must use the **archive-root** path (or unzip the zip first).

Optional: build the signed `.pkg` installer instead of copying the `.app` by hand:

```bash
export MOZC_QT_PATH=/opt/homebrew/opt/qt   # if using Qt tools
bazelisk build package --config oss_macos --config release_build
open bazel-bin/mac/Mozc.pkg
```

(Stock installer paths still say `Mozc.app`; manual `.app` install is preferred for marinaMozc until **M2** is fixed.)

### Install over an existing copy

From `~/Code/marinaMozc/src` (after a successful build):

```bash
sudo rm -rf "/Library/Input Methods/marinaMozc.app"
sudo ditto "bazel-bin/mac/mozc_macos_archive-root/marinaMozc.app" \
  "/Library/Input Methods/marinaMozc.app"
```

Alternative: unzip and copy

```bash
cd bazel-bin/mac
unzip -o mozc_macos.zip
sudo rm -rf "/Library/Input Methods/marinaMozc.app"
sudo ditto marinaMozc.app "/Library/Input Methods/marinaMozc.app"
```

### Restart background services

Use LaunchAgents whose `Program` paths point at  
`marinaMozc.app/Contents/Resources/marinaMozcConverter.app` and `marinaMozcRenderer.app`  
(see **M2** below if you have not created these yet).

```bash
launchctl bootout gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist 2>/dev/null
launchctl bootout gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist 2>/dev/null
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist
```

Then toggle marinaMozc off and on in **System Settings → Keyboard → Input Sources**, or log out and back in.

## Goals

1. **Feature parity** with marinaMozc on Linux for historian-focused workflows: shin/kyū (OpenCC), odoriji palette, Manyōshū mode, macron vowels, floating toolbar.
2. **Install side-by-side** with stock Mozc as `marinaMozc.app` under `/Library/Input Methods/`.
3. **Do not break Linux** — macOS changes live under `src/mac/` or `__APPLE__` guards where possible.

## Architecture (short)

| Piece | Role |
|-------|------|
| `marinaMozc.app` | IMK bundle; `MozcImkInputController` handles keys and preedit |
| `marinaMozcConverter.app` | Session server (conversion, keymap, odoriji logic) |
| `marinaMozcRenderer.app` | Candidate window UI |
| `mozc_toolbar.mm` | Non-activating panel; sends session commands via `ClientInterface` |
| `KeyCodeMap.mm` | NSEvent → `KeyEvent` (modifiers + key code) |
| `system://*.tsv` in `Resources/keymap/` | Key bindings per IME state |

Toolbar actions that open the **candidate window** must route through the active `MozcImkInputController` (`sendCommand:` → `processOutput` → `updateCandidates`), not only `client_->SendCommand`. Linux does the same via `MozcEngine::SendToolbarSessionCommand` → `UpdateAll`.

## Completed (recent)

| Item | Notes |
|------|--------|
| `kProductPrefix` → `marinaMozc` on `__APPLE__` | Server/tool paths match `marinaMozc.app` layout |
| OpenCC bundled in app `Resources/opencc` | Shin/kyū conversion on macOS |
| Floating toolbar | Mode, shin/kyū, odoriji, dict, shortcuts popup |
| Keymap: `Ctrl Shift f` alias | macOS sends lowercase letter for Ctrl+Shift+letter; fixes shin/kyū shortcut |
| Kotoeri: shin/kyū in Composition / Precomposition | Same states as MS-IME keymap |
| Kotoeri: odoriji in Composition / Precomposition | `Ctrl+Shift+1` / `2` (and `!` / `@` on US keyboards) |
| Toolbar odoriji → IMK `sendCommand:` | Palette output reaches renderer |

## Testing checklist (after each install)

1. Rebuild and reinstall (commands above).
2. Ensure LaunchAgents point at `marinaMozcConverter` / `marinaMozcRenderer` under `marinaMozc.app/Contents/Resources/` (not `Mozc.app`).
3. Reload agents or log out/in; select marinaMozc in System Settings → Keyboard → Input Sources.
4. Verify:
   - [ ] Japanese conversion (server running)
   - [ ] Toolbar: mode, shin/kyū, odoriji palette, dict, shortcuts
   - [ ] `Ctrl+Shift+F` shin/kyū while composing (Kotoeri / MS-IME)
   - [ ] `Ctrl+Shift+1` default odoriji, `Ctrl+Shift+2` palette while composing
   - [ ] Candidate window F5/F6 behavior unchanged
5. Logs: `~/Library/Logs/marinaMozc/marinaMozc.log`

## Known issues / backlog

### High

| ID | Issue | Suggested fix |
|----|--------|----------------|
| M1 | **Kotoeri Conversion: `Ctrl+Shift+2` duplicate** — both `ShowOdorijiPalette` and `ToggleFullHalfWidth`; last line in TSV wins (palette blocked on keyboard) | Remove or rebind `ToggleFullHalfWidth` on 2/@; keep toolbar odoriji |
| M2 | **Installer LaunchAgents** still reference `Mozc.app` / `MozcConverter` | Rebrand plists in `src/mac/installer/LaunchAgents/` to `marinaMozc` paths |

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

## Keymap notes (macOS)

- Default session keymap on Mac: **Kotoeri** (`config_handler.cc`).
- **Ctrl+letter** shortcuts use lowercase in TSV (`Ctrl j`); they work on Mac.
- **Ctrl+Shift+letter** on Mac sends **lowercase** key code + SHIFT modifier; TSV entries with uppercase letters need a **lowercase alias** (e.g. `Ctrl Shift F` and `Ctrl Shift f`).
- **Digits with Shift** on US layout: bind both `Ctrl Shift 1` and `Ctrl Shift !` (and `2` / `@`) so IBus-style and Mac-style key codes match.

## File map (macOS-specific)

| Path | Purpose |
|------|---------|
| `src/mac/mozc_imk_input_controller.mm` | IMK controller, `processOutput`, renderer |
| `src/mac/mozc_toolbar.mm` | Floating toolbar |
| `src/mac/KeyCodeMap.mm` | Keyboard translation |
| `src/mac/BUILD.bazel` | `marinaMozc` bundle, toolbar, resources |
| `src/data/keymap/kotoeri.tsv` | Default Mac keymap (marinaMozc extensions) |
| `src/base/const.h` | `kProductPrefix` on Apple |

## Related docs

- [ODORIJI_PALETTE.md](ODORIJI_PALETTE.md) — palette behavior (all platforms)
- [SHIN_KYU_TOOLBAR.md](SHIN_KYU_TOOLBAR.md) — shin/kyū UI
- [OPENCC_INTEGRATION.md](OPENCC_INTEGRATION.md) — OpenCC / traditional kanji
- [GTK_TOOLBAR.md](GTK_TOOLBAR.md) — Linux toolbar reference implementation

## Revision log

| Date | Change |
|------|--------|
| 2026-05-27 | Initial plan; Kotoeri odoriji in Composition/Precomposition; document M1–M7 |
