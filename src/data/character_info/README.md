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
(Unicode 18.0.0, 2026-07-31) and `--glyphwiki-itaiji` (below).

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
| `variants` | related characters, space-separated: Unihan's variant fields, then GlyphWiki itaiji links (both directions) |
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

## cjkvi-variants (not shipped)

`variants` used to include pairs from two tables in
<https://github.com/cjkvi/cjkvi-variants>, `cjkvi-variants.txt` and
`hydzd-variants.txt`. They were removed on 2026-11-13. The repository has no
licence file, `cjkvi-variants.txt` carries a "Copyright (c) 2014 CJKVI
Database" header with no grant, and `hydzd-variants.txt` is taken from the
漢語大字典, a printed dictionary. The maintainers did not answer a request for
a licence, so we have no permission to redistribute either table.

`build_char_info.py` still accepts `--cjkvi-variants DIR` so a developer can
build a **local** file for their own use. Never commit or ship a
`char_info.tsv` built that way.

Do not take `twedu-variants.txt` from that repository either: it comes from the
Taiwanese Ministry of Education's 異體字字典, whose data may be supplied only on
application and only for non-commercial use.

## GlyphWiki itaiji

`--glyphwiki-itaiji dump_newest_only.txt` (the GlyphWiki dump, see
`../kage/README.kage.md`) adds a link, both ways, for each glyph named
`uXXXX-itaiji-NNN` whose data is only another character `uYYYY`: GlyphWiki's
editors made it as an 異体字 of XXXX. Only pairs where both characters lie in
the composer blocks are kept; the rare extension characters it also reaches are
mostly components, not variants. The links are unreviewed, so some are loose
(for example 来 ↔ 耒). GlyphWiki's licence is in `../kage/LICENSE.GlyphWiki`.
The checked-in file got 269 links from the 2026-11-12 dump.

## Drawing data (GlyphWiki)

To improve KAGE drawings, rebuild `../kage/` from a GlyphWiki dump (see
`../kage/README.kage.md`). That pipeline is separate on purpose: different
licence, different consumers, and much larger files.
