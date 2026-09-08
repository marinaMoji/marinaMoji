#!/bin/bash
# Run one reproducible marinaMoji install trial and write a labelled report.
#
# Intended for a macOS VM restored to a snapshot before each run, so that trials
# are comparable. The protocol deliberately separates three phases:
#
#   before   passive state of the machine before anything is installed
#   after    passive state once the installer has finished -- NOTHING that could
#            itself register the input source runs in this phase, so the report
#            records what the user would actually have seen
#   repair   active attempts (registration, agent restarts), run only after the
#            observation above has been recorded
#
# Keeping repair out of the observation matters: running the registration to
# "check whether it worked" is what makes a failed install look like a success.
#
# Usage, as the logged-in user (not root, not sudo):
#   bash vm_trial_marinamoji.sh --label fresh-japanese-gui
#   bash vm_trial_marinamoji.sh --label upgrade-cli --pkg ~/Downloads/marinaMoji.pkg
#   bash vm_trial_marinamoji.sh --label baseline --collect-only
#
# With --pkg the package is installed via the `installer` CLI, which runs no
# plugin panes. Without it you are prompted to install by hand, which is how you
# exercise the GUI installer and ActivatePane.
set -uo pipefail

APP="/Library/Input Methods/marinaMoji.app"
IMK="${APP}/Contents/MacOS/marinaMoji"
INSTALL_LOG="/var/log/install.log"

LABEL=""
PKG=""
COLLECT_ONLY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --label) LABEL="${2:-}"; shift 2 ;;
    --pkg) PKG="${2:-}"; shift 2 ;;
    --collect-only) COLLECT_ONLY=1; shift ;;
    -h|--help) sed -n '2,28p' "$0"; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
done

if [[ -z "${LABEL}" ]]; then
  echo "ERROR: --label is required (it names the report directory)." >&2
  exit 2
fi
if [[ "$(id -u)" -eq 0 ]]; then
  echo "ERROR: run as the logged-in user, not with sudo." >&2
  echo "       The trial reads per-user input source state." >&2
  exit 2
fi
if [[ -n "${PKG}" && ! -f "${PKG}" ]]; then
  echo "ERROR: no such package: ${PKG}" >&2
  exit 2
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
OUT="${HOME}/Desktop/marinaMoji-trial-${LABEL}-${STAMP}"
mkdir -p "${OUT}"

say() { printf '\n=== %s ===\n' "$1"; }

# A clean macOS install has no developer tools, so lipo and strings are absent.
# Their absence must not be reported as a measurement: `strings | grep -c`
# silently yields 0, which reads as "this build lacks the registration flag"
# when it actually means "the check could not run". /usr/bin/grep is always
# present and reads binaries fine with -a.
has_reg_flag() {
  [[ -f "${IMK}" ]] || { echo "?(no binary)"; return; }
  # grep -c prints 0 and exits 1 when there is no match, so a `|| echo 0`
  # fallback would emit the count twice. Capture instead.
  local n
  n="$(grep -ac "register_input_source" "${IMK}" 2>/dev/null)"
  echo "${n:-0}"
}

binary_arch() {
  [[ -f "${IMK}" ]] || { echo "?(no binary)"; return; }
  # /usr/bin/lipo exists even with no developer tools: it is a stub that prints
  # an xcode-select notice, so `command -v` is not a usable test.
  local out
  out="$(lipo -archs "${IMK}" 2>/dev/null)"
  if [[ -z "${out}" || "${out}" == *"xcode-select"* ]]; then
    echo "?(lipo unavailable - no developer tools installed)"
  else
    echo "${out}"
  fi
}

# --- state capture -----------------------------------------------------------

capture_machine() {
  echo "macOS:        $(sw_vers -productVersion) ($(sw_vers -buildVersion))"
  echo "Arch:         $(uname -m)"
  echo "Uptime:       $(uptime | sed 's/.*up //; s/,.*users.*//')"
  echo "User:         $(id -un) (uid $(id -u))"
  echo "Languages:    $(defaults read -g AppleLanguages 2>/dev/null | tr -d '\n ' )"
  echo "Locale:       $(defaults read -g AppleLocale 2>/dev/null || echo '?')"
}

capture_bundle() {
  if [[ ! -d "${APP}" ]]; then
    echo "marinaMoji:   NOT INSTALLED"
    return
  fi
  echo "marinaMoji:   installed"
  echo "Version:      $(defaults read "${APP}/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo '?')"
  echo "CFBundleVer:  $(defaults read "${APP}/Contents/Info.plist" CFBundleVersion 2>/dev/null || echo '?')"
  echo "Bundle mtime: $(stat -f%Sm "${APP}")"
  echo "Binary mtime: $(stat -f%Sm "${IMK}" 2>/dev/null || echo '?')"
  echo "Binary arch:  $(binary_arch)"
  echo "Binary sha:   $(shasum -a 256 "${IMK}" 2>/dev/null | awk '{print $1}')"
  echo "Has reg flag: $(has_reg_flag)"
  echo "Signed by:    $(codesign -dvvv "${APP}" 2>&1 | grep '^Authority' | head -1 | sed 's/^Authority=//')"
}

