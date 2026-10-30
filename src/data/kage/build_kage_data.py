#!/usr/bin/env python3
# Copyright 2026 marinaMoji contributors.
# SPDX-License-Identifier: BSD-3-Clause
"""Builds kage_glyphs.tsv and kage_layout.tsv for the character composer.

kage_glyphs.tsv: the GlyphWiki glyphs the composer draws.
kage_layout.tsv: how GlyphWiki itself lays components out, learned from the
glyphs that are made of two components, so a composed glyph looks like a
real one (木 narrowed and kept to the left in 機, 心 flattened at the bottom of
思, the 辶 wrapped round what it carries).

Input is either GlyphWiki's `dump_newest_only.txt` (lines "name | related | KAGE
data", fields space-padded) or a JSON object {name: KAGE data} such as the
Grognard editor's `kageCore.json`. Output is "name<TAB>KAGE data" per line.

It keeps every GlyphWiki name whose base code point is in the blocks below
(canonical `u<hex>` and variants such as `u6728-01`) plus every glyph they
place (recursively), and drops the "@N" render parameter from the references
(the engine draws the default form).

    python3 build_kage_data.py --glyphs kageCore.json --out kage_glyphs.tsv \\
        --layout-out kage_layout.tsv --ids ../character_composer/ids_table.tsv
"""

import argparse
import collections
import json
import re
import statistics
import sys

# The characters a user can type into the composer and expect to see drawn:
# radicals, kana, CJK Extension A and the Unified Ideographs. Rarer blocks are
# left to the composer's text fallback.
BLOCKS = [
    (0x2E80, 0x2FDF),  # CJK Radicals Supplement, Kangxi Radicals
    (0x3040, 0x30FF),  # Hiragana, Katakana
    (0x3400, 0x4DBF),  # Extension A
    (0x4E00, 0x9FFF),  # Unified Ideographs
    (0xF900, 0xFAFF),  # Compatibility Ideographs
]

CANONICAL = re.compile(r'^u([0-9a-f]{4,6})$')


def read_glyphs(path):
    with open(path, encoding='utf-8') as f:
        head = f.read(1)
        f.seek(0)
        if head == '{':
            return json.load(f)
        glyphs = {}
        for line in f:
            parts = [p.strip() for p in line.split('|', 2)]
            if len(parts) == 3 and parts[0]:
                glyphs[parts[0]] = parts[2]
        return glyphs


def references(data):
    for record in data.split('$'):
        fields = record.split(':')
        if fields[0] == '99' and len(fields) >= 8 and fields[7]:
            yield fields[7].split('@')[0]


def strip_parameters(data):
    records = []
    for record in data.split('$'):
        fields = record.split(':')
        if fields[0] == '99' and len(fields) >= 8:
            fields[7] = fields[7].split('@')[0]
        records.append(':'.join(fields))
    return '$'.join(records)


NAME = re.compile(r'^u[0-9a-f]{4,6}(-[0-9a-z]+)*$')
# Bare Unicode glyph or any of its GlyphWiki variants (u6728, u6728-01, …).
NAMED_IN_BLOCKS = re.compile(r'^u([0-9a-f]{4,6})(?:-.*)?$')
OPERATORS = {c: (2 if c not in '\u2ff2\u2ff3' else 3) for c in map(chr, range(0x2FF0, 0x2FFC))}


def box_center(b):
    return ((b[0] + b[2]) / 2, (b[1] + b[3]) / 2)


def read_ids(path):
    """{char: ids} for characters whose IDS is one operator over single characters."""
    table = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            ids, _, char = line.rstrip('\n').partition('\t')
            if not ids or ids[0] not in OPERATORS or len(char) != 1:
                continue
            parts = ids[1:]
            if len(parts) != OPERATORS[ids[0]] or any(c in OPERATORS for c in parts):
                continue
            table.setdefault(char, ids)
    return table


def two_part_records(glyphs, char):
    """The 99: records that make up |char| in GlyphWiki, following the
    one-line glyphs that merely point at a variant (u6a5f -> u6a5f-j)."""
    name = 'u%x' % ord(char)
    for _ in range(4):
        data = glyphs.get(name)
        if data is None:
            return None
        records = data.split('$')
        if not all(r.startswith('99:') for r in records):
            return None
        if len(records) == 1:
            name = records[0].split(':')[7].split('@')[0]
            continue
        return [r.split(':') for r in records]
    return None


