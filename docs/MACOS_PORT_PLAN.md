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

From your clone’s **`src/`** directory (where `MODULE.bazel` lives):

```bash
cd ~/Code/marinaMozc/src
export MOZC_QT_PATH=/opt/homebrew/opt/qt
bazelisk build --config=oss_macos //mac:mozc_macos
bash mac/install_marinamoji.sh
```

`install_marinamoji.sh` copies `bazel-bin/mac/mozc_macos_archive-root/marinaMoji.app`, installs LaunchAgents, registers the IME, runs the Qt path/sign fix (`fix_qt_bundled_paths.sh`), verifies the converter binary changed, and restarts IMK/converter processes.

Manual equivalent (not recommended — easy to skip Qt fix or registration):

```bash
sudo ditto bazel-bin/mac/mozc_macos_archive-root/marinaMoji.app "/Library/Input Methods/marinaMoji.app"
bash mac/install_launchagents.sh
bash mac/register_marinamoji.sh
sudo bash mac/fix_qt_bundled_paths.sh "/Library/Input Methods/marinaMoji.app" "-"
killall imklaunchagent TextInputMenuAgent marinaMoji marinaMojiConverter marinaMojiRenderer 2>/dev/null || true
```

For a **VM or second Mac**, build and install the pkg instead (bundles Qt deps for machines without Homebrew):

```bash
bazelisk build --config=oss_macos --spawn_strategy=local //mac:package
open bazel-bin/mac/marinaMoji.pkg
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

| Artifact | Path (relative to `src/`) |
|----------|---------------------------|
| Zip | `bazel-bin/mac/mozc_macos.zip` |
| Unpacked `.app` (use for install) | `bazel-bin/mac/mozc_macos_archive-root/marinaMoji.app` |
| Installer `.pkg` (separate build; see below) | `bazel-bin/mac/marinaMoji.pkg` |

There is no `bazel-bin/mac/marinaMoji.app` symlink; `ditto` must use the **archive-root** path (or unzip the zip first).

**`bazel-bin/` is easy to miss:** it lives under **`src/`** (same directory as `MODULE.bazel`), not at the repo root. It is listed in `.gitignore`, so **Cursor and most project file trees will not show it** — use Terminal to list or open artifacts.

### `.pkg` installer (optional)

The app build (`//mac:mozc_macos`) does **not** produce the installer. Build the pkg separately:

```bash
cd ~/Code/marinaMozc/src   # must be src/ — adjust clone path
export MOZC_QT_PATH=/opt/homebrew/opt/qt   # if using Homebrew Qt
bazelisk build --config=oss_macos --spawn_strategy=local //mac:package
```

On macOS 26+, if Bazel fails with an `xcode-locator` / `kLSExecutableIncorrectFormat` error, `tools/local_xcode/BUILD` should pin `version` to the absolute developer dir (e.g. `/Applications/Xcode.app/Contents/Developer`), not only `26.5.0.17F42`. Regenerate with `bash mac/fix_bazel_xcode_config.sh --write-local`, then rebuild.

| Artifact | Path (relative to `src/`) |
|----------|---------------------------|
| Installer pkg | `bazel-bin/mac/marinaMoji.pkg` |

Example absolute path:

```text
~/Code/marinaMozc/src/bazel-bin/mac/marinaMoji.pkg
```

Verify and install:

```bash
cd ~/Code/marinaMozc/src
ls -lh bazel-bin/mac/marinaMoji.pkg
open bazel-bin/mac/marinaMoji.pkg
```

Copy somewhere visible (optional):

```bash
cp bazel-bin/mac/marinaMoji.pkg ~/Desktop/
```

The `.pkg` installs `marinaMoji.app`, LaunchAgents for `marinaMojiConverter`, `marinaMojiRenderer`, and `marinaMojiSync`, plus helper symlinks under `/Applications/marinaMoji/`.

**Dev install without the pkg:** from `src/`, after `//mac:mozc_macos` succeeds:

```bash
bash mac/install_marinamoji.sh
```

