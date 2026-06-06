#!/bin/bash
# Remove marinaMoji from the system: app bundle, LaunchAgents, TIS registration,
# and org.mozc entries in HIToolbox / inputsources plists.
#
# Use standalone before re-install, or via reset_hitoolbox_for_marinamoji.sh --full.
set -euo pipefail

STAMP="$(date +%Y%m%d-%H%M%S)"
DESKTOP="${HOME}/Desktop"
PREFS="${HOME}/Library/Preferences"
HIT="${PREFS}/com.apple.HIToolbox.plist"
INPUTSRC="${PREFS}/com.apple.inputsources.plist"
APP_DST="/Library/Input Methods/marinaMoji.app"
MM_SUPPORT="${HOME}/Library/Application Support/marinaMoji"
MM_OLD="${HOME}/Library/Application Support/marinaMozc"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"

YES=0
BACKUP_SUPPORT=1
for arg in "$@"; do
  case "${arg}" in
    --yes|-y) YES=1 ;;
    --no-backup) BACKUP_SUPPORT=0 ;;
  esac
done

mozc_count() {
  {
    defaults read com.apple.inputsources 2>/dev/null || true
    defaults read com.apple.HIToolbox 2>/dev/null || true
    [[ -d "${APP_DST}" ]] && echo "APP_PRESENT"
  } | rg -c 'org\.mozc\.inputmethod\.Japanese' || true
}

app_present() {
  [[ -d "${APP_DST}" ]]
}

echo "=== Scrub marinaMoji from system (${STAMP}) ==="
echo
echo "This will DELETE:"
echo "  • /Library/Input Methods/marinaMoji.app"
echo "  • org.mozc LaunchAgents (system + user)"
echo "  • org.mozc rows in HIToolbox / inputsources plists"
echo "  • TIS registration for org.mozc.inputmethod.Japanese (disable + delete app + scrub plists)"
if [[ "${BACKUP_SUPPORT}" -eq 1 ]]; then
  echo "  • (Application Support is moved to Desktop backup, not deleted)"
fi
echo
echo "Before continuing, remove every marinaMoji row in"
echo "System Settings → Keyboard → Input Sources (all of them)."
echo
if [[ "${YES}" -eq 0 ]]; then
  read -r -p "Done? Press Enter to continue (Ctrl-C to abort)..."
fi

stop_processes() {
  killall marinaMoji 2>/dev/null || true
  killall marinaMojiConverter 2>/dev/null || true
  killall marinaMojiRenderer 2>/dev/null || true
  killall marinaMojiSync 2>/dev/null || true
  killall imklaunchagent 2>/dev/null || true
  killall TextInputMenuAgent 2>/dev/null || true
  killall cfprefsd 2>/dev/null || true
}

unload_and_remove_launchagents() {
  local gui="gui/$(id -u)"
  local plist label
  for label in org.mozc.inputmethod.Japanese.Converter \
               org.mozc.inputmethod.Japanese.Renderer \
               org.mozc.inputmethod.Japanese.Sync; do
    plist="${HOME}/Library/LaunchAgents/${label}.plist"
    if [[ -f "${plist}" ]]; then
      launchctl bootout "${gui}" "${plist}" 2>/dev/null || true
      rm -f "${plist}"
      echo "  removed user LaunchAgent ${label}"
    fi
  done
  for plist in \
    /Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist \
    /Library/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist \
    /Library/LaunchAgents/org.mozc.inputmethod.Japanese.Sync.plist; do
    if [[ -f "${plist}" ]]; then
      sudo launchctl bootout "system/${plist}" 2>/dev/null || true
      sudo rm -f "${plist}"
      echo "  removed system LaunchAgent $(basename "${plist}")"
    fi
  done
}

disable_tis_sources() {
  echo "Disabling org.mozc via Text Input Services..."
  local found
  found="$(swift - <<'SWIFT'
import Carbon
import Foundation

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
    guard id.hasPrefix("org.mozc.inputmethod.Japanese") else { continue }
    count += 1
    let status = TISDisableInputSource(src)
    fputs("  TISDisableInputSource \(id): \(status)\n", stderr)
}
print(count)
SWIFT
)"
  echo "  disabled ${found} org.mozc source(s) (macOS has no TISUnregister API — app removal + plist scrub does the rest)"
}

remove_app_bundle() {
  local path backup
  for path in \
    "${APP_DST}" \
    "/Applications/marinaMoji" \
    "/Applications/marinaMoji.app" \
    "/Applications/UninstallmarinaMoji.app"; do
    if [[ -e "${path}" ]]; then
      if [[ "${path}" == "${APP_DST}" ]]; then
        backup="${DESKTOP}/marinaMoji.app.removed.${STAMP}"
        echo "  moving ${path} → ${backup} (sudo)"
        sudo mv "${path}" "${backup}"
        "${LSREGISTER}" -u "${backup}" 2>/dev/null || true
      else
        echo "  removing ${path} (sudo)"
        sudo rm -rf "${path}"
      fi
    fi
  done
  # Leftover partial-reset copies on Desktop must not sit in Input Methods paths only;
  # also unregister any mdfind hits outside expected install dir.
  while IFS= read -r extra; do
    [[ -z "${extra}" ]] && continue
    [[ "${extra}" == "${APP_DST}" ]] && continue
    [[ "${extra}" == "${DESKTOP}/"* ]] && continue
    echo "  extra bundle found: ${extra}"
    "${LSREGISTER}" -u "${extra}" 2>/dev/null || true
  done < <(mdfind 'kMDItemCFBundleIdentifier == "org.mozc.inputmethod.Japanese"' 2>/dev/null || true)
}

