#!/usr/bin/env python3
"""Builds the character composer's data files from the upstream sources.

A Python port of two scripts from the Grognard editor
(scripts/build-ids-unicode-index.mjs and scripts/build-radical-forms.mjs), so
the data can be rebuilt from upstream without Node.

Outputs (both GPL-2.0-or-later, derived from CHISE IDS; see README.md):
  ids_table.tsv      "<IDS>\\t<character>" per line.
  radical_forms.tsv  one radical family per line, tab-separated, base first.

Inputs:
  --ids FILE...        IDS files in CHISE layout, in priority order: cjkvi-ids'
                       ids.txt first, then IDS-UCS-Ext-G.txt, -H.txt, -I.txt
                       (cjkvi-ids stops at U+2FA1D). Lines are
                       U+XXXX<TAB>char<TAB>IDS[<TAB>altIDS...]. Skipped:
                       comment lines (# or ;), columns starting with "@"
                       (annotations, not alternatives) and any IDS containing
                       an "&CDP-xxxx;"-style reference to a component with no
                       Unicode character. A trailing "[GTKV]"-style regional tag
                       is stripped. The first-listed form of a duplicate IDS wins.
  --kage-core FILE     GlyphWiki's kageCore.json (glyph name -> KAGE data). Only
                       the set of glyph names is used: a radical form GlyphWiki
                       cannot draw is dropped from its family.
  --families FILE      radical_families.json (in this directory).

Not ported: the composer in Grognard also mines GlyphWiki for "this radical
takes this form in this position" rules to choose the glyph it draws. An input
method only needs the families, so that step is left out.

Usage:
  build_ids_data.py --ids ids.txt IDS-UCS-Ext-G.txt IDS-UCS-Ext-H.txt \\
      IDS-UCS-Ext-I.txt --kage-core kageCore.json \\
      --families radical_families.json --out-dir src/data/character_composer
"""

import argparse
import json
import os
import re
import sys

_REGIONAL_TAG = re.compile(r'\[[A-Z,]+\]"?$')
_ENTITY_REFERENCE = re.compile(r'&[^;]*;')


def read_ids_entries(paths):
  """Returns [(ids, char)] in source order, as the Grognard script does."""
  lines = []
  for path in paths:
    with open(path, encoding='utf-8', newline='') as f:
      text = f.read()
    lines.extend(line for line in text.split('\n')
                 if line and not line.startswith('#') and not line.startswith(';'))

  entries = []
  seen = set()
  skipped_unencoded = 0
  for line in lines:
    cols = line.split('\t')
    if len(cols) < 3:
      continue
    char = cols[1]
    for col in cols[2:]:
      if col.startswith('@'):
        continue
      ids = _REGIONAL_TAG.sub('', col)
      if _ENTITY_REFERENCE.search(ids):
        skipped_unencoded += 1
        continue
      if ids in seen:
        continue
      seen.add(ids)
      entries.append((ids, char))
  print(f'Source lines: {len(lines)}')
  print(f'Skipped (component with no Unicode character): {skipped_unencoded}')
  print(f'Unique IDS keys: {len(entries)}')
  return entries


def glyph_name(char):
  return 'u%04x' % ord(char[0])


def read_radical_families(families_path, kage_names):
  """Returns [[forms...]] per family: bases, positional forms, then alternates.

  Members GlyphWiki has no glyph for are dropped, and a form group left empty
  disappears, exactly as build-radical-forms.mjs does before writing its
  `families` field.
  """
  with open(families_path, encoding='utf-8') as f:
    families = json.load(f)
  dropped = []

  def keep(chars):
    kept = []
    for char in chars:
      if glyph_name(char) in kage_names:
        kept.append(char)
      else:
        dropped.append(char)
    return kept

  result = []
  for family in families:
    bases = keep(family['bases'])
    alt = keep(family.get('alt', []))
    groups = [g for g in (keep(group) for group in family['forms']) if g]
    result.append(bases + [c for group in groups for c in group] + alt)
  if dropped:
    print('Family members with no GlyphWiki glyph, dropped: ' + ' '.join(dropped))
  return result


def main():
  parser = argparse.ArgumentParser(description=__doc__,
                                   formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('--ids', nargs='+', required=True)
  parser.add_argument('--kage-core', required=True)
  parser.add_argument('--families', required=True)
  parser.add_argument('--out-dir', required=True)
  args = parser.parse_args()

  entries = read_ids_entries(args.ids)
  for ids, char in entries:
    if '\t' in ids + char or '\n' in ids + char or '\r' in ids + char:
      sys.exit(f'unusable entry: {ids!r} {char!r}')

  with open(args.kage_core, encoding='utf-8') as f:
    kage_names = set(json.load(f))
  families = read_radical_families(args.families, kage_names)

  os.makedirs(args.out_dir, exist_ok=True)
  with open(os.path.join(args.out_dir, 'ids_table.tsv'), 'w',
            encoding='utf-8', newline='\n') as out:
    for ids, char in entries:
      out.write(f'{ids}\t{char}\n')
  with open(os.path.join(args.out_dir, 'radical_forms.tsv'), 'w',
            encoding='utf-8', newline='\n') as out:
    for family in families:
      out.write('\t'.join(family) + '\n')
  print(f'Wrote {len(entries)} IDS entries and {len(families)} radical families')


if __name__ == '__main__':
  main()