That copies `bazel-bin/mac/mozc_macos_archive-root/marinaMoji.app`, runs `install_launchagents.sh`, `register_marinamoji.sh`, and `fix_qt_bundled_paths.sh` (required for Preferences / Dictionary Tool on a clean Mac).

### Second Mac / VM: Preferences / Qt GUI tools

OSS builds are **not notarized** and use **ad-hoc signing** (`codesign -s -`). That is normal for local development. Installing from a **shared folder or USB** usually avoids Gatekeeper quarantine; you typically do **not** need the Apple Developer Program for your own dev Mac and VM.

**Three different checks (don’t confuse them):**

| Check | What it blocks |
|-------|----------------|
| **Gatekeeper / quarantine** | First open of software **downloaded from the internet** |
| **Runtime signature (AMFI/dyld)** | Loading a **tampered** library (common Preferences crash on VM) |
| **Developer ID + notarization** | Smooth installs for **strangers** downloading your pkg |

#### Build the pkg (recommended for VM)

From `src/`:

```bash
export MOZC_QT_PATH=/opt/homebrew/opt/qt
bazelisk build --config=oss_macos --spawn_strategy=local //mac:package
```

Output: `bazel-bin/mac/marinaMoji.pkg`. The pkg pipeline runs `bundle_qt_deps.sh` (ICU, glib, QtDBus, …) and `fix_qt_bundled_paths.sh` (paths + **framework re-sign**).

#### “Can’t be opened because of a problem” / immediate crash

**Cause A — missing libraries (no Homebrew on VM):** Qt binaries still pointed at `/opt/homebrew/...`. Fixed by bundling in the pkg (2026-06).

**Cause B — invalid signature:** Crash report shows `SIGKILL (Code Signature Invalid)` or `codesign --verify` fails on `QtGui.framework`. `install_name_tool` invalidates signatures; frameworks must be re-signed individually, not only the outer `.app`.

**Verify on the VM:**

```bash
CD="/Library/Input Methods/marinaMoji.app/Contents/Resources/ConfigDialog.app"
codesign --verify --deep --strict --verbose=2 "$CD"
```

**Re-sign on the VM without reinstalling** (temporary test fix):

```bash
CD="/Library/Input Methods/marinaMoji.app/Contents/Resources/ConfigDialog.app"
FW="$CD/Contents/Frameworks"
find "$FW" -name "_CodeSignature" -exec rm -rf {} + 2>/dev/null
for fw in QtCore QtDBus QtGui QtPrintSupport QtWidgets; do
  codesign --force --sign - "$FW/${fw}.framework/Versions/A/${fw}" 2>/dev/null
  codesign --force --sign - "$FW/${fw}.framework"
done
for dylib in "$FW"/*.dylib; do [[ -f "$dylib" ]] && codesign --force --sign - "$dylib"; done
codesign --force --sign - "$CD/Contents/Resources/plugins/platforms/libqcocoa.dylib"
codesign --force --sign - "$CD"
```

**Fix after install from older builds:**

```bash
sudo bash "/Library/Input Methods/marinaMoji.app/Contents/Resources/fix_qt_bundled_paths.sh" \
  "/Library/Input Methods/marinaMoji.app" "-"
```

Dev install (`install_marinamoji.sh`) and pkg postinstall run the same script automatically on new builds.

**Also check:**

- **Architecture:** pkg built on Apple Silicon is `arm64` only. An Intel VM needs an Intel build, or an Apple Silicon VM with arm64 macOS.
- **Quarantine** (if you downloaded the pkg in a browser on the VM): `xattr -dr com.apple.quarantine ~/Downloads/marinaMoji.pkg` before installing.
- **Launch Preferences from Terminal:**

```bash
"/Library/Input Methods/marinaMoji.app/Contents/Resources/ConfigDialog.app/Contents/MacOS/ConfigDialog"
```

**One-time Gatekeeper bypass:** Control-click `ConfigDialog.app` in Finder → **Open** → confirm.

