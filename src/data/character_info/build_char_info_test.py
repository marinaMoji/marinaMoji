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

  def test_region_forms_group_regions_by_sans_or_serif_cid(self):
    def cmap(entries):
      body = ''.join(f'<{cp:08x}> {cid}\n' for cp, cid in entries)
      return f'1 begincidchar\n{body}endcidchar\n'

    def cmap_range(lo, hi, cid):
      return f'1 begincidrange\n<{lo:08x}> <{hi:08x}> {cid}\nendcidrange\n'

    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      # U+9AA8: Sans CN=KR, HK=TW, JP alone. Serif says JP=HK, which joins
      # {HK,TW} and {JP}. U+4E00: all agree. U+4E01: only in CN and JP.
      sans = {'CN': 10, 'HK': 11, 'TW': 11, 'JP': 12, 'KR': 10}
      serif = {'CN': 20, 'HK': 21, 'TW': 22, 'JP': 21, 'KR': 20}
      for family, cids in (('Sans', sans), ('Serif', serif)):
        for region, cid in cids.items():
          text = cmap([(0x9AA8, cid)]) + cmap_range(0x4E00, 0x4E00, 5)
          if region in ('CN', 'JP'):
            text += cmap([(0x4E01, 7 if region == 'CN' else 8)])
          (root / f'UniSourceHan{family}{region}-UTF32-H').write_text(text)
      rows = {0x9AA8: {}, 0x4E00: {}, 0x4E01: {}, 0x4E02: {}}
      added = b.add_region_forms(rows, root)
      self.assertEqual(rows[0x9AA8]['region_forms'], 'CN KR|HK TW JP')
      self.assertNotIn('region_forms', rows[0x4E00])
      self.assertEqual(rows[0x4E01]['region_forms'], 'CN|JP')
      self.assertNotIn('region_forms', rows[0x4E02])
      self.assertEqual(added, 2)

  def test_tghb_and_nyukan_links_go_both_ways_to_existing_rows_only(self):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      (root / 'variants.txt').write_text(
          '序号\t规范字\t繁体字\t异体字\t註解\n'
          '0006\t厂\t廠\t廠\t\n'
          '0008\t卜\t卜\t蔔\t\n', encoding='utf-8')
      (root / 'nyukanseiji.json').write_text(
          '[["table4_1", [["3400", "4e18"], ["342a", "4ea5", "5b9b"]]],'
          ' ["table4_2", [["3403", "4ed0"]]]]', encoding='utf-8')
      rows = {0x5382: {}, 0x5EE0: {}, 0x535C: {}, 0x8514: {},
              0x3400: {}, 0x4E18: {}, 0x342A: {}, 0x4EA5: {},
              0x3403: {}, 0x4ED0: {}}
      self.assertEqual(b.add_tghb_variants(rows, root / 'variants.txt'), 4)
      self.assertEqual(rows[0x5382]['variants'], '廠')
      self.assertEqual(rows[0x535C]['variants'], '蔔')
      self.assertEqual(rows[0x8514]['variants'], '卜')
      self.assertEqual(b.add_nyukan_itaiji(rows, root / 'nyukanseiji.json'), 4)
      self.assertEqual(rows[0x3400]['variants'], '\u4e18')
      self.assertNotIn(chr(0x5B9B), rows[0x342A]['variants'])  # row absent
      self.assertEqual(rows[0x342A]['variants'], '\u4ea5')
      self.assertNotIn('variants', rows[0x3403])  # table4_2 is ignored

  def test_ivs_forms_count_sequences_per_collection(self):
    with tempfile.TemporaryDirectory() as tmp:
      path = pathlib.Path(tmp) / 'IVD_Sequences.txt'
      path.write_text(
          '# IVD_Sequences\n'
          '3402 E0100; Adobe-Japan1; CID+13698\n'
          '3402 E0101; Adobe-Japan1; CID+13697\n'
          '3402 E0102; Moji_Joho; MJ000123\n'
          '3402 E0100; Unknown; X\n'
          '9999 E0100; Hanyo-Denshi; HD1\n', encoding='utf-8')
      rows = {0x3402: {}, 0x4E00: {}}
      self.assertEqual(b.add_ivs_forms(rows, path), 1)
      self.assertEqual(rows[0x3402]['ivs_forms'], 'A2 M1')
      self.assertNotIn('ivs_forms', rows[0x4E00])

  def test_cjkvi_variants_are_added_both_ways_to_existing_rows_only(self):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      (root / 'cjkvi-variants.txt').write_text(
          '# Copyright (c) 2014 CJKVI Database\n'
          'cjkvi/variant,<name>,header\n'
          '鐗,cjkvi/variant,鐧\n'
          '鐗,cjkvi/variant,鐗\n'
          '鐗,cjkvi/variant,鍵鍵\n', encoding='utf-8')
      # Only cjkvi-variants.txt is read; other tables in the folder are ignored.
      (root / 'hydzd-variants.txt').write_text(
          '鐧,hydzd/variant,𨰓\n', encoding='utf-8')
      rows = {0x9427: {'variants': '锏'}, 0x9417: {}}
      added = b.add_cjkvi_variants(rows, root)
      self.assertEqual(rows[0x9427]['variants'], '锏 鐗')
      self.assertEqual(rows[0x9417]['variants'], '鐧')
      self.assertNotIn(0x28C13, rows)
      self.assertEqual(added, 2)

  def test_glyphwiki_itaiji_aliases_link_existing_rows_both_ways(self):
    with tempfile.TemporaryDirectory() as tmp:
      dump = pathlib.Path(tmp) / 'dump_newest_only.txt'
      dump.write_text(
          ' name | related | data\n'
          '------+---------+-----\n'
          ' u58fc-itaiji-001 | u58fc | 99:0:0:0:0:200:200:u58f7\n'
          ' u58fc-itaiji-002 | u58fc | 99:0:0:0:0:200:200:u58f7-02@3\n'
          ' u6765-itaiji-001 | u6765 | 99:0:0:0:0:200:200:u8012$1:0:0:0:0:1:1\n'
          ' u58fc-var-001 | u58fc | 99:0:0:0:0:200:200:u6765\n'
          ' u58fc-itaiji-003 | u58fc | 99:0:0:0:0:200:200:u2b740\n',
          encoding='utf-8')
      rows = {0x58FC: {}, 0x58F7: {'variants': 'x'}, 0x6765: {}}
      added = b.add_glyphwiki_itaiji(rows, dump)
      self.assertEqual(rows[0x58FC]['variants'], chr(0x58F7))
      self.assertEqual(rows[0x58F7]['variants'], 'x ' + chr(0x58FC))
      self.assertEqual(rows[0x6765], {})
      self.assertEqual(added, 2)
      rows = {0x58FC: {}, 0x2B740: {}}
      self.assertEqual(b.add_glyphwiki_itaiji(rows, dump), 0)


if __name__ == '__main__':
  unittest.main()
