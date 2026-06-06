## Sync Experiment Snapshot (June 2026)

This document records the current experimental sync integration state before rollback.

### What is included in this snapshot

- New encrypted user sync module under `src/sync/` (crypto, bundle, merge, config, scheduler).
- Session/client/proto wiring for sync operations.
- Config dialog + dictionary tool UI integration for sync controls.
- Documentation drafts for sync behavior, plan, and manual QA.

### Why this snapshot is being preserved

- Recent macOS IMK startup behavior became unstable during this integration window.
- We want to keep all sync work for later reuse while restoring the main branch to a known-working baseline.

### Intended next step

- Return local working tree to the previous stable commit.
- Reintegrate sync incrementally from this experimental branch, validating macOS IMK behavior at each step.
