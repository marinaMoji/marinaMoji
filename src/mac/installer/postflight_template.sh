#!/bin/sh
CONSOLE_USER=`/usr/bin/stat -f%Su /dev/console`
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall marinaMojiConverter > /dev/null
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall marinaMojiRenderer > /dev/null
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall marinaMoji > /dev/null
FIX="/Library/Input Methods/marinaMoji.app/Contents/Resources/fix_qt_bundled_paths.sh"
if [ -x "$FIX" ]; then
  /bin/bash "$FIX" "/Library/Input Methods/marinaMoji.app" "-"
fi
/usr/bin/true
