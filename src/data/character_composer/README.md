# Character composer data (GPL-2.0-or-later)

**This directory is not BSD-licensed.** Unlike the rest of marinaMoji, the data
files here are derived from the CHISE IDS project and are licensed under the
**GNU General Public License, version 2 or (at your option) any later version**.
The full text is in `LICENSE.GPL-2.0`.

| File | Contents |
|------|----------|
| `ids_table.tsv` | 101,298 `IDS<TAB>character` pairs: an ideographic description sequence and the encoded character it describes (`⿰言某` → 謀). |
| `radical_forms.tsv` | 25 radical families (人 亻 ⺅ 𠆢, 言 訁 …), tab-separated, base form first. |
| `cjkvi-ids-README.md` | Upstream README (conventions and licence statement). |

## Provenance

Built by `build_ids_data.py`, a Python port of two scripts from the Grognard
editor (`build-ids-unicode-index.mjs`, `build-radical-forms.mjs`), from:

- `ids.txt` in <https://github.com/cjkvi/cjkvi-ids>, commit
  `86b4d16159f0079437870408f0ca186e529015db` (2019-02-07, the last one). It is
  derived from the CHISE project (<http://www.chise.org/>); its README says it
  "follows CHISE's terms".
- `IDS-UCS-Ext-G.txt`, `-H.txt` and `-I.txt` in <https://github.com/chise/ids>,
  commit `ac35413635608512f5e10187812cfab3b6ea47c1` (2026-10-06), for the
  Extension G–I blocks that cjkvi-ids does not cover. Its README says "GPL
  version 2, or any later version".
- GlyphWiki's `kageCore.json` (from the Grognard project, built from the
  GlyphWiki dump, which is free to use with no attribution), used only to drop a
  radical form from `radical_forms.tsv` when GlyphWiki cannot draw it.
- `radical_families.json` (here), the list of radical forms, written by hand in
  the Grognard project.

Rebuild:

```bash
python3 build_ids_data.py \
    --ids cjkvi-ids/ids.txt chise-ids/IDS-UCS-Ext-G.txt \
          chise-ids/IDS-UCS-Ext-H.txt chise-ids/IDS-UCS-Ext-I.txt \
    --kage-core kageCore.json --families radical_families.json --out-dir .
```

Rules of the conversion: comment lines and `@` annotation columns are skipped; a
trailing `[GTKV]`-style regional tag is stripped; an IDS that refers to a
component with no Unicode character (`&CDP-xxxx;`) is skipped; the first-listed
form of a duplicate IDS wins. A few Extension G/H lines are kept as upstream
has them although they are not clean IDS (digits, private-use characters,
`a / b` alternatives); the composer never matches or offers those.

Not ported from Grognard: its mining of GlyphWiki for "this radical takes this
form in this position" rules, which choose the glyph it draws. An input method
only needs the families.

## How marinaMoji uses it

The files are installed as plain data beside `mozc_server`
(`<server dir>/character_composer/`) and read at run time by the character
composer in `mozc_tool`. They are **not** compiled or linked into any marinaMoji
binary. The composer code (`src/composer_ids`, `src/gui/character_composer`)
remains BSD-3-Clause. If the files are missing the composer still builds IDS
strings; only the character lookup is unavailable.

Anyone who redistributes marinaMoji must keep this directory, its licence text
and this notice together with the data files.
