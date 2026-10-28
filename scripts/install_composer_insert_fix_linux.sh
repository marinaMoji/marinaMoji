#!/usr/bin/env bash
# Install a matched set: IBus engine + mozc_server + mozc_renderer + mozc_tool.
# Installing only the engine against the package server causes
# "incompatible conversion engine" (version .102 client vs .2 server).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${ROOT}/src"

TOOL_BIN="${SRC}/bazel-bin/gui/tool/mozc_tool"
ENGINE_BIN="${SRC}/bazel-bin/unix/ibus/ibus_mozc"
SERVER_BIN="${SRC}/bazel-bin/server/mozc_server"
RENDERER_BIN="${SRC}/bazel-bin/renderer/qt/mozc_renderer"

TOOL_DEST="/usr/lib/marinamoji/mozc_tool"
ENGINE_DEST="/usr/lib/ibus-marinamoji/ibus-engine-marinamoji"
SERVER_DEST="/usr/lib/marinamoji/mozc_server"
RENDERER_DEST="/usr/lib/marinamoji/mozc_renderer"

for p in "${TOOL_BIN}" "${ENGINE_BIN}" "${SERVER_BIN}" "${RENDERER_BIN}"; do
  if [[ ! -e "${p}" ]]; then
    echo "Missing ${p}. Build first:"
    echo "  cd ${SRC} && bazelisk build \\"
    echo "    //gui/tool:mozc_tool //unix/ibus:ibus_mozc \\"
    echo "    //server:mozc_server //renderer/qt:mozc_renderer \\"
    echo "    --config oss_linux --config release_build \\"
    echo "    --repo_env=CC=clang --repo_env=CXX=clang++"
    exit 1
  fi
done

install_one() {
  local src=$1 dest=$2
  echo "Installing $(readlink -f "${src}") -> ${dest}"
  sudo cp -a "$(readlink -f "${src}")" "${dest}.new"
  sudo chmod 755 "${dest}.new"
  sudo mv -f "${dest}.new" "${dest}"
}

echo "Stopping IBus / marinaMoji so binaries can be replaced..."
ibus exit || true
pkill -f 'ibus-engine-marinamoji|mozc_server|mozc_renderer' 2>/dev/null || true
for _ in 1 2 3 4 5 6 7 8 9 10; do
  if ! pgrep -f 'ibus-engine-marinamoji|mozc_server|mozc_renderer' >/dev/null 2>&1; then
    break
  fi
  sleep 0.3
done
pkill -9 -f 'ibus-engine-marinamoji|mozc_server|mozc_renderer' 2>/dev/null || true
sleep 0.2

install_one "${TOOL_BIN}" "${TOOL_DEST}"
install_one "${ENGINE_BIN}" "${ENGINE_DEST}"
install_one "${SERVER_BIN}" "${SERVER_DEST}"
install_one "${RENDERER_BIN}" "${RENDERER_DEST}"

echo "Starting IBus..."
ibus-daemon -drx || true
echo "Done. Re-select marinaMoji in input settings if needed."
echo "Check: toolbar should float/drag again (XWayland), and not steal editor focus."
echo "After Insert, click the editor if the character does not appear (Wayland FocusIn)."
