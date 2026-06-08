# Encrypted user data sync (implementation reference)

This document describes the shipped v1 sync implementation for marinaMoji on **macOS** and **Linux (IBus)**.

## Scope

v1 sync covers:

- user dictionary entries (TSV merge),
- user history (portable TSV merge).

Data is stored in one encrypted bundle file chosen by the user (for example, in a Nextcloud folder). Transport is out of scope: marinaMoji only reads and writes a local file path.

## Process architecture

### macOS

| Process | Role |
|---------|------|
| `marinaMojiSync` | Scheduler, cooldown, crypto/merge, status file, orchestrates converter IPC |
| `marinaMojiConverter` | Lock/unlock, idle query, flush, reload only — no UI, no sync logic |
| `marinaMoji` (IMK) | Watch status file, block keys, center-screen overlay + beep |

### Linux (IBus)

| Process | Role |
|---------|------|
| `mozc_sync` (`/usr/lib/marinamoji/mozc_sync`) | Same as `marinaMojiSync` on macOS |
| `mozc_server` | Lock/unlock, idle query, flush, reload only |
| `ibus-engine-marinamoji` | Watch status file, block keys, GTK center-screen overlay + beep |

Background daemon: systemd **user** unit `marinamoji-sync.service` (install via `src/unix/install_sync_daemon.sh` after `mozc.zip`).

Sync does **not** run inside `SessionHandler` via `PERFORM_USER_SYNC`. Config is read/written directly from `sync.conf` by the sync daemon and GUI tools.

## Files and storage

- Sync sidecar config:
  - macOS: `~/Library/Application Support/marinaMoji/sync.conf`
  - Linux: `~/.config/marinamoji/sync.conf`
- Live status (atomic JSON): `sync.status.json` in the same profile directory
- Activity timestamps (written by IMK / IBus): `sync.activity.json` (`last_composition_end`, `last_ime_deactivated`)
- Encrypted sync bundle: user-chosen path (recommended suffix `.mmz.enc`)
- Local key storage:
  - macOS: `~/Library/Application Support/marinaMoji/.sync_key` (mode `0600`)
  - Linux: `~/.config/marinamoji/.sync_key` (or legacy `~/.sync_key`)
  - never stored in the cloud bundle

### sync.conf fields (selected)

- `enabled`, `sync_file_path`, merge toggles, direction, auto-sync mode/interval
- `sync_cooldown_seconds` (default **60**) — minimum idle time after typing or IME deactivation before auto-sync
- `last_sync_time`, `last_sync_status`, `last_sync_message`

### sync.status.json

```json
{"state":"running","phase":"merge","progress":0.42,"message":"…","updated_at_unix":…}
```

States: `idle`, `running`, `done`, `error`.

## Bundle format

Plaintext bundle (zip) includes:

- `manifest.txt`
- `dictionary.tsv`
- `dictionary_tombstones.tsv` (delete log; compacted after merge)
- `history.tsv`

Encrypted container:

- magic header `MMZENC1`
- libsodium salt
- libsodium secretstream header
- encrypted payload

Write path is atomic (`.tmp` then rename).

## Crypto design

- Library: `libsodium`
- KDF: `crypto_pwhash`
- Encryption: `crypto_secretstream_xchacha20poly1305`

This is intentionally separate from Mozc machine-bound encryption, so synced files stay portable across devices.

## Merge rules

- **Dictionary**
  - key: `(reading, surface, pos, locale)`
  - additive union of live rows, then tombstones remove stale rows from one side
  - tombstone row: `reading\tsurface\tpos\tlocale\tdeleted_at_unix\tdevice_id`
  - local tombstone log: `dictionary_tombstones.local.tsv` in the profile directory (written when Dictionary Tool saves deletions)
  - compaction: drop tombstones for re-added keys; drop tombstones older than 90 days when the key is absent from the merged dictionary
- **History**
  - key: `Fingerprint(key, value)`
  - sum `suggestion_freq` and `shown_freq`
  - keep max `last_access_time`

If incognito mode is enabled, history export is skipped.

## Converter IPC (lock protocol)

Added to `commands.proto`:

