#!/bin/bash
# Push marinaMozc's copy of the shin/kyu CSV tables back to the
# character_conversion sibling repo (tables_output/), for when a quick fix
# was hand-edited here instead of there. Run this BEFORE the next
# import_opencc_tables.sh, or the edit will be silently overwritten.
#
# Usage: edit a CSV in data/marina_opencc/tables/, then from marinaMozc/src:
#   bash export_opencc_tables.sh
#
# Set MARINA_OPENCC_SRC to override the default sibling-checkout location.
# This only copies the CSVs — it does not run character_conversion's own
# generate/compile step there; do that separately if you also maintain the
# Rime-based marinaMoji IME's dictionaries from that repo.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/data/marina_opencc/tables"
DST="${MARINA_OPENCC_SRC:-$ROOT/../../marinaMoji/character_conversion/tables_output}"

if [[ ! -d "$DST" ]]; then
  echo "ERROR: character_conversion tables_output not found at $DST" >&2
  echo "Set MARINA_OPENCC_SRC to the character_conversion tables_output directory." >&2
  exit 1
fi

cp "$SRC/char_complete_shin_kyu_table_manual.csv" \
   "$SRC/char_one_to_many_table_manual.csv" \
   "$SRC/phrases_substitution_table_final.csv" \
   "$DST/"

echo "Exported CSVs to $DST"
echo "Remember to regenerate/recompile there too if it feeds another IME (e.g. marinaMoji/Rime)."
