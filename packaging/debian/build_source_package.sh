#!/usr/bin/env bash
# Build Debian orig (+ optional vendor component) tarballs for marinaMoji.
#
# Usage (repo root):
#   bash packaging/debian/build_source_package.sh
#   bash packaging/debian/build_source_package.sh --skip-vendor
#   bash packaging/debian/build_source_package.sh --with-dpkg-source
#   bash packaging/debian/build_source_package.sh --ref v0.0.21-rc11
#
# The main orig tarball is produced with `git archive`, so it contains only
# tracked files (no local caches, credentials or build output), minus the
# paths listed in Files-Excluded in packaging/debian/copyright. That header
# is the single source of truth for the DFSG repack.
#
# Reads the product version from src/marina_product_version.txt (at --ref)
# and writes, under dist/debian-source/ (override with --output-dir):
#   marinamoji_${UPSTREAM}+ds1.orig.tar.xz
#   marinamoji_${UPSTREAM}+ds1.orig-vendor.tar.xz     (unless --skip-vendor)
# With --with-dpkg-source it also builds the .dsc and debian tarball.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"
OUT_DIR="${MARINAMOJI_SOURCE_OUT:-$REPO_ROOT/dist/debian-source}"
VENDOR_SRC="${MARINAMOJI_VENDOR_SRC:-}"
REF="HEAD"
SKIP_VENDOR=0
WITH_DPKG_SOURCE=0

usage() {
  sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-vendor) SKIP_VENDOR=1; shift ;;
    --with-dpkg-source) WITH_DPKG_SOURCE=1; shift ;;
    --vendor-src) VENDOR_SRC="$2"; shift 2 ;;
    --output-dir) OUT_DIR="$2"; shift 2 ;;
    --ref) REF="$2"; shift 2 ;;
    -h|--help) usage 0 ;;
    *) echo "unknown option: $1" >&2; usage 1 ;;
  esac
done

# macOS bsdtar stores extended attributes as AppleDouble "._*" entries,
# which pollute the tarballs. Disable that and prefer GNU tar.
export COPYFILE_DISABLE=1
if command -v gtar >/dev/null 2>&1; then
  TAR=gtar
else
  TAR=tar
fi
TAR_CREATE=("$TAR")
if "$TAR" --version 2>/dev/null | grep -q 'GNU tar'; then
  TAR_CREATE+=(--owner=0 --group=0 --numeric-owner --sort=name)
fi

cd "$REPO_ROOT"
git rev-parse --verify --quiet "$REF^{commit}" >/dev/null \
  || { echo "build_source_package.sh: unknown git ref: $REF" >&2; exit 1; }

if [[ "$REF" == "HEAD" && -n "$(git status --porcelain --untracked-files=no)" ]]; then
  echo "build_source_package.sh: WARNING: uncommitted changes are NOT in the" >&2
  echo "build_source_package.sh:          tarball (git archive uses committed state)." >&2
fi

PRODUCT_VERSION="$(git show "$REF:src/marina_product_version.txt" | tr -d '[:space:]')"
# 0.0.21-rc11 → 0.0.21~rc11
UPSTREAM="${PRODUCT_VERSION/-rc/~rc}"
DS_UPSTREAM="${UPSTREAM}+ds1"
ORIG_NAME="marinamoji_${DS_UPSTREAM}.orig.tar.xz"
VENDOR_NAME="marinamoji_${DS_UPSTREAM}.orig-vendor.tar.xz"
TOPDIR="marinamoji-${DS_UPSTREAM}"

if [[ -z "$VENDOR_SRC" ]]; then
  if [[ -d "$HERE/vendor-linux" ]]; then
    VENDOR_SRC="$HERE/vendor-linux"
  elif [[ -d "$HERE/vendor" ]]; then
    VENDOR_SRC="$HERE/vendor"
  fi
fi

mkdir -p "$OUT_DIR"
STAGE="$(mktemp -d "${TMPDIR:-/tmp}/marinamoji-orig.XXXXXX")"
trap 'rm -rf "$STAGE"' EXIT

echo "build_source_package.sh: product=$PRODUCT_VERSION upstream=$DS_UPSTREAM ref=$REF"

# Files-Excluded paths from the copyright header paragraph (continuation
# lines of the field, i.e. lines starting with whitespace).
EXCLUDES=()
while IFS= read -r line; do
  EXCLUDES+=("$line")