# Passive only: reads preference domains, never touches Text Input Services.
capture_input_state() {
  echo -n "Selected:     "
  defaults read com.apple.HIToolbox AppleSelectedInputSources 2>/dev/null | grep -i 'org.mozc' || echo "(no marinaMoji)"
  echo -n "Enabled:      "
  defaults read com.apple.HIToolbox AppleEnabledInputSources 2>/dev/null | grep -i 'org.mozc' || echo "(no marinaMoji)"
  echo -n "ThirdParty:   "
  defaults read com.apple.inputsources AppleEnabledThirdPartyInputSources 2>/dev/null | grep -i 'org.mozc' | head -1 || echo "(no marinaMoji)"
  echo "Agents:"
  launchctl print "gui/$(id -u)" 2>/dev/null | grep -i mozc | sed 's/^/  /' || echo "  (none loaded)"
  echo "Processes:"
  pgrep -lf 'marinaMoji|TextInputMenuAgent|imklaunchagent' | sed 's/^/  /' || echo "  (none)"
}

snapshot() {
  say "Machine";      capture_machine
  say "Bundle";       capture_bundle
  say "Input state";  capture_input_state
}

# --- phase 0: before ---------------------------------------------------------

LOG_LINES_BEFORE=0
if [[ -r "${INSTALL_LOG}" ]]; then
  LOG_LINES_BEFORE="$(wc -l < "${INSTALL_LOG}" | tr -d ' ')"
fi

snapshot > "${OUT}/00-before.txt" 2>&1
echo "Trial:  ${LABEL}"
echo "Report: ${OUT}"
echo
sed -n '/=== Bundle ===/,/=== Input state ===/p' "${OUT}/00-before.txt" | head -4

if [[ "${COLLECT_ONLY}" -eq 1 ]]; then
  cp "${OUT}/00-before.txt" "${OUT}/report.md"
  echo
  echo "Collected state only (no install performed)."
  echo "Report: ${OUT}/report.md"
  exit 0
fi

# --- phase 1: install --------------------------------------------------------

if [[ -n "${PKG}" ]]; then
  {
    echo "Method: installer CLI (no plugin panes, ActivatePane does NOT run)"
    echo "Package: ${PKG}"
    echo "sha256: $(shasum -a 256 "${PKG}" | awk '{print $1}')"
    echo
  } > "${OUT}/01-install.txt"
  echo
  echo "Installing via the installer CLI (sudo will prompt for your password)..."
  sudo installer -pkg "${PKG}" -target / >> "${OUT}/01-install.txt" 2>&1
  echo "installer exit: $?" >> "${OUT}/01-install.txt"
else
  echo
  echo "Which package are you about to install? Drag it into this window for the"
  echo "path, so the trial records exactly which build was used."
  read -r -p "Package path (Enter to skip): " MANUAL_PKG
  MANUAL_PKG="${MANUAL_PKG%\"}"; MANUAL_PKG="${MANUAL_PKG#\"}"
  {
    echo "Method: manual / GUI install (ActivatePane DOES run)"
    if [[ -n "${MANUAL_PKG}" && -f "${MANUAL_PKG}" ]]; then
      echo "Package: ${MANUAL_PKG}"
      echo "sha256: $(shasum -a 256 "${MANUAL_PKG}" | awk '{print $1}')"
      echo "size: $(stat -f%z "${MANUAL_PKG}") bytes"
    else
      echo "Package: NOT RECORDED (operator skipped)"
    fi
  } > "${OUT}/01-install.txt"
  echo
  echo "Install the package by hand now (double-click it, complete the installer)."
  echo "Do NOT open System Settings yet -- that comes next, and opening it early"
  echo "changes what the trial measures."
  read -r -p "Press Enter once the installer reports that it has finished..."
fi

# --- phase 2: after, passive -------------------------------------------------

{
  snapshot
  say "install.log (new lines from this trial)"
  if [[ -r "${INSTALL_LOG}" ]]; then
    tail -n "+$((LOG_LINES_BEFORE + 1))" "${INSTALL_LOG}" | grep -i 'marinamoji\|package_script_service\|PackageKit: Install' | tail -60
  else
    echo "(${INSTALL_LOG} not readable by this user)"
  fi
  say "postinstall lines only"
  tail -n "+$((LOG_LINES_BEFORE + 1))" "${INSTALL_LOG}" 2>/dev/null | grep 'marinaMoji postinstall' ||
    echo "(none -- package predates the registering postinstall, or it never ran)"
} > "${OUT}/02-after-passive.txt" 2>&1

