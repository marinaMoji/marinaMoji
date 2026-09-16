#!/bin/bash
# Field diagnostic for "marinaMoji installed but does not appear in Input Sources".
#
# Safe to run on any user's Mac: read-only apart from one registration attempt.
# Needs no Xcode, no Homebrew, no developer tools. Run as the logged-in user:
#   bash diagnose_marinamoji.sh
set -uo pipefail

APP="/Library/Input Methods/marinaMoji.app"
IMK="${APP}/Contents/MacOS/marinaMoji"
PKG_ID="org.mozc.pkg.JapaneseInput"
DESKTOP="${HOME}/Desktop"

section() { printf '\n=== %s ===\n' "$1"; }

check_packagekit_relocation() {
  section "PackageKit relocation traps"
  local receipt=0 relocated=0
  if pkgutil --pkgs 2>/dev/null | grep -qx "${PKG_ID}"; then
    receipt=1
    echo "Receipt:    ${PKG_ID} present"
    pkgutil --pkg-info "${PKG_ID}" 2>/dev/null | sed 's/^/  /'
  else
    echo "Receipt:    (none)"
  fi

  echo "Expected:   ${APP}"
  if [[ -d "${APP}" ]]; then
    echo "            present"
  else
    echo "            MISSING"
  fi

  echo "Other copies (PackageKit may follow these on upgrade if relocatable):"
  local found=0 path
  while IFS= read -r path; do
    [[ -z "${path}" ]] && continue
    [[ "${path}" == "${APP}" ]] && continue
    echo "  ${path}"
    found=1
    relocated=1
  done < <(mdfind 'kMDItemCFBundleIdentifier == "org.mozc.inputmethod.Japanese"' 2>/dev/null || true)

  # Spotlight can lag; also check common Desktop leftover names.
  local leftover
  for leftover in \
    "${DESKTOP}/marinaMoji.app" \
    "${DESKTOP}"/marinaMoji.app.removed.* \
    "${DESKTOP}"/marinaMoji.app.disabled \
    "${DESKTOP}"/marinaMoji.app.disabled.*; do
    [[ -e "${leftover}" ]] || continue
    echo "  ${leftover}"
    found=1
    relocated=1
  done
  if [[ "${found}" -eq 0 ]]; then
    echo "  (none)"
  fi

  if [[ ! -d "${APP}" && ( "${receipt}" -eq 1 || "${relocated}" -eq 1 ) ]]; then
    echo
    echo "LIKELY CAUSE: the IME is not under /Library/Input Methods/, but a"
    echo "package receipt and/or a Desktop leftover remains. Older packages"
    echo "followed that leftover on reinstall (PackageKit relocation)."
    echo "Fix: delete Desktop marinaMoji.app* leftovers, then:"
    echo "  sudo pkgutil --forget ${PKG_ID}"
    echo "and reinstall. Prefer scrub_marinamoji.sh (deletes; does not move)."
  fi
}

section "Machine"
echo "macOS:      $(sw_vers -productVersion) ($(sw_vers -buildVersion))"
echo "Host arch:  $(uname -m)"
echo "Booted:     $(uptime | sed 's/.*up //; s/,.*users.*//')"
echo "User:       $(id -un) (uid $(id -u))"
echo "Console:    $(stat -f%Su /dev/console)"

check_packagekit_relocation

section "Installed bundle"
if [[ ! -d "${APP}" ]]; then
  echo "MISSING: ${APP} — the package did not install, or was removed."
  exit 1
fi
echo "Version:    $(defaults read "${APP}/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo '?')"
echo "Installed:  $(stat -f%Sm "${APP}")"
# /usr/bin/lipo is a stub without developer tools: it prints an xcode-select
# notice rather than failing, so its output has to be inspected.
ARCH_OUT="$(lipo -archs "${IMK}" 2>/dev/null)"
if [[ -z "${ARCH_OUT}" || "${ARCH_OUT}" == *"xcode-select"* ]]; then
  echo "Binary:     ?(lipo unavailable - no developer tools installed)"
else
  echo "Binary:     ${ARCH_OUT}"
fi
REG_FLAG="$(grep -ac "register_input_source" "${IMK}" 2>/dev/null)"
echo "Reg flag:   ${REG_FLAG:-0}"
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
echo "If it is still missing, log out and log back in once (TIS can succeed"
echo "while the current session still hides the input source)."
