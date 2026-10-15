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
# Upgrade verification (non-interactive apart from the App Management prompt,
# which --yes skips):
#   bash vm_trial_marinamoji.sh --label upgrade-0.0.7-to-0.0.8 --verify-upgrade \
#        --pkg ~/Downloads/marinaMoji.pkg --yes
# Needs an existing install. It checks that the converter and renderer are
# loaded in launchd before the upgrade, right after it, and after the app is
# started; that the bundle version changed; that the installer's own health
# check passed; and that the app re-registers agents when launchd has dropped
# them. Prints PASS/FAIL per check and exits non-zero if any FAIL.
#
# With --pkg the package is installed via the `installer` CLI, which runs no
# GUI steps. Without it you are prompted to install by hand, which is how you
# exercise the GUI installer.
set -uo pipefail

APP="/Library/Input Methods/marinaMoji.app"
IMK="${APP}/Contents/MacOS/marinaMoji"
INSTALL_LOG="/var/log/install.log"

LABEL=""
PKG=""
COLLECT_ONLY=0
VERIFY_UPGRADE=0
ASSUME_YES=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --label) LABEL="${2:-}"; shift 2 ;;
    --pkg) PKG="${2:-}"; shift 2 ;;
    --collect-only) COLLECT_ONLY=1; shift ;;
    --verify-upgrade) VERIFY_UPGRADE=1; shift ;;
    --yes) ASSUME_YES=1; shift ;;
    -h|--help) sed -n '2,40p' "$0"; exit 0 ;;
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

if [[ "${VERIFY_UPGRADE}" -eq 1 ]]; then
  if [[ -z "${PKG}" ]]; then
    echo "ERROR: --verify-upgrade needs --pkg (the new package)." >&2
    exit 2
  fi
  if [[ ! -d "${APP}" ]]; then
    echo "ERROR: --verify-upgrade needs an existing install to upgrade." >&2
    exit 2
  fi
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

# --- launchd health ----------------------------------------------------------

AGENT_PREFIX="org.mozc.inputmethod.Japanese"
UID_NUM="$(id -u)"

agent_loaded() {
  launchctl print "gui/${UID_NUM}/${AGENT_PREFIX}.$1" > /dev/null 2>&1
}

# Converter and renderer both loaded. The sync agent publishes no Mach service
# and is not needed for typing, so it is not part of the health check.
agents_loaded() {
  agent_loaded Converter && agent_loaded Renderer
}

# True if launchd has a process running for the agent (not merely loaded).
agent_running() {
  launchctl print "gui/${UID_NUM}/${AGENT_PREFIX}.$1" 2>/dev/null |
    grep -q $'^\tstate = running'
}

agent_states() {
  local a
  for a in Converter Renderer Sync; do
    if ! agent_loaded "${a}"; then
      echo "${a}=NOT-loaded"
    elif agent_running "${a}"; then
      echo "${a}=running"
    elif launchctl print "gui/${UID_NUM}/${AGENT_PREFIX}.${a}" 2>/dev/null |
        grep -q -E 'spawn failed|needs LWCR update'; then
      # launchd's stored launch constraint no longer matches the upgraded binary.
      echo "${a}=SPAWN-FAILED(needs re-registration)"
    else
      echo "${a}=loaded-not-running"
    fi
  done | tr '\n' ' '
}

# The IME registers its agents at startup, so starting it is what "typing
# something" does on a real machine.
start_app() { open -g "${APP}" > /dev/null 2>&1; }

wait_for_agents() {
  local i
  for ((i = 0; i < $1; i++)); do
    agents_loaded && return 0
    sleep 1
  done
  return 1
}

# The marinaMoji release version (0.0.8), from the package receipt. The bundle's
# CFBundleShortVersionString is the upstream Mozc number (3.34.6345) and does not
# change between marinaMoji releases, so it cannot show that an upgrade happened.
# True if macOS recently reported a marinaMoji background item as "requires
# approval" (SMAppService status 2): the "Allow in the Background" switch for
# marinaMoji in System Settings > General > Login Items & Extensions is off.
# Only the user can turn it on, and with it off nothing else in this script can
# succeed, so failures are labelled with it rather than reported generically.
approval_needed() {
  /usr/bin/log show --last 2m --style compact --predicate 'process == "smd"' 2>/dev/null |
    grep -q "Found status: 2"
}

approval_hint() {
  if approval_needed; then
    echo " -- CAUSE: macOS says marinaMoji is awaiting approval. Switch it on in System Settings > General > Login Items & Extensions > Allow in the Background, then rerun"
  fi
}

installed_version() {
  local v
  v="$(pkgutil --pkg-info org.mozc.pkg.JapaneseInput 2>/dev/null | awk '/^version:/ {print $2}')"
  echo "${v:-?}"
}

