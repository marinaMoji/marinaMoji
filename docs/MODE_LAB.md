# Mode Lab — macOS input-mode test harness

Mode Lab is a **minimal Input Method Kit (IMK) app** plus a **host application** for debugging how macOS synchronises composition modes (`setValue:forTag:client:` ↔ `selectInputMode:`) without running the full marinaMoji converter, toolbar, or renderer.

Use it to reproduce and fix issues **M1b** (freeze on `selectInputMode` re-entry) and **M8** (menu bar icon vs real mode) before changing `mozc_imk_input_controller.mm`.

## Build

From `src/`:

```bash
cd ~/Code/marinaMozc/src
bazelisk build --config=oss_macos \
  //mac/mode_lab:mode_lab_ime \
  //mac/mode_lab:mode_lab_host
```

## Install

```bash
bash mac/mode_lab/install_mode_lab.sh
```

The install script registers with Text Input Services and **fails loudly** if no TIS sources appear (catches silent registration bugs).

**Important:** Mode Lab must use bundle ID `org.mozc.inputmethod.ModeLab` (same family as marinaMoji). An earlier build used `org.marinaMoji.ModeLab`, which macOS accepted but never listed — if you installed that build, reinstall with the current script.

If install succeeded but System Settings still hides it:

```bash
bash mac/mode_lab/register_mode_lab.sh   # removes duplicate ~/Library copy if present
bash mac/mode_lab/activate_mode_lab.sh   # enables Mode Lab in menu bar directly
```

**Common gotcha:** a copy in `~/Library/Input Methods/ModeLab.app` *and* `/Library/Input Methods/ModeLab.app` registers **12 duplicate TIS sources** and can hide the IME from System Settings. The install script removes the user copy automatically.

**Localization:** the visible hiragana mode must be named `Mode Lab` in `InfoPlist.strings` (`com.apple.inputmethod.Japanese = "Mode Lab"`). Without that, macOS shows the raw key `com.apple.inputmethod.Japanese` and the Japanese picker may omit the IME.

This installs:

| App | Location |
|-----|----------|
| Mode Lab IME | `/Library/Input Methods/ModeLab.app` |
| Mode Lab Host | `/Applications/ModeLabHost.app` |

Mode Lab uses bundle ID `org.mozc.inputmethod.ModeLab` (same TIS family as marinaMoji, separate IME).

## Setup

1. **System Settings → Keyboard → Input Sources → Edit → +**
2. Select **Japanese** in the left column, then pick **Mode Lab** on the right (do not rely on search alone — English search may hide it).
3. Open **Mode Lab Host** from Applications.
4. Select **Mode Lab** in the menu bar input source picker.
5. Click in the host text field and experiment.

## Automated sequence

Click **Run Automated Sequence** in Mode Lab Host (with **Mode Lab** selected as the input source and focus in the text field). The harness runs **25 steps** spaced **2 seconds** apart (~50 seconds total):

| Phase | What it sends |
|-------|----------------|
| TIS selects | Hiragana, Katakana, Roman, Half kana, Wide Latin via `TISSelectInputSource` |
| IME commands | `switch_mode`, `sync_display` (`selectInputMode:`) via distributed notifications |
| Focus | Switches to a **second host window** and back (triggers macOS `setValue` resync) |
| Inject | Simulated `setValue:` calls (katakana/hiragana/roman resync) |
| Rapid burst | Five TIS/IME/sync actions at 0.4s intervals (re-entrancy stress test) |

During the run, every IME log line is captured per step. At the end a report is written:

| File | Contents |
|------|----------|
| `~/Library/Application Support/marinaMoji/mode_lab_run_report.txt` | Human-readable summary with **ANOMALY** flags |
| `~/Library/Application Support/marinaMoji/mode_lab_run.jsonl` | Machine-readable step/response log |
| `~/Library/Logs/marinaMoji/mode_lab.log` | Full append-only trace |

Anomalies flagged automatically include: TIS select failures, IME mode drift, TIS/IME mismatch after sync, `setValue` re-entry during sync (M1b pattern), focus resync behaviour, and rapid `setValue` storms.

Use **Open Report** when the run finishes, or toggle policy flags before running to compare M1n vs M1b behaviour.