For production installs on other people’s Macs you would need Apple Developer ID signing, notarization, and stapling. Long-term we may switch from Homebrew Qt to `build_qt.py` (upstream Mozc path); framework re-sign after deploy remains required either way.

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
bash ./mac/install_launchagents.sh
```

### Restart background services

Use LaunchAgents whose `Program` paths point at  
`marinaMoji.app/Contents/Resources/marinaMojiConverter.app` and `marinaMojiRenderer.app`.

From `src/` after install:

```bash
./mac/install_launchagents.sh
```

If you see `permission denied`, either run `chmod +x mac/install_launchagents.sh` once, or use `bash mac/install_launchagents.sh` (do **not** use `sudo`).

Or install `marinaMoji.pkg` (places plists under `/Library/LaunchAgents/`), then log out/in or bootstrap manually:

```bash
launchctl bootout gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist 2>/dev/null
launchctl bootout gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist 2>/dev/null
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist
```

Then toggle **marinaMoji** off and on in **System Settings → Keyboard → Input Sources**, or log out and back in.

### macOS 26+ `InputMethodConnectionName` (IMK “Refusing connection name”)

On **macOS 26+**, `imklaunchagent` rejects the legacy Mozc name `marinaMoji_1_Connection` / `Mozc_1_Connection`.  
`Info.plist` must use the **bundle-ID form**:

```xml
<key>InputMethodConnectionName</key>
<string>org.mozc.inputmethod.Japanese_Connection</string>
```

Symptoms if wrong: `Refusing connection name for bundle: unrecognized 'InputMethodConnectionName' value`, `LaunchInputMethod() Error, status=-50`, no conversion. Rebuild and reinstall after changing `src/mac/Info.plist`.

### HIToolbox reset (IMK “Refusing connection name” / NO Endpoint)

**Warning:** `defaults delete com.apple.HIToolbox` removes **your entire Input Sources list** — U.S./ABC, Dvorak, Chinese, emoji palette, everything — not just marinaMoji. macOS stores all keyboard/IME choices in that one domain. Export a backup first (`defaults export com.apple.HIToolbox ~/Desktop/hitoolbox.backup.plist`).

Adding marinaMoji **once** registers **six internal modes** (hiragana, katakana, half-width kana, etc.) in `com.apple.inputsources`. Only **one** should appear in the menu (`tsInputModeIsVisibleKey`). If you add marinaMoji repeatedly while debugging, System Settings can show **many duplicate rows** (e.g. 13) — remove **all** of them, clear `inputsources`, then add **once**.

Symptoms: spinning wheel, no conversion, toolbar stuck, and logs like:

```bash
/usr/bin/log show --last 2m --style compact --predicate 'process == "imklaunchagent"' \
  | rg -i 'Refusing connection name|NO Endpoint'
```

This is **macOS input-source state**, not marinaMoji source code. A partial reset (only `com.apple.HIToolbox.plist`) is **not enough**:

- **`com.apple.inputsources.plist`** holds third-party IME entries separately; stale marinaMoji rows can remain while HIToolbox no longer lists marinaMoji as enabled.
- **`~/Library/Keyboard Layouts/dpm.bundle`** is auto-discovered on every login. macOS re-adds **dpm** to Input Sources until that bundle is moved aside.

From `src/` (recommended — **deletes marinaMoji.app**, scrubs org.mozc registry, quarantines dpm, wipes all Input Sources prefs):

```bash
bash ./mac/reset_hitoolbox_for_marinamoji.sh --full
```

**Critical:** `--full` removes `marinaMoji.app` from `/Library/Input Methods/`. If the app stays installed across logout, macOS re-discovers it and System Settings can show many duplicate rows (e.g. 13). Do **not** reinstall until **after** logout; use `post_reset_marinamoji.sh`.

Scrub marinaMoji only (keep other Input Sources prefs):

```bash
bash ./mac/scrub_marinamoji.sh
```

**Then log out and back in** (required — do not skip). After login:

```bash
bash ./mac/post_reset_marinamoji.sh
# add marinaMoji ONCE in System Settings, then:
bash ./mac/activate_marinamoji.sh
```

Light reset (HIToolbox wipe only — app stays; dpm may return):

```bash
bash ./mac/reset_hitoolbox_for_marinamoji.sh
```

If the converter still crashes after IMK logs are clean, move user data aside (backup) and retry:

```bash
mv ~/Library/Application\ Support/marinaMoji \
   ~/Library/Application\ Support/marinaMoji.bak.$(date +%Y%m%d-%H%M%S)
