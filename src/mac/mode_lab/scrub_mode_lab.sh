#!/bin/bash
# Remove ALL Mode Lab input sources (fixes duplicate rows in System Settings).
#
# macOS has no TISUnregister API. Duplicates accumulate in com.apple.inputsources
# when Mode Lab is installed/re-registered repeatedly (especially across logout).
# This script disables TIS sources, removes every ModeLab.app copy, and scrubs
# org.mozc.inputmethod.ModeLab from HIToolbox / inputsources plists.
#
# Does NOT touch marinaMoji (org.mozc.inputmethod.Japanese).
set -euo pipefail

STAMP="$(date +%Y%m%d-%H%M%S)"
DESKTOP="${HOME}/Desktop"
PREFS="${HOME}/Library/Preferences"
HIT="${PREFS}/com.apple.HIToolbox.plist"
INPUTSRC="${PREFS}/com.apple.inputsources.plist"
TIS_PREFIX="org.mozc.inputmethod.ModeLab"
BUNDLE_ID="org.mozc.inputmethod.ModeLab"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"

YES=0
for arg in "$@"; do
  case "${arg}" in
    --yes|-y) YES=1 ;;
  esac
done

modelab_count() {
  {
    defaults read com.apple.inputsources 2>/dev/null || true
    defaults read com.apple.HIToolbox 2>/dev/null || true
  } | rg -c "${TIS_PREFIX}" || true
}

echo "=== Scrub Mode Lab (${STAMP}) ==="
echo
echo "This will:"
echo "  • disable all ${TIS_PREFIX} Text Input Services entries"
echo "  • remove every ModeLab.app on disk (system + user + test copies)"
echo "  • scrub Mode Lab rows from com.apple.inputsources and com.apple.HIToolbox"
echo "  • leave marinaMoji and other input sources untouched"
echo
echo "Before continuing, remove every **Mode Lab** row in"
echo "System Settings → Keyboard → Input Sources (all of them)."
echo
if [[ "${YES}" -eq 0 ]]; then
  read -r -p "Done? Press Enter to continue (Ctrl-C to abort)..."
fi

stop_processes() {
  killall ModeLab imklaunchagent TextInputMenuAgent 2>/dev/null || true
  killall cfprefsd 2>/dev/null || true
}

disable_tis_sources() {
  echo "Disabling Mode Lab via Text Input Services..."
  swift - "${TIS_PREFIX}" <<'SWIFT'
import Carbon
import Foundation

let prefix = CommandLine.arguments[1]

func tisString(_ src: TISInputSource, _ key: CFString) -> String? {
    guard let raw = TISGetInputSourceProperty(src, key) else { return nil }
    return Unmanaged<CFString>.fromOpaque(raw).takeUnretainedValue() as String
}

guard let list = TISCreateInputSourceList(nil, true)?.takeRetainedValue() as? [TISInputSource] else {
    print(0)
    exit(0)
}

var count = 0
for src in list {
    guard let id = tisString(src, kTISPropertyInputSourceID) else { continue }
    guard id.hasPrefix(prefix) else { continue }
    count += 1
    let status = TISDisableInputSource(src)
    fputs("  TISDisableInputSource \(id): \(status)\n", stderr)
}
print(count)
SWIFT
}

remove_app_bundles() {
  echo "Removing ModeLab.app bundles..."
  local path
  while IFS= read -r path; do
    [[ -z "${path}" ]] && continue
    echo "  unregister ${path}"
    "${LSREGISTER}" -u "${path}" 2>/dev/null || true
    if [[ -w "${path}" ]] || [[ -w "$(dirname "${path}")" ]]; then
      rm -rf "${path}"
    else
      sudo rm -rf "${path}"
    fi
  done < <(mdfind "kMDItemCFBundleIdentifier == '${BUNDLE_ID}'" 2>/dev/null || true)

  for path in \
    "/Library/Input Methods/ModeLab.app" \
    "${HOME}/Library/Input Methods/ModeLab.app" \
    "${HOME}/Library/Input Methods/ModeLab-test.app"; do
    if [[ -e "${path}" ]]; then
      echo "  removing ${path}"
      "${LSREGISTER}" -u "${path}" 2>/dev/null || true
      if [[ -w "${path}" ]] || [[ -w "$(dirname "${path}")" ]]; then
        rm -rf "${path}"
      else
        sudo rm -rf "${path}"
      fi
    fi
  done

  if [[ -d "/Applications/ModeLabHost.app" ]]; then
    echo "  removing /Applications/ModeLabHost.app"
    sudo rm -rf "/Applications/ModeLabHost.app" 2>/dev/null || rm -rf "/Applications/ModeLabHost.app"
  fi
}

