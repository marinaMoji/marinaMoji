#!/bin/bash
# Rewire Qt GUI tools to bundled frameworks (not /opt/homebrew/...).
# Required for Preferences, Dictionary Tool, etc. on machines without Homebrew Qt.
#
# Usage:
#   fix_qt_bundled_paths.sh /path/to/marinaMoji.app [codesign_identity]
#
# When codesign_identity is given (use "-" for ad-hoc), re-signs Qt bundles after
# install_name_tool so macOS will launch them on a clean machine.
set -euo pipefail

APP="${1:-/Library/Input Methods/marinaMoji.app}"
SIGN_IDENTITY="${2:-}"

QT_FRAMEWORKS=(QtCore QtGui QtPrintSupport QtWidgets)

discover_qt_frameworks() {
  local fw_dir="$1"
  local fw
  QT_FRAMEWORKS=()
  for fw in "${fw_dir}"/*.framework; do
    [[ -d "$fw" ]] || continue
    QT_FRAMEWORKS+=("$(basename "$fw" .framework)")
  done
}

framework_for_ref() {
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

is_qt_dylib_ref() {
  [[ "$1" == *".framework"* ]] && [[ "$1" == *"/Qt"* || "$1" == *"qtbase"* || "$1" == *"qt/"* ]]
}

has_rpath() {
  local bin="$1"
  local rpath="$2"
  otool -l "$bin" 2>/dev/null | awk -v want="$rpath" '
    /cmd LC_RPATH/ { getline; if ($2 == want) found=1 }
    END { exit !found }
  '
}

add_rpath_if_missing() {
  local bin="$1"
  local rpath="$2"
  if [[ ! -f "$bin" ]]; then
    return 0
  fi
  if has_rpath "$bin" "$rpath"; then
    return 0
  fi
  install_name_tool -add_rpath "$rpath" "$bin" 2>/dev/null || true
}

fix_qt_refs_in_binary() {
  local bin="$1"
  local prefix="$2"
  if [[ ! -f "$bin" ]]; then
    return 0
  fi
  local old fw new
  while IFS= read -r old; do
    [[ -n "$old" ]] || continue
    if ! is_qt_dylib_ref "$old"; then
      continue
    fi
    fw="$(framework_for_ref "$old")" || continue
    new="${prefix}/${fw}.framework/Versions/A/${fw}"
    if [[ "$old" == "$new" ]]; then
      continue
    fi
    install_name_tool -change "$old" "$new" "$bin" 2>/dev/null || true
  done < <(otool -L "$bin" 2>/dev/null | awk 'NR>1 {print $1}')
}

fix_frameworks_in_dir() {
  local fw_dir="$1"
  local fw fw_bin
  discover_qt_frameworks "$fw_dir"
  for fw in "${QT_FRAMEWORKS[@]}"; do
    fw_bin="${fw_dir}/${fw}.framework/Versions/A/${fw}"
    if [[ ! -f "$fw_bin" ]]; then
      continue
    fi
    install_name_tool -id "@rpath/${fw}.framework/Versions/A/${fw}" "$fw_bin"
    add_rpath_if_missing "$fw_bin" "@loader_path/../../.."
    fix_qt_refs_in_binary "$fw_bin" "@rpath"
  done
}

fix_plugins_in_app() {
  local qt_app="$1"
  local fw_prefix="$2"
  shift 2
  local extra_rpaths=("$@")
  local plugins_dir="${qt_app}/Contents/Resources/plugins"
  if [[ ! -d "$plugins_dir" ]]; then
    return 0
  fi
  local plugin rpath
  find "$plugins_dir" -name "*.dylib" -print0 | while IFS= read -r -d '' plugin; do
    fix_qt_refs_in_binary "$plugin" "$fw_prefix"
    add_rpath_if_missing "$plugin" "$fw_prefix"
    if ((${#extra_rpaths[@]})); then
      for rpath in "${extra_rpaths[@]}"; do
        add_rpath_if_missing "$plugin" "$rpath"
      done
    fi
  done
}

fix_config_dialog_app() {
  local config_app="$1"
  local fw_dir="${config_app}/Contents/Frameworks"
  local bin="${config_app}/Contents/MacOS/ConfigDialog"

  if [[ ! -d "$fw_dir" ]]; then
    echo "ERROR: bundled Qt frameworks not found under ConfigDialog.app" >&2
    exit 1
  fi

  discover_qt_frameworks "$fw_dir"

  echo "Fixing Qt frameworks in ConfigDialog.app..."
  fix_frameworks_in_dir "$fw_dir"

  echo "Fixing ConfigDialog binary..."
  fix_qt_refs_in_binary "$bin" "@executable_path/../Frameworks"
  add_rpath_if_missing "$bin" "@executable_path/../Frameworks"

  echo "Fixing ConfigDialog Qt plugins..."
  fix_plugins_in_app "$config_app" "@executable_path/../Frameworks"
}

fix_nested_qt_app() {
  local qt_app="$1"
  local host_fw="$2"
  local app_name
  app_name="$(basename "$qt_app" .app)"
  local bin="${qt_app}/Contents/MacOS/${app_name}"
  local fw_prefix="@executable_path/${host_fw}"
  echo "Fixing ${app_name}..."
  fix_qt_refs_in_binary "$bin" "$fw_prefix"
  fix_plugins_in_app "$qt_app" "$fw_prefix" \
    "@loader_path/../../../../../ConfigDialog.app/Contents/Frameworks"
}

codesign_path() {
  local path="$1"
  if [[ -z "$SIGN_IDENTITY" || ! -e "$path" ]]; then
    return 0
  fi
  echo "Codesigning ${path}..."
  /usr/bin/codesign --force --sign "$SIGN_IDENTITY" "$path" 2>/dev/null || true
}

remove_stale_signatures() {
  local root="$1"
  find "$root" -name "_CodeSignature" -print0 2>/dev/null |
    while IFS= read -r -d '' item; do
      rm -rf "$item"
    done
}

codesign_qt_apps() {
  if [[ -z "$SIGN_IDENTITY" ]]; then
    return 0
  fi

  local resources="${APP}/Contents/Resources"
  local fw_dir="${CONFIG_APP}/Contents/Frameworks"

  echo "Removing stale code signatures after install_name_tool..."
  remove_stale_signatures "$CONFIG_APP"

  if [[ -d "$fw_dir" ]]; then
    echo "Codesigning bundled dylibs and Qt frameworks..."
    local item fw fw_bin
    for item in "$fw_dir"/*.dylib; do
      [[ -f "$item" ]] || continue
      codesign_path "$item"
    done
    for fw in "$fw_dir"/*.framework; do
      [[ -d "$fw" ]] || continue
      fw_bin="${fw}/Versions/A/$(basename "$fw" .framework)"
      if [[ -f "$fw_bin" ]]; then
        codesign_path "$fw_bin"
      fi
      codesign_path "$fw"
    done
  fi

  find "$resources" -path "*/Contents/Resources/plugins/*.dylib" -print0 2>/dev/null |
    while IFS= read -r -d '' item; do
      codesign_path "$item"
    done

  find "$resources" -name "*.app" -depth -print | while read -r item; do
    codesign_path "$item"
  done
}

if [[ ! -d "$APP" ]]; then
  echo "ERROR: marinaMoji.app not found at: $APP" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_APP="${APP}/Contents/Resources/ConfigDialog.app"
if [[ -f "${SCRIPT_DIR}/bundle_qt_deps.sh" ]]; then
  echo "Bundling Qt third-party dependencies into ConfigDialog.app..."
  /bin/bash "${SCRIPT_DIR}/bundle_qt_deps.sh" "$CONFIG_APP"
fi

fix_config_dialog_app "$CONFIG_APP"

HOST_FW="../../../ConfigDialog.app/Contents/Frameworks"
for app in AboutDialog DictionaryTool ErrorMessageDialog WordRegisterDialog; do
  fix_nested_qt_app "${APP}/Contents/Resources/${app}.app" "$HOST_FW"
done

for prelauncher in "${APP}/Contents/Resources/"*Prelauncher.app; do
  [[ -d "$prelauncher" ]] || continue
  fix_nested_qt_app "$prelauncher" "$HOST_FW"
done

codesign_qt_apps

echo "Done. Qt tools should run without Homebrew Qt installed."
