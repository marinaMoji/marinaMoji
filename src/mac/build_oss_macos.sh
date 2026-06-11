#!/bin/bash
# Build marinaMoji for macOS (//mac:mozc_macos).  On macOS 26+, Bazel's
# xcode-locator fails inside the darwin sandbox unless fix_bazel_xcode_config.sh
# runs first in the same shell.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "${ROOT}"

export MOZC_QT_PATH="${MOZC_QT_PATH:-/opt/homebrew/opt/qt}"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"

bash mac/fix_bazel_xcode_config.sh

TARGET="${1:-//mac:mozc_macos}"
shift || true
# --spawn_strategy=local is also set in .bazelrc (macos_env); repeat here for clarity.
bazelisk build --config=oss_macos --spawn_strategy=local "${TARGET}" "$@"