done < <(git show "$REF:packaging/debian/copyright" | awk '
  /^Files-Excluded:/ { in_f = 1; next }
  in_f && /^[[:space:]]+[^[:space:]]/ { gsub(/^[[:space:]]+|[[:space:]]+$/, ""); print; next }
  in_f { exit }')
if [[ ${#EXCLUDES[@]} -eq 0 ]]; then
  echo "build_source_package.sh: no Files-Excluded found in debian/copyright" >&2
  exit 1
fi

mkdir -p "$STAGE/tree"
git archive --format=tar --prefix="${TOPDIR}/" "$REF" | tar -C "$STAGE/tree" -xf -
for path in "${EXCLUDES[@]}"; do
  if [[ -e "$STAGE/tree/${TOPDIR}/$path" ]]; then
    echo "build_source_package.sh: Files-Excluded: $path"
    rm -rf "${STAGE:?}/tree/${TOPDIR}/$path"
  else
    echo "build_source_package.sh: WARNING: Files-Excluded path not in tree: $path" >&2
  fi
done

echo "build_source_package.sh: writing $OUT_DIR/$ORIG_NAME"
"${TAR_CREATE[@]}" -C "$STAGE/tree" -cf - "$TOPDIR" | xz -T"${MARINAMOJI_XZ_THREADS:-0}" -6 > "$OUT_DIR/$ORIG_NAME"
ls -lh "$OUT_DIR/$ORIG_NAME"

if [[ "$SKIP_VENDOR" -eq 0 ]]; then
  if [[ -z "$VENDOR_SRC" || ! -d "$VENDOR_SRC" ]]; then
    echo "build_source_package.sh: no vendor tree; run offline/vendor_deps.sh on Linux or pass --vendor-src / --skip-vendor" >&2
    exit 1
  fi
  echo "build_source_package.sh: writing $OUT_DIR/$VENDOR_NAME from $VENDOR_SRC"
  # dpkg-source expects a component tarball's single top-level directory to
  # be named after the component ("vendor").
  mkdir -p "$STAGE/vendor-pack"
  cp -a "$VENDOR_SRC" "$STAGE/vendor-pack/vendor"
  # The lockfile is gitignored, so it is not in the orig; --lockfile_mode=error
  # needs it. vendor_deps.sh already stores it in the vendor tree; fall back
  # to the local copy for older trees.
  if [[ ! -f "$STAGE/vendor-pack/vendor/MODULE.bazel.lock" ]]; then
    if [[ -f "$REPO_ROOT/src/MODULE.bazel.lock" ]]; then
      cp "$REPO_ROOT/src/MODULE.bazel.lock" "$STAGE/vendor-pack/vendor/MODULE.bazel.lock"
    else
      echo "build_source_package.sh: no MODULE.bazel.lock in vendor tree or src/" >&2
      exit 1
    fi
  fi
  "${TAR_CREATE[@]}" -C "$STAGE/vendor-pack" -cf - vendor | xz -T"${MARINAMOJI_XZ_THREADS:-0}" -6 > "$OUT_DIR/$VENDOR_NAME"
  ls -lh "$OUT_DIR/$VENDOR_NAME"
fi

if [[ "$WITH_DPKG_SOURCE" -eq 1 ]]; then
  if ! command -v dpkg-source >/dev/null 2>&1; then
    echo "build_source_package.sh: dpkg-source not found (install dpkg-dev)" >&2
    exit 1
  fi
  # dpkg-source -b looks for the orig tarballs next to the source directory.
  BUILD="$STAGE/build"
  mkdir -p "$BUILD"
  cp "$OUT_DIR/$ORIG_NAME" "$BUILD/"
  tar -C "$BUILD" -xJf "$BUILD/$ORIG_NAME"
  SRC_DIR="$BUILD/$TOPDIR"
  if [[ "$SKIP_VENDOR" -eq 0 ]]; then
    cp "$OUT_DIR/$VENDOR_NAME" "$BUILD/"
    tar -C "$SRC_DIR" -xJf "$BUILD/$VENDOR_NAME"
  fi
  # packaging/debian becomes debian/ (tracked files only).
  mkdir -p "$SRC_DIR/debian"
  git archive --format=tar "$REF" packaging/debian \
    | tar -C "$STAGE" -xf -
  cp -a "$STAGE/packaging/debian/." "$SRC_DIR/debian/"
  if ! head -1 "$SRC_DIR/debian/changelog" | grep -qF "(${DS_UPSTREAM}-"; then
    echo "build_source_package.sh: debian/changelog's top entry is not for ${DS_UPSTREAM}-N" >&2
    head -1 "$SRC_DIR/debian/changelog" >&2
    exit 1
  fi
  echo "build_source_package.sh: dpkg-source -b"
  (cd "$BUILD" && dpkg-source -b "$TOPDIR")
  cp "$BUILD"/marinamoji_*.dsc "$BUILD"/marinamoji_*.debian.tar.* "$OUT_DIR/"
  ls -la "$OUT_DIR"
fi

echo "build_source_package.sh: done"
