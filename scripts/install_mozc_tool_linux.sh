#!/usr/bin/env bash
# Install a freshly built mozc_tool (character composer → ONLYOFFICE Linux hand-off).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${ROOT}/src"
BIN="${SRC}/bazel-bin/gui/tool/mozc_tool"
DEST="/usr/lib/marinamoji/mozc_tool"

if [[ ! -e "${BIN}" ]]; then
  echo "Binary missing. Build first:"
  echo "  cd ${SRC} && bazelisk build //gui/tool:mozc_tool --config oss_linux --config release_build --repo_env=CC=clang --repo_env=CXX=clang++"
  exit 1
fi

# Resolve through bazel-bin symlink so we copy the real file.
REAL="$(readlink -f "${BIN}")"
echo "Installing ${REAL}"
echo "     -> ${DEST}"
sudo cp -a "${REAL}" "${DEST}"
sudo chmod 755 "${DEST}"
echo "Installed ${DEST}"
if strings "${DEST}" | grep -F 'onlyoffice/desktopeditors' >/dev/null; then
  echo "OK: Linux ONLYOFFICE hand-off path is present."
else
  echo "WARNING: Linux ONLYOFFICE path string not found in binary."
fi
