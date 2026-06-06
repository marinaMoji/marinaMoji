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
6. Click **Generate sync key** and copy the key to a safe place.

## Step 2: Set up device B

1. Open the same **Sync** tab on device B.
2. Select the exact same sync file path.
3. Click **Enter sync key** and paste the key from device A.
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
- **Every N minutes**
- **On shutdown**

When interval mode is enabled, marinaMoji also checks bundle file modification time and can pull updates from another device.

## Privacy behavior

- Sync key is stored locally, never in the cloud bundle.
- If **Privacy mode / incognito** is active, history export is skipped.
- Sync is opt-in. If disabled, marinaMoji behaves as local-only.

## Troubleshooting

- **"Sync key not set"**: generate or enter a key first.
- **"Invalid sync file magic" / decryption error**: wrong key or corrupted file.
- **No updates from other device**:
  - verify both devices use the same file path and key,
  - verify cloud sync finished copying the file,
  - click **Sync now** once on each device.

## Safety notes

- Keep backups of your synced folder.
- Do not manually edit `.mmz.enc` files.
- Use one shared file per profile to avoid confusion.

