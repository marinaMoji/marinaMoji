#!/bin/bash
# Install marinaMoji.app from the latest Bazel build and verify the converter
# binary actually changed (catches stale installs when archive-root was missing).
# Converter/renderer/sync register via SMAppService when the IME starts; this
# script only clears leftover external LaunchAgent copies from older installs.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_SRC="$ROOT/bazel-bin/mac/mozc_macos_archive-root/marinaMoji.app"
APP_DST="/Library/Input Methods/marinaMoji.app"
CONVERTER_REL="Contents/Resources/marinaMojiConverter.app/Contents/MacOS/marinaMojiConverter"
IMK_REL="Contents/MacOS/marinaMoji"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"

if [[ ! -d "$APP_SRC" ]]; then
  echo "ERROR: Build output not found at:" >&2
  echo "  $APP_SRC" >&2
  echo "Run from src/:" >&2
  echo "  export MOZC_QT_PATH=/opt/homebrew/opt/qt" >&2
  echo "  bash sync_marina_opencc.sh   # after updating OpenCC tables" >&2
  echo "  bazelisk build --config oss_macos //mac:mozc_macos" >&2
  exit 1
fi

NEW_CONVERTER_HASH="$(shasum -a 256 "$APP_SRC/$CONVERTER_REL" | awk '{print $1}')"
NEW_IMK_HASH="$(shasum -a 256 "$APP_SRC/$IMK_REL" | awk '{print $1}')"
if [[ -f "$APP_DST/$CONVERTER_REL" ]]; then
  OLD_CONVERTER_HASH="$(shasum -a 256 "$APP_DST/$CONVERTER_REL" | awk '{print $1}')"
else
  OLD_CONVERTER_HASH=""
fi
if [[ -f "$APP_DST/$IMK_REL" ]]; then
  OLD_IMK_HASH="$(shasum -a 256 "$APP_DST/$IMK_REL" | awk '{print $1}')"
else
  OLD_IMK_HASH=""
fi

echo "New build converter hash: $NEW_CONVERTER_HASH"
echo "New build IMK hash: $NEW_IMK_HASH"
if [[ -n "$OLD_CONVERTER_HASH" ]]; then
  echo "Installed converter hash: $OLD_CONVERTER_HASH"
fi
if [[ -n "$OLD_IMK_HASH" ]]; then
  echo "Installed IMK hash: $OLD_IMK_HASH"
fi

# Replace atomically: ditto into an existing tree merges leftovers and can
# break the code-signature seal ("sealed resource is missing or invalid").
sudo rm -rf "$APP_DST"
sudo ditto "$APP_SRC" "$APP_DST"
sudo chown -R root:wheel "$APP_DST"
sudo chmod -R go-w "$APP_DST"
sudo touch "$APP_DST" "$APP_DST/Contents/Info.plist"
"$LSREGISTER" -f "$APP_DST"
# Converter/renderer/sync register via SMAppService inside the app (main.mm).
# Drop any leftover external LaunchAgent copies from older installs.
bash "$ROOT/mac/install_launchagents.sh"
bash "$ROOT/mac/register_marinamoji.sh"

# Prefer an explicit identity, then Developer ID Application from the keychain.
# Ad-hoc "-" is last resort only: on macOS 26+ SMAppService helpers need a
# notarized Developer ID build (use a GitHub Releases .pkg for day-to-day).
SIGN_IDENTITY="${MARINAMOJI_CODESIGN_IDENTITY:-}"
if [[ -z "${SIGN_IDENTITY}" ]]; then
  SIGN_IDENTITY="$(
    security find-identity -v -p codesigning 2>/dev/null |
      sed -n 's/.*"\(Developer ID Application:[^"]*\)".*/\1/p' | head -1
  )"
fi
if [[ -n "${SIGN_IDENTITY}" ]]; then
  echo "Re-signing Qt tools with: ${SIGN_IDENTITY}"
  sudo bash "$ROOT/mac/fix_qt_bundled_paths.sh" "$APP_DST" "${SIGN_IDENTITY}"
  echo "NOTE: SMAppService Converter/Renderer still require Apple notarization."
  echo "      Unnotarized Developer ID installs fail with Launch Constraint Violation."
  echo "      For a working local IME, install a notarized .pkg from GitHub Releases,"
  echo "      or notarize this build before selecting marinaMoji."
else
  echo "WARNING: no Developer ID identity found; signing Qt tools ad-hoc (-)."
  echo "         Converter/Renderer will likely fail on macOS 26+ (exit 78)."
  sudo bash "$ROOT/mac/fix_qt_bundled_paths.sh" "$APP_DST" "-"
fi

INSTALLED_CONVERTER_HASH="$(shasum -a 256 "$APP_DST/$CONVERTER_REL" | awk '{print $1}')"
INSTALLED_IMK_HASH="$(shasum -a 256 "$APP_DST/$IMK_REL" | awk '{print $1}')"
if [[ "$INSTALLED_CONVERTER_HASH" != "$NEW_CONVERTER_HASH" ]]; then
  echo "ERROR: Install verification failed — converter hash still differs from build." >&2
  exit 1
fi
if [[ "$INSTALLED_IMK_HASH" != "$NEW_IMK_HASH" ]]; then
  echo "ERROR: Install verification failed — IMK hash still differs from build." >&2
  exit 1
fi

echo "Install verified: converter and IMK hashes match build."

killall imklaunchagent 2>/dev/null || true
killall TextInputMenuAgent 2>/dev/null || true
killall marinaMoji 2>/dev/null || true
killall marinaMojiConverter 2>/dev/null || true
killall marinaMojiRenderer 2>/dev/null || true
killall DictionaryTool 2>/dev/null || true
killall WordRegisterDialog 2>/dev/null || true
killall CharacterComposer 2>/dev/null || true

echo "Done. Select marinaMoji in Input Sources, then verify registration:"
echo '  defaults read com.apple.HIToolbox AppleSelectedInputSources | rg -i "org.mozc|Japanese"'
echo "(On recent macOS, AppleEnabledInputSources may stay empty for third-party IMEs — Selected is what matters.)"
echo '  bash ./mac/activate_marinamoji.sh   # if conversion still fails'
