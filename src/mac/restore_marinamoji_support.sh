#!/bin/bash
# Restore marinaMoji Application Support from a Desktop backup created by scrub.
set -euo pipefail

SUPPORT="${HOME}/Library/Application Support/marinaMoji"
DESKTOP="${HOME}/Desktop"

if [[ $# -ge 1 ]]; then
  BACKUP="$1"
else
  BACKUP="$(ls -dt "${DESKTOP}"/marinaMoji.support.backup.* 2>/dev/null | head -1)"
fi

if [[ -z "${BACKUP}" || ! -d "${BACKUP}" ]]; then
  echo "ERROR: no backup found. Pass path:" >&2
  echo "  bash $0 ~/Desktop/marinaMoji.support.backup.YYYYMMDD-HHMMSS" >&2
  exit 1
fi

if [[ ! -f "${BACKUP}/config1.db" ]]; then
  echo "WARN: ${BACKUP} has no config1.db — trying another backup..."
  for candidate in "${DESKTOP}"/marinaMoji.support.backup.*; do
    [[ -f "${candidate}/config1.db" ]] || continue
    BACKUP="${candidate}"
    break
  done
fi

if [[ ! -f "${BACKUP}/config1.db" ]]; then
  echo "ERROR: no backup with config1.db on Desktop." >&2
  exit 1
fi

echo "Stopping marinaMoji processes..."
killall marinaMoji marinaMojiConverter marinaMojiRenderer 2>/dev/null || true
sleep 1

if [[ -d "${SUPPORT}" ]]; then
  STAMP="$(date +%Y%m%d-%H%M%S)"
  mv "${SUPPORT}" "${DESKTOP}/marinaMoji.support.before-restore.${STAMP}"
  echo "Moved current support aside → marinaMoji.support.before-restore.${STAMP}"
fi

cp -a "${BACKUP}" "${SUPPORT}"
echo "Restored support from: ${BACKUP}"

bash "$(cd "$(dirname "$0")/.." && pwd)/mac/install_launchagents.sh"

echo "Done. Run: bash $(dirname "$0")/activate_marinamoji.sh"
echo "Then test in TextEdit (not Cursor first)."
