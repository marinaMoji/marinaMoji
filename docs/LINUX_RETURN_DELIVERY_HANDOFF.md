# Handoff: "Return does nothing until you click elsewhere" (Linux/ibus)

**Status:** uncommitted changes sit in the working tree on top of `9334d27a5`.
Nothing here has been compiled. Written 2026-08-31 as the brief for whoever
picks this up in an Ubuntu VM.

**Who this is for:** the next agent or person to work on this. It assumes no
knowledge of the preceding conversation. Read it before touching the code.

---

## 1. The bug

A collaborator in Japan, typing Japanese daily, reports that Return
intermittently does nothing, and that clicking elsewhere in the document makes
it work again. Reported eight times in 70 minutes on 2026-08-31 (16:37, 16:38,
16:39, 16:40, 16:42, 17:02, 17:05, 17:30 JST). She is the only known
reproducer.

Debug logs live in `~/ShareDocs/@Home/marinaMoji Docs/logs/marinamoji-ibus-debug.tsv`
(tab-separated: timestamp, pid, tag, message; appended to, so it holds several
sessions — filter by date). Logging is opt-in via the
`MARINAMOJI_IBUS_DEBUG_LOG` env var; see `src/unix/ibus/ibus_debug_log.cc`.

---

## 2. What is established, and what is not

### 2.1 Established: the engine is not at fault

This is the important part, because it closes off most of the search space.
The 2026-08-31 session (pid 81280) was the first taken with lifecycle logging
in place, and it caught the failure end to end:

```
16:42:37.743  RET press -> server_output consumed=0 pre=0 res=0  -> return false   [nothing]
16:42:40.218  RET press -> server_output consumed=0 pre=0 res=0  -> return false   [nothing]
16:42:41.4    reset x3 / set_content_type purpose=0 hints=0
16:42:41.5    focus_out -> focus_in -> focus_out -> focus_in
16:42:41.665  set_content_type purpose=0 hints=64                                  [she clicked]
16:42:42.756  RET press -> server_output consumed=0 pre=0 res=0  -> return false   [worked]
```

Three Return presses, byte-identical engine behaviour, only the third one
landed. The engine declined all three (`return false`), which obliges IBus to
hand the key to the application.

Across the whole session: all 395 Return presses were handled correctly (330
committed a composition, 55 declined, 10 forwarded while the IME was inactive);
all 330 commits had a genuinely live preedit, so zero phantom commits; and the
keystroke transcripts read as ordinary romaji input.

**Do not go looking for the bug inside `MozcEngine::ProcessKeyEventInternal`.**
It is in the delivery of a key the engine declined — client side, or in the
IBus plumbing between them.

### 2.2 The lead: synthetic events from the echo-back Backspace path

Per-minute averages, her eight reported minutes against the other 41:

| | reported | other | |
|---|---|---|---|
| echo-back Backspaces | 16.88 | 2.76 | **6.1x** |
| Backspace releases | 16.00 | 3.41 | 4.7x |
| Return presses | 8.88 | 7.93 | flat |
| focus_out | 2.75 | 4.93 | *lower* |
| reset | 3.50 | 7.00 | *lower* |

It is not general activity and not focus churn — both are *lower* in the bad
minutes. Returns in those minutes also sit 3x closer in time to an echo-back
Backspace (median 14.0s against 44.6s).

`IbusEngineWrapper::ForwardBackspaceForEchoBack` was forwarding **three**
synthetic events per Backspace — press, release, and a bare `Shift_L` release —
while the engine swallowed both real ones (the press via
`TryHandleEchoBackBackspace` returning true, the release via
`return_backspace_release down_consumed=1`). At ~17 Backspaces a minute that is
~50 fabricated events a minute entering the client.

**Hypothesised mechanism (unproven):** the GTK IBus IM module keeps a queue of
events it has already handed to the engine, so it can recognise its own
forwarded events rather than reprocessing them. A surplus there could make a
later genuinely-declined key match a stale entry and be dropped; re-establishing
the input context would clear it, which is what the click at 16:42:41 did.
Confirmed that the forwarded events do not re-enter the engine: 0 of 248
echo-backs produced an engine-visible `Shift_L` release.

### 2.3 Not established

- **Direction of the Backspace correlation.** Heavy backspacing may partly be a
  *consequence* of the bug (delete and retry) rather than its cause. At 16:42
  the Backspaces precede the dead Returns; at 16:37 they follow.
