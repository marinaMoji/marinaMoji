#!/bin/bash
# Pull the curated shin/kyu CSV tables from the character_conversion sibling
# repo (the source of truth — edit tables there, using its full tooling) and
# rebuild marinaMozc's copy of the OpenCC dictionaries in one step.
#
# Usage: edit a CSV in character_conversion, then from marinaMozc/src run:
#   bash import_opencc_tables.sh
#
# Set MARINA_OPENCC_SRC to override the default sibling-checkout location.
# Requires: python3, and the opencc_dict CLI (see regen_opencc.sh).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="${MARINA_OPENCC_SRC:-$ROOT/../../marinaMoji/character_conversion/tables_output}"
DST="$ROOT/data/marina_opencc/tables"

for name in char_complete_shin_kyu_table_manual.csv \
            char_one_to_many_table_manual.csv \
            phrases_substitution_table_final.csv; do
  if [[ ! -f "$SRC/$name" ]]; then
    echo "ERROR: $name not found in $SRC" >&2
    echo "Set MARINA_OPENCC_SRC to the character_conversion tables_output directory." >&2
    exit 1
  fi
done

cp "$SRC/char_complete_shin_kyu_table_manual.csv" \
   "$SRC/char_one_to_many_table_manual.csv" \
   "$SRC/phrases_substitution_table_final.csv" \
   "$DST/"

echo "Imported CSVs from $SRC"
bash "$ROOT/regen_opencc.sh"
