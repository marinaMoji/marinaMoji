#!/bin/bash
# Copy Homebrew/third-party dylibs and missing Qt frameworks into ConfigDialog.app
# so Qt GUI tools run on Macs without Homebrew.
set -euo pipefail

APP="${1:-}"
QT_PREFIX="${2:-${MOZC_QT_PATH:-/opt/homebrew/opt/qt}/lib}"

if [[ -z "$APP" || ! -d "$APP" ]]; then
  echo "Usage: bundle_qt_deps.sh /path/to/ConfigDialog.app [qt_lib_dir]" >&2
  exit 1
fi

FW_DIR="${APP}/Contents/Frameworks"
mkdir -p "$FW_DIR"

is_mach_o() {
  file "$1" 2>/dev/null | grep -q 'Mach-O'
}

link_prefix_for() {
  local bin="$1"
  if [[ "$bin" == *".framework/Versions/"*"/"* ]]; then
    echo "@loader_path/../../../"
  elif [[ "$bin" == "${FW_DIR}/"* ]]; then
    echo "@loader_path/"
  else
    echo "@executable_path/../Frameworks/"
  fi
}

is_external_lib() {
  case "$1" in
    /opt/homebrew/*|/usr/local/*) ;;
    *) return 1 ;;
  esac
  if [[ "$1" == *".framework"* ]]; then
    return 1
  fi
  return 0
}

framework_name_for_ref() {
  local ref="$1"
  if [[ "$ref" != *".framework"* ]]; then
    return 1
  fi
  if [[ "$ref" =~ /([^/]+)\.framework/ ]]; then
    echo "${BASH_REMATCH[1]}"
    return 0
  fi
  return 1
}

copy_qt_framework_if_needed() {
  local fw_name="$1"
  local dest="${FW_DIR}/${fw_name}.framework"
  if [[ -d "$dest" ]]; then
    return 0
  fi
  local src_real=""
  for base in "$QT_PREFIX" "/opt/homebrew/opt/qtbase/lib" "/opt/homebrew/opt/qt/lib"; do
    if [[ -d "${base}/${fw_name}.framework" ]]; then
      src_real="$(realpath "${base}/${fw_name}.framework")"
      break
    fi
  done
  if [[ -z "$src_real" || ! -d "$src_real" ]]; then
    echo "WARNING: ${fw_name}.framework not found under Homebrew Qt" >&2
    return 1
  fi
  echo "Copying ${fw_name}.framework..."
  # Homebrew Qt frameworks are often symlinks; ditto copies the real bundle.
  /usr/bin/ditto "$src_real" "$dest"
  return 0
}

ensure_qt_frameworks() {
  local copied=1
  while (( copied )); do
    copied=0
    local bin dep fw
    while IFS= read -r -d '' bin; do
      while IFS= read -r dep; do
        [[ -n "$dep" ]] || continue
        if [[ "$dep" == @rpath/*".framework"* ]] ||
           [[ "$dep" == /opt/homebrew/*".framework"* ]] ||
           [[ "$dep" == /usr/local/*".framework"* ]]; then
          fw="$(framework_name_for_ref "$dep")" || continue
          [[ "$fw" == Qt* ]] || continue
          if [[ ! -d "${FW_DIR}/${fw}.framework" ]]; then
            if copy_qt_framework_if_needed "$fw" &&
               [[ -d "${FW_DIR}/${fw}.framework" ]]; then
              copied=1
            fi
          fi
        fi
      done < <(otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}')
    done < <(find "$APP" -type f -print0 | while IFS= read -r -d '' f; do
      is_mach_o "$f" && printf '%s\0' "$f"
    done)
  done
}

bundle_external_dylibs() {
  local changed=1
  local pass=0
  while (( changed )) && (( pass < 50 )); do
    changed=0
    pass=$((pass + 1))
    while IFS= read -r -d '' bin; do
      local prefix
      prefix="$(link_prefix_for "$bin")"
      while IFS= read -r dep; do
        [[ -n "$dep" ]] || continue
        if ! is_external_lib "$dep"; then
          continue
        fi
        local base dest src
        base="$(basename "$dep")"
        dest="${FW_DIR}/${base}"
        if [[ ! -f "$dest" ]]; then
          src="$(realpath "$dep" 2>/dev/null || true)"
          if [[ -z "$src" || ! -f "$src" ]]; then
            echo "WARNING: cannot resolve ${dep}" >&2
            continue
          fi
          echo "Bundling ${base}..."
          cp -f "$src" "$dest"
          chmod 755 "$dest"
          changed=1
        fi
        local new="${prefix}${base}"
        if [[ "$dep" != "$new" ]]; then
          if install_name_tool -change "$dep" "$new" "$bin" 2>/dev/null; then
            changed=1
          fi
        fi
      done < <(otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}')
    done < <(find "$APP" -type f -print0 | while IFS= read -r -d '' f; do
      is_mach_o "$f" && printf '%s\0' "$f"
    done)
  done
}

echo "Ensuring Qt frameworks referenced via @rpath..."
ensure_qt_frameworks

echo "Bundling third-party dylibs into Frameworks..."
bundle_external_dylibs

fix_bundled_dylib_ids() {
  local dylib base
  for dylib in "${FW_DIR}"/*.dylib; do
    [[ -f "$dylib" ]] || continue
    base="$(basename "$dylib")"
    install_name_tool -id "@loader_path/${base}" "$dylib" 2>/dev/null || true
  done
}

echo "Fixing bundled dylib install names..."
fix_bundled_dylib_ids
bundle_external_dylibs

echo "Done bundling Qt dependencies for ${APP}."
