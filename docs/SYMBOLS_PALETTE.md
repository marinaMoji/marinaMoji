# Symbols Palette (macOS Toolbar)

This document describes the macOS toolbar Symbols Palette added in marinaMozc.

## Overview

- A new toolbar button (icon: `symbols_light.svg` / `symbols_dark.svg`) opens a tabbed palette.
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
- Default behavior is **insert and close**.
- If `Pin palette` is checked, the palette stays open after insertion.
- `Odoriji` and `Kaeriten` tabs include a note that keyboard shortcuts exist in the active keymap.

## Tab contents

- `Odoriji`: common iteration marks (e.g. `々`, `ゝ`, `ゞ`, `ヽ`, `ヾ`, `〻`, `〱`, `〲`).
- `Kaeriten`: loaded from `kaeriten.tsv` (with fallback defaults).
- `Symbols`: general text/editorial symbols (brackets, marks, etc.), excluding odoriji duplicates.
- `User`: short strings from config (`user_symbol_strings`), editable in Preferences.

## Preferences integration

In the config dialog (`Preferences`), under **Dictionary -> User dictionary**, use:

- `Edit user symbols...`

Entries are one per line and saved in:

- `Config.user_symbol_strings` (repeated string)

## Persistence model

- Per-device UI state (pin + last tab) is stored in `toolbar.conf` in the app support directory.
- User symbol content is stored in config protobuf and loaded via normal config flow.

## Key files

- `src/mac/mozc_toolbar.mm` (toolbar button, palette UI, local prefs)
- `src/gui/config_dialog/config_dialog.ui` (Edit user symbols button)
- `src/gui/config_dialog/config_dialog.cc` (symbol editor + config mapping)
- `src/protocol/config.proto` (`repeated string user_symbol_strings`)
- `src/unix/ibus/toolbar_icons/symbols_light.svg`
- `src/unix/ibus/toolbar_icons/symbols_dark.svg`

