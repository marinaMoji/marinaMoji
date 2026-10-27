#!/usr/bin/env bash
# Fail if the marinaMoji product version baked into this build does not match
# the expected release string (no leading "v"). Used by the release workflow
# after `bazel build` so About / update checks cannot ship a stale
# marina_product_version.txt (e.g. still "0.0.16" while the tag is 0.0.18-rc7).
#
# Usage:
#   verify_product_version.sh --expected 0.0.18-rc7 [--header PATH | --search DIR]
#                             [--binary PATH]
#
# --search DIR finds product_version_def.h anywhere under DIR (pass
# `bazel info output_path`): bazel-bin is a single configuration, and builds
# with --macos_cpus or --config put the header under another one.
set -euo pipefail

EXPECTED=''
HEADER=''
SEARCH=''
BINARY=''

while [[ $# -gt 0 ]]; do
  case "$1" in
    --expected) EXPECTED="$2"; shift 2 ;;
    --header) HEADER="$2"; shift 2 ;;
    --search) SEARCH="$2"; shift 2 ;;
    --binary) BINARY="$2"; shift 2 ;;
    *)
      echo "usage: $0 --expected <x.y.z[-rcN]> [--header product_version_def.h | --search DIR] [--binary mozc_tool]" >&2
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

if [[ -z "$HEADER" && -n "$SEARCH" ]]; then
  [[ -d "$SEARCH" ]] || fail "search dir not found: $SEARCH"
  HEADER="$(find -L "$SEARCH" -name product_version_def.h -print -quit)"
  [[ -n "$HEADER" ]] || fail "product_version_def.h not found under $SEARCH"
fi

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

# Linked binary (About dialog / update checker): version.cc embeds the line
# "marinaMoji-product-version=<version>"; match it whole so rc9 cannot pass
# for rc90 or a stale rc.
if [[ -n "$BINARY" ]]; then
  [[ -f "$BINARY" ]] || fail "binary not found: $BINARY"
  MARKER="marinaMoji-product-version=$EXPECTED"
  # grep -ao instead of strings(1): not every runner (Git Bash) ships binutils.
  found() { LC_ALL=C grep -aoE 'marinaMoji-product-version=[ -~]+' "$BINARY" || true; }
  if ! found | grep -Fx "$MARKER" >/dev/null; then
    echo "---- version markers in binary ----" >&2
    found | head -5 >&2
    fail "$BINARY does not contain \"$MARKER\" (About would show a stale version)"
  fi
  echo "OK: $BINARY contains $MARKER"
fi

if [[ -z "$HEADER" && -z "$BINARY" ]]; then
  fail "pass --header/--search and/or --binary"
fi
