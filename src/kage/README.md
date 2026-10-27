# KAGE engine (C++ port)

> **Licence: GPL-3.0-or-later.** Everything in this directory is GPL-3.0 (see
> `LICENSE`), unlike the rest of marinaMoji, which is BSD-3-Clause. A program
> that links `//kage` is a combined work and is distributed under the GPL.

A port of the Mincho half of the KAGE engine, the program GlyphWiki uses to turn
stroke data into glyph outlines. The character composer uses it to draw a
structure that has no Unicode character as one new glyph instead of a row of
small ones.

The port follows [kurgm's `@kurgm/kage-engine`](https://github.com/kurgm/kage-engine)
0.7.0 (GPL-3.0), which is itself derived from the original KAGE engine by
[Kamichi Koichi](https://github.com/kamichikoichi/kage-engine) (GPL-3.0). It is
the same algorithm, not a clean-room design: stroke types, widths, the
neighbour-dependent adjustments of hooks, ends and joins, and the placement of
components with `99:` records. Left out: the Gothic style, curve (off-curve
point) output, EPS and SVG writers.

## Matching the original

`kage_engine_test` renders real GlyphWiki glyphs and composed layouts and
compares every polygon point with the JavaScript engine's output
(`testdata/kage_reference.tsv`). Regenerate the fixtures with
`testdata/make_reference.mjs` if the engine or the test glyphs change.

## Files

| File | |
|---|---|
| `kage_polygon.{h,cc}` | contours and the list of them |
| `kage_engine.{h,cc}` | KAGE data parsing, components, the Mincho font |
| `kage_data.{h,cc}` | reads the glyph table (`data/kage/kage_glyphs.tsv`) |
