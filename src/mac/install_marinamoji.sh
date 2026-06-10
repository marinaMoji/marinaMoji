#!/bin/bash
# Install marinaMoji.app from the latest Bazel build and verify the converter
# binary actually changed (catches stale installs when archive-root was missing).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_SRC="$ROOT/bazel-bin/mac/mozc_macos_archive-root/marinaMoji.app"
APP_DST="/Library/Input Methods/marinaMoji.app"
CONVERTER_REL="Contents/Resources/marinaMojiConverter.app/Contents/MacOS/marinaMojiConverter"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"

if [[ ! -d "$APP_SRC" ]]; then
  echo "ERROR: Build output not found at:" >&2
  echo "  $APP_SRC" >&2
  echo "Run from src/:" >&2
  echo "  export MOZC_QT_PATH=/opt/homebrew/opt/qt" >&2
  echo "  bazelisk build --config oss_macos //mac:mozc_macos" >&2
  exit 1
fi

NEW_HASH="$(shasum -a 256 "$APP_SRC/$CONVERTER_REL" | awk '{print $1}')"
if [[ -f "$APP_DST/$CONVERTER_REL" ]]; then
  OLD_HASH="$(shasum -a 256 "$APP_DST/$CONVERTER_REL" | awk '{print $1}')"
else
  OLD_HASH=""
fi

echo "New build converter hash: $NEW_HASH"
if [[ -n "$OLD_HASH" ]]; then
  echo "Installed converter hash: $OLD_HASH"
fi

sudo ditto "$APP_SRC" "$APP_DST"
sudo chown -R root:wheel "$APP_DST"
sudo chmod -R go-w "$APP_DST"
sudo touch "$APP_DST" "$APP_DST/Contents/Info.plist"
"$LSREGISTER" -f "$APP_DST"
bash "$ROOT/mac/install_launchagents.sh"
bash "$ROOT/mac/register_marinamoji.sh"
sudo bash "$ROOT/mac/fix_qt_bundled_paths.sh" "$APP_DST" "-"

INSTALLED_HASH="$(shasum -a 256 "$APP_DST/$CONVERTER_REL" | awk '{print $1}')"
if [[ "$INSTALLED_HASH" != "$NEW_HASH" ]]; then
  echo "ERROR: Install verification failed — installed hash still differs from build." >&2
  exit 1
fi

echo "Install verified: converter hash matches build."

killall imklaunchagent 2>/dev/null || true
killall TextInputMenuAgent 2>/dev/null || true
killall marinaMoji 2>/dev/null || true
killall marinaMojiConverter 2>/dev/null || true
killall marinaMojiRenderer 2>/dev/null || true
killall DictionaryTool 2>/dev/null || true
killall WordRegisterDialog 2>/dev/null || true

echo "Done. Select marinaMoji in Input Sources, then verify registration:"
echo '  defaults read com.apple.HIToolbox AppleSelectedInputSources | rg -i "org.mozc|Japanese"'
echo "(On recent macOS, AppleEnabledInputSources may stay empty for third-party IMEs — Selected is what matters.)"
echo '  bash ./mac/activate_marinamoji.sh   # if conversion still fails'
