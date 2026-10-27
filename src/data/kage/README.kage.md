# KAGE glyph data

`kage_glyphs.tsv` is the stroke data the character composer uses to draw a
structure that has no Unicode character (for example ⿰木巻) as one new glyph.
Each line is `name<TAB>KAGE data`: a GlyphWiki glyph name such as `u6728` and its
strokes in the [KAGE format](https://glyphwiki.org/wiki/GlyphWiki:KAGEデータ仕様).
Glyphs place other glyphs with `99:` records, so the file is closed under that
relation.

## Licence

The data is from [GlyphWiki](https://glyphwiki.org/)'s public dump, which its
authors release as free software: anyone may use, copy and distribute it, with
or without modification, commercially or not, without warranty. The text is in
`LICENSE.GlyphWiki`. No attribution is required; this notice records where it
comes from.

The code that reads it, `src/kage`, is a different matter: it is GPL-3.0-or-later.

## What is in it

The canonical Unicode glyphs (`u<hex>`) of the Kangxi and CJK radicals, hiragana,
katakana, CJK Extension A, the Unified Ideographs and the compatibility
ideographs, plus every glyph those place, recursively. A leaf outside these
blocks has no strokes here, and the composer draws that structure from the
components as text instead. The `@N` render parameter on a reference is dropped:
the default form is drawn.

## Rebuild

Needs a GlyphWiki dump (`dump_newest_only.txt` from
<https://glyphwiki.org/wiki/GlyphWiki:Dump>) or a `{name: data}` JSON of it.

```bash
python3 build_kage_data.py --glyphs dump_newest_only.txt --out kage_glyphs.tsv
```

The checked-in file was built from the 2026-09-23 dump through the Grognard
editor's `kageCore.json`, which is the same data in JSON.
