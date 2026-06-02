#!/bin/sh
CONSOLE_USER=`/usr/bin/stat -f%Su /dev/console`
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall marinaMojiConverter > /dev/null
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall marinaMojiRenderer > /dev/null
/usr/bin/sudo -u $CONSOLE_USER /usr/bin/killall marinaMoji > /dev/null
/usr/bin/true
