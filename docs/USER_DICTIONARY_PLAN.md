# marinaMoji user dictionary plan

Planning document for improving marinaMoji user dictionary workflows on macOS, with focus on:

1. Importing Apple Japanese IME dictionaries.
2. Adding a privacy mode that disables learning/recording behavior.
3. Syncing user dictionary data through a shared Nextcloud folder.

This document is implementation-oriented and intentionally phased so changes can be shipped safely in small steps.

## Goals and non-goals

### Goals

- Accept dictionary files exported from Apple Japanese IME (Kotoeri workflow).
- Offer a clear "Privacy mode" users can toggle from UI.
- Let users synchronize dictionary data across devices via Nextcloud.
- Keep local typing experience stable (no data loss, no freezes, no regressions in conversion speed).

### Non-goals (for first iteration)

- Real-time cloud API integration.
- Editing `user_dictionary.db` directly in a shared folder.
- Solving every historical Apple export variant in v1 (start with current macOS format + compatibility fallback).

## Current baseline in marinaMoji

- The dictionary importer already supports multiple IME formats, including `KOTOERI`.
- Configuration already includes privacy-related controls:
  - `incognito_mode`
  - `history_learning_level` (`DEFAULT_HISTORY`, `READ_ONLY`, `NO_HISTORY`)
- User dictionary storage currently persists to local `user://user_dictionary.db`.

Implication: we can build this project as additive improvements, not a rewrite.

## Design principles

- **User safety first:** never destroy local dictionaries during sync/import.
- **Text interchange format for sync:** sync plain text records, not binary DB internals.
- **Visible state:** privacy and sync status must be obvious in UI.
- **Small reversible steps:** each phase should be independently testable and shippable.

## Workstream A: Apple IME import compatibility

### Problem statement

Apple export files can vary by OS version and export path (delimiter, quoting, field labels, part-of-speech labels, encoding). Existing `KOTOERI` parser handles one known shape but may reject some real files.

### v1 scope

- Support currently observed Apple export format(s) from target macOS versions.
- Preserve current importer behavior for existing Mozc/ATOK/MSIME formats.
- Provide clear user-facing error guidance when entries are skipped.

### Proposed approach

1. Collect fixture files:
   - Export dictionaries from at least 2 macOS versions and 2 keyboard layouts.
   - Include files with comments/special characters/emoji.
2. Add robust format detection:
   - Keep auto-detect path.
   - Add explicit Apple/Kotoeri normalization branch if needed.
3. Parsing and normalization:
   - Parse row fields (reading, surface, POS, optional comment).
   - Normalize POS labels into existing Mozc `PosType`.
   - Keep unknown POS behavior as "skip with warning count".
4. Error reporting:
   - Reuse existing import summary dialogs.
   - Improve wording for Apple-specific unsupported rows.

### Test plan

- Unit tests: parser detection + row parsing + POS mapping.
- Integration tests: import fixture files into empty and populated dictionary.
- Manual QA: import from Dictionary Tool on macOS; verify resulting entries in conversion candidates.

## Workstream B: Privacy mode

### User expectation

When privacy mode is on, marinaMoji should avoid recording or learning from typing activity.

### v1 behavior definition

When privacy mode is enabled:

- Set `incognito_mode = true`.
- Set `history_learning_level = NO_HISTORY`.
- Keep user dictionary readable for conversions unless strict mode is requested later.
- Do not erase existing learned data automatically.

When disabled:

- Restore previous `history_learning_level` preference.
- Set `incognito_mode = false`.

### UX proposal

- Add a clear toggle in toolbar/menu: `Privacy mode`.
- Show active indicator in toolbar (icon or badge text).
- Optional confirmation text on first enable: "New learning is paused while Privacy mode is on."

### Risks and mitigations

- Risk: users assume dictionary lookup is also disabled.
  - Mitigation: clarify in tooltip/help text what is disabled.
- Risk: hidden state confusion.
  - Mitigation: keep always-visible status indicator.

### Test plan

- Toggle persistence across app restart.
- Verify no history growth during privacy mode sessions.
- Verify normal learning resumes after disabling mode.

## Workstream C: Encrypted cross-device sync

### Key architecture decision

Do **not** sync `user_dictionary.db` directly.

Reasons:

- it is a local protobuf store,
- concurrent writes across devices are unsafe,
- process-level lock files are not conflict-safe for cloud replication.

### v1 sync model

Use one encrypted bundle file in a user-selected shared folder (for example Nextcloud):

- `marinamoji_sync.mmz.enc`

Bundle contents are merged and encrypted as one unit:

- `manifest.txt`
- `dictionary.tsv`
- `history.tsv`

### Sync flow (v1)

1. Export local dictionary/history snapshot.
2. Decrypt remote bundle if present.
3. Merge by section:
   - dictionary dedupe key `(reading, surface, pos, locale)`,
   - history frequency merge by fingerprint.
4. Re-encrypt and atomically write bundle.
5. Import merged snapshot back to local state.

### Conflict strategy (v1)

- Dictionary merge is additive union with tombstone deletes (see `dictionary_tombstones.tsv` in `SYNC_PLAN.md`).
- History merge sums frequencies and preserves newest access time.

### Trigger model

- Manual **Sync now** from Preferences and Dictionary Tool.
- Optional interval scheduler and shutdown sync in `mozc_server`.
- File mtime polling to notice remote updates.

### Security and privacy notes

- Bundle encryption uses libsodium passphrase-based encryption.
- Sync key is stored locally and is never uploaded in plaintext.
- When privacy mode is on, history export is skipped.
- See `docs/SYNC_PLAN.md` and `docs/HOW_SYNC_WORKS.md` for details.

## Milestones

### M1: Import hardening

- Apple fixture set committed.
- Parser auto-detect improvements.
- Import tests passing.

### M2: Privacy mode UX + behavior

- Toolbar/menu toggle wired to config.
- Status indicator implemented.
- Learning suppression verified.

### M3: Manual Nextcloud sync

- Export/import TSV commands.
- Merge/dedupe logic.
- Basic sync report UI.

### M4: Quality and rollout

- End-to-end manual QA across 2+ devices.
- Recovery documentation (backup/restore).
- Optional background sync RFC.

## Open questions

- Which exact Apple export formats should be "required" in v1?
- Should privacy mode hide user dictionary candidates (strict mode) or only stop new learning?
- ~~Should sync ever support deletions?~~ Tombstone deletes shipped (v2); compaction retains 90-day window.
- Where should sync controls live first (Dictionary Tool, toolbar, or config dialog)?

## Implementation checklist (starter)

- [ ] Add this plan to docs index/reference where appropriate.
- [ ] Collect real Apple export fixtures (anonymized).
- [ ] Write failing tests for unsupported Apple rows.
- [ ] Implement parser/detection updates.
- [ ] Add privacy toggle UI and config wiring.
- [ ] Implement TSV export/import merge utility.
- [ ] Add manual sync commands and result dialogs.
- [ ] Write user documentation for setup with Nextcloud shared folder.

## Suggested first coding task

Start with M1 fixture collection + importer tests before UI changes. This de-risks all later work because both privacy and sync workflows depend on reliable import/export behavior.
