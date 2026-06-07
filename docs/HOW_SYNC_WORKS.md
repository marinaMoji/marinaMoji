# How encrypted sync works (user guide)

marinaMoji can sync your settings, user dictionary, and learning history across devices using one encrypted file.

## Before you start

You need:

- two devices running marinaMoji,
- one shared folder (for example Nextcloud, iCloud Drive, Dropbox, Syncthing),
- access to marinaMoji Preferences / Properties.

marinaMoji does not provide cloud transport. It only reads and writes a local file path.

## Step 1: Enable sync on device A

1. Open **Preferences / Properties**.
2. Open the **Sync** tab.
3. Enable encrypted sync.
4. Pick a sync file path (for example `.../marinamoji_sync.mmz.enc` in your synced folder).
5. Choose what to sync:
   - Settings
   - User dictionary
   - Commit / learning history
6. Click **Generate sync key** once and copy it (or **Show sync key** on the primary device later).

## Step 2: Set up device B

1. Open the same **Sync** tab on device B.
2. Select the exact same sync file path.
3. Click **Enter sync key** and paste the key from device A (after the first successful sync on A, that button becomes **Show sync key** so you can copy it again).
4. Click **Sync now**.

Both devices now share the same encrypted bundle.

## Direction and auto-sync

Direction:

- **Bidirectional**: merge both local and remote.
- **Upload only**: write local state to the bundle.
- **Download only**: read and import from the bundle.

Auto-sync:

- **Never**
- **Manual only**
- **Every N minutes** — wake every N minutes, compare SHA-256 fingerprints of the remote bundle and local sync data; sync only if something changed
- **On shutdown**

When interval mode is enabled, marinaMoji also checks bundle file modification time and can pull updates from another device.

## Privacy behavior

- Sync key is stored locally, never in the cloud bundle.
- If **Privacy mode / incognito** is active, history export is skipped.
- Sync is opt-in. If disabled, marinaMoji behaves as local-only.

## Troubleshooting

- **"Sync key not set"**: generate or enter a key first (see key path below).
- **Dictionary syncs but settings / history do not**: rebuild after the converter reload fix (`SessionHandler::ReloadAndWait` must call `ConfigHandler::Reload()`). Until then, settings were written to `config1.db` but the running converter kept the old in-memory config.
- **Settings that sync (whitelist only)**: traditional kanji (`use_traditional_kanji`), history learning level, dictionary/history suggestions, auto/realtime conversion, preedit method, keymap, punctuation/symbol/space forms, selection shortcut — not toolbar layout or every Preferences field.
- **History not exported**: skipped while **Privacy mode / incognito** is on, or if **Commit / learning history** is unchecked in Sync. Commit a phrase several times on the source machine, sync, then sync on the other device.
- **"Invalid sync file magic" / decryption error**: wrong key or corrupted file.
- **`PERMISSION_DENIED: Cannot write sync file`**: marinaMoji could read the bundle but **cannot write** to the sync file path. Common on a second Mac / VM:
  - Sync file path points at a folder that is **read-only** (shared folder mounted read-only).
  - Path is copied from the other machine (e.g. `/Users/daniel/...` on the VM where that home folder does not exist).
  - Parent folder does not exist — create it first.
  - **Fix:** On the VM, set the sync path to **that machine’s** path to the shared file (e.g. the VM mount of your shared folder), then test:
    ```bash
    touch "/path/to/your/marinamoji_sync.mmz.enc"
    ```
    For a first pull only, set direction to **Download only**, sync once, then switch back to **Bidirectional**.
- **`PERMISSION_DENIED: Cannot write sync key`** or **`Cannot write sync.conf`**: profile folder missing or not writable. On macOS the profile is `~/Library/Application Support/marinaMoji/` — use **Enter sync key** in Preferences (do not only copy a file to `~/.sync_key`; that path is wrong on macOS).
- **No updates from other device**:
  - verify both devices use the same **logical** cloud/shared file and the **same key**,
  - verify cloud sync finished copying the file,
  - click **Sync now** once on each device.

### macOS file locations (important)

| Item | Path |
|------|------|
| Sync settings | `~/Library/Application Support/marinaMoji/sync.conf` |
| Sync key | `~/Library/Application Support/marinaMoji/.sync_key` |
| Sync status | `~/Library/Application Support/marinaMoji/sync.status.json` |

The sync key is **not** stored in `~/.sync_key` on macOS (that path is Linux / legacy docs only).

To inspect the last error on either machine:

```bash
cat ~/Library/Application\ Support/marinaMoji/sync.status.json
```

## Safety notes

- Keep backups of your synced folder.
- Do not manually edit `.mmz.enc` files.
- Use one shared file per profile to avoid confusion.

