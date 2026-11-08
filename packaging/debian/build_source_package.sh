#!/usr/bin/env bash
# Build Debian orig (+ optional vendor component) tarballs for marinaMoji.
#
# Usage (repo root):
#   bash packaging/debian/build_source_package.sh
#   bash packaging/debian/build_source_package.sh --skip-vendor
#   bash packaging/debian/build_source_package.sh --with-dpkg-source
#
# Reads the product version from src/marina_product_version.txt and writes:
#   dist/debian-source/marinamoji_${UPSTREAM}+ds1.orig.tar.xz
#   dist/debian-source/marinamoji_${UPSTREAM}+ds1.orig-vendor.tar.xz
# Optionally runs dpkg-source -b after staging debian/ + vendor/.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"
OUT_DIR="${MARINAMOJI_SOURCE_OUT:-$REPO_ROOT/dist/debian-source}"
VENDOR_SRC="${MARINAMOJI_VENDOR_SRC:-}"
SKIP_VENDOR=0
WITH_DPKG_SOURCE=0

usage() {
  sed -n '2,14p' "$0" | sed 's/^# //;s/^#//'
  exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-vendor) SKIP_VENDOR=1; shift ;;
    --with-dpkg-source) WITH_DPKG_SOURCE=1; shift ;;
    --vendor-src) VENDOR_SRC="$2"; shift 2 ;;
    --output-dir) OUT_DIR="$2"; shift 2 ;;
    -h|--help) usage 0 ;;
    *) echo "unknown option: $1" >&2; usage 1 ;;
  esac
done

PRODUCT_VERSION="$(tr -d '[:space:]' < "$REPO_ROOT/src/marina_product_version.txt")"
# 0.0.21-rc11 → 0.0.21~rc11
UPSTREAM="${PRODUCT_VERSION/-rc/~rc}"
DS_UPSTREAM="${UPSTREAM}+ds1"
ORIG_NAME="marinamoji_${DS_UPSTREAM}.orig.tar.xz"
VENDOR_NAME="marinamoji_${DS_UPSTREAM}.orig-vendor.tar.xz"

if [[ -z "$VENDOR_SRC" ]]; then
  if [[ -d "$HERE/vendor-linux" ]]; then
    VENDOR_SRC="$HERE/vendor-linux"
  elif [[ -d "$HERE/vendor" ]]; then
    VENDOR_SRC="$HERE/vendor"
  fi
fi

mkdir -p "$OUT_DIR"
STAGE="$(mktemp -d "${TMPDIR:-/tmp}/marinamoji-orig.XXXXXX")"
cleanup() { rm -rf "$STAGE"; }
trap cleanup EXIT

TREE="$STAGE/marinamoji-${DS_UPSTREAM}"
mkdir -p "$TREE"

echo "build_source_package.sh: product=$PRODUCT_VERSION upstream=$DS_UPSTREAM"

# Main orig: upstream tree minus Files-Excluded and packaging caches.
rsync -a \
  --exclude '.git/' \
  --exclude '.github/' \
  --exclude '.cursor/' \
  --exclude '.vscode/' \
  --exclude '.vs/' \
  --exclude '*.code-workspace' \
  --exclude 'packaging/debian/vendor/' \
  --exclude 'packaging/debian/vendor-linux/' \
  --exclude 'dist/' \
  --exclude 'build/' \
  --exclude 'oss/' \
  --exclude 'src/bazel-*' \
  --exclude 'src/third_party/ndk/' \
  --exclude 'src/third_party/qt/' \
  --exclude 'src/third_party/qt_host/' \
  --exclude 'src/third_party/qt_src/' \
  --exclude 'src/third_party/wix/' \
  --exclude 'src/third_party/llvm/' \
  --exclude 'src/third_party/msys64/' \
  --exclude 'src/win32/' \
  --exclude 'src/renderer/win32/' \
  --exclude 'src/gui/tool/mozc_tool.rc' \
  --exclude 'src/server/mozc_server.rc' \
  --exclude 'src/build_tools/mozc_win32_resource_template.rc' \
  --exclude '.DS_Store' \
  "$REPO_ROOT/" "$TREE/"

