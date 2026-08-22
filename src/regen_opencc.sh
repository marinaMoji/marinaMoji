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

for name in Phrases Characters Variants; do
  opencc_dict --input "$TABLES/marinaShin2Kyu${name}.txt" \
              --output "$DIR/marinaShin2Kyu${name}.ocd2" \
              --from text --to ocd2
done

echo "Regenerated marina OpenCC tables in $DIR"
