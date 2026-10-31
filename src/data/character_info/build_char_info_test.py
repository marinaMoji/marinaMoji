#!/usr/bin/env python3
# Copyright 2026 marinaMoji contributors.
# SPDX-License-Identifier: BSD-3-Clause
"""Tiny fixture test for build_char_info.py (no full Unihan download)."""

from __future__ import annotations

import pathlib
import tempfile
import unittest

import build_char_info as b


class BuildCharInfoTest(unittest.TestCase):
  def test_parses_fixture_and_writes_row(self):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      (root / 'Unihan_Readings.txt').write_text(
          '# comment\n'
          'U+8B00\tkMandarin\tmóu\n'
          'U+8B00\tkJapaneseOn\tBOU MU\n'
          'U+8B00\tkJapaneseKun\thakaru\n'
          'U+8B00\tkJapanese\tボウ はかる\n'
          'U+8B00\tkDefinition\tplan; plot\n',
          encoding='utf-8')
      (root / 'Unihan_DictionaryLikeData.txt').write_text(
          'U+8B00\tkTotalStrokes\t16\n', encoding='utf-8')
      (root / 'Unihan_RadicalStrokeCounts.txt').write_text(
          "U+8B00\tkRSUnicode\t149'.9\n", encoding='utf-8')
      (root / 'Unihan_Variants.txt').write_text(
          'U+8B00\tkZVariant\tU+8C08\n'
          'U+8B00\tkJapaneseOldVariant\tU+8C09\n'
          'U+8B00\tkJapaneseNewVariant\tU+8C0A\n', encoding='utf-8')

      out = root / 'char_info.tsv'
      rows = b.load_unihan(root)
      n = b.write_tsv(rows, out, composer_blocks=True)
      self.assertEqual(n, 1)
      lines = [ln for ln in out.read_text(encoding='utf-8').splitlines()
               if not ln.startswith('#')]
      self.assertEqual(len(lines), 1)
      cells = lines[0].split('\t')
      self.assertEqual(cells[0], '謀')
      self.assertEqual(cells[1], 'U+8B00')
      self.assertEqual(cells[2], '16')
      self.assertEqual(cells[3], 'móu')
      self.assertEqual(cells[4], 'BOU MU')
      self.assertEqual(cells[5], 'hakaru')
      self.assertEqual(cells[6], "149'.9")
      self.assertEqual(cells[7], 'plan; plot')
      self.assertEqual(cells[8], chr(0x8C08))
      self.assertEqual(cells[9], 'ボウ はかる')
      self.assertEqual(cells[10], chr(0x8C09))
      self.assertEqual(cells[11], chr(0x8C0A))

  def test_cjkvi_variants_are_added_both_ways_to_existing_rows_only(self):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      (root / 'cjkvi-variants.txt').write_text(
          '# Copyright (c) 2014 CJKVI Database\n'
          'cjkvi/variant,<name>,header\n'
          '鐗,cjkvi/variant,鐧\n'
          '鐗,cjkvi/variant,鐗\n'
          '鐗,cjkvi/variant,鍵鍵\n', encoding='utf-8')
      (root / 'hydzd-variants.txt').write_text(
          '鐧,hydzd/variant,𨰓\n鐧,hydzd/variant,鐗\n', encoding='utf-8')
      rows = {0x9427: {'variants': '锏'}, 0x9417: {}}
      added = b.add_cjkvi_variants(rows, root)
      self.assertEqual(rows[0x9427]['variants'], '锏 鐗 𨰓')
      self.assertEqual(rows[0x9417]['variants'], '鐧')
      self.assertNotIn(0x28C13, rows)
      self.assertEqual(added, 3)


if __name__ == '__main__':
  unittest.main()
