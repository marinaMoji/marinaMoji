# Changelog

Notable marinaMoji-specific changes, kept by hand. This is not upstream
Mozc's release history (see [`docs/release_history.md`](docs/release_history.md)
for that) — it tracks work specific to this fork, most usefully the parts
that touch more than one file or aren't obvious from a commit subject line.

Format: newest entry first, grouped by date. Each entry should say what
changed and, where it isn't obvious, why.

## Unreleased

### Windows floating toolbar: parity pass with macOS, plus fixes

The Windows toolbar (`renderer/win32/toolbar_window.cc`) and Symbols Palette
(`renderer/win32/symbols_palette_window.cc`) were compared against the macOS
implementation (`mac/mozc_toolbar.mm`) and brought up to parity, with a
number of pre-existing bugs fixed along the way. **macOS and Linux/GTK
toolbar code is untouched** — the shared pieces below are new, Windows-only
modules, not refactors of the mac/Linux implementations.

Bugs fixed:

- Symbols Palette: window size no longer omits the non-client area
  (caption/borders), which was clipping the bottom row of symbols.
- Symbols Palette: content taller than the window now scrolls instead of
  being silently cut off (e.g. a long `user_symbols.txt`).
- Symbols Palette: child controls now get an explicit `WM_SETFONT` (shell UI
  font, enlarged for symbol glyphs) instead of falling back to the legacy
  bitmap `SYSTEM_FONT`, which rendered CJK/symbol glyphs poorly.
- Symbols Palette: full per-monitor DPI scaling (previously all metrics were
  raw, unscaled pixels).
- Symbols Palette: dark mode is now respected (background, text, and common
  controls via `DarkMode_Explorer` theming); previously always light.
- Toolbar: DPI is now re-read after a drag and on `WM_DPICHANGED`, so moving
  the toolbar to a monitor with a different scale factor rescales it.
  Previously only `WM_DISPLAYCHANGE` (a resolution change) triggered a
  refresh.
- Toolbar: icon-tier selection now rounds ties up (was rounding down),
  fixing under-scaled icons at 125% and 175% Windows scaling.
- Toolbar: `SavePosition()` no longer truncates `toolbar.conf` down to just
  `x=`/`y=`, which was silently dropping the `toolbar_visible` key written
  by the TIP from a different process.
- Toolbar: a position restored from `toolbar.conf` is now clamped back onto
  a currently-attached monitor, so unplugging the monitor the toolbar was on
  can no longer strand it off-screen with no way to drag it back.
- Toolbar: a `Hide()` suppressed while the mode menu is open is now
  re-applied once the menu closes, instead of being dropped — previously the
  toolbar could stay on screen indefinitely after a focus change during menu
  use.
- Toolbar: renamed the `LoadIcon` helper to `LoadToolbarIcon` — it was
  silently colliding with the `<winuser.h>` `LoadIcon`/`LoadIconW` macro.
