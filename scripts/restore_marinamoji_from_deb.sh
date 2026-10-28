#!/usr/bin/env bash
# Put the packaged binaries back so the IBus engine and mozc_server match again.
set -euo pipefail
DEB="${1:-}"
if [[ -z "${DEB}" ]]; then
  for c in \
    /tmp/marinamoji_0.0.18-rc14_arm64.deb \
    /home/d/code/marinaMoji/marinamoji_0.0.18-rc14_arm64.deb; do
    [[ -f "$c" ]] && DEB=$c && break
  done
fi
if [[ -z "${DEB}" || ! -f "${DEB}" ]]; then
  echo "Usage: $0 /path/to/marinamoji_*.deb"
  exit 1
fi
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
dpkg-deb -x "${DEB}" "${TMP}"

echo "Stopping IBus / marinaMoji processes..."
ibus exit || true
pkill -f 'ibus-engine-marinamoji|mozc_server|mozc_renderer|mozc_tool' 2>/dev/null || true
sleep 0.5

install_one() {
  local src=$1 dest=$2
  echo "Restoring ${dest}"
  sudo cp -a "${src}" "${dest}.new"
  sudo chmod 755 "${dest}.new"
  sudo mv -f "${dest}.new" "${dest}"
}

install_one "${TMP}/usr/lib/ibus-marinamoji/ibus-engine-marinamoji" \
  /usr/lib/ibus-marinamoji/ibus-engine-marinamoji
install_one "${TMP}/usr/lib/marinamoji/mozc_server" /usr/lib/marinamoji/mozc_server
install_one "${TMP}/usr/lib/marinamoji/mozc_renderer" /usr/lib/marinamoji/mozc_renderer
install_one "${TMP}/usr/lib/marinamoji/mozc_tool" /usr/lib/marinamoji/mozc_tool

echo "Starting IBus..."
ibus-daemon -drx || true
echo "Restored from ${DEB}. marinaMoji should work again (package versions)."
