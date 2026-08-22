#!/usr/bin/env python3
"""
Generate OpenCC dictionary text files from the curated shin/kyu CSV tables.

OpenCC expects tab-separated plain text:
    key<TAB>value
    key<TAB>value1 value2 value3   ← one-to-many: several values on one line

Mirrors OpenCC's Japanese jp2t layout (three layers, same order in config):

  1. marinaShin2KyuPhrases.txt    — multi-character compounds (longest match first)
  2. marinaShin2KyuCharacters.txt — one-to-many single characters only
  3. marinaShin2KyuVariants.txt   — one-to-one single characters (fallback layer)

Ported from character_conversion/generate_opencc_tables.py so the tables can
be hand-edited and rebuilt entirely within marinaMozc. Source CSVs live next
to this script (in this directory); the .txt outputs are also written here,
and marinaShin2Kyu.json is written to the parent directory (data/marina_opencc/),
which is where the compiled .ocd2 files live too. Use ../../regen_opencc.sh
(marinaMozc/src/regen_opencc.sh) to run this and compile the .ocd2 outputs in
one step.

Usage (from this directory, or via regen_opencc.sh):
    python3 generate_opencc_tables.py
"""
from __future__ import annotations

import csv
import json
import os
from collections import defaultdict

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = SCRIPT_DIR
CONFIG_DIR = os.path.dirname(SCRIPT_DIR)  # data/marina_opencc/

CHAR_COMPLETE = os.path.join(SCRIPT_DIR, "char_complete_shin_kyu_table_manual.csv")
CHAR_ONE_TO_MANY = os.path.join(SCRIPT_DIR, "char_one_to_many_table_manual.csv")
PHRASES = os.path.join(SCRIPT_DIR, "phrases_substitution_table_final.csv")

OUT_PHRASES = "marinaShin2KyuPhrases.txt"
OUT_CHARACTERS = "marinaShin2KyuCharacters.txt"
OUT_VARIANTS = "marinaShin2KyuVariants.txt"
OUT_CONFIG = "marinaShin2Kyu.json"