VERIFY_FAILS=0
verify_result() {  # verify_result PASS|FAIL|INFO "message"
  [[ "$1" == "FAIL" ]] && VERIFY_FAILS=$((VERIFY_FAILS + 1))
  printf '%-5s %s\n' "$1" "$2" | tee -a "${OUT}/05-verify-upgrade.txt"
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

# --- phase 0b: upgrade baseline ----------------------------------------------

VERSION_BEFORE="$(installed_version)"
PRE_LOADED=0
if [[ "${VERIFY_UPGRADE}" -eq 1 ]]; then
  : > "${OUT}/05-verify-upgrade.txt"
  echo
  echo "Verifying upgrade from ${VERSION_BEFORE}: starting the installed app first..."
  start_app
  if wait_for_agents 30; then
    PRE_LOADED=1
    verify_result PASS "baseline: converter and renderer loaded before upgrade (${VERSION_BEFORE})"
  else
    verify_result INFO "baseline: agents NOT loaded before upgrade ($(agent_states)); the starting point is already broken, so the post-upgrade result is inconclusive$(approval_hint)"
  fi
fi

# --- phase 1: install --------------------------------------------------------

# Upgrading an existing install rewrites another app's bundle, which macOS
# blocks unless the terminal has App Management permission. Without it the
# install fails halfway through, and the partially modified machine is no longer
# a valid baseline -- so warn before the snapshot is spent rather than after.
if [[ -n "${PKG}" && -d "${APP}" ]]; then
  echo
  echo "This is an UPGRADE over an existing install."
  echo "macOS will block it unless this terminal has App Management permission:"
  echo "  System Settings > Privacy & Security > App Management > enable your terminal"
  echo "A blocked install still removes the old launch agents, which ruins the"
  echo "baseline for a retry -- restore the snapshot again if that happens."
  if [[ "${ASSUME_YES}" -eq 1 ]]; then
    APPMGMT=y
  else
    read -r -p "Permission granted, and terminal restarted since? [y/N] " APPMGMT
  fi
  if [[ ! "${APPMGMT}" =~ ^[Yy] ]]; then
    echo "Aborted before touching the machine; the snapshot is still clean."
    rm -rf "${OUT}"
    exit 1
  fi
fi

if [[ -n "${PKG}" ]]; then
  {
    echo "Method: installer CLI (GUI installer steps do not run)"
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
    echo "Method: manual / GUI install (GUI installer steps run)"
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

# --- phase 2b: upgrade verification ------------------------------------------

if [[ "${VERIFY_UPGRADE}" -eq 1 ]]; then
  echo
  echo "Verifying the upgraded install..."
  VERSION_AFTER="$(installed_version)"
  if [[ "${VERSION_AFTER}" != "${VERSION_BEFORE}" ]]; then
    verify_result PASS "installed release version changed: ${VERSION_BEFORE} -> ${VERSION_AFTER}"
  else
    verify_result FAIL "installed release version unchanged (${VERSION_AFTER}); the package may be the same version or the install did not replace the app"
  fi

  # What the user has the instant the installer finishes, before anything
  # starts the app again. The postinstall's own health check may already have
  # started it.
  verify_result INFO "immediately after install: $(agent_states)"

  start_app
  if wait_for_agents 30; then
    verify_result PASS "after starting the app: converter and renderer loaded"
    # Sync publishes no Mach service, so it can sit enabled-but-unloaded, or
    # loaded-but-killed (an upgrade kills it, exit status -9), with no visible
    # symptom. "Loaded" is not enough: it must be running, or auto-sync is dead.
    SYNC_OK=0
    for ((i = 0; i < 20; i++)); do
      agent_running Sync && { SYNC_OK=1; break; }
      sleep 1
    done
    if [[ "${SYNC_OK}" -eq 1 ]]; then
      verify_result PASS "after starting the app: sync agent running"
    else
      verify_result FAIL "after starting the app: sync agent not running ($(agent_states))"
    fi
  else
    verify_result FAIL "after starting the app: agents not loaded ($(agent_states)) -- settings would report 'Failed to retrieve the current configuration values'$(approval_hint)"
  fi

  HEALTH="$(grep 'postinstall: health check' "${OUT}/02-after-passive.txt" | sed 's/.*postinstall: //' | sort -u | tr '\n' ';')"
  case "${HEALTH}" in
    *"is loaded"*) verify_result PASS "installer health check: ${HEALTH}" ;;
    *NOT\ loaded*) verify_result FAIL "installer health check: ${HEALTH}" ;;
    *) verify_result INFO "installer health check: not logged (package predates it, or install.log unreadable)" ;;
  esac

  # Self-heal: reproduce the stale state this upgrade bug leaves behind (jobs
  # unloaded while Background Task Management still says Enabled) and require
  # the app to repair it by itself on the next start.
  launchctl bootout "gui/${UID_NUM}/${AGENT_PREFIX}.Converter" > /dev/null 2>&1
  launchctl bootout "gui/${UID_NUM}/${AGENT_PREFIX}.Renderer" > /dev/null 2>&1
  pkill -x marinaMoji > /dev/null 2>&1
  sleep 2
  if agents_loaded; then
    verify_result INFO "self-heal: could not unload the agents for the test (still loaded), skipped"
  else
    start_app
    if wait_for_agents 30; then
      verify_result PASS "self-heal: app re-registered agents that launchd had dropped"
    else
      verify_result FAIL "self-heal: agents still not loaded after restarting the app ($(agent_states))$(approval_hint)"
    fi
  fi
fi

if [[ "${VERIFY_UPGRADE}" -eq 1 ]]; then
  APPEARS="n/a (--verify-upgrade)"
  NOTES=""
  APPEARS_AFTER="n/a"
  echo "(skipped in --verify-upgrade mode)" > "${OUT}/03-observation.txt"
  echo "(skipped in --verify-upgrade mode)" > "${OUT}/04-repair.txt"
else
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
  if [[ "${VERIFY_UPGRADE}" -eq 1 ]]; then
    echo
    echo "## Upgrade verification (${VERSION_BEFORE} -> ${VERSION_AFTER:-?})"
    echo
    sed 's/^/- /' "${OUT}/05-verify-upgrade.txt"
  fi
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

if [[ "${VERIFY_UPGRADE}" -eq 1 ]]; then
  if [[ "${VERIFY_FAILS}" -gt 0 ]]; then
    echo "UPGRADE VERIFICATION FAILED (${VERIFY_FAILS} check(s))"
    exit 1
  fi
  echo "Upgrade verification passed."
fi
