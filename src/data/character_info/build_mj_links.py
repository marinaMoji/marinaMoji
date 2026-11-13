#!/usr/bin/env python3
# Copyright 2026 marinaMoji contributors.
# SPDX-License-Identifier: BSD-3-Clause
"""Builds char_info_mj.tsv: variant -> standard-form links from the MJ data.

The input is IPA's MJ縮退マップ (JSON) and MJ文字情報一覧表 (xlsx), both
CC BY-SA 2.1 Japan. The output is a derived work and keeps that licence; it
must stay a separate file and never be merged into char_info.tsv (BSD/Unicode
terms). See README.md.

    python3 build_mj_links.py --mj-list mji.00602.xlsx \\
        --mj-map MJShrinkMap.1.2.0.json --char-info char_info.tsv \\
        --out char_info_mj.tsv

Only relations that name a variant and its standard form are read:
  koseki   戸籍統一文字情報 親字・正字
  tsutatsu 民一2842号通達別表 誤字俗字・正字一覧表
  tsuchi   民二5202号通知別表 正字・俗字等対照表
  jis      JIS包摂規準・UCS統合規則
Not read: 辞書類等による関連字 (loose, tracked in a GitHub issue),
読み・字形による類推 (guesses), and 法務省告示582号別表第四 (a government
notice, already in char_info.tsv).
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
import xml.etree.ElementTree as ET
import zipfile

KOSEKI_KINDS = {
    '戸籍統一文字情報 親字・正字': 'koseki',
    '民一2842号通達別表 誤字俗字・正字一覧表': 'tsutatsu',
    '民二5202号通知別表 正字・俗字等対照表': 'tsuchi',
}
UCS_RE = re.compile(r'^U\+([0-9A-Fa-f]{4,6})$')


def ucs_char(value: str | None) -> str | None:
    m = UCS_RE.match(value or '')
    return chr(int(m.group(1), 16)) if m else None


def load_mj_ucs(xlsx: pathlib.Path) -> dict[str, str]:
    """Maps MJ文字図形名 (MJ000001) to its 対応するUCS character."""
    z = zipfile.ZipFile(xlsx)
    ns = ''
    for name in ('http://purl.oclc.org/ooxml/spreadsheetml/main',
                 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'):
        if ('xmlns="%s"' % name).encode() in z.read('xl/sharedStrings.xml')[:600]:
            ns = '{%s}' % name
    if not ns:
        raise SystemExit('unrecognised spreadsheet namespace')
    strings = [''.join(t.text or '' for t in si.iter(ns + 't'))
               for si in ET.fromstring(z.read('xl/sharedStrings.xml')).iter(ns + 'si')]

    def column(ref: str) -> int:
        n = 0
        for ch in re.match(r'[A-Z]+', ref).group():
            n = n * 26 + ord(ch) - 64
        return n - 1

    result: dict[str, str] = {}
    for _, el in ET.iterparse(z.open('xl/worksheets/sheet1.xml')):
        if el.tag != ns + 'row':
            continue
        cells = {}
        for c in el.iter(ns + 'c'):
            v = c.find(ns + 'v')
            if v is not None:
                cells[column(c.get('r'))] = (
                    strings[int(v.text)] if c.get('t') == 's' else v.text)
        el.clear()
        mj, ch = cells.get(2, ''), ucs_char(cells.get(3))
        if mj.startswith('MJ') and ch:
            result[mj] = ch
    return result


def mj_pairs(mj_map: dict, mj_ucs: dict[str, str]) -> set[tuple[str, str, str]]:
    """(variant, standard, kind) triples from the MJ縮退マップ content."""
    triples = set()
    for entry in mj_map['content']:
        a = mj_ucs.get(entry.get('MJ文字図形名', ''))
        if not a:
            continue
        for t in entry.get('法務省戸籍法関連通達・通知', []):
            kind = KOSEKI_KINDS.get(t.get('種別'))
            b = ucs_char(t.get('UCS'))
            if kind and b and b != a:
                triples.add((a, b, kind))
        for t in entry.get('JIS包摂規準・UCS統合規則', []):
            b = ucs_char(t.get('UCS'))
            if b and b != a:
                triples.add((a, b, 'jis'))
    return triples


def load_chars(char_info: pathlib.Path) -> set[str]:
    with char_info.open(encoding='utf-8') as f:
        return {line.split('\t', 1)[0] for line in f if not line.startswith('#')}


def write_links(triples, chars: set[str], out: pathlib.Path) -> int:
    kept = sorted((a, b, k) for a, b, k in triples if a in chars and b in chars)
    with out.open('w', encoding='utf-8', newline='\n') as f:
        f.write('# Variant -> standard-form links from the IPA MJ data.\n')
        f.write('# (c) IPA, Information-technology Promotion Agency, Japan.\n')
        f.write('# Licensed CC BY-SA 2.1 Japan: '
                'https://creativecommons.org/licenses/by-sa/2.1/jp/\n')
        f.write('# Derived from MJ縮退マップ 1.2.0 and MJ文字情報一覧表 006.02 by '
                'build_mj_links.py; see README.md.\n')
        f.write('#variant\tstandard\tkind\n')
        for a, b, k in kept:
            f.write(f'{a}\t{b}\t{k}\n')
    return len(kept)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--mj-list', type=pathlib.Path, required=True,
                    help='mji.00602.xlsx (MJ文字情報一覧表)')
    ap.add_argument('--mj-map', type=pathlib.Path, required=True,
                    help='MJShrinkMap.1.2.0.json')
    ap.add_argument('--char-info', type=pathlib.Path, required=True,
                    help='char_info.tsv; keeps only characters it has')
    ap.add_argument('--out', type=pathlib.Path, required=True)
    args = ap.parse_args()
    mj_map = json.loads(args.mj_map.read_text(encoding='utf-8'))
    n = write_links(mj_pairs(mj_map, load_mj_ucs(args.mj_list)),
                    load_chars(args.char_info), args.out)
    print(f'{n} links -> {args.out}', file=sys.stderr)


if __name__ == '__main__':
    main()
