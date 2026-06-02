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

## Current baseline in marinaMozc

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

## Workstream C: Nextcloud sync

### Key architecture decision

Do **not** make Nextcloud sync target `user_dictionary.db` directly.

Reasons:

- `user_dictionary.db` is a binary protobuf store.
- Concurrent writes across devices can corrupt or overwrite data.
- File-level locking is local-process oriented, not multi-device conflict-safe.

### Recommended sync model

Use a portable text exchange file in shared folder, for example:

- `marina_user_dictionary.tsv`

Columns (v1):

1. reading
2. surface
3. pos
4. comment (optional)
5. locale (optional)
6. updated_at (optional for future conflict policies)

### Sync flow (v1)

1. Local export task writes current dictionary entries to TSV snapshot.
2. Import task reads shared TSV and merges into local dictionary.
3. Deduplicate by `(reading, surface, pos, locale)`.
4. Save merge report (added/skipped/invalid counts).

### Conflict strategy (v1)

- Last-writer-wins at row level is acceptable initially.
- Prefer additive merge (never auto-delete local entries in v1).
- Keep manual cleanup path via Dictionary Tool.

### Trigger model

Start simple:

- Manual actions:
  - `Sync now (pull)`
  - `Sync now (push)`
- Later:
  - timed background sync with backoff.

### Security and privacy notes

- Nextcloud folder permissions remain user-managed.
- For highly sensitive users, recommend pairing with Privacy mode.
- Consider optional encryption-at-rest for sync file in later phase.

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
- Should sync ever support deletions, or stay additive-only long term?
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
