#!/usr/bin/env python3
# Copyright 2026 marinaMoji contributors.
# SPDX-License-Identifier: BSD-3-Clause
"""Extracts total stroke counts from the Unicode Unihan database.

Reads Unihan_IRGSources.txt (the file that holds the kTotalStrokes field) and
writes stroke_counts.tsv: one "character<TAB>count[,count...]" line per
character, in code point order. A character with several counts (one per
region, e.g. "8 9" in Unihan) lists them all, comma-separated.

  python3 build_stroke_counts.py --unihan Unihan_IRGSources.txt \
      --out stroke_counts.tsv
"""

import argparse


def main():
  parser = argparse.ArgumentParser(description=__doc__,
                                   formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('--unihan', required=True)
  parser.add_argument('--out', required=True)
  args = parser.parse_args()

  counts = {}
  with open(args.unihan, encoding='utf-8') as f:
    for line in f:
      if line.startswith('#'):
        continue
      fields = line.rstrip('\n').split('\t')
      if len(fields) != 3 or fields[1] != 'kTotalStrokes':
        continue
      code = int(fields[0][2:], 16)
      values = [v for v in fields[2].split() if v.isdigit()]
      if values:
        counts[code] = values

  with open(args.out, 'w', encoding='utf-8', newline='\n') as out:
    for code in sorted(counts):
      out.write(f'{chr(code)}\t{",".join(counts[code])}\n')
  print(f'{len(counts)} characters written to {args.out}')


if __name__ == '__main__':
  main()
