# Character info (display metadata)

Small per-character facts the character composer can show beside a result:
code point, stroke count, pinyin, Japanese on/kun, Unihan variants, radical–
stroke, and a short English gloss. **Not** for drawing: strokes and layout
variants stay in `../kage/` (GlyphWiki). **Not** for IDS lookup: that stays in
`../character_composer/` (CHISE / cjkvi, GPL-2).

## Licence

Built from the [Unicode Character Database](https://www.unicode.org/ucd/)
Unihan files. Redistribution of Unihan data must follow the
[Unicode Terms of Use](https://www.unicode.org/copyright.html). Keep
`UNICODE_NOTICE.txt` and this README with any generated `char_info.tsv`. The
build script is BSD-3-Clause like the rest of marinaMoji tooling.

`char_info.tsv` in this tree was built with `--composer-blocks` from Unihan
(Unicode 18.0.0, 2026-07-31) and `--cjkvi-variants` (below).

## Output

`char_info.tsv` — one row per character, tab-separated:

| column | meaning |
|--------|---------|
| `char` | the character (one Unicode scalar) |
| `cp` | `U+XXXX` |
| `strokes` | `kTotalStrokes` (first value if several) |
| `pinyin` | `kMandarin` (space-separated readings) |
| `ja_on` | `kJapaneseOn` |
| `ja_kun` | `kJapaneseKun` |
| `rs` | `kRSUnicode` (e.g. `149'.9`) |
| `definition` | `kDefinition` (may be empty) |
| `variants` | related characters, space-separated: Unihan's variant fields, then the cjkvi-variants pairs (both directions) |
| `ja_kana` | `kJapanese`: on in katakana then kun in hiragana (covers far more characters than `ja_on`/`ja_kun`) |
| `old_form` | `kJapaneseOldVariant`: the 旧字体 of this character (set on a 新字体) |
| `new_form` | `kJapaneseNewVariant`: the 新字体 of this character (set on a 旧字体) |

Empty fields are left blank. The file is generated; it is not hand-edited.

## Rebuild

1. Download and unzip Unihan (e.g. from
   <https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip>).
2. Point the script at the folder that contains `Unihan_Readings.txt`,
   `Unihan_DictionaryLikeData.txt`, `Unihan_Variants.txt`,
   `Unihan_RadicalStrokeCounts.txt`, `Unihan_IRGSources.txt` (`kJapanese` and the
old/new-form fields need Unicode 18.0 or later):

```bash
python3 build_char_info.py --unihan /path/to/Unihan --out char_info.tsv
```

Optional: limit to the same blocks the composer draws by default:

```bash
python3 build_char_info.py --unihan /path/to/Unihan --out char_info.tsv --composer-blocks
```

## cjkvi-variants

`variants` also draws on two tables from
<https://github.com/cjkvi/cjkvi-variants>: `cjkvi-variants.txt` (header:
"Copyright (c) 2014 CJKVI Database") and `hydzd-variants.txt` (variant forms
from the 漢語大字典). Each `A,relation,B` pair is added in both directions, only
to characters that already have a row. They add 鐧 → 鐗 and 𨰓, which Unihan
does not link.

**Licence caveat.** The repository has no licence file. The tables are
collected from other works, so they are not covered by Unicode's terms. They are
used here on the maintainer's decision. Do not take `twedu-variants.txt` from
the same repository: it comes from the Taiwanese Ministry of Education's
異體字字典, whose data may be supplied only on application. If the licence
becomes a problem, rebuild without `--cjkvi-variants`.

To rebuild with them, put the two files in one folder (fetched from
`https://raw.githubusercontent.com/cjkvi/cjkvi-variants/master/<file>`) and add
`--cjkvi-variants /that/folder`.

## Drawing data (GlyphWiki)

To improve KAGE drawings, rebuild `../kage/` from a GlyphWiki dump (see
`../kage/README.kage.md`). That pipeline is separate on purpose: different
licence, different consumers, and much larger files.
