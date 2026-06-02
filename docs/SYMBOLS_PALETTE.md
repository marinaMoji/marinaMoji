# Symbols Palette (macOS and GTK toolbars)

This document describes the toolbar Symbols Palette in marinaMoji (macOS and Linux IBus).

## Overview

- A toolbar **Symbols** button (icons: `toolbar_symbols_light.svg` / `toolbar_symbols_dark.svg`) opens a tabbed palette.
- Tabs:
  - `Odoriji`
  - `Kaeriten`
  - `Symbols`
  - `User`
- Palette state is remembered per device:
  - last-open tab
  - `Pin palette` checkbox state

## Behavior

- Clicking a symbol inserts it immediately.
- On the **Odoriji** tab, the choice also becomes your default odoriji (same as the main Odoriji palette / Ctrl+Shift+1).
- Default behavior is **insert and close**.
- If `Pin palette` is checked, the palette stays open after insertion.
- `Odoriji` and `Kaeriten` tabs include a note that keyboard shortcuts exist in the active keymap.

## Tab contents

- `Odoriji`: common iteration marks (e.g. `々`, `ゝ`, `ゞ`, `ヽ`, `ヾ`, `〻`, `〱`, `〲`).
- `Kaeriten`: loaded from `kaeriten.tsv` (with fallback defaults).
- `Symbols`: general text/editorial symbols (brackets, marks, etc.), excluding odoriji duplicates.
- `User`: short strings from `user_symbols.txt`, editable in Preferences (macOS and Linux).

## Preferences integration

In the config dialog (`Preferences`), under **Dictionary → User dictionary**, use:

- **Edit user symbols...**

A multiline editor opens (one symbol per line). Entries are saved to a text file, not in the config protobuf.

## Persistence model

- Per-device UI state (pin + last tab) is stored in `toolbar.conf`:
  - macOS: app support directory (same folder as other toolbar prefs)
  - Linux: `~/.config/ibus/marinamoji/toolbar.conf`
- User symbol lines:
  - macOS: app support `user_symbols.txt`
  - Linux: `~/.config/ibus/marinamoji/user_symbols.txt`

## Key files

- `src/mac/mozc_toolbar.mm` (macOS toolbar button, palette UI, local prefs)
- `src/unix/ibus/mozc_toolbar.cc` (GTK toolbar button, palette UI, local prefs)
- `src/unix/ibus/mozc_engine.{h,cc}` (`CommitToolbarText`, `SendToolbarSessionCommand` with candidate id)
- `src/gui/config_dialog/config_dialog.ui` (Edit user symbols button)
- `src/gui/config_dialog/config_dialog.cc` (symbol editor, macOS and Linux)
- `src/unix/ibus/path_util.{h,cc}` (`GetUserDataDirectory()` for Linux paths)
- `src/unix/ibus/toolbar_icons/toolbar_symbols_light.svg`
- `src/unix/ibus/toolbar_icons/toolbar_symbols_dark.svg`

