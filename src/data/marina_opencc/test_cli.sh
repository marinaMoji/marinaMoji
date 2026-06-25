#!/usr/bin/env bash
# Quick CLI checks for opencc_runtime/ (does not require the IME).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG="$ROOT/marinaShin2Kyu.json"

if ! command -v opencc >/dev/null 2>&1; then
  echo "opencc CLI not found. Install with: brew install opencc" >&2
  exit 1
fi

for required in marinaShin2Kyu.json \
  marinaShin2KyuPhrases.ocd2 \
  marinaShin2KyuCharacters.ocd2 \
  marinaShin2KyuVariants.ocd2; do
  if [[ ! -f "$ROOT/$required" ]]; then
    echo "Missing $ROOT/$required" >&2
    exit 1
  fi
done

convert() {
  local input=$1
  local expected=$2
  local output
  output="$(printf '%s' "$input" | opencc -c "$CONFIG")"
  echo "$input → $output"
  if [[ -n "$expected" && "$output" != "$expected" ]]; then
    echo "  expected: $expected" >&2
    return 1
  fi
}

echo "Config: $CONFIG"
echo

convert "会" "會"
convert "車両" "車輛"
compound="$(printf '%s' '青森車両センター' | opencc -c "$CONFIG")"
echo "青森車両センター → $compound"
if [[ "$compound" != *車輛* ]]; then
  echo "  expected 車輛 in compound output" >&2
  exit 1
fi
bench="$(printf '%s' '弁護士' | opencc -c "$CONFIG")"
echo "弁護士 → $bench"
if [[ "$bench" == "弁護士" ]]; then
  echo "  expected some kyûjitai conversion" >&2
  exit 1
fi

echo
echo "OK — opencc_runtime dictionaries load and convert."
