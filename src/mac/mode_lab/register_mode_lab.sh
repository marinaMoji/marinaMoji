#!/bin/bash
# Register ModeLab.app with Text Input Services (same pattern as register_marinamoji.sh).
set -euo pipefail

APP="/Library/Input Methods/ModeLab.app"
USER_APP="${HOME}/Library/Input Methods/ModeLab.app"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
TIS_PREFIX="org.mozc.inputmethod.ModeLab"

if [[ ! -d "${APP}" ]]; then
  echo "ERROR: ${APP} not found. Run install_mode_lab.sh first." >&2
  exit 1
fi

for extra in "${USER_APP}" "${HOME}/Library/Input Methods/ModeLab-test.app"; do
  if [[ -d "${extra}" ]]; then
    echo "WARNING: Removing duplicate install at ${extra}" >&2
    echo "         (multiple copies cause duplicate rows in System Settings)" >&2
    "${LSREGISTER}" -u "${extra}" 2>/dev/null || true
    rm -rf "${extra}"
  fi
done

echo "Registering Mode Lab with Text Input Services..."
"${LSREGISTER}" -f "${APP}"

count="$(swift - "${APP}" "${TIS_PREFIX}" <<'SWIFT'
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
        let status = TISRegisterInputSource(url)
        fputs("TISRegisterInputSource status: \(status)\n", stderr)
        if status != 0 { exit(1) }
    }
}

guard let list = TISCreateInputSourceList(nil, true)?.takeRetainedValue() as? [TISInputSource] else {
    fputs("ERROR: TISCreateInputSourceList failed\n", stderr)
    exit(1)
}

var seen = Set<String>()
var count = 0
for src in list {
    guard let id = tisString(src, kTISPropertyInputSourceID) else { continue }
    guard id.hasPrefix(prefix) else { continue }
    if seen.contains(id) { continue }
    seen.insert(id)
    count += 1
    let name = tisString(src, kTISPropertyLocalizedName) ?? id
    fputs("  \(id) — \(name)\n", stderr)
}
if count == 0 {
    fputs("ERROR: TISRegisterInputSource succeeded but no \(prefix) sources listed\n", stderr)
    exit(1)
}
if count != 6 {
    fputs("WARNING: expected 6 unique sources, got \(count) (duplicate install?)\n", stderr)
}

// Visible hiragana mode must localize to "Mode Lab" for System Settings picker.
for src in list {
    guard let id = tisString(src, kTISPropertyInputSourceID) else { continue }
    guard id == "\(prefix).base" else { continue }
    let name = tisString(src, kTISPropertyLocalizedName) ?? ""
    if name != "Mode Lab" {
        fputs("WARNING: .base mode name is '\(name)', expected 'Mode Lab'\n", stderr)
        fputs("         Reinstall after scrub: bash mac/mode_lab/scrub_mode_lab.sh && bash mac/mode_lab/install_mode_lab.sh\n", stderr)
        fputs("         (macOS caches old TIS metadata until app is removed and re-registered)\n", stderr)
    }
    break
}
print(count)
SWIFT
)"

echo "Registered ${count} unique ${TIS_PREFIX} source(s) with macOS."
echo
echo "Add via System Settings → Keyboard → Input Sources → Edit → + → Japanese → Mode Lab"
echo "Or skip the picker: bash $(dirname "$0")/activate_mode_lab.sh"
echo
echo "If the + dialog still does not show it, quit System Settings and reopen it."
