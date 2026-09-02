#!/bin/bash
# Register marinaMoji with macOS Text Input Services and select it.
# Use when System Settings shows marinaMoji but conversion/IMK still fails,
# or when AppleEnabledInputSources does not list Japanese (common on macOS 15+).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="/Library/Input Methods/marinaMoji.app"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"

if [[ ! -d "$APP" ]]; then
  echo "ERROR: $APP not found. Install first: bash $ROOT/mac/install_marinamoji.sh" >&2
  exit 1
fi

echo "Registering and enabling marinaMoji via Text Input Services..."
# Handled by the installed IME binary so that no Swift toolchain is required.
"$APP/Contents/MacOS/marinaMoji" --select_input_source > /dev/null

echo "Refreshing LaunchServices and LaunchAgents..."
"$LSREGISTER" -f "$APP"
bash "$ROOT/mac/install_launchagents.sh"

killall imklaunchagent 2>/dev/null || true
killall TextInputMenuAgent 2>/dev/null || true
killall marinaMoji 2>/dev/null || true
killall marinaMojiConverter 2>/dev/null || true
killall marinaMojiRenderer 2>/dev/null || true

echo
echo "=== Verification (any one of these should show org.mozc) ==="
echo -n "Selected:  "
defaults read com.apple.HIToolbox AppleSelectedInputSources 2>/dev/null | rg -i 'org.mozc|Japanese' || echo "(empty)"
echo -n "Enabled:   "
defaults read com.apple.HIToolbox AppleEnabledInputSources 2>/dev/null | rg -i 'org.mozc|Japanese' || echo "(empty — OK on recent macOS if Selected shows mozc)"
echo -n "ThirdParty:"
defaults read com.apple.inputsources AppleEnabledThirdPartyInputSources 2>/dev/null | rg -i 'org.mozc' | head -1 || echo "(empty)"
echo
echo "Switch to marinaMoji in the menu bar, type in TextEdit, then check IMK:"
echo '  /usr/bin/log show --last 2m --style compact --predicate '"'"'process == \"imklaunchagent\"'"'"' | rg -i "Refusing connection|NO Endpoint"'