## What to test

### Scenario A — Focus resync (M1n)

1. In the host, enable **Ignore composition resync** (default — matches current marinaMoji).
2. Switch to Katakana via **Ctrl+Shift+3** (in the IME) or the host’s Katakana button.
3. **Window → New Window**; click in the second window.
4. Watch the log: macOS may send `setValue IGNORED resync HIRAGANA <- …`.
5. Confirm IME `mode` in the status line stays **KATAKANA**.

### Scenario B — Display sync (M1b)

1. Enable **Sync display on IME change**.
2. Switch modes with Ctrl+Shift+2/3/4.
3. Watch for `switchDisplayMode selectInputMode:` followed by `setValue SKIPPED sync=1`.
4. If the session freezes or loops, adjust policy flags and compare with marinaMoji trace logs.

### Scenario C — Pre-M1n behaviour

1. Enable **Honor all setValue** (disables ignore-resync).
2. Repeat Scenario A — mode may snap back to hiragana on focus change.

### Scenario D — Menu bar icon (M8)

1. Enable **Sync display on IME change**.
2. Change mode from the IME menu (**Input Mode → Katakana**) or host TIS buttons.
3. Observe whether the menu bar input icon updates.

## Policy flags

Toggled in Mode Lab Host (saved to `~/Library/Application Support/marinaMoji/mode_lab_policy.plist`):

| Flag | Default | marinaMoji equivalent |
|------|---------|----------------------|
| Ignore composition resync | ON | M1n `setValue` policy |
| Sync display on IME change | OFF | Intentionally disabled in `processOutput` |
| Honor all setValue | OFF | Pre-M1n behaviour |
| Persist last mode | ON | `last_composition_mode.txt` |

## Keyboard shortcuts (IME)

With focus in a text field and Mode Lab active:

| Shortcut | Mode |
|----------|------|
| Ctrl+Shift+1 | Direct |
| Ctrl+Shift+2 | Hiragana |
| Ctrl+Shift+3 | Katakana |
| Ctrl+Shift+4 | Latin |
| Ctrl+Shift+5 | Wide Latin |

## Log files

| File | Purpose |
|------|---------|
| `~/Library/Logs/marinaMoji/mode_lab.log` | Append-only event log |
| `~/Library/Application Support/marinaMoji/mode_lab_state.json` | Live IME mode + last event |
| `~/Library/Application Support/marinaMoji/mode_lab_last_mode.txt` | Persisted mode (when enabled) |

## IME menu

Right-click the input mode icon (or use the Input menu) while Mode Lab is selected:

- **Input Mode** submenu — enabled here (disabled in marinaMoji until sync is fixed).
- **Sync display mode now** — manual `selectInputMode:` test.

## Porting fixes to marinaMoji

When a policy combination works in Mode Lab:

1. Mirror the guard logic in `src/mac/mozc_imk_input_controller.mm`.
2. Re-enable the Input Mode submenu (`#if 0` block in `setupMarinaImeMenuIfNeeded`).
3. Optionally re-enable `switchDisplayMode` from `processOutput`.
4. Run manual QA from [macOS_mode_persistence.md](https://github.com/marinaMoji/marinaMoji/blob/master/docs/MACOS_PORT_PLAN.md) and ShareDocs `macOS_mode_persistence.md`.

## Uninstall / fix duplicate rows

If System Settings shows many **Mode Lab** entries (e.g. 13), repeated install/register left stale rows in `com.apple.inputsources`.

1. Remove **every** Mode Lab row in System Settings → Keyboard → Input Sources.
2. Run:

```bash
bash mac/mode_lab/scrub_mode_lab.sh
```

3. **Log out and back in** (important — macOS caches input-source metadata).
4. Install **once**: `bash mac/mode_lab/install_mode_lab.sh`
5. Add **once** (or `bash mac/mode_lab/activate_mode_lab.sh`).

The install script auto-scrubs if it detects an existing Mode Lab registration.

## Related docs

- `docs/MACOS_PORT_PLAN.md` — issues M1b, M1n, M8
- ShareDocs `documentation/Implementation/macOS_mode_persistence.md`
