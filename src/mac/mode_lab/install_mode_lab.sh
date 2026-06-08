#!/bin/bash
# Install Mode Lab IME + Host for macOS input-mode debugging.
# Uses bundle ID org.mozc.inputmethod.ModeLab (same TIS family as marinaMoji).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
APP_SRC="${SRC_DIR}/bazel-bin/mac/mode_lab/mode_lab_ime_archive-root/ModeLab.app"
HOST_SRC="${SRC_DIR}/bazel-bin/mac/mode_lab/mode_lab_host_archive-root/ModeLabHost.app"
IME_DEST="/Library/Input Methods/ModeLab.app"
HOST_DEST="/Applications/ModeLabHost.app"

if [[ ! -d "${APP_SRC}" ]] || [[ ! -d "${HOST_SRC}" ]]; then
  echo "Build first (from src/):" >&2
  echo "  bazelisk build --config=oss_macos //mac/mode_lab:mode_lab_ime //mac/mode_lab:mode_lab_host" >&2
  exit 1
fi

# Prevent duplicate rows in System Settings (common after repeated install/register).
if defaults read com.apple.inputsources 2>/dev/null | rg -q "org.mozc.inputmethod.ModeLab" 2>/dev/null; then
  echo "Existing Mode Lab registration detected — scrubbing first..."
  if ! bash "${SCRIPT_DIR}/scrub_mode_lab.sh" --yes; then
    echo "WARN: scrub incomplete (macOS may keep stale rows until you remove them in System Settings)."
    echo "      Continuing install — activate_mode_lab.sh will re-enable TIS sources."
  fi
fi

echo "Installing Mode Lab IME to ${IME_DEST}..."
sudo rm -rf "${IME_DEST}"
# User-level duplicate breaks TIS and hides the IME from System Settings.
rm -rf "${HOME}/Library/Input Methods/ModeLab.app" 2>/dev/null || true
sudo ditto "${APP_SRC}" "${IME_DEST}"
sudo chown -R root:wheel "${IME_DEST}"
sudo chmod -R go-w "${IME_DEST}"
sudo touch "${IME_DEST}" "${IME_DEST}/Contents/Info.plist"

echo "Installing Mode Lab Host to ${HOST_DEST}..."
sudo rm -rf "${HOST_DEST}"
sudo ditto "${HOST_SRC}" "${HOST_DEST}"

echo "Registering with Text Input Services..."
bash "${SCRIPT_DIR}/register_mode_lab.sh"

# Verify installed strings (picker requires com.apple.inputmethod.Japanese = "Mode Lab").
if ! plutil -p "${IME_DEST}/Contents/Resources/English.lproj/InfoPlist.strings" 2>/dev/null | rg -q 'com.apple.inputmethod.Japanese.*Mode Lab'; then
  echo "ERROR: Installed app is missing input-mode localization in InfoPlist.strings." >&2
  echo "       Rebuild //mac/mode_lab:mode_lab_ime and run this script again." >&2
  exit 1
fi

echo "Activating Mode Lab in the menu bar..."
bash "${SCRIPT_DIR}/activate_mode_lab.sh"

echo
echo "Done. Next steps:"
echo "  1. Open /Applications/ModeLabHost.app"
echo "  2. Select **Mode Lab** in the menu bar input picker; type in the host text field"
echo
echo "If Mode Lab is missing from the picker, run again:"
echo "  bash mac/mode_lab/activate_mode_lab.sh"
echo
echo "Logs: ~/Library/Logs/marinaMoji/mode_lab.log"
echo "To remove: sudo rm -rf \"${IME_DEST}\" \"${HOST_DEST}\""