```

### Clean up duplicate marinaMoji rows (e.g. 13 entries)

Do **not** restore an old HIToolbox backup wholesale — backups from this session may already contain corrupted state (dpm + marinaMoji modes split across plists).

1. System Settings → Keyboard → Input Sources → remove **every** marinaMoji row.
2. Re-add other sources you need: **U.S.** or **ABC**, **Dvorak**, etc.
3. Terminal:

```bash
killall marinaMoji marinaMojiConverter marinaMojiRenderer imklaunchagent TextInputMenuAgent cfprefsd 2>/dev/null || true
defaults delete com.apple.inputsources
killall cfprefsd 2>/dev/null || true
```

4. Log out and back in.
5. Add **marinaMoji once** (expect **one** menu entry, not a dozen).
6. Verify: `defaults read com.apple.HIToolbox AppleSelectedInputSources | rg -i 'org.mozc|Japanese'`  
   (`AppleEnabledInputSources` is often empty for third-party IMEs on recent macOS — that alone is not a failure.)
7. `bash ./mac/activate_marinamoji.sh` then `bash ./mac/install_marinamoji.sh`

Restore **dpm** only after marinaMoji works:  
`mv ~/Library/Keyboard\ Layouts/dpm.bundle.off ~/Library/Keyboard\ Layouts/dpm.bundle` then add dpm once in Input Sources.

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
   - [ ] **Right Shift** alone toggles hiragana ↔ Manyōshū (release without typing)
   - [ ] Candidate window F5/F6 behavior unchanged
   - [ ] Preferences → General → Appearance: candidate font size 14 vs 36 updates the candidate list (and usage examples) after OK, without restarting the IME
   - [ ] Candidate window: **rounded** panel, **toolbar-matched** border (1pt `rgba(0,0,0,0.08)`), **opaque white** background, rounded scrollbar track
   - [ ] Footer logo loads from `toolbar_icons/logo_long_light.svg` (sharp at 14 pt and 36 pt font)
   - [ ] Infolist (“用例”) panel matches candidate rounding and white/toolbar-border theme
5. Logs: `~/Library/Logs/marinaMoji/marinaMoji.log`

## Known issues / backlog

### High

| ID | Issue | Suggested fix |
|----|--------|----------------|
| M1d | **Toolbar crash on activate** (macOS 26; `MozcToolbarView initWithClient`) — can take down CotEditor / System Settings via TSM when IME dies | **Workaround:** `~/Library/Application Support/marinaMoji/toolbar.conf` with `toolbar_visible` = false (must match profile dir; not `Application Support/toolbar.conf` alone). **Fix:** toolbar prefs path + guard async show; sync hide on deactivate. |
| M1 | **Kotoeri Conversion: `Ctrl+Shift+2` duplicate** — both `ShowOdorijiPalette` and `ToggleFullHalfWidth`; last line in TSV wins (palette blocked on keyboard) | **Resolved**: number-row mappings now use `1` odoriji default, `2` palette, `3` shin/kyū, `4` Manyōshū, `5` hiragana/direct in Kotoeri/MS-IME/ATOK keymaps. |
| M1b | **`Ctrl+Shift+5` freeze when returning from Direct** — `setValue:` / `handleConfig` / `selectInputMode` re-entry | Mitigations: no `switchDisplayMode` from keys; `setValue:` skips server + `handleConfig`; 200ms `setValue` suppress after keyboard mode change; `processOutput` depth limit. **Debug:** `MARINA_IMK_TRACE=1` → `~/Library/Logs/marinaMoji/marinaMoji.log`; or standalone **Mode Lab** harness — [MODE_LAB.md](MODE_LAB.md) |
| M1c | **Ctrl+Shift+1–4 beep on Dvorak/AZERTY** | Fixed: physical number-row mapping runs before empty-`characters` check in `KeyCodeMap.mm` |
| M1e | **Shin/kyū shortcut or toolbar toggle “loops”** (rapid flip / freeze after “start in shin”) | Fixed: ignore `isARepeat` for Ctrl+Shift keys; `Ctrl+Shift+3` toggles config without `TURN_ON_IME` first; toolbar shin/kyū uses active controller `sendCommand:` and loads `use_traditional_kanji` on first show. |
| M1f | **`Ctrl+Shift+` ` won’t return to hiragana** after “direct” | Fixed: dedicated `` dispatchMarinaBacktickShortcut`` (`SendKey` when session active, `TURN_ON_IME` when off); grave/tilde in `KeyCodeMap`. **Note:** `` ` `` = hiragana ↔ half-width; `` Ctrl+Shift+5 `` = full IME off/on. |
| M1g | **Shin/kyū “sticks”** (toolbar click or shortcuts; freeze/loop) | Fixed: ``TOGGLE_TRADITIONAL_KANJI`` uses ``OutputComposition`` only; Mac ``processOutput`` skips preedit/candidate refresh when absent; toolbar icon from server only (no optimistic flip); ``g_active_controller`` ``sendCommand:``. Idle session: ``PopOutput`` was already a no-op (converter inactive). |
| M1h | **History / zero-query window stays after Esc, delete-all, or switch to Dvorak** | Fixed: ``syncCandidatesWithOutput`` + ``deactivateServer`` uses ``clearCandidates``; cancel ``delayedUpdateCandidates``; ``imeServerActive_`` guard when input source is not marinaMoji. |
| M1i | **Duplicate character when switching input after commit** (e.g. type ``x``, commit, switch to Dvorak → ``xx``) | Fixed: after commit with no ``preedit`` field, ``applyCommitAndPreeditFromOutput`` clears marked text; ``deactivateServer`` clears stale ``composedString_``. Regression from gating ``updateComposedString`` on ``has_preedit()`` only. |
| M1j | **Ghost character / late ``あの`` after Esc then IME off** (e.g. ``ano`` + Space, Esc leaves ``◊`` in Word; switching IME inserts ``あの``) | Fixed: ``applyCommitAndPreeditFromOutput`` clears marked text when response has no ``preedit``/``result`` (Escape); ``flushCompositionBeforeDeactivate`` sends Esc + clears before ``deactivateServer``. |
| M1k | **Backspace wipes entire preedit** (e.g. ``daigenk`` → Backspace clears all, not one char; commits may not learn) | Fixed: M1j clear-without-preedit must not run on ``consumed=false`` echo-back (Kotoeri Precomposition Backspace → ``Revert``). Use ``allowClearWithoutPreedit`` only for consumed Escape/Cancel. |
| M1l | **History still off after privacy mode disabled** (e.g. ``daigenguu`` → 大元宮 never ranks up) | Fixed: ``TogglePrivacyMode`` no longer persists ``history_learning_level = NO_HISTORY``; turning privacy off migrates stuck ``NO_HISTORY`` back to ``DEFAULT_HISTORY``. **Workaround now:** Preferences → General → “Adjust conversion based on previous input” → not “No history”. |
| M1m | **Privacy mode on after every restart** | Fixed: IME-menu privacy is **session-only** (``SetIncognitoModeInMemory``); ``Reload`` clears a leftover ``incognito_mode`` in ``config1.db``. Fresh defaults: incognito off, ``DEFAULT_HISTORY``. |
| M1n | **Mode resets to hiragana on window/focus change** (e.g. Spotlight direct → refocus) | Fixed: ``setValue:`` honours DIRECT/off only (ignores macOS composition resync); ``refreshModeFromServer`` keeps DIRECT when user left IME off; last mode persisted in ``last_composition_mode.txt``. See ShareDocs ``macOS_mode_persistence.md``. |
| M2 | ~~Installer LaunchAgents / `.pkg` paths~~ | **Done:** plists, postflight, `tweak_installer_files.py`, and `marinaMoji.pkg` use `marinaMoji` paths |

### Medium

| ID | Issue | Suggested fix |
|----|--------|----------------|
| M3 | ~~**Right Shift → Manyōshū**~~ | **Done:** `KeyCodeMap` tracks `kVK_RightShift` release; IMK dispatches `RIGHT_SHIFT` to session (Linux `IBUS_Shift_R` parity) |
| M4 | **Toolbar mode menu** uses `client_->SendCommand` only | Route through active controller `sendCommand:` for full `processOutput` sync |
| M5 | **Macron `Ctrl+Alt+Shift+Letter`** — TSV may use uppercase; Mac sends lowercase with modifiers | Add lowercase aliases (same pattern as `Ctrl Shift f`) |

### Low / by design

| ID | Issue | Notes |
|----|--------|--------|
| M6 | **⌘ Command** combos ignored | `KeyCodeMap.mm` returns NO when Command is held |
| M7 | **Candidate window position** | Some apps return bad cursor rects; same class of issue as stock Mozc |
| M8 | **Input menu icon vs composition mode** | **Resolved as by-design (2026-06-08).** With one visible TIS mode, the menu bar icon stays on that mode’s TIFF; mode changes are shown on the **toolbar**, not by swapping the system menu icon. **Investigation conclusion:** the *only* way to make the menu-bar icon follow the composition mode is to make the sub-modes visible (`tsInputModeIsVisibleKey = true`) so `selectInputMode:` has visible targets. But on macOS a visible input mode is a separately-selectable input **source** — it joins the global Ctrl+Space / globe-key cycle. There is no plist key for "visible in my menu but excluded from the cycle", so making the five modes visible turns marinaMoji into five separately-cycled sources (cycle pollution), which is worse UX than a static icon. We therefore **keep the single-visible-mode design**: one marinaMoji entry in the cycle, mode shown on the floating toolbar. M1n (ignore composition resync) + M1b (sync display via `selectInputMode:` with echo guards) were validated in **Mode Lab** and prototyped in `mozc_imk_input_controller.mm`, confirmed free of the old freeze/loop, but **reverted** because they only have a visible effect with cycle-polluting visible modes. Prototype harness — [MODE_LAB.md](MODE_LAB.md). |

## Keymap notes (macOS)

- Default session keymap on Mac: **Kotoeri** (`config_handler.cc`).
- **Ctrl+letter** shortcuts use lowercase in TSV (`Ctrl j`); they work on Mac.
- **Ctrl+Shift+letter** on Mac sends **lowercase** key code + SHIFT modifier; TSV entries with uppercase letters need a **lowercase alias** (e.g. `Ctrl Shift F` and `Ctrl Shift f`).
- **Digits with Shift** on US layout: bind both `Ctrl Shift 1` and `Ctrl Shift !` (and `2` / `@`) so IBus-style and Mac-style key codes match.
- **Number-row shortcuts (macOS):** `KeyCodeMap` maps **physical** `kVK_ANSI_1`..`0` + Ctrl+Shift to digit `1`..`0` so Dvorak / AZERTY / custom layouts match QWERTY keymap rows (`Ctrl Shift 1` = odoriji, `3` = shin/kyū, `4` = Manyōshū, `5` = hiragana/direct).
- **marinaMoji (Kotoeri):** `Ctrl+Shift+3` / `#` → shin/kyū (`ToggleTraditionalKanji`); `Ctrl+Shift+4` / `$` → hiragana/Manyōshū (`ToggleManyoshuHiragana`). `Ctrl+Shift+5` / `%` → hiragana/direct toggle.
- **Right Shift alone (macOS):** `KeyCodeMap` emits `RIGHT_SHIFT` on `kVK_RightShift` release when no other key was typed with it held (same idea as Linux IBus `IBUS_Shift_R`).
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
| 2026-06-08 | M8 investigated + resolved as by-design: validated M1n+M1b (display sync) and visible sub-modes in Mode Lab and in the real controller, but reverted — visible modes pollute the global input-source cycle, so the single-visible-mode + toolbar design stays. Kept: pre-existing `//base/mac:mac_util` → `//base:file_util` build-dep fix; `UpdateCandidates` test fix (`imeServerActive` test accessor). |