scrub_domain() {
  local domain="$1"
  local label="$2"
  local tmpdir
  tmpdir="$(mktemp -d)"

  if ! defaults read "${domain}" >/dev/null 2>&1; then
    echo "  ${label} not present"
    rm -rf "${tmpdir}"
    return 0
  fi

  local backup="${DESKTOP}/$(echo "${domain}" | tr '.' '_').backup.${STAMP}.plist"
  defaults export "${domain}" "${backup}" 2>/dev/null || true
  echo "  exported ${domain} → ${backup}"

  defaults export "${domain}" "${tmpdir}/scrubbed.plist"
  local py_status=0
  set +e
  TIS_PREFIX="${TIS_PREFIX}" python3 - "${tmpdir}/scrubbed.plist" <<'PY'
import os
import plistlib
import sys

PREFIX = os.environ["TIS_PREFIX"]
path = sys.argv[1]

with open(path, "rb") as f:
    data = plistlib.load(f)

def is_modelab_entry(item):
    if not isinstance(item, dict):
        return False
    bundle = item.get("Bundle ID") or item.get("BundleID")
    if bundle == PREFIX:
        return True
    tis = item.get("TISInputSourceID")
    return isinstance(tis, str) and tis.startswith(PREFIX)

changed = False
if isinstance(data, dict):
    for key, value in list(data.items()):
        if isinstance(value, list):
            filtered = [x for x in value if not is_modelab_entry(x)]
            if len(filtered) != len(value):
                data[key] = filtered
                changed = True
        elif isinstance(value, dict) and is_modelab_entry(value):
            del data[key]
            changed = True

if not changed:
    print("  no Mode Lab entries in export")
    sys.exit(2)

with open(path, "wb") as f:
    plistlib.dump(data, f)
print("  removed Mode Lab entries from export")
PY
  py_status=$?
  set -e
  if [[ "${py_status}" -eq 2 ]]; then
    rm -rf "${tmpdir}"
    return 0
  fi
  if [[ "${py_status}" -ne 0 ]]; then
    rm -rf "${tmpdir}"
    return 1
  fi

  # macOS blocks direct plist writes (PermissionError). Replace via defaults.
  killall cfprefsd 2>/dev/null || true
  defaults delete "${domain}" 2>/dev/null || true
  rm -f "${PREFS}/${domain}.plist" 2>/dev/null || true
  if ! defaults import "${domain}" "${tmpdir}/scrubbed.plist"; then
    echo "  ERROR: defaults import failed for ${domain}" >&2
    echo "  Restore from ${backup} if needed: defaults import ${domain} ${backup}" >&2
    rm -rf "${tmpdir}"
    return 1
  fi
  echo "  re-imported scrubbed ${label}"
  rm -rf "${tmpdir}"
}

scrub_plists() {
  echo "Scrubbing Mode Lab from preference plists..."
  export TIS_PREFIX
  scrub_domain "com.apple.inputsources" "inputsources"
  scrub_domain "com.apple.HIToolbox" "HIToolbox"
  rm -f "${PREFS}"/org.mozc.inputmethod.ModeLab*.plist 2>/dev/null || true
  killall cfprefsd 2>/dev/null || true
}

verify_clean() {
  local count errors=0
  echo
  echo "=== Scrub verification ==="
  count="$(modelab_count | tr -d '[:space:]')"
  if [[ -z "${count}" ]]; then count=0; fi
  if [[ "${count}" -gt 0 ]]; then
    echo "  FAIL: ${count} Mode Lab reference(s) still in defaults"
    defaults read com.apple.inputsources 2>/dev/null | rg "${TIS_PREFIX}" || true
    errors=$((errors + 1))
  else
    echo "  OK: no Mode Lab in inputsources / HIToolbox"
  fi
  local hits
  hits="$(mdfind "kMDItemCFBundleIdentifier == '${BUNDLE_ID}'" 2>/dev/null | wc -l | tr -d ' ')"
  if [[ "${hits}" != "0" ]]; then
    echo "  WARN: mdfind still finds ${hits} bundle(s):"
    mdfind "kMDItemCFBundleIdentifier == '${BUNDLE_ID}'" 2>/dev/null || true
    errors=$((errors + 1))
  else
    echo "  OK: no ModeLab.app on disk"
  fi
  return "${errors}"
}

stop_processes
disable_tis_sources
remove_app_bundles
scrub_plists
stop_processes

if ! verify_clean; then
  echo
  echo "Scrub incomplete. Log out and back in, then run:"
  echo "  bash mac/mode_lab/scrub_mode_lab.sh --yes"
  exit 1
fi

echo
echo "Mode Lab scrubbed. Log out and back in (recommended), then install ONCE:"
echo "  bash mac/mode_lab/install_mode_lab.sh"
echo "Add Mode Lab only once in System Settings, or use:"
echo "  bash mac/mode_lab/activate_mode_lab.sh"