- `GET_SYNC_STATE = 37` — returns `SyncState` (`sync_locked`, `any_composing`, `active_session_count`)
- `BEGIN_SYNC_LOCK = 38` — sets lock; rejects new keys/commands with `Output::SYNC_LOCKED`
- `END_SYNC_LOCK = 39` — clears lock (sync daemon must call even on error)

Legacy sync config IPC (`GET_USER_SYNC_CONFIG`, `PERFORM_USER_SYNC`, etc.) is **not** handled by the converter in v1.

## Main flow (`RunSync` in `marinaMojiSync` / `mozc_sync`)

1. `GET_SYNC_STATE` → abort if any session is composing (unless `--force`).
2. Check cooldown vs `sync.activity.json` + `sync_cooldown_seconds` (unless forced).
3. Write `sync.status.json` state `running`.
4. `BEGIN_SYNC_LOCK` → `SYNC_DATA` (flush) → file export/merge/encrypt/import → `RELOAD_AND_WAIT` (reload dictionary + history in converter) → `END_SYNC_LOCK`.
5. Write status `done` / `error`; update `sync.conf` last-sync fields.

## Background scheduling

**macOS:** `marinaMojiSync --daemon` runs as a LaunchAgent (`org.mozc.inputmethod.Japanese.Sync`).

**Linux:** `mozc_sync --daemon` runs as a systemd user service (`marinamoji-sync.service`).

Both:

- Poll interval: `max(60, auto_sync_interval_minutes * 60)` seconds
- **Every N minutes** mode: compute SHA-256 of the remote bundle and local sync data; sync only when either fingerprint changed since the last successful sync (first poll records a baseline without syncing)
- Skips when composing, cooldown not met, or sync already running

Manual sync: `marinaMojiSync --now` / `mozc_sync --now` or `--force` (GUI “Sync now” spawns the platform binary).

## IME behavior during sync (IMK / IBus)

- Polls `sync.status.json` (~250 ms) while the engine is running
- Shows centered non-activating overlay: “marinaMoji synchronising…” (AppKit on macOS, GTK on Linux)
- Blocks keyboard events to the converter; beeps and flashes overlay if user types (rate-limited)
- Hides candidate window while sync is active
- Writes `last_ime_deactivated` on IME disable / deactivate
- Writes `last_composition_end` when preedit clears

## GUI integration

Config dialog Sync tab and Dictionary Tool “Sync now”:

- Read/write `sync.conf` and sync key directly (no converter sync IPC)
- Cooldown spinbox (seconds)
- Spawn sync `--now --force` (`marinaMojiSync` or `mozc_sync`) and poll `sync.status.json`

## Test coverage

Unit tests in `src/sync/`:

- `sync_crypto_test` (round-trip + wrong key),
- `sync_bundle_test` (pack/unpack),
- `sync_merge_test` (dictionary/history merge behavior),
- `sync_status_test` (atomic JSON read/write),
- `sync_runner_test` (lock order, composition/cooldown gating).

Session tests:

- `SyncLockRejectsSendKey`, `GetSyncStateReportsSessionCount` in `session_handler_test`.

Build/test command:

```bash
# macOS
bazel test //sync:all --config=macos
bazel test //session:session_handler_test --config=macos

# Linux
bazel test //sync:all
bazel build package --config oss_linux --config release_build
```

## Linux install (summary)

1. `sudo unzip -o bazel-bin/unix/mozc.zip -d /`
2. `./unix/install_sync_daemon.sh` (as your user)
3. `ibus write-cache && ibus restart`

Verify: `test -x /usr/lib/marinamoji/mozc_sync` and `systemctl --user is-active marinamoji-sync.service`.

## Manual QA checklist

See [SYNC_MANUAL_QA.md](SYNC_MANUAL_QA.md). Platform-specific checks:

**macOS**

1. LaunchAgent: after install, `pgrep marinaMojiSync` shows daemon; scrub script removes Sync agent.

**Linux**

1. After install, `pgrep mozc_sync` shows daemon; `docs/uninstall_linux_marinamozc.sh` stops the user systemd unit.
2. During sync: type in any app → GDK beep + GTK overlay; keys must not reach converter.
