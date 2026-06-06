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
swift - "$APP" <<'SWIFT'
import Carbon
import Foundation

let appPath = CommandLine.arguments[1]

func tisString(_ src: TISInputSource, _ key: CFString) -> String? {
    guard let raw = TISGetInputSourceProperty(src, key) else { return nil }
    return Unmanaged<CFString>.fromOpaque(raw).takeUnretainedValue() as String
}

appPath.withCString { cstr in
    if let url = CFURLCreateFromFileSystemRepresentation(nil, cstr, strlen(cstr), false) {
        let status = TISRegisterInputSource(url)
        fputs("TISRegisterInputSource status: \(status)\n", stderr)
    }
}

guard let list = TISCreateInputSourceList(nil, true)?.takeRetainedValue() as? [TISInputSource] else {
    fputs("ERROR: TISCreateInputSourceList failed\n", stderr)
    exit(1)
}

var enabledAny = false
for src in list {
    guard let id = tisString(src, kTISPropertyInputSourceID) else { continue }
    guard id.hasPrefix("org.mozc.inputmethod.Japanese") else { continue }
    let enableStatus = TISEnableInputSource(src)
    fputs("TISEnableInputSource \(id): \(enableStatus)\n", stderr)
    enabledAny = true
    if id.hasSuffix(".base") || id == "org.mozc.inputmethod.Japanese" {
        let selectStatus = TISSelectInputSource(src)
        fputs("TISSelectInputSource \(id): \(selectStatus)\n", stderr)
    }
}

if !enabledAny {
    fputs("ERROR: no org.mozc.inputmethod.Japanese sources found after register\n", stderr)
    exit(1)
}
SWIFT

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
