#!/bin/sh
# Legacy helper kept so older docs/scripts that call this name still succeed.
#
# Converter / renderer / sync are no longer bootstrapped from
# ~/Library/LaunchAgents or /Library/LaunchAgents. Their plists live inside
# marinaMoji.app (Contents/Library/LaunchAgents/) and are registered by the
# IME via SMAppService when marinaMoji starts (see mac/main.mm).
#
# This script only removes leftover *external* LaunchAgent copies from older
# installs so they cannot fight SMAppService for the same Mach service names.
set -e

if [ "$(id -u)" -eq 0 ]; then
  echo "Do not run this script with sudo. Run as your normal user:" >&2
  echo "  bash mac/install_launchagents.sh" >&2
  exit 1
fi

UID_NUM=$(id -u)
GUI_DOMAIN="gui/${UID_NUM}"
LEGACY_AGENTS="org.mozc.inputmethod.Japanese.Converter \
org.mozc.inputmethod.Japanese.Renderer \
org.mozc.inputmethod.Japanese.Sync"

removed=0
for agent in ${LEGACY_AGENTS}; do
  user_plist="${HOME}/Library/LaunchAgents/${agent}.plist"
  if [ -f "${user_plist}" ]; then
    launchctl bootout "${GUI_DOMAIN}/${agent}" 2>/dev/null || true
    launchctl bootout "${GUI_DOMAIN}" "${user_plist}" 2>/dev/null || true
    rm -f "${user_plist}"
    echo "  removed legacy user LaunchAgent ${agent}.plist"
    removed=1
  fi
  system_plist="/Library/LaunchAgents/${agent}.plist"
  if [ -f "${system_plist}" ]; then
    # Domain and path must be separate args (see scrub_marinamoji.sh).
    sudo launchctl bootout system "${system_plist}" 2>/dev/null || true
    sudo rm -f "${system_plist}"
    echo "  removed legacy system LaunchAgent ${agent}.plist"
    removed=1
  fi
done

if [ "${removed}" -eq 0 ]; then
  echo "No legacy external LaunchAgents to remove."
fi

echo "Converter/renderer/sync register via SMAppService when marinaMoji runs."
echo "If conversion fails, check System Settings → General → Login Items for"
echo "marinaMoji background items, then: bash mac/activate_marinamoji.sh"
echo
echo "Checking processes..."
pgrep -lf marinaMojiConverter || echo "  (converter not listed yet; select marinaMoji in Input Sources)"
pgrep -lf marinaMojiRenderer || echo "  (renderer not listed yet)"
pgrep -lf marinaMojiSync || echo "  (sync daemon not listed yet)"
