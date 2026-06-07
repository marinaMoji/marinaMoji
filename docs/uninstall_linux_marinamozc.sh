#!/usr/bin/env bash
# Remove manual marinaMozc (Mozc zip) and/or marinaMoji Rime (ibus-marinamoji) installs.
set -euo pipefail

REMOVE_MOZC=1
REMOVE_RIME=0
REMOVE_USER_CONFIG=0

usage() {
  cat <<EOF
Usage: sudo $0 [OPTIONS]

  Default: remove Mozc-fork only (marinamozc / marinamoji from mozc.zip).

Options:
  --all-marina       Also remove Rime marinaMoji (marina.xml, ibus-marinamoji, …)
  --rime-only        Remove Rime marinaMoji only (skip Mozc-fork paths)
  --remove-config    Also remove user settings (see below)
  -h, --help         Show this help

With --remove-config (as the user who ran sudo, via \$SUDO_USER):
  ~/.config/marinamozc  ~/.config/marinamoji  ~/.config/mozc
  ~/.config/ibus/marinaMoji   (Rime schemas & deploy cache)
  ~/.config/fontconfig/conf.d/70-marinamoji-cjk-fallback.conf

Does NOT remove pacman packages (librime, ibus, …) or BabelStone Han in
/usr/local/share/fonts/truetype/babelstone (optional manual cleanup).
EOF
}

for arg in "$@"; do
  case "$arg" in
    --all-marina) REMOVE_RIME=1 ;;
    --rime-only) REMOVE_MOZC=0; REMOVE_RIME=1 ;;
    --remove-config) REMOVE_USER_CONFIG=1 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $arg" >&2; usage >&2; exit 1 ;;
  esac
done

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "Run with sudo: sudo $0 $*"
  exit 1
fi

remove_mozc_fork() {
  echo "==> Removing Mozc-fork (zip install)…"
  rm -rf /usr/lib/marinamozc
  rm -rf /usr/lib/ibus-marinamozc
  rm -rf /usr/share/ibus-marinamozc
  rm -rf /usr/share/icons/marinamozc
  rm -f  /usr/share/ibus/component/marinamozc.xml

  rm -rf /usr/lib/marinamoji
  rm -rf /usr/lib/ibus-marinamoji
  rm -f  /usr/share/ibus/component/marinamoji.xml
  rm -rf /usr/share/icons/marinamoji

  rm -f /usr/bin/mozc_emacs_helper
  rm -rf /usr/share/emacs/site-lisp/emacs-mozc
}

remove_rime_marinamoji() {
  echo "==> Removing Rime marinaMoji (ibus-marinamoji)…"
  rm -rf /usr/libexec/ibus-marinamoji
  rm -f  /usr/share/ibus/component/marina.xml
  rm -f  /usr/share/rime-data/ibus_marinamoji.yaml
  rm -rf /usr/share/marinamoji
  # Engine / toolbar / menu icons (from CMake install)
  rm -rf /usr/share/ibus-marinamoji

  # Panel icons (hicolor + Adwaita-dark)
  find /usr/share/icons/hicolor -name 'marinamoji.png' -delete 2>/dev/null || true
  find /usr/share/icons/Adwaita-dark -name 'marinamoji.png' -delete 2>/dev/null || true

  if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
  fi
}

remove_user_config() {
  local users=()
  [[ -n "${SUDO_USER:-}" ]] && users+=("$(eval echo "~${SUDO_USER}")")
  users+=("${HOME}")

  echo "==> Removing user config…"
  for u in "${users[@]}"; do
    [[ -n "$u" && -d "$u" ]] || continue
    rm -rf "${u}/.config/marinamozc"
    rm -rf "${u}/.config/marinamoji"
    rm -rf "${u}/.config/mozc"
    rm -rf "${u}/.config/ibus/marinaMoji"
    rm -f  "${u}/.config/fontconfig/conf.d/70-marinamoji-cjk-fallback.conf"
  done

  if command -v fc-cache >/dev/null 2>&1; then
    fc-cache -f >/dev/null 2>&1 || true
  fi
}

stop_sync_daemon() {
  if ! command -v systemctl >/dev/null 2>&1; then
    return 0
  fi

  local run_user="${SUDO_USER:-}"
  if [[ -n "$run_user" && "$run_user" != "root" ]]; then
    echo "==> Stopping sync daemon (as ${run_user})…"
    local uid
    uid="$(id -u "$run_user")"
    local runtime_dir="/run/user/${uid}"
    if [[ -d "$runtime_dir" ]]; then
      sudo -u "$run_user" env XDG_RUNTIME_DIR="$runtime_dir" \
        systemctl --user disable --now marinamoji-sync.service 2>/dev/null || true
      sudo -u "$run_user" rm -f \
        "$(eval echo "~${run_user}")/.config/systemd/user/marinamoji-sync.service" \
        2>/dev/null || true
      sudo -u "$run_user" env XDG_RUNTIME_DIR="$runtime_dir" \
        systemctl --user daemon-reload 2>/dev/null || true
    else
      echo "    Skipped sync daemon stop (no session at ${runtime_dir})."
      echo "    Run as yourself: systemctl --user disable --now marinamoji-sync.service"
    fi
  elif [[ -n "${XDG_RUNTIME_DIR:-}" ]]; then
    systemctl --user disable --now marinamoji-sync.service 2>/dev/null || true
    rm -f "${HOME}/.config/systemd/user/marinamoji-sync.service" 2>/dev/null || true
    systemctl --user daemon-reload 2>/dev/null || true
  fi
}

refresh_ibus() {
  if ! command -v ibus >/dev/null 2>&1; then
    return 0
  fi

  # ibus must run in the user's session, not root's (no XDG_RUNTIME_DIR under sudo).
  local run_user="${SUDO_USER:-}"
  if [[ -n "$run_user" && "$run_user" != "root" ]]; then
    echo "==> Refreshing IBus cache (as ${run_user})…"
    local uid
    uid="$(id -u "$run_user")"
    local runtime_dir="/run/user/${uid}"
    if [[ -d "$runtime_dir" ]]; then
      sudo -u "$run_user" env XDG_RUNTIME_DIR="$runtime_dir" \
        ibus write-cache 2>/dev/null || true
      sudo -u "$run_user" env XDG_RUNTIME_DIR="$runtime_dir" \
        ibus restart 2>/dev/null || true
    else
      echo "    Skipped ibus restart (no session at ${runtime_dir})."
      echo "    After uninstall, run as yourself: ibus write-cache && ibus restart"
    fi
  elif [[ -n "${XDG_RUNTIME_DIR:-}" ]]; then
    ibus write-cache 2>/dev/null || true
    ibus restart 2>/dev/null || true
  else
    echo "==> Skipped ibus restart (not in a desktop session)."
    echo "    Run as yourself: ibus write-cache && ibus restart"
  fi
}

[[ "$REMOVE_MOZC" -eq 1 ]] && stop_sync_daemon
[[ "$REMOVE_MOZC" -eq 1 ]] && remove_mozc_fork
[[ "$REMOVE_RIME" -eq 1 ]] && remove_rime_marinamoji
[[ "$REMOVE_USER_CONFIG" -eq 1 ]] && remove_user_config

refresh_ibus

echo ""
echo "Done. Remove stale input sources in Settings → Keyboard → Input Sources if needed."
echo "Optional: sudo rm -rf /usr/local/share/fonts/truetype/babelstone  (BabelStone Han from Rime installer)"
