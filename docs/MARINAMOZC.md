# marinaMoji install branding (technical)

The **product** is [marinaMoji](https://github.com/marinaMoji/marinaMoji). **Install paths and bundle IDs** still use the `marinaMozc` / `marinamozc` prefix so the fork can sit **next to** stock Mozc without overwriting it (see `src/config.bzl`).

## Identity

- **Product name (user-facing):** marinaMoji (IBus panel, menus, About dialog).
- **Install / server prefix:** marinaMozc (paths below).
- **IBus component:** `com.marinamozc.IBus.Mozc` (separate from `com.google.IBus.Mozc`).
- **Component file:** `marinamozc.xml` (installed under `/usr/share/ibus/component/`).
- **Engine executable:** `ibus-engine-marinamozc` (under `/usr/lib/ibus-marinamozc/`).
- **Server directory:** `/usr/lib/marinamozc/` (mozc_server, mozc_tool, mozc_renderer).
- **Icons / data:** `/usr/share/ibus-marinamozc/`, `/usr/share/icons/marinamozc/`.

## Configuration

- **`src/config.bzl`** – `BRANDING = "marinaMozc"` and the paths above. Change these to use a different prefix (e.g. `/usr/local`) if needed.
- **`src/unix/ibus/gen_mozc_xml.py`** – When `--branding=marinaMozc`, generates the marinaMozc component name and textdomain.

After building and installing, add **marinaMoji** as an input method in IBus; it will appear alongside Mozc in the list.
