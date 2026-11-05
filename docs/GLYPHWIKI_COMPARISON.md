# GlyphWiki comparison: licence, data policy, and harness

Tracking issue: [#154](https://github.com/marinaMoji/marinaMoji/issues/154).
Ordered work: [#150](https://github.com/marinaMoji/marinaMoji/issues/150)
(this policy) → [#151](https://github.com/marinaMoji/marinaMoji/issues/151)
(render-and-diff harness) → [#152](https://github.com/marinaMoji/marinaMoji/issues/152)
(refine layouts from the results).

## Licence terms (recorded)

### GlyphWiki glyph data

Source: [GlyphWiki](https://glyphwiki.org/) public dump. The exact text shipped
with the data is `src/data/kage/LICENSE.GlyphWiki`.

In short:

- The data files are free software.
- Anyone may use, copy and distribute them, with or without modification,
  commercially or not.
- There is no warranty.
- **No attribution is required.** The README in `src/data/kage/` records where
  the data comes from as a courtesy, not as a licence condition.

What marinaMoji already ships under that licence:

- `src/data/kage/kage_glyphs.tsv` — stroke data
- `src/data/kage/kage_layout.tsv` — layout table learned offline from GlyphWiki
- `src/data/kage/kage_subtrees.tsv` — nested IDS → ready-made GlyphWiki glyphs

### KAGE engine

`src/kage/` is a C++ port of the Mincho half of the KAGE engine
([kurgm/kage-engine](https://github.com/kurgm/kage-engine) 0.7.0, itself from
Kamichi Koichi's original). Both upstream and the port are
**GPL-3.0-or-later**. See `src/kage/LICENSE` and `src/kage/README.md`.

Anything that links `//kage` is distributed under the GPL. That already
applies to `mozc_tool` and the standalone character composer; the comparison
harness links the same libraries and is likewise GPL as a combined work. The
IME itself does not link `//kage`.

## Data policy for the comparison harness

Decisions for #150 / #151:

1. **No new GlyphWiki data is added to the repo for the harness.** The
   comparison uses the GlyphWiki stroke and layout tables already shipped in
   `src/data/kage/`. A full GlyphWiki dump may be fetched locally to rebuild
   those tables (`build_kage_data.py`); the dump itself is never committed.
2. **The comparison corpus is not a second data ship.** Characters under test
   are those that already have an IDS in `ids_table.tsv` and a registered
   `uXXXX` glyph in the bundled `kage_glyphs.tsv`. Scores and a reviewed
   baseline (numbers and character lists) may be committed; glyph dumps may
   not.
3. **Nothing is copied from GlyphWiki into the repo beyond what the existing
   rebuild pipeline already documents.** The harness does not fetch the
   GlyphWiki API and does not require network access in CI.

## Privacy

The harness is **developer / test-time only**. It is not packaged into user
installs, does not run when people type, and contacts no server.

The optional runtime “Prefer GlyphWiki drawings” lookup is already described
in [`PRIVACY.md`](PRIVACY.md). This harness needs **no privacy-policy entry**:
it sends nothing from users’ machines because it never runs there.

## What the harness compares

For each character *C* with a recorded IDS *I* and a registered GlyphWiki
glyph `uXXXX`:

1. **Composer drawing** — render *I* with the composer’s layout table and the
   KAGE engine (`ComposerOutlines`).
2. **GlyphWiki drawing** — render the registered glyph for *C* with the same
   engine, size and stroke style (`NamedGlyphOutlines`).
3. Rasterise both outlines on the same grid and score overlap (IoU) plus how
   many contours differ.

A ranked report (worst first), grouped by IDS operator and by component, is
printed by one command. A reviewed baseline with a tolerance fails the test if
a score regresses. Style differences (Mincho registered glyph versus our
composition) are accepted once when the baseline is written; later drops are
regressions.

### Commands

```bash
# Ranked report (first N IDS-layout characters from ids_table.tsv):
bazel run //gui/character_composer:glyphwiki_compare_main -- --limit=200

# Regression test (reviewed baseline + IoU unit checks):
bazel test //gui/character_composer:glyphwiki_compare_test
```

Both use only the bundled `kage_glyphs.tsv` / `kage_layout.tsv` /
`kage_subtrees.tsv` and `ids_table.tsv`. No network.

## What low scores mean (triage)

About two thirds of registered GlyphWiki glyphs in the first few hundred
unified characters are pure `99:` compositions of components; the rest are
hand-tuned stroke data. Comparing a composition to stroke data always mixes
**layout** error with **style** difference — those stay low even when the
composer is doing the right thing (林, 炎, 三, …).

When GlyphWiki *is* a composition, remaining gaps are usually:

1. **Box overhang.** Positional forms (`u6728-02`, …) already encode
   narrowing; GlyphWiki then places them in wide boxes, often starting left of
   0. Clean half-boxes double-squeeze them. The composer now uses median
   overlap defaults, widens boxes for positional forms, and pulls the right
   box left in proportion to how narrow the left is
   (`ExpandBoxForPositionalForm`, `HarmonizeHorizontalOverlap`).
2. **Sibling context.** `kage_layout.tsv` may carry a seventh column — the
   other leaf of a two-part IDS — so 木 next to 目 can stretch to x=208 while
   the global 木-on-the-left median stays tighter. Rebuild with
   `build_kage_data.py --layout-only`.
3. **Chosen region variants.** PreferPositionalVariant only upgrades a *bare*
   `uXXXX`; a table (or context) row that already names `u76ee-j` is left
   alone, not forced to `-02`.
4. **Nested IDS.** When a slot is itself an IDS (侃 = ⿰亻⿱口𫶧), synthesizing
   the inner layout often diverges from GlyphWiki's ready-made compound
   (`u2ff1-…` or a `cdp-…` piece). `kage_subtrees.tsv` maps those fragments to
   the glyph GlyphWiki actually placed; the composer also tries the systematic
   `u2ffN-…` name before falling back to `__nestedN`.

The ranked report labels each row `layout` or `style` and prints operator
means for layout-only rows, so triage can ignore stroke-drawn GlyphWiki
glyphs.

## Acknowledgements

No change to the README acknowledgements is required: GlyphWiki’s data licence
asks for no attribution, and the existing notes under `src/data/kage/` and the
main README already name GlyphWiki and the KAGE engine. This document is the
record of the comparison-specific decisions.
