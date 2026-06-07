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

swift - "${APP}" <<'SWIFT'
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
        if status != 0 { exit(1) }
    }
}

guard let list = TISCreateInputSourceList(nil, true)?.takeRetainedValue() as? [TISInputSource] else {
    fputs("ERROR: TISCreateInputSourceList failed\n", stderr)
    exit(1)
}

var count = 0
for src in list {
    guard let id = tisString(src, kTISPropertyInputSourceID) else { continue }
    guard id.hasPrefix("org.mozc.inputmethod.Japanese") else { continue }
    count += 1
}
if count == 0 {
    fputs("ERROR: TISRegisterInputSource succeeded but no org.mozc sources listed\n", stderr)
    exit(1)
}
print(count)
SWIFT

echo "Registered org.mozc.inputmethod.Japanese modes with macOS."
echo
echo "In System Settings → Keyboard → Input Sources → Edit → + :"
echo "  • search or browse for **Japanese**, then pick **marinaMoji**"
echo "  (It may not appear if you search the list for English-only names.)"
echo
echo "If the + dialog still does not show it, quit System Settings and reopen it."
