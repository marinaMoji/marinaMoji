#!/bin/sh
# Install LaunchAgents for marinaMojiConverter, marinaMojiRenderer, and marinaMojiSync.
# Required after manual `ditto` install (the .pkg does this under /Library/LaunchAgents).
set -e

if [ "$(id -u)" -eq 0 ]; then
  echo "Do not run this script with sudo. Run as your normal user:" >&2
  echo "  ./mac/install_launchagents.sh" >&2
  exit 1
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
AGENT_SRC="${SCRIPT_DIR}/installer/LaunchAgents"
UID_NUM=$(id -u)
GUI_DOMAIN="gui/${UID_NUM}"

if [ ! -d "${AGENT_SRC}" ]; then
  echo "Missing LaunchAgents directory: ${AGENT_SRC}" >&2
  exit 1
fi

mkdir -p "${HOME}/Library/LaunchAgents"
for plist in org.mozc.inputmethod.Japanese.Converter.plist \
             org.mozc.inputmethod.Japanese.Renderer.plist \
             org.mozc.inputmethod.Japanese.Sync.plist; do
  cp "${AGENT_SRC}/${plist}" "${HOME}/Library/LaunchAgents/${plist}"
  launchctl bootout "${GUI_DOMAIN}" \
    "${HOME}/Library/LaunchAgents/${plist}" 2>/dev/null || true
  launchctl bootstrap "${GUI_DOMAIN}" \
    "${HOME}/Library/LaunchAgents/${plist}"
  label=$(basename "${plist}" .plist)
  launchctl kickstart -k "${GUI_DOMAIN}/${label}" 2>/dev/null || true
done

echo "LaunchAgents installed. Checking processes..."
pgrep -lf marinaMojiConverter || echo "  (converter not listed yet; toggle marinaMoji in Input Sources)"
pgrep -lf marinaMojiRenderer || echo "  (renderer not listed yet)"
pgrep -lf marinaMojiSync || echo "  (sync daemon not listed yet)"