# --- phase 3: the observation that actually matters --------------------------

echo
echo "Now open System Settings -> Keyboard -> Input Sources -> Edit -> +"
echo "and look for marinaMoji under Japanese (日本語)."
echo
read -r -p "Does marinaMoji appear in the + list? [y/n] " APPEARS
read -r -p "Anything else worth noting (free text, Enter to skip): " NOTES
{
  echo "Appears in + list: ${APPEARS}"
  echo "Operator notes: ${NOTES}"
} > "${OUT}/03-observation.txt"

# --- phase 4: repair, active -------------------------------------------------

echo
read -r -p "Run the active repair steps now? They mutate state. [y/n] " DOREPAIR
if [[ "${DOREPAIR}" =~ ^[Yy] ]]; then
  {
    say "marinaMoji --register_input_source"
    # A build without the flag cannot be repaired this way, and reporting the
    # failure as a result would wrongly implicate the registration logic.
    if [[ "$(has_reg_flag)" == "0" ]]; then
      echo "SKIPPED: this build has no --register_input_source flag."
      echo "Repair is not applicable; the installed build predates it."
    elif [[ -x "${IMK}" ]]; then
      "${IMK}" --register_input_source 2>&1
      echo "exit: $?"
    else
      echo "(binary missing at ${IMK})"
    fi
    say "discard system input source table"
    sudo rm -f "/System/Library/Caches/com.apple.IntlDataCache.le" \
               "/System/Library/Caches/com.apple.IntlDataCache.le.kbdx" &&
      echo "removed IntlDataCache files" || echo "could not remove IntlDataCache files"

    say "restart input source agents"
    killall TextInputMenuAgent 2>/dev/null && echo "killed TextInputMenuAgent" || echo "TextInputMenuAgent not running"
    killall imklaunchagent 2>/dev/null && echo "killed imklaunchagent" || echo "imklaunchagent not running"
    sleep 2
    say "input state after repair"
    capture_input_state
  } > "${OUT}/04-repair.txt" 2>&1
  echo
  echo "Repair attempted. Re-check the + list."
  read -r -p "Does marinaMoji appear now? [y/n] " APPEARS_AFTER
  echo "Appears after repair: ${APPEARS_AFTER}" >> "${OUT}/04-repair.txt"
else
  echo "(skipped)" > "${OUT}/04-repair.txt"
  APPEARS_AFTER="n/a"
fi

# --- report ------------------------------------------------------------------

BUNDLE_BEFORE="$(grep 'Binary sha:' "${OUT}/00-before.txt" | awk '{print $3}')"
BUNDLE_AFTER="$(grep 'Binary sha:' "${OUT}/02-after-passive.txt" | awk '{print $3}')"
# Deduplicated: the same log line appears in both the full-delta and the
# postinstall-only sections of 02-after-passive.txt.
POSTINSTALL="$(grep 'marinaMoji postinstall' "${OUT}/02-after-passive.txt" | sed 's/.*postinstall: //' | sort -u | tr '\n' ';')"

{
  echo "# marinaMoji install trial: ${LABEL}"
  echo
  echo "- Date: $(date)"
  echo "- macOS: $(sw_vers -productVersion) ($(sw_vers -buildVersion)), $(uname -m)"
  echo "- Language: $(defaults read -g AppleLanguages 2>/dev/null | tr -d '\n ')"
  echo "- Install method: $(head -1 "${OUT}/01-install.txt" | sed 's/^Method: //')"
  echo
  echo "## Verdict"
  echo
  if [[ "${BUNDLE_BEFORE}" == "${BUNDLE_AFTER}" && -n "${BUNDLE_BEFORE}" ]]; then
    echo "- Payload: **binary unchanged** (same sha256 before and after)"
  else
    echo "- Payload: binary changed (${BUNDLE_BEFORE:-none} -> ${BUNDLE_AFTER:-none})"
  fi
  echo "- postinstall said: ${POSTINSTALL:-(nothing logged)}"
  echo "- Appeared in + list without help: **${APPEARS}**"
  echo "- Appeared after active repair: ${APPEARS_AFTER}"
  [[ -n "${NOTES}" ]] && echo "- Notes: ${NOTES}"
  echo
  echo "## Files"
  echo
  echo '```'
  ls -1 "${OUT}"
  echo '```'
} > "${OUT}/report.md"

echo
echo "=== Verdict ==="
sed -n '/## Verdict/,/## Files/p' "${OUT}/report.md" | sed '1,2d;$d'
echo "Full report: ${OUT}/report.md"
