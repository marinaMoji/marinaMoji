#!/bin/bash
# Copy compiled marina OpenCC runtime files into marinaMozc bundle inputs.
# Run from marinaMozc/src/ after regenerating tables in character_conversion.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="${MARINA_OPENCC_SRC:-$ROOT/../marinaMoji/character_conversion/opencc_runtime}"
DST="$ROOT/data/marina_opencc"

if [[ ! -f "$SRC/marinaShin2Kyu.json" ]]; then
  echo "ERROR: marina opencc runtime not found at: $SRC" >&2
  echo "Set MARINA_OPENCC_SRC or build opencc_runtime in character_conversion first." >&2
  exit 1
fi

cp "$SRC/marinaShin2Kyu.json" \
   "$SRC/marinaShin2KyuPhrases.ocd2" \
   "$SRC/marinaShin2KyuCharacters.ocd2" \
   "$SRC/marinaShin2KyuVariants.ocd2" \
   "$DST/"

echo "Synced marina OpenCC data to $DST"
