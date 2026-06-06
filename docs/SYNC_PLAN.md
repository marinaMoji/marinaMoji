# Encrypted user data sync (implementation reference)

This document describes the shipped v1 sync implementation for marinaMoji on **macOS**. Linux sync daemon and IBus notice are deferred.

## Scope

v1 sync covers:

- selected user settings (`config.proto` whitelist),
- user dictionary entries (TSV merge),
- user history (portable TSV merge).

Data is stored in one encrypted bundle file chosen by the user (for example, in a Nextcloud folder). Transport is out of scope: marinaMoji only reads and writes a local file path.

## Process architecture (macOS v1)

| Process | Role |
|---------|------|
| `marinaMojiSync` | Scheduler, cooldown, crypto/merge, status file, orchestrates converter IPC |
| `marinaMojiConverter` | Lock/unlock, idle query, flush, reload only — no UI, no sync logic |
| `marinaMoji` (IMK) | Watch status file, block keys, center-screen overlay + beep |

Sync does **not** run inside `SessionHandler` via `PERFORM_USER_SYNC`. Config is read/written directly from `sync.conf` by the sync daemon and GUI tools.

## Files and storage

- Sync sidecar config:
  - macOS: `~/Library/Application Support/marinaMoji/sync.conf`
  - Linux: `~/.config/marinamoji/sync.conf` (config I/O exists; daemon not shipped in v1)
- Live status (atomic JSON): `sync.status.json` in the same profile directory
- Activity timestamps (written by IMK): `sync.activity.json` (`last_composition_end`, `last_ime_deactivated`)
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
- `settings.pb`
- `dictionary.tsv`
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
  - additive merge (no automatic delete in v1)
- **Settings**
  - whitelist-only merge in `sync_merge`
  - local defaults preserved when remote values are missing
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

## Main flow (`RunSync` in `marinaMojiSync`)

1. `GET_SYNC_STATE` → abort if any session is composing (unless `--force`).
2. Check cooldown vs `sync.activity.json` + `sync_cooldown_seconds` (unless forced).
3. Write `sync.status.json` state `running`.
4. `BEGIN_SYNC_LOCK` → `SYNC_DATA` (flush) → file export/merge/encrypt/import → `RELOAD_AND_WAIT` → `END_SYNC_LOCK`.
5. Write status `done` / `error`; update `sync.conf` last-sync fields.

## Background scheduling

`marinaMojiSync --daemon` runs as a LaunchAgent (`org.mozc.inputmethod.Japanese.Sync`):

- Poll interval: `max(60, auto_sync_interval_minutes * 60)` seconds
- Also triggers when remote bundle mtime changes
- Skips when composing, cooldown not met, or sync already running
- `KeepAlive=false` (no persistent respawn loop beyond `RunAtLoad`)

Manual sync: `marinaMojiSync --now` or `--force` (GUI “Sync now” spawns this).

## IMK behavior during sync

- Polls `sync.status.json` (~250 ms) while IME is active
- Shows centered non-activating overlay: “marinaMoji synchronising…”
- Blocks keyboard events to the converter; beeps and flashes overlay if user types (rate-limited)
- Hides candidate window while sync is active
- Writes `last_ime_deactivated` on `deactivateServer`
- Writes `last_composition_end` when preedit clears

## GUI integration

Config dialog Sync tab and Dictionary Tool “Sync now”:

- Read/write `sync.conf` and sync key directly (no converter sync IPC)
- Cooldown spinbox (seconds)
- Spawn `marinaMojiSync --now --force` and poll `sync.status.json`

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
bazel test //sync:all --config=macos
bazel test //session:session_handler_test --config=macos
```

## Manual QA checklist (macOS)

1. Enable sync in Config dialog, set bundle path, generate key, save.
2. Copy key to second machine; enter key; verify bidirectional merge.
3. Auto-sync after typing: type, wait for cooldown, confirm sync runs without blocking keys beforehand.
4. Manual “Sync now” from Config dialog and Dictionary Tool; confirm status updates.
5. During sync: type in TextEdit → beep + overlay flash; keys must not reach converter.
6. Turn IME off → confirm `sync.activity.json` updates; auto-sync respects cooldown.
7. Verify converter and IMK survive sync; dictionary/history changes appear after reload.
8. LaunchAgent: after install, `pgrep marinaMojiSync` shows daemon; scrub script removes Sync agent.