- `win32/base/toolbar_config.cc`: `LoadToolbarVisiblePreference()` is now
  cached with a 1-second TTL instead of hitting disk on every renderer
  update (i.e. every keystroke, on the TSF thread, inside the host
  application's process). The TTL keeps cross-process propagation working
  (the TIP loads separately into every application).

UX/feature parity added:

- Toolbar: hover and pressed-state highlighting on all six buttons (custom
  GDI-composited, since the toolbar has no child HWNDs).
- Toolbar: tooltips on every button, via a `TOOLTIPS_CLASS` control.
- Toolbar: an `IAccessible` (MSAA) implementation
  (`renderer/win32/toolbar_accessible.{h,cc}`) exposing the toolbar as a
  `ROLE_SYSTEM_TOOLBAR` with six `ROLE_SYSTEM_PUSHBUTTON` children — the
  toolbar was previously invisible to screen readers/UI Automation, since it
  draws everything into one composited bitmap. `accDoDefaultAction` posts
  back to the toolbar's own thread (MSAA calls arrive on an RPC thread).
- Toolbar: right-click context menu (Dictionary Tool, Keyboard Shortcuts,
  Settings, Hide toolbar) — previously the Dictionary Tool and "hide
  toolbar" were unreachable from the toolbar itself.
- Toolbar and Symbols Palette: localized strings (EN/FR/JA), via new
  `renderer/win32/marina_localized_string.{h,cc}` — previously all UI text
  was hardcoded English literals. Mirrors the `MM.*` keys used by
  `mac/Resources/{en,fr,ja}.lproj/Localizable.strings` so wording stays in
  step across platforms, without sharing code (this file is Windows-only).
- **New: Keyboard Shortcuts viewer for Windows**
  (`renderer/win32/shortcuts_window.{h,cc}`), reachable from the toolbar's
  Shortcuts button and its context menu. Mirrors macOS's
  `MozcShortcutsWindowController` (Script / Composition / Kaeriten tabs).
  Supporting plumbing:
  - New `session/marina_shortcut_list_util.{h,cc}` (Windows-only,
    `visibility = ["//win32/tip:__pkg__"]`): builds the three shortcut
    lists from the effective keymap table, marina number-row bindings, and
    kaeriten table. Modelled on macOS's private helpers in
    `mac/mozc_toolbar.mm` but implemented separately — reads keymap tables
    via `ConfigFileStream`'s `system://` scheme rather than loose files,
    and de-duplicates `(command, key)` pairs. Has its own test suite
    (`marina_shortcut_list_util_test.cc`, 9 cases).
  - New `RendererCommand::ApplicationInfo::ShortcutsInfo` proto message
    (`protocol/renderer_command.proto`), populated TIP-side
    (`win32/tip/tip_ui_handler_conventional.cc`'s `FillShortcutsInfo`) since
    the renderer deliberately doesn't link config/keymap parsing.
  - New `SessionCommand` types `SHOW_SHORTCUTS_WINDOW` /
    `HIDE_SHORTCUTS_WINDOW` (local UI-visibility signals, intercepted in
    `win32/tip/tip_edit_session.cc`, same pattern as the existing
    `SHOW_SYMBOLS_PALETTE`/`HIDE_SYMBOLS_PALETTE`) and a new
    `TipPrivateContext::shortcuts_window_visible()` flag.
- New `SessionCommand::LAUNCH_DICTIONARY_TOOL` (wired into
  `session/session.cc`'s `Session::LaunchDictionaryTool`) and
  `SessionCommand::HIDE_TOOLBAR` (writes the `toolbar_visible` preference
  from the TIP side), both reachable from the toolbar's new context menu.
- `renderer/renderer_server.cc`: allowlisted and routed the four new
  `SessionCommand` types above through the existing renderer→TIP channel.

Files changed: `renderer/win32/toolbar_window.{h,cc}`,
`renderer/win32/symbols_palette_window.{h,cc}`,
`renderer/win32/window_manager.{h,cc}`, `renderer/renderer_server.cc`,
`session/session.cc`, `win32/base/toolbar_config.cc`,
`win32/tip/tip_edit_session.cc`, `win32/tip/tip_private_context.{h,cc}`,
`win32/tip/tip_ui_handler_conventional.cc`, `protocol/commands.proto`,
`protocol/renderer_command.proto`, `base/const.h`, plus the `BUILD.bazel`
files for `bazel/win32`, `composer`, `renderer/win32`, `session`, and
`win32/tip`.

New files: `renderer/win32/marina_localized_string.{h,cc}`,
`renderer/win32/toolbar_accessible.{h,cc}`,
`renderer/win32/shortcuts_window.{h,cc}`,
`session/marina_shortcut_list_util.{h,cc}`,
`session/marina_shortcut_list_util_test.cc`.

**Verification:** the cross-platform pieces (both edited protos, `session`
and `session_test`, `renderer_server`, `mac:mozc_toolbar`, and
`marina_shortcut_list_util` with its new 9-test suite) were built and
tested on macOS. The Windows-only files could not be compiled in this
environment (no MSVC toolchain available to Bazel here) — they were instead
checked via Bazel `BUILD`-graph queries on every touched target, a
`somepath` check confirming the new shortcuts window links into
`marinamoji_renderer.exe`, a declaration-vs-definition cross-check across
all three window classes, a localization-key audit, a generated-proto
accessor check against the built `.pb.h`, and a dep-ordering check across
all `BUILD.bazel` lists touched. **This is not a substitute for an actual
Windows build**, which should happen before this is trusted.