scrub_mozc_from_plists() {
  echo "Scrubbing org.mozc entries from preference plists..."
  killall cfprefsd 2>/dev/null || true

  # inputsources is protected from direct plist writes on recent macOS — delete the domain.
  if [[ -f "${INPUTSRC}" ]] || defaults read com.apple.inputsources >/dev/null 2>&1; then
    defaults delete com.apple.inputsources 2>/dev/null || true
    rm -f "${INPUTSRC}" 2>/dev/null || true
    echo "  deleted com.apple.inputsources domain"
  else
    echo "  com.apple.inputsources not present"
  fi

  if [[ ! -f "${HIT}" ]]; then
    echo "  com.apple.HIToolbox.plist not present"
    return
  fi

  python3 <<'PY'
import copy
import os
import plistlib

HOME = os.environ["HOME"]
HIT = os.path.join(HOME, "Library", "Preferences", "com.apple.HIToolbox.plist")
MOZC = "org.mozc.inputmethod.Japanese"

def is_mozc_entry(item):
    if not isinstance(item, dict):
        return False
    bundle = item.get("Bundle ID") or item.get("BundleID")
    if bundle == MOZC:
        return True
    tis = item.get("TISInputSourceID")
    return isinstance(tis, str) and tis.startswith(MOZC)

if not os.path.isfile(HIT):
    print("  com.apple.HIToolbox.plist not present")
    raise SystemExit(0)

with open(HIT, "rb") as f:
    data = plistlib.load(f)
original = copy.deepcopy(data)

if isinstance(data, dict):
    for key, value in list(data.items()):
        if isinstance(value, list):
            data[key] = [x for x in value if not is_mozc_entry(x)]
        elif isinstance(value, dict) and is_mozc_entry(value):
            del data[key]

if data == original:
    print("  no org.mozc entries in com.apple.HIToolbox.plist")
    raise SystemExit(0)

try:
    with open(HIT, "wb") as f:
        plistlib.dump(data, f)
    print("  scrubbed com.apple.HIToolbox.plist")
except PermissionError:
    print("  WARN: cannot write HIToolbox.plist (use reset --full to wipe Input Sources)")
    raise SystemExit(1)
PY
}

backup_support_dirs() {
  if [[ "${BACKUP_SUPPORT}" -eq 0 ]]; then
    return
  fi
  if [[ -d "${MM_SUPPORT}" ]]; then
    local backup="${DESKTOP}/marinaMoji.support.backup.${STAMP}"
    mv "${MM_SUPPORT}" "${backup}"
    echo "  moved marinaMoji support → ${backup}"
  fi
  if [[ -d "${MM_OLD}" ]]; then
    local backup_old="${DESKTOP}/marinaMozc.support.backup.${STAMP}"
    mv "${MM_OLD}" "${backup_old}"
    echo "  moved marinaMozc support → ${backup_old}"
  fi
}

remove_mozc_prefs() {
  rm -f "${PREFS}"/org.mozc.inputmethod.Japanese*.plist 2>/dev/null || true
  echo "  removed org.mozc.* preference plists (if any)"
}

verify_clean() {
  local count errors=0
  echo
  echo "=== Scrub verification ==="
  if app_present; then
    echo "  FAIL: ${APP_DST} still exists"
    errors=$((errors + 1))
  else
    echo "  OK: marinaMoji.app not in Input Methods"
  fi
  count="$(mozc_count | tr -d '[:space:]')"
  if [[ -z "${count}" ]]; then count=0; fi
  if [[ "${count}" -gt 0 ]]; then
    echo "  FAIL: ${count} org.mozc reference(s) still in defaults / app"
    defaults read com.apple.inputsources 2>/dev/null | rg 'org\.mozc' || true
    defaults read com.apple.HIToolbox 2>/dev/null | rg 'org\.mozc' || true
    errors=$((errors + 1))
  else
    echo "  OK: no org.mozc in HIToolbox / inputsources"
  fi
  local hits
  hits="$(mdfind 'kMDItemCFBundleIdentifier == "org.mozc.inputmethod.Japanese"' 2>/dev/null | wc -l | tr -d ' ')"
  if [[ "${hits}" != "0" ]]; then
    echo "  WARN: mdfind still finds ${hits} bundle(s) (may be Desktop backups — OK if not in Input Methods):"
    mdfind 'kMDItemCFBundleIdentifier == "org.mozc.inputmethod.Japanese"' 2>/dev/null || true
  fi
  return "${errors}"
}

echo "Stopping marinaMoji / IMK processes..."
stop_processes
unload_and_remove_launchagents
stop_processes
disable_tis_sources
remove_app_bundle
backup_support_dirs
remove_mozc_prefs
scrub_mozc_from_plists
stop_processes

if ! verify_clean; then
  echo
  echo "Scrub incomplete — log out, log back in, run this script again before reinstall."
  exit 1
fi

echo
echo "marinaMoji scrubbed. Input Methods folder should not contain marinaMoji.app."
echo "After a full reset: log out/in, then bash ./mac/post_reset_marinamoji.sh"