- **The mechanism itself.** Section 2.2 is a hypothesis fitted to an association
  over eight self-reported minutes. Treat it as the first thing to try to
  falsify, not as a diagnosis.
- **Her display server.** Wayland and X11 have different key-delivery paths and
  the log does not record which. **Ask her.** A VM on the wrong one verifies the
  build but says little about the bug.

---

## 3. What is in the working tree

Five modified files on top of `9334d27a5`, none compiled:

| File | Change |
|---|---|
| `src/unix/ibus/ibus_wrapper.cc` | Removed the bare `Shift_L` release injection; `ForwardKeyEvent` is now the single logged choke point for synthetic events; `DeleteSurroundingText` logs too |
| `src/unix/ibus/mozc_engine.cc` | `TryHandleEchoBackBackspace` names which of its exits it took; new `LogClientCapabilities` called from `FocusIn` |
| `src/unix/ibus/mozc_engine.h` | Declares `LogClientCapabilities` |
| `src/unix/ibus/BUILD.bazel` | `ibus_debug_log` extracted into its own `mozc_cc_library` so `ibus_wrapper` can log; removed from `ibus_mozc_lib` srcs/hdrs, added as a dep in two places |
| `CHANGELOG.md` | Entry describing the above |

One behaviour change (the `Shift_L` removal). Everything else is logging.

New log tags to expect:

- `engine.forward forward_key keyval=… keycode=… modifiers=0x… release=…` — every synthetic key event the engine injects.
- `engine.forward delete_surrounding offset=… size=…`
- `engine.echoback delete_surrounding | forward_no_preceding_text | forward_surrounding_text_failed | forward_no_surrounding_cap` — which exit the echo-back Backspace took. **Only the first injects nothing.**
- `engine.lifecycle capabilities caps=0x… surrounding_text=…` — at every `focus_in`.

### 3.1 Prior context on these files

`74441068f "Linux return bug stab"` (2026-08-27) was the previous attempt: it
fixed a real leak (the held-key map was keyed on `keyval`, which is
modifier-dependent, so `Shift`+letter leaked an entry that could never be
erased) and added the lifecycle logging that made section 2.1 possible. It did
**not** fix the Return symptom.

Neither half of it can account for the report that things got worse: the 2s age
guard never fired at all (`skip_stale_release` appears 0 times in the session),
and replaying her log shows the keycode fix behaving as designed — keyval-keying
would have produced 77 synthetic releases (14 stale) where keycode-keying
produced 63 (1 stale), matching the 60 logged. `9334d27a5`'s ibus changes are
inert here too (`odoriji_show_pending_` was never set; no `engine.odoriji`
lines). Exposure also changed sharply: 2026-08-27 was ~7h of testing with the
IME off 40% of the time and 203 Return presses, against 70 minutes of real
Japanese writing and 395 Return presses on 2026-08-31.

See `docs/SHIFT_RETURN_STUCK_MODIFIER_STOPGAP.md` for the older, related
stuck-modifier work and its conclusion that there is no reliable manual repro
in a short window.

---

## 4. Tasks, in order

### 4.1 Compile and test it — the blocking task

Two consecutive patches have gone to her without ever being built. This is the
whole reason for the VM trip. **Do this before anything else.**

All Bazel commands run from **`marinaMoji/src`**, not the repo root.

```bash
bazelisk test //unix/ibus:ibus_mozc_test --config oss_linux \
  --repo_env=CC=clang --repo_env=CXX=clang++
```

That target includes two regression tests added on 2026-08-27 that have never
executed: `ShiftedKeyReleasedAfterShiftIsUntracked` and
`StalePressIsNotForwarded` (`src/unix/ibus/key_event_handler_test.cc`).

Then the full build and install:

```bash
bazelisk build package --config oss_linux --config release_build \
  --repo_env=CC=clang --repo_env=CXX=clang++
sudo unzip -o bazel-bin/unix/mozc.zip -d /
ibus write-cache && ibus restart
```

The `BUILD.bazel` change in section 3 is the likeliest thing to break, and it
will break at link time (duplicate or missing symbols), not compile time.
Build steps and install paths: `docs/compiling_instructions_for_marina.md` for
day-to-day, `docs/build_marinamoji_for_linux.md` for the reference.