def read_shin_kyu_pairs(csv_path: str) -> list[tuple[str, str]]:
    """Read a CSV with 'shin' and 'kyu' columns (by header name; column order
    varies between the source CSVs); return (shin, kyu) pairs."""
    pairs: list[tuple[str, str]] = []
    with open(csv_path, encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames or "shin" not in reader.fieldnames or "kyu" not in reader.fieldnames:
            raise ValueError(f"{csv_path} must have 'shin' and 'kyu' columns")
        for row in reader:
            shin = (row.get("shin") or "").strip()
            kyu = (row.get("kyu") or "").strip()
            if shin and kyu:
                pairs.append((shin, kyu))
    return pairs


def group_by_shin(pairs: list[tuple[str, str]]) -> dict[str, list[str]]:
    """Collapse rows to one OpenCC line per shin (values sorted, de-duplicated)."""
    grouped: dict[str, set[str]] = defaultdict(set)
    for shin, kyu in pairs:
        grouped[shin].add(kyu)
    return {shin: sorted(values) for shin, values in sorted(grouped.items())}


def split_characters_and_variants(
    grouped: dict[str, list[str]],
) -> tuple[dict[str, list[str]], dict[str, list[str]]]:
    """
    Split character pairs like OpenCC's JPShinjitaiCharacters vs JPVariantsRev.

    - Characters: shin keys with two or more distinct kyu (one-to-many)
    - Variants: shin keys with exactly one kyu (one-to-one)
    """
    characters: dict[str, list[str]] = {}
    variants: dict[str, list[str]] = {}
    for shin, kyus in grouped.items():
        if len(kyus) > 1:
            characters[shin] = kyus
        else:
            variants[shin] = kyus
    return characters, variants


def write_opencc_dict(out_path: str, grouped: dict[str, list[str]], header_lines: list[str]) -> int:
    """Write one OpenCC dictionary file (lines starting with # are comments)."""
    with open(out_path, "w", encoding="utf-8", newline="\n") as handle:
        for line in header_lines:
            handle.write(line + "\n")
        handle.write("\n")
        for shin, kyus in grouped.items():
            handle.write(f"{shin}\t{' '.join(kyus)}\n")
    return len(grouped)


def count_values(grouped: dict[str, list[str]]) -> int:
    return sum(len(v) for v in grouped.values())


def build_phrases_table() -> dict[str, list[str]]:
    return group_by_shin(read_shin_kyu_pairs(PHRASES))


def build_character_layers() -> tuple[dict[str, list[str]], dict[str, list[str]]]:
    pairs = read_shin_kyu_pairs(CHAR_COMPLETE)
    pairs.extend(read_shin_kyu_pairs(CHAR_ONE_TO_MANY))
    return split_characters_and_variants(group_by_shin(pairs))


def write_opencc_config(out_path: str) -> None:
    """Write marinaMoji OpenCC config (same layout as stock jp2t, marina .ocd2 names)."""
    config = {
        "name": "Japanese shinjitai to kyūjitai for marinaMoji",
        "segmentation": {
            "type": "mmseg",
            "dict": {
                "type": "ocd2",
                "file": "marinaShin2KyuPhrases.ocd2",
            },
        },
        "conversion_chain": [{
            "dict": {
                "type": "group",
                "dicts": [
                    {"type": "ocd2", "file": "marinaShin2KyuPhrases.ocd2"},
                    {"type": "ocd2", "file": "marinaShin2KyuCharacters.ocd2"},
                    {"type": "ocd2", "file": "marinaShin2KyuVariants.ocd2"},
                ],
            },
        }],
    }
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(config, f, ensure_ascii=False, indent=2)
        f.write("\n")


def main() -> None:
    phrases = build_phrases_table()
    phrases_path = os.path.join(OUT_DIR, OUT_PHRASES)
    write_opencc_dict(
        phrases_path,
        phrases,
        header_lines=[
            "# marinaMoji OpenCC dictionary (jp2t layer 1 - Phrases)",
            f"# File: {OUT_PHRASES}",
            "# Format: shin_phrase<TAB>kyu_phrase(s)",
            "# Source: phrases_substitution_table_final.csv",
            "# OpenCC analogue: JPShinjitaiPhrases.txt",
        ],
    )
    print(f"Phrases:    {len(phrases):,} keys, {count_values(phrases):,} mappings -> {phrases_path}")

    characters, variants = build_character_layers()
    characters_path = os.path.join(OUT_DIR, OUT_CHARACTERS)
    write_opencc_dict(
        characters_path,
        characters,
        header_lines=[
            "# marinaMoji OpenCC dictionary (jp2t layer 2 - Characters, one-to-many only)",
            f"# File: {OUT_CHARACTERS}",
            "# Format: shin_char<TAB>kyu1 kyu2 ...",
            "# Source: char_complete_shin_kyu_table_manual.csv + char_one_to_many_table_manual.csv",
            "# OpenCC analogue: JPShinjitaiCharacters.txt",
        ],
    )
    print(f"Characters: {len(characters):,} keys, {count_values(characters):,} mappings -> {characters_path}")

    variants_path = os.path.join(OUT_DIR, OUT_VARIANTS)
    write_opencc_dict(
        variants_path,
        variants,
        header_lines=[
            "# marinaMoji OpenCC dictionary (jp2t layer 3 - Variants, one-to-one)",
            f"# File: {OUT_VARIANTS}",
            "# Format: shin_char<TAB>kyu_char",
            "# Source: char_complete_shin_kyu_table_manual.csv + char_one_to_many_table_manual.csv",
            "# OpenCC analogue: JPVariantsRev.txt (we write shin->kyu directly)",
        ],
    )
    print(f"Variants:   {len(variants):,} keys, {count_values(variants):,} mappings -> {variants_path}")

    config_path = os.path.join(CONFIG_DIR, OUT_CONFIG)
    write_opencc_config(config_path)
    print(f"Config:     {OUT_CONFIG} -> {config_path}")


if __name__ == "__main__":
    main()
