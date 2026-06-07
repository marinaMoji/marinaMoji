#!/bin/bash
# Full macOS input-source reset for marinaMoji IMK failures.
#
# --full: scrub marinaMoji completely (delete app + registry), wipe all Input
#         Sources prefs, quarantine dpm. marinaMoji must be reinstalled AFTER
#         logout via post_reset_marinamoji.sh — do not leave the app installed
#         across logout or macOS will re-add duplicate rows (e.g. 13 entries).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAMP="$(date +%Y%m%d-%H%M%S)"
DESKTOP="${HOME}/Desktop"
PREFS="${HOME}/Library/Preferences"
HIT="${PREFS}/com.apple.HIToolbox.plist"
INPUTSRC="${PREFS}/com.apple.inputsources.plist"
DPM_BUNDLE="${HOME}/Library/Keyboard Layouts/dpm.bundle"
DPM_OFF="${HOME}/Library/Keyboard Layouts/dpm.bundle.off.${STAMP}"
APP_DST="/Library/Input Methods/marinaMoji.app"

FULL=0
YES=0
for arg in "$@"; do
  case "${arg}" in
    --full) FULL=1 ;;
    --yes|-y) YES=1 ;;
  esac
done

echo "=== marinaMoji input-source reset (${STAMP}) ==="
echo
if [[ "${FULL}" -eq 1 ]]; then
  echo "Mode: FULL — scrub marinaMoji from disk + wipe all Input Sources prefs."
  echo
  echo "⚠️  WARNING: Also deletes U.S./Dvorak/Chinese/emoji palette entries."
  echo "    Backups go to Desktop. marinaMoji.app will be REMOVED (not left installed)."
else
  echo "Mode: light — HIToolbox / inputsources wipe only (app stays unless you run scrub)."
  echo "      For IMK failures, prefer: bash ./mac/reset_hitoolbox_for_marinamoji.sh --full"
fi
echo
if [[ "${YES}" -eq 0 ]]; then
  read -r -p "Press Enter to continue (Ctrl-C to abort)..."
fi

export_domain() {
  local domain="$1"
  local outfile="${DESKTOP}/${domain//./_}.backup.${STAMP}.plist"
  if defaults read "${domain}" >/dev/null 2>&1; then
    defaults export "${domain}" "${outfile}" || true
    echo "  exported ${domain} → ${outfile}"
  else
    echo "  (${domain} not present — skip export)"
  fi
}

delete_domain() {
  local domain="$1"
  defaults delete "${domain}" 2>/dev/null || true
  echo "  deleted domain ${domain}"
}

remove_plist_if_present() {
  local path="$1"
  if [[ -f "${path}" ]]; then
    rm -f "${path}" && echo "  removed $(basename "${path}")"
  fi
}

# Step 1: always scrub marinaMoji on --full (delete app, agents, mozc plist rows).
if [[ "${FULL}" -eq 1 ]]; then
  echo
  echo "--- Step 1: scrub marinaMoji from system ---"
  SCRUB_ARGS=(--yes)
  bash "${ROOT}/mac/scrub_marinamoji.sh" "${SCRUB_ARGS[@]}"
fi

# Step 2: wipe Input Sources domains (removes stale rows System Settings still shows).
echo
echo "--- Step 2: wipe Input Sources preference domains ---"
killall marinaMoji marinaMojiConverter marinaMojiRenderer imklaunchagent TextInputMenuAgent cfprefsd 2>/dev/null || true
export_domain "com.apple.HIToolbox"
export_domain "com.apple.inputsources"
delete_domain "com.apple.HIToolbox"
delete_domain "com.apple.inputsources"
remove_plist_if_present "${HIT}"
remove_plist_if_present "${INPUTSRC}"

for stale in "${PREFS}/com.apple.HIToolbox.plist.bak."*; do
  [[ -e "${stale}" ]] || continue
  rm -f "${stale}" && echo "  removed stale $(basename "${stale}")"
done

if [[ "${FULL}" -eq 1 ]]; then
  echo
  echo "--- Step 3: quarantine dpm keyboard layout ---"
  if [[ -d "${DPM_BUNDLE}" ]]; then
    mv "${DPM_BUNDLE}" "${DPM_OFF}"
    echo "  quarantined dpm.bundle → ${DPM_OFF}"
  else
    echo "  (dpm.bundle not found — already quarantined or removed)"
  fi
fi

killall cfprefsd imklaunchagent TextInputMenuAgent 2>/dev/null || true

echo
echo "--- Final verification (must pass before logout) ---"
if [[ -d "${APP_DST}" ]]; then
  echo "  FAIL: ${APP_DST} still present — scrub did not remove the app."
  exit 1
fi
echo "  OK: no marinaMoji.app in Input Methods"
if defaults read com.apple.inputsources 2>/dev/null | rg -q 'org\.mozc'; then
  echo "  FAIL: org.mozc still in com.apple.inputsources"
  exit 1
fi
echo "  OK: no org.mozc in inputsources"
if defaults read com.apple.HIToolbox 2>/dev/null | rg -q 'org\.mozc'; then
  echo "  FAIL: org.mozc still in com.apple.HIToolbox"
  exit 1
fi
echo "  OK: no org.mozc in HIToolbox"

echo
echo "=== Required next steps (do not skip) ==="
echo
echo "  1. Confirm System Settings → Input Sources has NO marinaMoji rows."
echo "     (If any remain, remove them manually — they are ghosts from before scrub.)"
echo
echo "  2. LOG OUT and log back in (or restart)."
echo "     Do NOT reinstall marinaMoji before logout — that causes duplicate rows."
echo
echo "  3. After login:"
echo "       cd ${ROOT} && bash ./mac/post_reset_marinamoji.sh"
echo "     Then add marinaMoji ONCE in System Settings."
echo "     Then: bash ./mac/activate_marinamoji.sh"
echo
echo "  4. Verify one menu entry + conversion in TextEdit:"
echo '       defaults read com.apple.HIToolbox AppleSelectedInputSources | rg -i "org.mozc|Japanese"'
echo
if [[ "${FULL}" -eq 1 && -d "${DPM_OFF}" ]]; then
  echo "Restore dpm later (after marinaMoji works):"
  echo "  mv \"${DPM_OFF}\" \"${DPM_BUNDLE}\""
fi
echo
echo "Do NOT restore old HIToolbox backups wholesale — they may contain corrupted state."