### 4.2 Find out which echo-back branch real apps take

**Highest information per minute of work, and it does not require reproducing
the bug.** With logging enabled, in each of a GTK app (gedit), Firefox, and an
Electron app: type nothing, press Backspace, read the `engine.echoback` line.

- `delete_surrounding` → no synthetic key events were being forwarded in the
  common case at all. The injection rate was already near zero and **the
  hypothesis in 2.2 is largely dead.** Say so and stop; do not ship on it.
- `forward_no_surrounding_cap` (or the other two) → the mechanism is live and
  the change is aimed correctly.

Also confirm from `engine.forward forward_key` lines that an echo-back Backspace
now emits **2** synthetic events, not 3.

### 4.3 Try to reproduce deterministically

The hypothesis in 2.2 is deterministic, not statistical: if a surplus of
forwarded events desyncs the IM module's queue, a scripted burst should fail
repeatably. Worth one attempt before falling back to her as the test harness.

In an app that section 4.2 showed takes a forwarding branch, script with
`xdotool`: a burst of Backspaces (enough to inject 50+ synthetic events), then
Return, and check whether the newline appears. Build with and without the
`Shift_L` release and compare. If this reproduces, the guessing stops.

If it does not reproduce, that is expected and not a failure — see
`SHIFT_RETURN_STUCK_MODIFIER_STOPGAP.md`. Move to 4.4.

### 4.4 Put the `Shift_L` removal behind an env var before handing her a build

**Done (2026-08-31):** `MARINAMOJI_IBUS_ECHO_BACK_SHIFT_L` in
`src/unix/ibus/ibus_debug_log.cc` / `ibus_wrapper.cc`. Default off (2 synthetic
events). Set to `1` to restore the legacy Shift_L release (3 events). Session
banner logs the flag plus display-server env vars. See
[`docs/LINUX_IBUS_DEBUG.md`](LINUX_IBUS_DEBUG.md).

Recommended regardless of the outcome above. Precedent:
`MARINAMOJI_IBUS_DEBUG_LOG` in `src/unix/ibus/ibus_debug_log.cc`.

Rationale: each round-trip with her costs a day and crosses a language barrier
and a time zone. An env var lets her toggle the one behaviour change mid-session
and produce a before/after from the *same* environment, app and typing, instead
of comparing builds a week apart. The previous patch's effect was ambiguous
precisely because everything changed at once.

---

## 5. Pitfalls

- **The ibus targets cannot be built on macOS.** They are
  `target_compatible_with = ["@platforms//os:linux"]`. Naming one directly
  errors, but a wildcard build skips them silently — which is how two unbuilt
  patches shipped. There is no Docker on the Mac either
  (`docker/ubuntu22.04`, `docker/ubuntu24.04` exist but the daemon is not
  installed), so a VM is the only path.
- **Do not add `IBUS_CAP_*` constants you have not verified.** `LogClientCapabilities`
  logs the raw mask and breaks out only `IBUS_CAP_SURROUNDING_TEXT`, which was
  already in use in that file. Two other bits were deliberately left undecoded
  because there were no ibus headers available to check the spellings against.
- **The `Shift_L` release was there for a reason.** Its comment cited "spurious
  focus_out/focus_in after forwarded Backspace". If that symptom returns, fix it
  without fabricating a key event — do not simply restore it.
- **`return_echo_back_backspace_consumed` predates section 3** and cannot
  distinguish the exits. When reading logs from before 2026-08-31, that tag
  alone tells you nothing about whether keys were injected.
- **The log file accumulates sessions.** Filter by date before drawing per-minute
  statistics, or you will mix her session with Daniel's test runs, which have
  very different key mixes (40% of keys with the IME off, in the 2026-08-27 run).

---

## 6. What would falsify the lead

State this plainly in whatever you report back:

1. Section 4.2 shows real apps taking the `delete_surrounding` exit → injected
   events were already rare, and the correlation in 2.2 is more likely an
   artifact of her backspacing *because* Return failed.
2. Section 4.3 reproduces the dead Return **with** the `Shift_L` release removed
   → the injected event count is not the trigger.
3. Her next session shows dead Returns in minutes with a low
   `engine.forward forward_key` count → same conclusion.

In any of those cases the next place to look is the client side: which
application, X11 or Wayland, and the GTK IM module's own event queue — not
further changes to the engine.
