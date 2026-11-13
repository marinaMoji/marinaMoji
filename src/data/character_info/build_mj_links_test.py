#!/usr/bin/env python3
# Copyright 2026 marinaMoji contributors.
# SPDX-License-Identifier: BSD-3-Clause
"""Fixture test for build_mj_links.py."""

from __future__ import annotations

import pathlib
import tempfile
import unittest
import zipfile

import build_mj_links as m

NS = 'http://purl.oclc.org/ooxml/spreadsheetml/main'


def make_xlsx(path: pathlib.Path, rows) -> None:
  strings = []

  def sidx(s):
    if s not in strings:
      strings.append(s)
    return strings.index(s)

  body = ''
  for r, (mj, ucs) in enumerate(rows, 1):
    body += (f'<row r="{r}"><c r="C{r}" t="s"><v>{sidx(mj)}</v></c>'
             f'<c r="D{r}" t="s"><v>{sidx(ucs)}</v></c></row>')
  with zipfile.ZipFile(path, 'w') as z:
    z.writestr('xl/sharedStrings.xml',
               f'<sst xmlns="{NS}">' +
               ''.join(f'<si><t>{s}</t></si>' for s in strings) + '</sst>')
    z.writestr('xl/worksheets/sheet1.xml',
               f'<worksheet xmlns="{NS}"><sheetData>{body}</sheetData></worksheet>')


class BuildMjLinksTest(unittest.TestCase):
  def test_reads_only_variant_to_standard_relations(self):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      make_xlsx(root / 'mji.xlsx', [
          ('MJ文字図形名', '対応するUCS'),
          ('MJ000001', 'U+780F'), ('MJ000002', 'U+7926'),
          ('MJ000003', 'U+4E18'), ('MJ000004', 'U+5775')])
      mj_ucs = m.load_mj_ucs(root / 'mji.xlsx')
      self.assertEqual(mj_ucs['MJ000001'], '砏')
      data = {'content': [
          {'MJ文字図形名': 'MJ000001',
           '法務省戸籍法関連通達・通知': [
               {'種別': '戸籍統一文字情報 親字・正字', 'UCS': 'U+7926'},
               {'種別': '未知の種別', 'UCS': 'U+4E18'}],
           '辞書類等による関連字': [{'UCS': 'U+5775'}],
           'JIS包摂規準・UCS統合規則': [{'UCS': 'U+780F'}]},
          {'MJ文字図形名': 'MJ000003',
           '法務省告示582号別表第四': [{'表': '二', 'UCS': 'U+5775'}]}]}
      triples = m.mj_pairs(data, mj_ucs)
      self.assertEqual(triples, {('砏', '礦', 'koseki')})
      out = root / 'out.tsv'
      n = m.write_links(triples, {'砏', '礦'}, out)
      self.assertEqual(n, 1)
      body = [l for l in out.read_text(encoding='utf-8').splitlines()
              if not l.startswith('#')]
      self.assertEqual(body, ['砏\t礦\tkoseki'])
      self.assertEqual(m.write_links(triples, {'砏'}, out), 0)


if __name__ == '__main__':
  unittest.main()
