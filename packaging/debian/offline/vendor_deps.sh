#!/usr/bin/env bash
# Populate packaging/debian/vendor/ with every external dep needed to build
# //:package offline (Bazel vendor mode). Requires network.
#
# Usage (from anywhere):
#   bash packaging/debian/offline/vendor_deps.sh [-- bazel args...]
#
# Examples:
#   bash packaging/debian/offline/vendor_deps.sh
#   bash packaging/debian/offline/vendor_deps.sh -- --config oss_linux --config release_build
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

if ! command -v bazelisk >/dev/null 2>&1; then
  echo "vendor_deps.sh: bazelisk not found in PATH" >&2
  exit 1
fi

mkdir -p "$VENDOR_DIR"
echo "vendor_deps.sh: repo=$REPO_ROOT"
echo "vendor_deps.sh: vendor_dir=$VENDOR_DIR"
echo "vendor_deps.sh: bazel args: ${BAZEL_ARGS[*]}"

cd "$SRC"
# Bazel 9 vendor mode copies external repos + BCR registry files into
# vendor_dir (see: bazel help vendor).
bazelisk vendor \
  --vendor_dir="$VENDOR_DIR" \
  //:package \
  "${BAZEL_ARGS[@]}"

# src/MODULE.bazel.lock is gitignored (not in the orig tarball) but
# --lockfile_mode=error needs the exact lockfile this vendor run produced.
# Ship it inside the vendor tree; debian/rules installs it if absent.
if [[ -f "$SRC/MODULE.bazel.lock" ]]; then
  cp "$SRC/MODULE.bazel.lock" "$VENDOR_DIR/MODULE.bazel.lock"
else
  echo "vendor_deps.sh: WARNING: $SRC/MODULE.bazel.lock missing" >&2
fi

echo "vendor_deps.sh: done. Tree size:"
du -sh "$VENDOR_DIR"
