#!/usr/bin/env bash
# Fail if the marinaMoji product version baked into this build does not match
# the expected release string (no leading "v"). Used by the release workflow
# after `bazel build` so About / update checks cannot ship a stale
# marina_product_version.txt (e.g. still "0.0.16" while the tag is 0.0.18-rc7).
#
# Usage:
#   verify_product_version.sh --expected 0.0.18-rc7 [--header PATH] [--binary PATH]
set -euo pipefail

EXPECTED=''
HEADER=''
BINARY=''

while [[ $# -gt 0 ]]; do
  case "$1" in
    --expected) EXPECTED="$2"; shift 2 ;;
    --header) HEADER="$2"; shift 2 ;;
    --binary) BINARY="$2"; shift 2 ;;
    *)
      echo "usage: $0 --expected <x.y.z[-rcN]> [--header product_version_def.h] [--binary mozc_tool]" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$EXPECTED" ]]; then
  echo "error: --expected is required" >&2
  exit 2
fi
if [[ "$EXPECTED" == v* ]]; then
  echo "error: pass the version without a leading v (got $EXPECTED)" >&2
  exit 2
fi

fail() {
  echo "error: $*" >&2
  exit 1
}

# Generated header: inline constexpr const char kProductVersion[] = "…";
if [[ -n "$HEADER" ]]; then
  [[ -f "$HEADER" ]] || fail "header not found: $HEADER"
  if ! grep -F "kProductVersion[] = \"$EXPECTED\"" "$HEADER" >/dev/null; then
    echo "---- $HEADER ----" >&2
    cat "$HEADER" >&2 || true
    fail "product_version_def.h does not embed \"$EXPECTED\""
  fi
  echo "OK: $HEADER embeds $EXPECTED"
fi

# Linked binary (About dialog / update checker): the version string must appear.
if [[ -n "$BINARY" ]]; then
  [[ -f "$BINARY" ]] || fail "binary not found: $BINARY"
  if ! strings "$BINARY" | grep -F "$EXPECTED" >/dev/null; then
    echo "---- strings (version-like) ----" >&2
    strings "$BINARY" | grep -E '^[0-9]+\.[0-9]+\.[0-9]+' | head -20 >&2 || true
    fail "$BINARY does not contain \"$EXPECTED\" (About would show a stale version)"
  fi
  echo "OK: $BINARY contains $EXPECTED"
fi

if [[ -z "$HEADER" && -z "$BINARY" ]]; then
  fail "pass --header and/or --binary"
fi
