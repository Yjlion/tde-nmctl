#!/bin/bash
# Runs the client test against the mock NetworkManager on a private system
# bus. Usage: tests/run-tests.sh <path to clienttest> [<path to tde-nmctl>]
# Needs root (to own org.freedesktop.NetworkManager on the system bus),
# python3-dbus and python3-gi.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
TEST=$1
NMCTL=$2
mkdir -p /run/dbus
if [ ! -S /run/dbus/system_bus_socket ]; then
	dbus-daemon --system --fork
	sleep 0.5
fi
[ -f /tmp/mock-nm.pid ] && kill "$(cat /tmp/mock-nm.pid)" 2>/dev/null || true
python3 "$HERE/mock-nm.py" > /tmp/mock-nm.log 2>&1 &
MOCK=$!
echo $MOCK > /tmp/mock-nm.pid
trap 'kill $MOCK 2>/dev/null || true' EXIT
for i in $(seq 1 50); do
	grep -q ready /tmp/mock-nm.log && break
	sleep 0.1
done
export HOME=$(mktemp -d)
"$TEST"
if [ -n "$NMCTL" ]; then
	"$NMCTL" --list | tee /tmp/nmctl-list.txt
	grep -q "HomeNet" /tmp/nmctl-list.txt
	grep -q "Wired connection 1" /tmp/nmctl-list.txt
	"$NMCTL" --show "Work VPN" | grep -q "vpn.service-type: 'org.freedesktop.NetworkManager.openvpn'"
fi
echo "all tests passed"
