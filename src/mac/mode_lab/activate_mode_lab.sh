#!/bin/bash
# Enable Mode Lab in the menu bar (when System Settings picker is awkward).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP="/Library/Input Methods/ModeLab.app"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
TIS_PREFIX="org.mozc.inputmethod.ModeLab"

if [[ ! -d "${APP}" ]]; then
  echo "ERROR: ${APP} not found. Run install_mode_lab.sh first." >&2
  exit 1
fi

# Duplicate user-level copy breaks TIS (shows 12 sources instead of 6).
if [[ -d "${HOME}/Library/Input Methods/ModeLab.app" ]]; then
  echo "Removing duplicate ${HOME}/Library/Input Methods/ModeLab.app ..."
  rm -rf "${HOME}/Library/Input Methods/ModeLab.app"
fi

bash "${SCRIPT_DIR}/register_mode_lab.sh"

echo "Enabling Mode Lab input source..."
swift - "${APP}" "${TIS_PREFIX}" <<'SWIFT'
import Carbon
import Foundation

let appPath = CommandLine.arguments[1]
let prefix = CommandLine.arguments[2]

func tisString(_ src: TISInputSource, _ key: CFString) -> String? {
    guard let raw = TISGetInputSourceProperty(src, key) else { return nil }
    return Unmanaged<CFString>.fromOpaque(raw).takeUnretainedValue() as String
}

appPath.withCString { cstr in
    if let url = CFURLCreateFromFileSystemRepresentation(nil, cstr, strlen(cstr), false) {
        _ = TISRegisterInputSource(url)
    }
}

guard let list = TISCreateInputSourceList(nil, true)?.takeRetainedValue() as? [TISInputSource] else {
    fputs("ERROR: TISCreateInputSourceList failed\n", stderr)
    exit(1)
}

var enabled = false
for src in list {
    guard let id = tisString(src, kTISPropertyInputSourceID) else { continue }
    guard id.hasPrefix(prefix) else { continue }
    let status = TISEnableInputSource(src)
    fputs("TISEnableInputSource \(id): \(status)\n", stderr)
    enabled = true
    if id.hasSuffix(".base") || id == prefix {
        let selectStatus = TISSelectInputSource(src)
        fputs("TISSelectInputSource \(id): \(selectStatus)\n", stderr)
    }
}

if !enabled {
    fputs("ERROR: no \(prefix) sources found\n", stderr)
    exit(1)
}
SWIFT

"${LSREGISTER}" -f "${APP}"
killall imklaunchagent TextInputMenuAgent ModeLab 2>/dev/null || true

echo
echo "Mode Lab should appear in the menu bar input picker now."
echo "Verify: defaults read com.apple.HIToolbox AppleSelectedInputSources | rg -i ModeLab"
