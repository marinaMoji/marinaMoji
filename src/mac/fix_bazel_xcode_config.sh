#!/bin/bash
# Repair Bazel's local_config_xcode after `bazel clean --expunge` when xcode-locator
# fails inside Bazel's repo rule (kLSExecutableIncorrectFormat).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"

if ! command -v xcodebuild >/dev/null; then
  echo "ERROR: xcodebuild not found. Install Xcode and run:" >&2
  echo "  sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
  exit 1
fi

OUTPUT_BASE="$(cd "${ROOT}" && bazelisk info output_base 2>/dev/null)" || {
  echo "ERROR: bazelisk info output_base failed. Run from ${ROOT}" >&2
  exit 1
}

XCODE_REPO="${OUTPUT_BASE}/external/bazel_tools+xcode_configure_extension+local_config_xcode"
mkdir -p "${XCODE_REPO}"

LOCATOR_LINE="$(env DEVELOPER_DIR="${DEVELOPER_DIR}" \
  "${XCODE_REPO}/xcode-locator-bin" -v 2>/dev/null || true)"
if [[ -z "${LOCATOR_LINE}" ]]; then
  # Build a fresh locator from bazel's source if missing.
  LOCATOR_SRC="${OUTPUT_BASE}/external/bazel_tools/tools/osx/xcode_locator.m"
  if [[ -f "${LOCATOR_SRC}" ]]; then
    env DEVELOPER_DIR="${DEVELOPER_DIR}" xcrun --sdk macosx clang -mmacosx-version-min=10.13 \
      -fobjc-arc -framework CoreServices -framework Foundation \
      -o "${XCODE_REPO}/xcode-locator-bin" "${LOCATOR_SRC}"
    LOCATOR_LINE="$("${XCODE_REPO}/xcode-locator-bin" -v 2>/dev/null || true)"
  fi
fi

# On macOS 26+, xcode-locator often fails with kLSApplicationNotFoundErr even
# outside the sandbox. Fall back to xcodebuild + DEVELOPER_DIR.
if [[ -z "${LOCATOR_LINE}" ]]; then
  XB_VER="$(env DEVELOPER_DIR="${DEVELOPER_DIR}" xcodebuild -version 2>/dev/null \
    | awk '/^Xcode / { print $2; exit }')"
  XB_BUILD="$(env DEVELOPER_DIR="${DEVELOPER_DIR}" xcodebuild -version 2>/dev/null \
    | awk '/^Build version / { print $3; exit }')"
  if [[ -z "${XB_VER}" ]]; then
    echo "ERROR: xcode-locator failed and xcodebuild -version returned no version." >&2
    exit 1
  fi
  LOCATOR_LINE="${XB_VER}.0.${XB_BUILD:-unknown}:${XB_VER}:${DEVELOPER_DIR}"
  echo "NOTE: xcode-locator unavailable; using xcodebuild fallback:"
  echo "  ${LOCATOR_LINE}"
fi

IFS=: read -r VERSION _ALIASES DEVDIR <<< "${LOCATOR_LINE}"
TARGET_NAME="version${VERSION//./_}"
# Use the developer-dir path as |version| so Bazel skips xcode-locator at action time
# (LaunchServices fails in Bazel's darwin sandbox on macOS 26+).
VERSION_FIELD="${DEVDIR:-${DEVELOPER_DIR}}"

SDK_OUT="$(env DEVELOPER_DIR="${DEVELOPER_DIR}" xcrun xcodebuild -version -sdk 2>/dev/null)"
ios_sdk="$(echo "${SDK_OUT}" | sed -n 's/.*iphoneos\([0-9.]*\).*/\1/p' | head -1)"
macos_sdk="$(echo "${SDK_OUT}" | sed -n 's/.*macosx\([0-9.]*\).*/\1/p' | head -1)"
tvos_sdk="$(echo "${SDK_OUT}" | sed -n 's/.*appletvos\([0-9.]*\).*/\1/p' | head -1)"
watchos_sdk="$(echo "${SDK_OUT}" | sed -n 's/.*watchos\([0-9.]*\).*/\1/p' | head -1)"
visionos_sdk="$(echo "${SDK_OUT}" | sed -n 's/.*xros\([0-9.]*\).*/\1/p' | head -1)"

cat > "${XCODE_REPO}/BUILD" <<EOF
package(default_visibility = ["//visibility:public"])

load("@apple_support//xcode:xcode_config.bzl", "xcode_config")
load("@apple_support//xcode:xcode_version.bzl", "xcode_version")

xcode_version(
    name = "${TARGET_NAME}",
    version = "${VERSION_FIELD}",
$( [[ -n "${ios_sdk}" ]] && echo "    default_ios_sdk_version = \"${ios_sdk}\"," )
$( [[ -n "${tvos_sdk}" ]] && echo "    default_tvos_sdk_version = \"${tvos_sdk}\"," )
$( [[ -n "${macos_sdk}" ]] && echo "    default_macos_sdk_version = \"${macos_sdk}\"," )
$( [[ -n "${visionos_sdk}" ]] && echo "    default_visionos_sdk_version = \"${visionos_sdk}\"," )
$( [[ -n "${watchos_sdk}" ]] && echo "    default_watchos_sdk_version = \"${watchos_sdk}\"," )
)

xcode_config(
    name = "host_xcodes",
    versions = [":${TARGET_NAME}"],
    default = ":${TARGET_NAME}",
)
EOF

LOCAL_XCODE="${ROOT}/tools/local_xcode/BUILD"
if [[ "${1:-}" == "--write-local" ]]; then
  cp "${XCODE_REPO}/BUILD" "${LOCAL_XCODE}"
  sed -i '' 's/@apple_support/@build_bazel_apple_support/g' "${LOCAL_XCODE}"
  echo "Updated ${LOCAL_XCODE}"
fi

echo "Repaired Bazel Xcode config at:"
echo "  ${XCODE_REPO}/BUILD"
echo "  Xcode ${VERSION}"
echo
echo "Run the build immediately in the same shell:"
echo "  bash mac/build_oss_macos.sh"
echo "Or manually:"
echo "  cd ${ROOT} && export MOZC_QT_PATH=/opt/homebrew/opt/qt && bazelisk build --config oss_macos //mac:mozc_macos"
echo
echo "If build still fails with 'Could not determine Xcode version', open Terminal.app"
echo "(not Cursor), export DEVELOPER_DIR, and avoid 'bazel clean --expunge'."
