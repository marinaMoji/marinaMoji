# Docket: reviewing unregistered vocabulary

This document describes the **docket**, a feature added to this fork for
absorbing rare or specialist vocabulary (e.g. 大元宮/daigenguu) that gets
typed and committed but never explicitly registered, into permanent, active
user-dictionary entries.

## Why

`Ctrl+Shift+0` (`LaunchWordRegisterDialog`) already prefills the word-register
dialog from the single most recent commit, but that slot is overwritten by
the very next commit and cleared on state reset. It works for "register this
right now," but not for vocabulary noticed in passing that the user wants to
deal with later — by the time there's leisure to review it, the slot is gone
and plain input history has usually emptied too.

The docket solves this by silently queuing every dictionary-unknown
committed compound to a small persistent store, reviewed later in a
minimalist table UI.

## Behaviour

- **Capture (silent, server-side):** on every commit of 2+ characters, the
  session checks whether the committed surface is already in the system or
  user dictionary. If not, it's appended to the docket — deduped by surface,
  capped at 200 pending entries (oldest evicted first) — along with a
  best-effort part-of-speech guess derived from the candidate's POS ID.
  Words already on the permanent "never" list (see below) are skipped.
- **Review (`docket_tool`):** a table with one row per pending entry —
  editable Surface and Reading cells, a POS dropdown prefilled with the
  best-effort guess, and three per-row actions:
  - **Yes** — registers the row (using whatever is currently in the Surface/
    Reading/POS cells, so a stray reading can be fixed first) into the user
    dictionary and removes it from the docket.
  - **No** — removes it from the docket for now. If the same word gets
    committed again later, it reappears (this is not a "never" signal).
  - **Never** — removes it from the docket and adds the surface to a
    permanent block-list, so it's never queued again.
- **Windows toolbar:** the toolbar button that used to open the single-entry
  "Add Word" dialog now opens the docket instead, with a small badge dot
  when entries are pending. `Ctrl+Shift+0` is unchanged and still does
  ad-hoc single-word registration via the old dialog — the docket is a
  separate, slower-paced review path, not a replacement for it.

## Implementation

- **Storage — `dictionary/docket_store.{h,cc}`:** `DocketStore` persists a
  small hand-rolled JSON file (`docket.json` under the user profile
  directory) holding the pending list and the never-list. Mutating calls
  (`AddPending`/`RemovePending`/`AddNever`) each do a locked
  read-modify-write round trip (`ProcessMutex`, same pattern as
  `UserDictionaryStorage`) so the server process and the review dialog don't
  clobber each other; `ReadDocketDataUnlocked()` is an unlocked best-effort
  read for callers that only want a snapshot (e.g. the toolbar badge count).
- **Capture gate — `engine/engine.{h,cc}`, `session/session.cc`:**
  `EngineInterface::IsKnownWord()`/`RecordDocketCandidate()` expose the
  dictionary-membership check and docket write, backed by
  `Modules::GetDictionary()`/`GetUserDictionary()` and a `DocketStore`
  instance owned by `Engine`. `Session::CommitInternal` calls these right
  after its existing last-commit-buffer capture, reusing `lid`/`rid` that
  already ride along on `commands::Result.tokens` — no converter-side
  plumbing needed for that part. `Engine::RecordDocketCandidate` maps `lid`
  through `PosMatcher` to a coarse POS label (falling back to 名詞) before
  writing the entry, so the review UI never needs its own `PosMatcher`
  access.
- **Review UI — `gui/docket/`:** `DocketDialog` (`docket_dialog.{h,cc}`) is a
  `QMainWindow` built directly in C++ rather than from a Qt Designer `.ui`
  file — the layout (a table plus a refresh button) is small enough that
  hand-authoring it is simpler than an unreviewable `.ui` XML blob, and the
  class defines no signals/slots/properties of its own, so it needs no
  `Q_OBJECT` and therefore no `moc` step either. Registration reuses
  `UserDictionaryStorage`/`dictionary::UserPos::ToPosType`, the same
  mechanism `WordRegisterDialog::SaveEntry()` uses.
- **Tool dispatch:** `commands::Output::ToolMode::DOCKET_DIALOG` and
  `commands::SessionCommand::LAUNCH_DOCKET_DIALOG` mirror the existing
  `DICTIONARY_TOOL`/`LAUNCH_DICTIONARY_TOOL` plumbing end to end:
  `Session::LaunchDocketDialog()` → `Client::TranslateProtoBufToMozcToolArg()`
  (and the win32 TIP's equivalent switch in
  `win32/base/keyevent_handler.cc`) → `mozc_tool --mode=docket_dialog` →
  `RunDocketDialog()` in `gui/docket/docket_dialog_libmain.cc`.
- **Windows toolbar — `renderer/win32/toolbar_window.{h,cc}`:** the existing
  `ButtonId::kDictionary` button is repointed from
  `SendLaunchWordRegisterDialog()` to `SendLaunchDocketDialog()`, its label
  changed to the new `MM.Docket` localized string
  (`renderer/win32/marina_localized_string.cc`). The pending count is
  re-read (unlocked, best-effort) at the top of every `Redraw()` and drawn
  as a small solid dot in the icon's corner via the same `FillRoundedRect`
  helper already used for the hover highlight — deliberately not a digit
  count, since GDI text drawn into the toolbar's raw premultiplied-alpha
  pixel buffer would need its own alpha-channel bookkeeping to composite
  correctly under `UpdateLayeredWindow`.

## Files

- `dictionary/docket_store.{h,cc}`, `dictionary/docket_store_test.cc`
- `engine/engine_interface.h`, `engine/engine.{h,cc}`, `engine/engine_mock.h`
- `session/session.{h,cc}`, `session/session_test.cc`
- `protocol/commands.proto` (`ToolMode::DOCKET_DIALOG`,
  `SessionCommand::LAUNCH_DOCKET_DIALOG`)
- `client/client.cc`, `win32/base/keyevent_handler.cc` (tool-mode → binary
  name translation)
- `gui/docket/` (`docket_dialog.{h,cc}`, `docket_dialog_libmain.cc`,
  `docket_dialog_main.cc`, `BUILD.bazel`)
- `gui/tool/mozc_tool_libmain.cc` (`docket_dialog` mode dispatch)
- `renderer/win32/toolbar_window.{h,cc}`,
  `renderer/win32/marina_localized_string.cc`

## Known limitations / follow-ups

- The docket button reuses the existing dictionary-tool icon asset rather
  than a new dedicated icon — cosmetic, swap the PNG later if desired.
- The badge is a presence dot, not a count, for the alpha-compositing reason
  above.
- Whether a segment got manually resegmented (`SegmentWidthExpand`/`Shrink`)
  is a plausible extra "the converter guessed wrong" signal for sorting or
  highlighting rows, but isn't captured yet — the dictionary-membership gate
  alone was judged sufficient for a first pass.
- `docket_tool` and the storage/capture layers are cross-platform, but only
  the Windows toolbar currently surfaces a button for it; other platforms
  can still launch it manually (`mozc_tool --mode=docket_dialog`) but have no
  UI entry point yet.
