# Encrypted sync — manual QA checklist

Use this checklist to verify cross-device sync after building or installing marinaMoji. You need **two devices** (or two user accounts/VMs) with marinaMoji installed and access to one shared folder (Nextcloud, Syncthing, iCloud Drive, etc.).

Estimated time: 30–45 minutes.

## Prerequisites

- [x] Device A and Device B both run a build that includes the Sync tab (Preferences / Properties).
- [x] Both devices can read and write the same folder path (or equivalent synced path on each machine).
- [x] Cloud/desktop sync for that folder is working independently (copy a test file both ways before starting).

## Setup (Device A — primary)

1. [x] Open **Preferences / Properties → Sync**.
2. [x] Enable **encrypted sync**.
3. [x] Set sync file path, e.g. `~/Nextcloud/marinamoji/marinamoji_sync.mmz.enc`.
4. [x] Enable: Settings, User dictionary, Commit / learning history.
5. [x] Set direction to **Bidirectional**.
6. [x] Set auto-sync to **Manual only** for the first test run.
7. [x] Click **Generate sync key** once; copy the key (or use **Show sync key** on this device later).
8. [x] Click **Sync now**; confirm success dialog (no error message).
9. [x] Confirm the `.mmz.enc` file exists in the shared folder and has non-zero size.

## Setup (Device B — secondary)

1. [x] Wait until the cloud folder shows the new sync file on Device B.
2. [x] Open **Preferences / Properties → Sync** on Device B.
3. [ ] Enable sync; use the **same logical file** (path on B that maps to the same cloud file as on A).
4. [x] Click **Enter sync key** on device B; paste the key from device A (on A, after a successful sync, use **Show sync key** to copy it again).
5. [x] Match checkboxes and direction with Device A.
6. [x] Click **Sync now**; confirm success.

## Test 1 — Dictionary propagates A → B

On **Device A**:

1. [x] Open **Dictionary Tool** (from Preferences or IME menu).
2. [x] Add a unique test entry, e.g. reading `てすと` / surface `試験同期A` / POS 名詞.
3. [x] Save if prompted; use **Sync now** from Dictionary Tool or Sync tab.

On **Device B**:

4. [x] Wait for cloud sync of the bundle file (or click **Sync now** after the file mtime updates).
5. [x] Open Dictionary Tool; confirm `試験同期A` appears.
6. [x] Type the reading in compose mode; confirm the entry ranks reasonably in candidates.

**Pass:** entry visible in dictionary and usable in conversion.

## Test 2 — Dictionary propagates B → A

On **Device B**:

1. [x] Add another unique entry, e.g. `てすと` / `試験同期B`.
2. [x] **Sync now**.

On **Device A**:

3. [x] **Sync now** after cloud replication.
4. [x] Confirm `試験同期B` in Dictionary Tool.

**Pass:** bidirectional dictionary merge works.

## Test 2b — Dictionary delete propagates

On **Device A** (after Test 1 or 2 added entries):

1. [ ] Delete `試験同期A` (or another test entry) in Dictionary Tool and save (close tool, switch dictionary, or **Sync now**).
2. [ ] **Sync now**.

On **Device B**:

3. [ ] Wait for cloud replication; **Sync now**.
4. [ ] Confirm the deleted entry is gone from Dictionary Tool and no longer appears as a strong candidate.

**Pass:** delete on A removes the entry on B after sync.

## Test 3 — Settings sync

On **Device A**:

1. [ ] Change a whitelisted setting (e.g. toggle **Traditional kanji** / `use_traditional_kanji` if exposed in General or toolbar).
2. [ ] **Sync now**.

On **Device B**:

3. [ ] **Sync now**; verify the same setting took effect (toolbar or Preferences).

**Pass:** setting matches on B without manual reconfiguration.

## Test 4 — History / learning (optional)

Skip if **Privacy mode** is on (history is not exported).

On **Device A**:

1. [ ] Disable Privacy mode temporarily.
2. [ ] Commit a distinctive phrase several times so it learns rank.
3. [ ] **Sync now**.

On **Device B**:

4. [ ] **Sync now**.
5. [ ] Type the same reading; check whether learned ranking improved vs. a fresh profile.

**Pass:** history merge reported in sync dialog (`history merged` > 0) or visible ranking change.

## Test 5 — Privacy mode skips history

On **Device A**:

1. [ ] Enable **Privacy mode**.
2. [ ] **Sync now** (note: history section should not overwrite remote learning while incognito).

**Pass:** no unexpected history export; dictionary/settings still sync if enabled.

## Test 6 — Wrong key rejected

On **Device B**:

1. [x] Enter an incorrect sync key via **Enter sync key** (or temporarily change stored key).
2. [x] **Sync now**.

**Pass:** error message (decryption / wrong key); local data not corrupted.

## Test 7 — Auto-sync interval (optional)

On **Device A**:

1. [ ] Set auto-sync to **Every N minutes** (e.g. 5 for testing).
2. [ ] Add a dictionary entry; do **not** click Sync now.
3. [ ] Wait one interval; confirm bundle file mtime updates.

On **Device B**:

4. [ ] Wait for cloud sync + interval (or mtime poll); pull via **Sync now** or wait for scheduler.

**Pass:** change appears without manual Sync on A.

## Test 8 — Composition skip (smoke test)

On **Device A**:

1. [ ] Start typing in compose mode (preedit visible); leave composition open.
2. [ ] Trigger scheduled sync window (wait for interval or use shutdown sync after closing compose).

**Pass:** sync does not run mid-composition; runs after compose is cleared or on shutdown.

## Cleanup

- [ ] Remove test dictionary entries (`試験同期A`, `試験同期B`) from both devices and sync once.
- [ ] Document any failures with: OS, build version, sync file path, error text, and whether cloud file mtime changed.

## Quick reference — file locations

| Item | macOS | Linux |
|------|-------|-------|
| Sync settings | `~/Library/Application Support/marinaMoji/sync.conf` | `~/.config/marinamoji/sync.conf` |
| Sync key (local) | `~/Library/Application Support/marinaMoji/.sync_key` | `~/.config/marinamoji/.sync_key` or `~/.sync_key` |
| User profile | `~/Library/Application Support/marinaMoji/` | `~/.config/marinamoji/` |

User guide: [HOW_SYNC_WORKS.md](HOW_SYNC_WORKS.md)  
Implementation: [SYNC_PLAN.md](SYNC_PLAN.md)
