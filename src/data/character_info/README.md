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
(UCD latest at build time).

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
| `variants` | related characters from Unihan variant fields, space-separated |

Empty fields are left blank. The file is generated; it is not hand-edited.

## Rebuild

1. Download and unzip Unihan (e.g. from
   <https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip>).
2. Point the script at the folder that contains `Unihan_Readings.txt`,
   `Unihan_DictionaryLikeData.txt`, `Unihan_Variants.txt`,
   `Unihan_RadicalStrokeCounts.txt`, `Unihan_IRGSources.txt`:

```bash
python3 build_char_info.py --unihan /path/to/Unihan --out char_info.tsv
```

Optional: limit to the same blocks the composer draws by default:

```bash
python3 build_char_info.py --unihan /path/to/Unihan --out char_info.tsv --composer-blocks
```

## Drawing data (GlyphWiki)

To improve KAGE drawings, rebuild `../kage/` from a GlyphWiki dump (see
`../kage/README.kage.md`). That pipeline is separate on purpose: different
licence, different consumers, and much larger files.
