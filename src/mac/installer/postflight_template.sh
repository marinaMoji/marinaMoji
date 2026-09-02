#!/bin/sh
# postinstall script for the marinaMoji package.
#
# Text Input Services registration is per-user and only takes effect inside the
# console user's GUI session, so it cannot be done directly from this script,
# which runs as root. The optional ActivatePane installer plugin normally does
# it, but it is silently skipped whenever the plugin does not load, and on a Mac
# whose login session has been alive for weeks (never restarted, never updated)
# the input-source agents keep serving cached state, so marinaMoji never shows
# up under System Settings -> Keyboard -> Input Sources until a logout.
#
# This script therefore performs the registration itself, in the console user's
# session, and verifies that macOS lists the input sources afterwards. It never
# fails the installation: a failure here is reported to /var/log/install.log and
# the user can still add the input source manually after a logout.

APP="/Library/Input Methods/marinaMoji.app"
IMK="${APP}/Contents/MacOS/marinaMoji"
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
LAUNCH_AGENTS="/Library/LaunchAgents"
AGENTS="org.mozc.inputmethod.Japanese.Converter org.mozc.inputmethod.Japanese.Renderer org.mozc.inputmethod.Japanese.Sync"

CONSOLE_USER=`/usr/bin/stat -f%Su /dev/console`
CONSOLE_UID=`/usr/bin/id -u "${CONSOLE_USER}" 2>/dev/null`

log() {
  echo "marinaMoji postinstall: $*"
}

# Runs a command in the console user's GUI (Aqua) session. `sudo -u` alone is
# not enough: Text Input Services and launchd need the per-user bootstrap
# namespace that `launchctl asuser` joins.
as_console_user() {
  /bin/launchctl asuser "${CONSOLE_UID}" /usr/bin/sudo -u "${CONSOLE_USER}" "$@"
}

HAVE_CONSOLE_USER=1
if [ -z "${CONSOLE_USER}" ] || [ "${CONSOLE_USER}" = "root" ] || [ -z "${CONSOLE_UID}" ]; then
  # Installed from the login window or a management tool with nobody logged in.
  # Registration will happen at the next login instead.
  HAVE_CONSOLE_USER=0
  log "no console user; skipping per-user input source registration"
fi

if [ "${HAVE_CONSOLE_USER}" = "1" ]; then
  /usr/bin/sudo -u "${CONSOLE_USER}" /usr/bin/killall marinaMojiConverter > /dev/null 2>&1
  /usr/bin/sudo -u "${CONSOLE_USER}" /usr/bin/killall marinaMojiRenderer > /dev/null 2>&1
  /usr/bin/sudo -u "${CONSOLE_USER}" /usr/bin/killall marinaMoji > /dev/null 2>&1
fi

FIX="${APP}/Contents/Resources/fix_qt_bundled_paths.sh"
if [ -x "${FIX}" ]; then
  /bin/bash "${FIX}" "${APP}" "-"
fi

# Refresh the LaunchServices record for the freshly installed bundle. Without
# this the system may keep serving metadata cached from a previous version.
if [ -x "${LSREGISTER}" ]; then
  "${LSREGISTER}" -f "${APP}" > /dev/null 2>&1
  if [ "${HAVE_CONSOLE_USER}" = "1" ]; then
    as_console_user "${LSREGISTER}" -f "${APP}" > /dev/null 2>&1
  fi
fi

if [ "${HAVE_CONSOLE_USER}" = "1" ]; then
  # Load the converter/renderer/sync agents now instead of waiting for the next
  # login. ActivatePane does this too, when it runs.
  for agent in ${AGENTS}; do
    plist="${LAUNCH_AGENTS}/${agent}.plist"
    [ -f "${plist}" ] || continue
    as_console_user /bin/launchctl bootstrap "gui/${CONSOLE_UID}" "${plist}" > /dev/null 2>&1 ||
      as_console_user /bin/launchctl load -S Aqua "${plist}" > /dev/null 2>&1
  done

  # Make the input source visible without a logout. Retried because the input
  # source list can lag right after the bundle is written.
  REGISTERED=0
  if [ -x "${IMK}" ]; then
    attempt=1
    while [ "${attempt}" -le 3 ]; do
      if as_console_user "${IMK}" --register_input_source > /dev/null 2>&1; then
        REGISTERED=1
        break
      fi
      # Restart the agents that own the stale input-source caches; launchd
      # brings both back automatically.
      as_console_user /usr/bin/killall TextInputMenuAgent > /dev/null 2>&1
      as_console_user /usr/bin/killall imklaunchagent > /dev/null 2>&1
      /bin/sleep 2
      attempt=`expr ${attempt} + 1`
    done
  else
    log "ERROR: ${IMK} is missing"
  fi

  if [ "${REGISTERED}" = "1" ]; then
    log "input sources registered for ${CONSOLE_USER}"
    # Nudge the input menu and System Settings so the new source shows up in an
    # already-running session.
    as_console_user /usr/bin/killall TextInputMenuAgent > /dev/null 2>&1
    as_console_user /usr/bin/killall imklaunchagent > /dev/null 2>&1
  else
    log "WARNING: could not register input sources for ${CONSOLE_USER};"
    log "         log out and back in, then add marinaMoji under"
    log "         System Settings -> Keyboard -> Input Sources."
  fi
fi

/usr/bin/true
