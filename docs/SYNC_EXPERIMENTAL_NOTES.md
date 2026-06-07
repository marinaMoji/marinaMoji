# Sync experimental notes

The separate-process sync design described in [SYNC_PLAN.md](SYNC_PLAN.md) superseded the earlier in-converter `PERFORM_USER_SYNC` / `SyncScheduler` experiment.

## What changed

- Sync runs in **`marinaMojiSync`**, not inside `mozc_server` / `SessionHandler`.
- Converter exposes only **lock + idle state + flush/reload** IPC.
- IMK watches **`sync.status.json`** and blocks input with an overlay during sync.
- Cooldown after typing/IME off is configurable in **`sync.conf`** (default 60 s).
- GUI reads/writes config files directly and spawns `marinaMojiSync --now`.

## Preserved from the experiment

- `src/sync/` crypto, bundle, merge, and config modules
- Encrypted `.mmz.enc` bundle format and merge rules
- Config dialog and dictionary tool sync UI (rewired to file I/O + spawn)

## Rollback reference

The June 2026 snapshot documented unstable IMK startup when sync lived in the converter hot path. The separate-process architecture keeps sync work off the typing path while reusing the same crypto/merge code.
