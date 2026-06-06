#!/bin/bash
# Run AFTER logout/login following reset_hitoolbox_for_marinamoji.sh --full.
# Installs a fresh marinaMoji build; you then add marinaMoji once in System Settings.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== marinaMoji post-reset install ==="
echo
if [[ -d "/Library/Input Methods/marinaMoji.app" ]]; then
  echo "NOTE: marinaMoji.app is already installed. If you have not logged out since"
  echo "      reset --full, log out first or you may get duplicate Input Source rows."
  echo
fi
echo "This will build (if needed) and install marinaMoji.app."
echo "Add marinaMoji in System Settings only AFTER this script finishes."
echo
read -r -p "Press Enter to continue (Ctrl-C to abort)..."

export MOZC_QT_PATH="${MOZC_QT_PATH:-/opt/homebrew/opt/qt}"
cd "${ROOT}"
bazelisk build --config oss_macos //mac:mozc_macos
bash "${ROOT}/mac/install_marinamoji.sh"

if [[ ! -f "${HOME}/Library/Application Support/marinaMoji/config1.db" ]]; then
  LATEST_BACKUP="$(ls -dt "${HOME}/Desktop"/marinaMoji.support.backup.* 2>/dev/null | head -1 || true)"
  if [[ -n "${LATEST_BACKUP}" && -f "${LATEST_BACKUP}/config1.db" ]]; then
    echo
    echo "No config1.db yet — restoring latest Desktop backup..."
    bash "${ROOT}/mac/restore_marinamoji_support.sh" "${LATEST_BACKUP}"
  else
    echo
    echo "NOTE: Fresh Application Support has no config1.db yet."
    echo "      marinaMoji will create defaults on first run, but restoring a"
    echo "      Desktop backup is safer if you had one before scrub."
  fi
fi

echo
echo "=== Add marinaMoji in System Settings ==="
echo "  Keyboard → Input Sources → Edit → +"
echo "  Choose **Japanese** (language), then **marinaMoji** — add ONCE."
echo "  Quit and reopen System Settings if it does not appear yet."
echo
echo "After adding, run:"
echo "  bash ${ROOT}/mac/activate_marinamoji.sh"
echo
echo "Switch to marinaMoji, open TextEdit, type nihongo + space."
echo "Verify:"
echo '  defaults read com.apple.HIToolbox AppleSelectedInputSources | rg -i "org.mozc|Japanese"'
