#!/usr/bin/env bash
# Install a freshly built mozc_tool (includes character composer → ONLYOFFICE Linux hand-off).
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)/src"
BIN="${SRC}/bazel-bin/gui/tool/mozc_tool"
DEST="/usr/lib/marinamoji/mozc_tool"
if [[ ! -x "${BIN}" ]]; then
  echo "Build first from marinaMoji/src:"
  echo "  bazelisk build //gui/tool:mozc_tool --config oss_linux --config release_build --repo_env=CC=clang --repo_env=CXX=clang++"
  exit 1
fi
sudo cp "${BIN}" "${DEST}"
sudo chmod 755 "${DEST}"
echo "Installed ${DEST}"
strings "${DEST}" | grep -F 'onlyoffice/desktopeditors' >/dev/null \
  && echo "OK: Linux ONLYOFFICE hand-off path is present." \
  || echo "WARNING: Linux ONLYOFFICE path string not found in binary."