# Drop any leftover Windows .rc under win32 if rsync race; directory already gone.
rm -f "$TREE"/src/win32/**/*.rc 2>/dev/null || true

echo "build_source_package.sh: writing $OUT_DIR/$ORIG_NAME"
tar -C "$STAGE" -cJf "$OUT_DIR/$ORIG_NAME" "marinamoji-${DS_UPSTREAM}"
ls -lh "$OUT_DIR/$ORIG_NAME"

if [[ "$SKIP_VENDOR" -eq 0 ]]; then
  if [[ -z "$VENDOR_SRC" || ! -d "$VENDOR_SRC" ]]; then
    echo "build_source_package.sh: no vendor tree; run offline/vendor_deps.sh on Linux or pass --vendor-src / --skip-vendor" >&2
    exit 1
  fi
  echo "build_source_package.sh: writing $OUT_DIR/$VENDOR_NAME from $VENDOR_SRC"
  # Component name "vendor" → unpacks as ./vendor/ beside the orig root contents
  # when using dpkg-source with multiple upstream tarballs. We pack the
  # *contents* under a top-level vendor/ directory for clarity when extracted
  # alone; dpkg-source expects the component tarball's top-level dir to match
  # the component name.
  VSTAGE="$STAGE/vendor-pack"
  mkdir -p "$VSTAGE/vendor"
  rsync -a "$VENDOR_SRC/" "$VSTAGE/vendor/"
  tar -C "$VSTAGE" -cJf "$OUT_DIR/$VENDOR_NAME" vendor
  ls -lh "$OUT_DIR/$VENDOR_NAME"
fi

if [[ "$WITH_DPKG_SOURCE" -eq 1 ]]; then
  if ! command -v dpkg-source >/dev/null 2>&1; then
    echo "build_source_package.sh: dpkg-source not found (install dpkg-dev)" >&2
    exit 1
  fi
  BUILD="$STAGE/build"
  mkdir -p "$BUILD"
  tar -C "$BUILD" -xJf "$OUT_DIR/$ORIG_NAME"
  SRC_DIR="$BUILD/marinamoji-${DS_UPSTREAM}"
  if [[ "$SKIP_VENDOR" -eq 0 ]]; then
    tar -C "$SRC_DIR" -xJf "$OUT_DIR/$VENDOR_NAME"
  fi
  # Place packaging/debian as debian/ for dpkg-source.
  rm -rf "$SRC_DIR/debian"
  mkdir -p "$SRC_DIR/debian"
  # Copy packaging files but not local vendor caches.
  rsync -a \
    --exclude vendor/ \
    --exclude vendor-linux/ \
    "$HERE/" "$SRC_DIR/debian/"
  # Changelog version must match +ds1 for the source package.
  # Keep UNRELEASED; rewrite the first line's version if needed.
  if ! head -1 "$SRC_DIR/debian/changelog" | grep -q "+ds1"; then
    sed -i.bak -E "1s/\\([^)]*\\)/(${DS_UPSTREAM}-1)/" "$SRC_DIR/debian/changelog"
    rm -f "$SRC_DIR/debian/changelog.bak"
  fi
  echo "build_source_package.sh: dpkg-source -b"
  (cd "$BUILD" && dpkg-source -b "marinamoji-${DS_UPSTREAM}")
  mv "$BUILD"/marinamoji_*.dsc "$BUILD"/marinamoji_*.debian.tar.* "$OUT_DIR/" 2>/dev/null || true
  # orig tarballs already in OUT_DIR; dpkg-source may expect them alongside
  ls -la "$OUT_DIR"
fi

echo "build_source_package.sh: done"
