#!/bin/bash
# Regenerate tools/local_xcode/BUILD from the Xcode installed on THIS machine.
#
# .bazelrc pins //tools/local_xcode:host_xcodes (xcode-locator fails inside
# Bazel on macOS 26+). That file records the SDK versions of whichever Mac last
# wrote it, so it is wrong on any machine with a different Xcode, for example a
# CI runner that is a few SDK releases behind a developer's Mac, and the build
# then fails with: SDK "macosx27.2" cannot be located.
#
# CI runs this before building so the pin always matches the runner. Developers
# can run it after upgrading Xcode. It needs only xcodebuild, not Bazel.
#
# Usage: bash mac/write_local_xcode_config.sh [path/to/tools/local_xcode/BUILD]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
OUT="${1:-${ROOT}/tools/local_xcode/BUILD}"

if ! command -v xcodebuild >/dev/null; then
  echo "ERROR: xcodebuild not found." >&2
  exit 1
fi

# xcodebuild occasionally aborts (exit 134) on a freshly provisioned CI runner,
# so retry it instead of failing the whole job on the first crash.
run_xcodebuild() {
  local out attempt
  for attempt in 1 2 3 4 5; do
    if out="$(xcodebuild "$@" 2>&1)"; then
      printf '%s\n' "${out}"
      return 0
    fi
    echo "xcodebuild $* failed (attempt ${attempt}); retrying" >&2
    sleep 3
  done
  return 1
}

XCODE_VERSION="$(run_xcodebuild -version | awk '/^Xcode / { print $2 }' | head -1)"
SDKS="$(run_xcodebuild -version -sdk || true)"
# Lines look like: "macOS27.2.sdk - macOS 27.2 (macosx27.2)".
sdk() { echo "${SDKS}" | sed -n "s/.*($1\([0-9.][0-9.]*\)).*/\1/p" | head -1; }

macos_sdk="$(sdk macosx)"
if [[ -z "${XCODE_VERSION}" || -z "${macos_sdk}" ]]; then
  echo "ERROR: could not read the Xcode or macOS SDK version from xcodebuild" \
    "(DEVELOPER_DIR=${DEVELOPER_DIR})." >&2
  exit 1
fi

# Other platforms fall back to the macOS value when Xcode lacks the SDK; they
# are not used by this project's macOS-only builds.
ios_sdk="$(sdk iphoneos)"
tvos_sdk="$(sdk appletvos)"
visionos_sdk="$(sdk xros)"
watchos_sdk="$(sdk watchos)"

TARGET_NAME="version${XCODE_VERSION//./_}"
mkdir -p "$(dirname "${OUT}")"
{
  echo 'load("@build_bazel_apple_support//xcode:xcode_config.bzl", "xcode_config")'
  echo 'load("@build_bazel_apple_support//xcode:xcode_version.bzl", "xcode_version")'
  echo
  echo 'package(default_visibility = ["//visibility:public"])'
  echo
  echo 'xcode_version('
  echo "    name = \"${TARGET_NAME}\","
  echo "    default_ios_sdk_version = \"${ios_sdk:-${macos_sdk}}\","
  echo "    default_macos_sdk_version = \"${macos_sdk}\","
  echo "    default_tvos_sdk_version = \"${tvos_sdk:-${macos_sdk}}\","
  echo "    default_visionos_sdk_version = \"${visionos_sdk:-${macos_sdk}}\","
  echo "    default_watchos_sdk_version = \"${watchos_sdk:-${macos_sdk}}\","
  echo "    version = \"${DEVELOPER_DIR}\","
  echo ')'
  echo
  echo 'xcode_config('
  echo '    name = "host_xcodes",'
  echo "    default = \":${TARGET_NAME}\","
  echo "    versions = [\":${TARGET_NAME}\"],"
  echo ')'
} > "${OUT}"

echo "Wrote ${OUT} (Xcode ${XCODE_VERSION}, macOS SDK ${macos_sdk})"
