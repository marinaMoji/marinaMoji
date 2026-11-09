#!/bin/bash
# Regenerate the marina shin/kyu OpenCC dictionaries from the hand-edited CSV
# tables in data/marina_opencc/tables/.
#
# Usage: edit a CSV, then from marinaMozc/src run:
#   bash regen_opencc.sh
#
# Requires: python3, and the opencc_dict CLI (Debian/Ubuntu: apt-get install
# opencc libopencc-dev; macOS: brew install opencc).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
DIR="$ROOT/data/marina_opencc"
TABLES="$DIR/tables"

python3 "$TABLES/generate_opencc_tables.py"

# opencc_dict rejects the generated '#' header lines ("Tabular not found")
# yet still exits 0, so feed it a comment-free copy and check the output.
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

for name in Phrases Characters Variants; do
  out="$DIR/marinaShin2Kyu${name}.ocd2"
  grep -v -e '^#' -e '^$' "$TABLES/marinaShin2Kyu${name}.txt" > "$TMP/${name}.txt"
  rm -f "$TMP/${name}.ocd2"
  opencc_dict --input "$TMP/${name}.txt" \
              --output "$TMP/${name}.ocd2" \
              --from text --to ocd2
  if [[ ! -s "$TMP/${name}.ocd2" ]]; then
    echo "ERROR: opencc_dict did not produce $out" >&2
    exit 1
  fi
  mv "$TMP/${name}.ocd2" "$out"
done

echo "Regenerated marina OpenCC tables in $DIR"
