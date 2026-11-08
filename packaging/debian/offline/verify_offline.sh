#!/usr/bin/env bash
# Build //:package using only packaging/debian/vendor/ (no downloads).
# Run vendor_deps.sh first.
#
# Usage:
#   bash packaging/debian/offline/verify_offline.sh [-- bazel args...]
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEB_DIR="$(cd "$HERE/.." && pwd)"
REPO_ROOT="$(cd "$DEB_DIR/../.." && pwd)"
SRC="$REPO_ROOT/src"
VENDOR_DIR="${MARINAMOJI_VENDOR_DIR:-$DEB_DIR/vendor}"

BAZEL_ARGS=(--config release_build)
if [[ $# -gt 0 ]]; then
  if [[ "$1" == "--" ]]; then
    shift
    BAZEL_ARGS=("$@")
  else
    BAZEL_ARGS=("$@")
  fi
fi

if [[ ! -d "$VENDOR_DIR" ]]; then
  echo "verify_offline.sh: missing vendor dir: $VENDOR_DIR" >&2
  echo "Run packaging/debian/offline/vendor_deps.sh first." >&2
  exit 1
fi

if ! command -v bazelisk >/dev/null 2>&1; then
  echo "verify_offline.sh: bazelisk not found in PATH" >&2
  exit 1
fi

# Default smoke target is //server:mozc_server: it proves vendor+offline
# resolution without macOS installer zip staging (which can fail on
# local unzip/symlink permissions unrelated to networking). Pass
# MARINAMOJI_OFFLINE_TARGET=//:package to exercise the full package.
TARGET="${MARINAMOJI_OFFLINE_TARGET:-//server:mozc_server}"
# Fresh output base avoids stale external/ repos from the networked vendor
# run (those can be incomplete or hard-linked oddly under Docker Desktop).
OUTPUT_BASE="${MARINAMOJI_OFFLINE_OUTPUT_BASE:-}"

echo "verify_offline.sh: vendor_dir=$VENDOR_DIR"
echo "verify_offline.sh: target=$TARGET"
echo "verify_offline.sh: bazel args: ${BAZEL_ARGS[*]}"
if [[ -n "$OUTPUT_BASE" ]]; then
  echo "verify_offline.sh: output_base=$OUTPUT_BASE"
  mkdir -p "$OUTPUT_BASE"
fi

cd "$SRC"
# lockfile_mode=error: do not refresh registry metadata.
# repository_disable_download: fail loudly if anything still tries ctx.download.
bazelisk \
  ${OUTPUT_BASE:+--output_base="$OUTPUT_BASE"} \
  build \
  --vendor_dir="$VENDOR_DIR" \
  --repository_disable_download \
  --lockfile_mode=error \
  "$TARGET" \
  "${BAZEL_ARGS[@]}"

echo "verify_offline.sh: offline build succeeded ($TARGET)"
