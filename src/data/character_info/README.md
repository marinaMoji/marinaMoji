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
(Unicode 18.0.0, 2026-07-31), `--cjkvi-variants` and `--glyphwiki-itaiji` (below).

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
| `variants` | related characters, space-separated: Unihan's variant fields, then the cjkvi-variants pairs and GlyphWiki itaiji links (both directions) |
| `ja_kana` | `kJapanese`: on in katakana then kun in hiragana (covers far more characters than `ja_on`/`ja_kun`) |
| `old_form` | `kJapaneseOldVariant`: the 旧字体 of this character (set on a 新字体) |
| `new_form` | `kJapaneseNewVariant`: the 新字体 of this character (set on a 旧字体) |

| `region_forms` | which regions draw this character alike in Source Han, e.g. `CN JP\|HK TW\|KR` (groups split by `\|`, regions CN HK TW JP KR); empty when all regions that have the character agree |

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

## Source Han regional forms

`region_forms` is derived from the ten Adobe CMaps
`UniSourceHan{Sans,Serif}{CN,HK,TW,JP,KR}-UTF32-H`
(<https://github.com/adobe-fonts/source-han-sans>, `-serif`; Copyright
1990-2025 Adobe, BSD-3-Clause in the file header; version 2.005 when fetched
2026-11-13). Source Han keeps one glyph pool and a code point to CID map per
region, so two regions draw a character the same way exactly when their CIDs
match. As in sxzz/hanji (the idea; no code is copied), regions count as alike
when either the Sans or the Serif map says so, which keeps design details of a
single typeface out. Only characters that already have a row are annotated.
Use `--source-han-cmaps DIR`. The maps are not shipped.

It reports how Source Han draws a character, not what each region's standard
says. Hong Kong coverage in Source Han is incomplete, so differences that only
involve Hong Kong may be under-reported.

The checked-in file got the column by appending it to the previous
`char_info.tsv` (the other 12 columns are unchanged); a rebuild from scratch
with `--source-han-cmaps` gives the same column.

## cjkvi-variants

`variants` also draws on one table from
<https://github.com/cjkvi/cjkvi-variants>, `cjkvi-variants.txt` (commit
`e4f1da248c9737a243f9930b5dc497cef5d5ae16`, SHA-256
`e366fb832c76a7cc05371b08cac0d12569c180b0a44c302fcb651918421581da`; header
"Copyright (c) 2014 CJKVI Database"). Each `A,relation,B` pair is added in both
directions, only to characters that already have a row. It adds about 6,000
links, for example 鐧 → 鐗, that Unihan does not give.

**Licence.** The repository has no licence file, but the kanji-database project
that owns the data states on <https://kanji-database.sourceforge.net/> that its
異体字データ is distributed under the MIT licence ("MITライセンスによる配布").
The same site's English summary says "basically GPL", so the two statements
disagree; we rely on the specific Japanese one. The MIT notice is recorded in
the copyright files. If the maintainers ever contradict this, rebuild without
`--cjkvi-variants`.

**Tables from that repository we do not use.**

- `hydzd-variants.txt`: taken from the 漢語大字典, a printed dictionary. The
  project's blanket MIT declaration cannot cover rights it does not hold. It was
  used until 2026-11-13 and removed.
- `twedu-variants.txt`: the Taiwanese Ministry of Education's 異體字字典, whose
  data may be supplied only on application and only for non-commercial use.
  The maintainers of cjkvi-variants cannot license MOE's rights.
- The remaining tables were not evaluated.

To rebuild with the table, fetch
`https://raw.githubusercontent.com/cjkvi/cjkvi-variants/<commit>/cjkvi-variants.txt`
into a folder and add `--cjkvi-variants /that/folder`.

## GlyphWiki itaiji

`--glyphwiki-itaiji dump_newest_only.txt` (the GlyphWiki dump, see
`../kage/README.kage.md`) adds a link, both ways, for each glyph named
`uXXXX-itaiji-NNN` whose data is only another character `uYYYY`: GlyphWiki's
editors made it as an 異体字 of XXXX. Only pairs where both characters lie in
the composer blocks are kept; the rare extension characters it also reaches are
mostly components, not variants. The links are unreviewed, so some are loose
(for example 来 ↔ 耒). GlyphWiki's licence is in `../kage/LICENSE.GlyphWiki`.
The checked-in file got 249 links from the 2026-11-12 dump (20 more were already in cjkvi-variants.txt).

## Drawing data (GlyphWiki)

To improve KAGE drawings, rebuild `../kage/` from a GlyphWiki dump (see
`../kage/README.kage.md`). That pipeline is separate on purpose: different
licence, different consumers, and much larger files.
