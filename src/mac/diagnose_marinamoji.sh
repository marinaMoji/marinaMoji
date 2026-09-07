#!/bin/bash
# Field diagnostic for "marinaMoji installed but does not appear in Input Sources".
#
# Safe to run on any user's Mac: read-only apart from one registration attempt.
# Needs no Xcode, no Homebrew, no developer tools. Run as the logged-in user:
#   bash diagnose_marinamoji.sh
set -uo pipefail

APP="/Library/Input Methods/marinaMoji.app"
IMK="${APP}/Contents/MacOS/marinaMoji"

section() { printf '\n=== %s ===\n' "$1"; }

section "Machine"
echo "macOS:      $(sw_vers -productVersion) ($(sw_vers -buildVersion))"
echo "Host arch:  $(uname -m)"
echo "Booted:     $(uptime | sed 's/.*up //; s/,.*users.*//')"
echo "User:       $(id -un) (uid $(id -u))"
echo "Console:    $(stat -f%Su /dev/console)"

section "Installed bundle"
if [[ ! -d "${APP}" ]]; then
  echo "MISSING: ${APP} — the package did not install, or was removed."
  exit 1
fi
echo "Version:    $(defaults read "${APP}/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo '?')"
echo "Installed:  $(stat -f%Sm "${APP}")"
echo "Binary:     $(lipo -archs "${IMK}" 2>&1)"
echo "Quarantine: $(xattr -p com.apple.quarantine "${IMK}" 2>/dev/null || echo none)"
echo "Signature:  $(codesign -dv "${APP}" 2>&1 | grep -i '^Authority' | head -1 || echo '?')"

section "Can the binary actually run on this Mac?"
# A package built for the wrong architecture fails here, not at install time:
# on a Mac that has never needed Rosetta 2, an Intel build cannot execute.
if "${IMK}" --register_input_source; then
  echo "-> registration OK (count printed above)"
else
  echo "-> registration FAILED (exit $?)"
fi

section "What the installer logged"
grep "marinaMoji postinstall" /var/log/install.log | tail -30 ||
  echo "(nothing — this package predates the registering postinstall, or it never ran)"

section "LaunchAgents in this session"
launchctl print "gui/$(id -u)" 2>/dev/null | grep -i mozc || echo "(none loaded)"

section "Input source preferences"
echo -n "Selected:   "
defaults read com.apple.HIToolbox AppleSelectedInputSources 2>/dev/null | grep -i 'org.mozc' || echo "(no marinaMoji)"
echo -n "Enabled:    "
defaults read com.apple.HIToolbox AppleEnabledInputSources 2>/dev/null | grep -i 'org.mozc' || echo "(no marinaMoji)"
echo -n "ThirdParty: "
defaults read com.apple.inputsources AppleEnabledThirdPartyInputSources 2>/dev/null | grep -i 'org.mozc' | head -1 || echo "(no marinaMoji)"

section "If registration above said OK but System Settings still hides it"
echo "Enable and select it directly, skipping the + dialog entirely:"
echo "  \"${IMK}\" --select_input_source"
echo "Then quit System Settings completely (Cmd-Q) and reopen it."
