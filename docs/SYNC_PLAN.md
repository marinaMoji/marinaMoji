# Encrypted user data sync (implementation reference)

This document describes the shipped v1 sync implementation for marinaMoji on macOS and Linux.

## Scope

v1 sync covers:

- selected user settings (`config.proto` whitelist),
- user dictionary entries (TSV merge),
- user history (portable TSV merge).

Data is stored in one encrypted bundle file chosen by the user (for example, in a Nextcloud folder). Transport is out of scope: marinaMoji only reads and writes a local file path.

## Files and storage

- Sync sidecar config:
  - macOS: `~/Library/Application Support/marinaMoji/sync.conf`
  - Linux: `~/.config/marinamoji/sync.conf`
- Encrypted sync bundle: user-chosen path (recommended suffix `.mmz.enc`)
- Local key storage:
  - currently stored locally as `~/.sync_key` with mode `0600`
  - never stored in the cloud bundle

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

## IPC commands

Added to `commands.proto`:

- `GET_USER_SYNC_CONFIG`
- `SET_USER_SYNC_CONFIG`
- `PERFORM_USER_SYNC`
- `GENERATE_USER_SYNC_KEY`

Messages:

- `UserSyncConfig`
- `UserSyncRequest`
- `UserSyncReport`

## Main flow (`PERFORM_USER_SYNC`)

1. Sync local engine state to disk.
2. Load config and sync key.
3. Export selected local sections.
4. If remote exists and direction allows: decrypt and merge.
5. If direction allows upload: encrypt and atomically write merged bundle.
6. If direction allows download: import merged data locally.
7. Reload engine and update sync status fields.

## Background scheduling

`SyncScheduler` runs in `mozc_server` and supports:

- interval sync (`EVERY_N_MINUTES`)
- shutdown sync (`ON_SHUTDOWN`)
- remote file mtime polling per interval
- composition-aware skip (scheduled sync does not run while active preedit/candidate state exists)

## Test coverage

Unit tests in `src/sync/`:

- `sync_crypto_test` (round-trip + wrong key),
- `sync_bundle_test` (pack/unpack),
- `sync_merge_test` (dictionary/history merge behavior).

Build/test command:

`bazel test //sync:all --config=macos`

