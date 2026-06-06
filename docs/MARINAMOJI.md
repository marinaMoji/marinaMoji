# marinaMoji install branding

The [marinaMoji](https://github.com/marinaMoji/marinaMoji) fork installs **next to** stock Mozc using distinct paths and IBus component IDs (see `src/config.bzl`).

## Identity

- **Product name:** marinaMoji (IBus panel, menus, About dialog, System Settings).
- **macOS bundle:** `marinaMoji.app` with `marinaMojiConverter.app` / `marinaMojiRenderer.app` inside.
- **IBus component:** `com.marinamoji.IBus.Mozc` (separate from `com.google.IBus.Mozc`).
- **Component file:** `marinamoji.xml` (under `/usr/share/ibus/component/`).
- **Engine executable:** `ibus-engine-marinamoji` (under `/usr/lib/ibus-marinamoji/`).
- **Server directory:** `/usr/lib/marinamoji/` (`mozc_server`, `mozc_tool`, `mozc_renderer`).
- **Icons / data:** `/usr/share/ibus-marinamoji/`, `/usr/share/icons/marinamoji/`.

## User config (Linux)

- Profile: `~/.config/marinamoji/` (or `~/.marinamoji/`).
- IBus toolbar: `~/.config/ibus/marinamoji/`.

**Legacy paths** (`marinamozc`, `~/.config/marinamozc/`, `~/.config/ibus/marinamozc/`) are still read if the new directory does not exist yet.

## Sync sidecar config

- macOS: `~/Library/Application Support/marinaMoji/sync.conf`
- Linux: `~/.config/marinamoji/sync.conf`

For setup and behavior details, see:

- `docs/HOW_SYNC_WORKS.md` (user guide)
- `docs/SYNC_PLAN.md` (implementation reference)

## Configuration

- **`src/config.bzl`** — `BRANDING = "marinaMoji"` and install paths above.
- **`src/unix/ibus/gen_mozc_xml.py`** — `--branding=marinaMoji` generates the component XML.

After building and installing, add **marinaMoji** in IBus; it appears alongside stock Mozc.
