#!/bin/sh
# Install the marinaMoji sync daemon as a systemd user service.
# Run as your normal user after installing mozc.zip (do not use sudo).
set -e

if [ "$(id -u)" -eq 0 ]; then
  echo "Do not run this script with sudo. Run as your normal user:" >&2
  echo "  ./unix/install_sync_daemon.sh" >&2
  exit 1
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
UNIT_SRC="${SCRIPT_DIR}/systemd/marinamoji-sync.service"
INSTALLED_UNIT="/usr/lib/marinamoji/marinamoji-sync.service"

if [ ! -f "${UNIT_SRC}" ] && [ -f "${INSTALLED_UNIT}" ]; then
  UNIT_SRC="${INSTALLED_UNIT}"
fi

if [ ! -f "${UNIT_SRC}" ]; then
  echo "Missing unit file. Expected:" >&2
  echo "  ${SCRIPT_DIR}/systemd/marinamoji-sync.service" >&2
  echo "  or ${INSTALLED_UNIT}" >&2
  exit 1
fi

if [ ! -x /usr/lib/marinamoji/mozc_sync ]; then
  echo "mozc_sync not found at /usr/lib/marinamoji/mozc_sync." >&2
  echo "Install mozc.zip first: sudo unzip -o bazel-bin/unix/mozc.zip -d /" >&2
  exit 1
fi

USER_UNIT_DIR="${HOME}/.config/systemd/user"
mkdir -p "${USER_UNIT_DIR}"
cp "${UNIT_SRC}" "${USER_UNIT_DIR}/marinamoji-sync.service"

systemctl --user daemon-reload
systemctl --user enable --now marinamoji-sync.service

echo "Sync daemon installed. Checking status..."
systemctl --user status marinamoji-sync.service --no-pager || true
pgrep -lf mozc_sync || echo "  (mozc_sync not listed yet; it starts on demand)"
