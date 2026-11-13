#!/bin/bash
# Starts an X session for screenshots and tests in a container:
# Xvfb on :1, the system D-Bus with the mock NetworkManager, DCOP and twin.
export DISPLAY=:1 PATH=/opt/trinity/bin:$PATH HOME=${HOME:-/root}
export XDG_CONFIG_DIRS=/opt/trinity/etc/xdg:/etc/xdg XDG_DATA_DIRS=/opt/trinity/share:/usr/share
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
pgrep -x Xvfb >/dev/null || (setsid Xvfb :1 -screen 0 ${SCREEN:-1280x900x24} -nolisten tcp >/dev/null 2>&1 &)
sleep 1
mkdir -p /run/dbus
[ -S /run/dbus/system_bus_socket ] || dbus-daemon --system --fork
nm_running() {
	dbus-send --system --print-reply --dest=org.freedesktop.DBus / \
		org.freedesktop.DBus.NameHasOwner string:org.freedesktop.NetworkManager 2>/dev/null | grep -q true
}
if ! nm_running; then
	setsid python3 "$HERE/tests/mock-nm.py" > /tmp/mock-nm.log 2>&1 < /dev/null &
	echo $! > /tmp/mock-nm.pid
	sleep 2
fi
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ] && [ -f /tmp/session-bus ]; then
	. /tmp/session-bus
fi
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
	dbus-launch --sh-syntax > /tmp/session-bus
	. /tmp/session-bus
fi
pgrep -x dcopserver >/dev/null || (setsid tdeinit >/dev/null 2>&1 &)
sleep 3
pgrep -x twin >/dev/null || (setsid twin >/dev/null 2>&1 &)
xsetroot -solid "#3a6ea5"
sleep 1
