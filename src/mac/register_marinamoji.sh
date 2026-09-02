#!/bin/bash
# Register marinaMoji.app with Text Input Services so it appears in
# System Settings → Keyboard → Input Sources after a fresh install/scrub.
set -euo pipefail

APP="/Library/Input Methods/marinaMoji.app"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"

if [[ ! -d "${APP}" ]]; then
  echo "ERROR: ${APP} not found. Run install_marinamoji.sh first." >&2
  exit 1
fi

echo "Registering marinaMoji with Text Input Services..."
"${LSREGISTER}" -f "${APP}"

# The installed IME binary performs the registration itself, so this works on
# machines without the Swift toolchain (i.e. every machine that is not a dev
# box). It exits non-zero if macOS still does not list the input sources.
"${APP}/Contents/MacOS/marinaMoji" --register_input_source > /dev/null

# Restart the agents that cache the input-source list; launchd restarts them.
killall TextInputMenuAgent 2>/dev/null || true
killall imklaunchagent 2>/dev/null || true

echo "Registered org.mozc.inputmethod.Japanese modes with macOS."
echo
echo "In System Settings → Keyboard → Input Sources → Edit → + :"
echo "  • search or browse for **Japanese**, then pick **marinaMoji**"
echo "  (It may not appear if you search the list for English-only names.)"
echo
echo "If the + dialog still does not show it, quit System Settings and reopen it."