def in_order(op, boxes):
    """Do the records come in the order the operator names its parts?"""
    cs = [box_center(b) for b in boxes]
    if op in '\u2ff0\u2ff2':
        return all(cs[i][0] < cs[i + 1][0] for i in range(len(cs) - 1))
    if op in '\u2ff1\u2ff3':
        return all(cs[i][1] < cs[i + 1][1] for i in range(len(cs) - 1))
    # Enclosures: the first part is the enclosing one and is the larger.
    area = lambda b: (b[2] - b[0]) * (b[3] - b[1])
    return area(boxes[0]) >= area(boxes[1])


def learn_layout(glyphs, ids):
    """How GlyphWiki places each component in each position of each layout.

    Learned from the characters whose IDS is one operator over single
    components: for each (operator, position, component) the variant glyph
    GlyphWiki most often uses (u6728-01 is 木 on the left) and the median box.
    The variant may be of another code point (the left form of 言 is u8a01-01).
    Returns {(op, position, char): (variant, box, count)}.
    """
    seen = collections.defaultdict(list)
    for char, expr in ids.items():
        op, parts = expr[0], expr[1:]
        records = two_part_records(glyphs, char)
        if records is None or len(records) != len(parts):
            continue
        boxes = []
        try:
            for f in records:
                boxes.append(tuple(int(float(x)) for x in f[3:7]))
        except ValueError:
            continue
        if not in_order(op, boxes):
            continue
        for i, (f, box) in enumerate(zip(records, boxes)):
            variant = f[7].split('@')[0]
            if NAME.match(variant):
                seen[(op, i, parts[i])].append((variant, box))

    layout = {}
    for key, entries in seen.items():
        variants = collections.Counter(v for v, _ in entries)
        variant, count = variants.most_common(1)[0]
        boxes = [bx for v, bx in entries if v == variant]
        median = tuple(int(statistics.median(bx[i] for bx in boxes)) for i in range(4))
        layout[key] = (variant, median, count)
    return layout


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--glyphs', required=True, help='GlyphWiki dump or {name: data} JSON')
    ap.add_argument('--out', required=True)
    ap.add_argument('--layout-out', help='write the learned layout table here')
    ap.add_argument('--ids', help='ids_table.tsv (IDS<TAB>char), needed with --layout-out')
    args = ap.parse_args()

    glyphs = read_glyphs(args.glyphs)
    # Seed with every GlyphWiki name whose base code point is in the composer
    # blocks, including positional variants (u6728-01, u5fc3-04, …). The older
    # canonical-only seed left most -01/-02 forms out unless some other glyph
    # referenced them, so left-side 木 often stayed full-width.
    wanted = set()
    for name in glyphs:
        m = NAMED_IN_BLOCKS.match(name)
        if m and any(lo <= int(m.group(1), 16) <= hi for lo, hi in BLOCKS):
            wanted.add(name)

    keep, stack = set(), list(wanted)
    while stack:
        name = stack.pop()
        if name in keep or name not in glyphs:
            continue
        keep.add(name)
        stack.extend(references(glyphs[name]))

    with open(args.out, 'w', encoding='utf-8', newline='\n') as out:
        out.write('# GlyphWiki KAGE glyphs for the marinaMoji character composer.\n')
        out.write('# Generated by build_kage_data.py; see README.md. name<TAB>KAGE data.\n')
        for name in sorted(keep):
            out.write(f'{name}\t{strip_parameters(glyphs[name])}\n')
    if args.layout_out:
        if not args.ids:
            ap.error('--layout-out needs --ids')
        layout = learn_layout(glyphs, read_ids(args.ids))
        with open(args.layout_out, 'w', encoding='utf-8', newline='\n') as out:
            out.write('# How GlyphWiki places components, learned from its glyphs that are one\n')
            out.write('# IDS operator over two or three components (build_kage_data.py).\n')
            out.write('# operator, position, component, variant glyph, left top right bottom, count.\n')
            for (op, pos, char), (variant, box, count) in sorted(layout.items()):
                # Only variants this table also holds can be drawn.
                if variant in keep:
                    out.write('\t'.join([op, str(pos), char, variant,
                                         ' '.join(map(str, box)), str(count)]) + '\n')
        print(f'{len(layout)} layout entries -> {args.layout_out}', file=sys.stderr)
    print(f'{len(keep)} glyphs ({len(wanted)} requested) -> {args.out}', file=sys.stderr)


if __name__ == '__main__':
    main()
