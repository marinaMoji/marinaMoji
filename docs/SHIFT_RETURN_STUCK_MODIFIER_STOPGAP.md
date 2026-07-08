# Stopgap plan: stuck Shift/Ctrl/Return failsafe + logging

**Status:** not implemented. Written 2026-07-08 as a fallback if the proper
per-key tracking refactor (see below) turns out to be too risky/slow to land
before the bug needs to stop affecting daily use.

## Background

`KeyEventHandler::ProcessModifiers` (`src/unix/ibus/key_event_handler.cc`)
tracks all physically-held keys in one shared `currently_pressed_modifiers_`
set plus a handful of chord-tracking bools. Two reset points —
`key_event_handler.cc:287-289` and `key_event_handler.cc:420-422` — clear
*all* tracked state whenever a routine zero-modifier or no-pending-modifier
event comes through, which can silently drop tracking for an unrelated key
(e.g. Return) that happens to still be physically held at that instant. This
is the confirmed root cause of the "shift/return fails to release" reports
in `temp/logs_mp_notes.md` (see chat history / commit `207dd2620`,
`2df4e2cf9` for prior partial fixes, Shift-only).

The real fix is a per-key state refactor (tracked separately). This doc is
the narrower, faster patch to apply *if* we need something shipped sooner:
reduce symptom frequency and instrument the failure so we can confirm the
refactor actually fixes it once it lands.

## Stopgap scope

1. **Generalize the existing Shift-only failsafe to Ctrl and Return.**
   - `TrackedShiftKeyvals()` / `ForwardTrackedShiftReleases()`
     (`key_event_handler.h:64,70`, `.cc:197-213`) already forward a
     synthetic `IBUS_RELEASE` for any Shift keyval still recorded as
     pressed. Extend this to also cover `IBUS_Control_L/R`, and add an
     equivalent path for `IBUS_Return` (which currently isn't covered
     because the `mozc_engine.cc:765-774` failsafe explicitly requires
     `!key.has_special_key()`, excluding Return).
   - Call sites: currently wired to `Disable`, `FocusOut`, `SetContentType`
     (`mozc_engine.cc:298,389-390,784-787`). These only fire on IME
     deactivation/focus loss/mode change — i.e., the failsafe only runs when
     the user *happens* to trigger one of those events. Consider also
     firing it opportunistically at the top of `ProcessModifiers` when a
     key has been tracked as pressed for longer than some threshold (see
     below).

2. **Time-based cleanup.** Add a timestamp per entry in
   `currently_pressed_modifiers_` (would need to become a
   `map<uint, timestamp>` instead of a `set<uint>` for this alone — note
   this overlaps with the real refactor's data structure change). If a
   tracked key exceeds ~2-3 seconds without a matching key-up, treat it as
   stuck: forward a synthetic release and log a warning. This directly
   targets the "every few minutes, no identifiable pattern" symptom instead
   of waiting for a lifecycle event to clean it up.

3. **Logging: capture the failure, not just the recovery.** The existing
   `2df4e2cf9` instrumentation logs the state *when* a failsafe fires
   (`return_modifier_release_failsafe_forward`,
   `forward_tracked_shift_release`) but not the event immediately preceding
   the moment tracking was lost. Add a log line at both `Clear()` call
   sites (`key_event_handler.cc:288`, `:421`) that dumps
   `currently_pressed_modifiers_` contents *before* clearing, tagged with
   which branch triggered it. This is the data we'd need to confirm (a)
   the stopgap failsafe caught the case, and (b) whether the per-key
   refactor, once landed, eliminates the trigger entirely rather than just
   the symptom.

## Testing plan for the stopgap

Since the bug is intermittent (~every few minutes, no known trigger),
there's no reliable manual repro in a 30-minute window. Approach:
- Land the stopgap + logging behind normal use (your collaborator's daily
  workflow already reproduces it reliably over hours).
- Compare `.tsv` debug logs before/after: look for whether
  `forward_tracked_shift_release` / the new Ctrl/Return equivalents fire
  measurably more often than user-visible complaints (i.e. the failsafe is
  quietly catching cases that would otherwise have surfaced as "stuck"
  reports).
- Treat any case where the *new* pre-`Clear()` logging shows a key was
  dropped but the failsafe did NOT fire (e.g. because it's not on a
  lifecycle hook and the time threshold hadn't elapsed yet) as a concrete
  test case for the eventual refactor's unit tests.

## Explicit caveat

This is a band-aid, not a fix. It reduces frequency/duration of the stuck
state and improves diagnosability, but the shared-state design that causes
one key's cleanup to clobber another's tracking remains. Do not consider
this "resolved" — track the per-key refactor as the actual fix.
