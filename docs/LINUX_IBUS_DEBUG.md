# marinaMoji IBus debug logging (Linux)

Use this when diagnosing the intermittent **Return does nothing** bug (or any
low-level key-delivery issue). Logging is **opt-in** and has almost no effect
when disabled.

Technical background: [`LINUX_RETURN_DELIVERY_HANDOFF.md`](LINUX_RETURN_DELIVERY_HANDOFF.md).

---

## 1. Install the debug build

Follow [`compiling_instructions_for_marina.md`](compiling_instructions_for_marina.md)
(or use a `.zip` build Daniel sends you):

```bash
sudo unzip -o bazel-bin/unix/mozc.zip -d /
ibus write-cache
```

Then restart IBus (see section 2).

---

## 2. Enable logging

Logging is controlled by environment variables read when **IBus starts**. They
must be set on the **`ibus-daemon`** process, not only in a terminal where you
type.

### 2.1 One-time setup: log file

Pick a path you can write to, for example:

```bash
mkdir -p ~/marinamoji-debug
export MARINAMOJI_IBUS_DEBUG_LOG="$HOME/marinamoji-debug/ibus-debug.tsv"
```

The file is **appended** (tab-separated). Filter by date when sharing logs.

### 2.2 Restart IBus with logging on

```bash
ibus exit
MARINAMOJI_IBUS_DEBUG_LOG="$HOME/marinamoji-debug/ibus-debug.tsv" ibus-daemon -drx
```

Re-select **marinaMoji** in your input settings if needed.

To make this permanent, put the `export` and a small wrapper script in
`~/.profile` or a desktop autostart entry — but only while debugging; the log
grows quickly.

### 2.3 Session banner (first line of each engine process)

When logging is on, the first event in a session writes something like:

```
engine.lifecycle  session_start version=… echo_back_shift_l=0 XDG_SESSION_TYPE=wayland …
```

**Please note `XDG_SESSION_TYPE`** (wayland vs x11) when reporting bugs — the
log does not record the display server on every line.

---

## 3. A/B test: echo-back Shift_L injection (optional)

Default (2026-08-31 build): echo-back Backspace injects **2** synthetic keys
(press + release).

Older builds also injected a bare **Shift_L release** (3 events per Backspace).
That is restored only when you opt in:

```bash
ibus exit
MARINAMOJI_IBUS_DEBUG_LOG="$HOME/marinamoji-debug/ibus-debug.tsv" \
MARINAMOJI_IBUS_ECHO_BACK_SHIFT_L=1 \
ibus-daemon -drx
```

Compare the same app and typing pattern **with and without**
`MARINAMOJI_IBUS_ECHO_BACK_SHIFT_L` in the same environment. The banner line
shows `echo_back_shift_l=0` or `1`.

When Shift_L is enabled, look for:

```
engine.forward  echo_back_shift_l_release keycode=42
```

---

## 4. What to log when Return fails

1. Note the **clock time** (to the second) and **application** (LibreOffice, Firefox, etc.).
2. Note whether **clicking elsewhere** made the next Return work.
3. Send the log file (or a slice ±30 seconds around the failure).

Useful grep patterns:

```bash
# Return presses the engine declined (handed to the app)
rg '65293|return_output_consumed' ~/marinamoji-debug/ibus-debug.tsv

# Echo-back Backspace path (which branch the app uses)
rg 'engine\.echoback|engine\.forward' ~/marinamoji-debug/ibus-debug.tsv

# Focus changes (often coincide with "click elsewhere" healing)
rg 'engine\.lifecycle' ~/marinamoji-debug/ibus-debug.tsv
```

Key tags:

| Tag | Meaning |
|-----|---------|
| `engine.key return_output_consumed consumed=0` | Engine passed Return to IBus/app |
| `engine.echoback delete_surrounding` | Backspace handled without synthetic keys |
| `engine.echoback forward_no_surrounding_cap` | App lacks surrounding text; keys injected |
| `engine.forward forward_key` | Synthetic key injected (count per echo-back Backspace) |
| `engine.lifecycle capabilities` | App capabilities at focus-in |
| `engine.lifecycle cursor_at_focus_in` | Caret rect when focus enters a field (`surround_stale` flag included) |
| `engine.lifecycle surround_stale_after_echo_back set` | Declined Return may have left surrounding text stale |
| `engine.lifecycle set_cursor_location … surround_stale_cleared=1` | Client reported a new caret; stale flag cleared |
| `engine.lifecycle surround_stale_after_echo_back cleared` | Stale flag cleared (`reason=consumed_key` or cursor update) |

If Return fails in the app but the log shows `return_output_consumed consumed=0`,
the bug is **after** the engine (IBus GTK module or the application), not inside
marinaMoji's key handler.

---

## 5. Environment variables (summary)

| Variable | Required | Effect |
|----------|----------|--------|
| `MARINAMOJI_IBUS_DEBUG_LOG` | Yes, for logging | Path to append-only TSV log |
| `MARINAMOJI_IBUS_ECHO_BACK_SHIFT_L` | No | `1` / `true` restores legacy Shift_L release on echo-back Backspace |

Truthy values for the Shift_L flag: `1`, `true`, `yes`, `on`. Falsy: unset, `0`,
`false`, `no`, `off`.
