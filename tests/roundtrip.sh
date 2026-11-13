#!/bin/bash
# Opens each connection of the mock in the editor, presses OK without
# changing anything and compares what NetworkManager (the mock) stores
# before and after. Run inside the X session of tools/session.sh.
export DISPLAY=${DISPLAY:-:1} PATH=/opt/trinity/bin:$PATH
fail=0
# with REAL_NM=1 the settings are read with nmcli from a real NetworkManager
dump() {
	if [ -n "$REAL_NM" ]; then
		nmcli --show-secrets -t -f all connection show "$1" | grep -v -E '^(GENERAL|IP4|IP6|DHCP4|DHCP6|VPN)\.' \
			| grep -v -E '^connection\.(timestamp|read-only|path):'
	else
		dbus-send --system --print-reply=literal --dest=org.freedesktop.NetworkManager \
			/org/freedesktop/NetworkManager org.trinitydesktop.NmctlMock.ConnectionDump string:"$1"
	fi
}
compare() {
	if [ -n "$REAL_NM" ]; then
		diff <(echo "$1") <(echo "$2") | sed 's/^/  /'
		[ "$1" = "$2" ]
	else
		python3 /src/tests/compare-settings.py "$1" "$2"
	fi
}
# answers TDE Wallet's password prompt (the wallet of the screenshot
# session uses the password in $WALLET_PASSWORD)
unlock_wallet() {
	local w
	w=$(xdotool search --onlyvisible --name "TDE Wallet Service" 2>/dev/null | head -1)
	[ -z "$w" ] && return 1
	xdotool windowactivate --sync "$w" 2>/dev/null
	sleep 0.3
	xdotool type --delay 30 "${WALLET_PASSWORD:-walletpw1}"
	# a new wallet asks for the password twice
	if xdotool search --onlyvisible --name "TDE Wallet Service" >/dev/null 2>&1; then
		xdotool key Tab; xdotool type --delay 30 "${WALLET_PASSWORD:-walletpw1}"
	fi
	xdotool key Return
	sleep 1
	return 0
}

tde-nmctl --list | awk -F'\t' '/^Connections:/{c=1;next} c&&NF>2{sub(/^ +/,"",$1); print $2"\t"$1}' > /tmp/conns.txt
while IFS=$'\t' read -r uuid name; do
	before=$(dump "$uuid")
	(setsid tde-nmctl --edit "$uuid" > /tmp/rt.log 2>&1 &)
	W=""
	for i in $(seq 1 40); do
		unlock_wallet
		W=$(xdotool search --onlyvisible --name "^Editing " 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.5
	done
	if [ -z "$W" ]; then echo "FAIL: $name: no editor window"; fail=1; continue; fi
	sleep 1
	xdotool windowactivate --sync "$W" 2>/dev/null; xdotool key --window "$W" Return
	for i in $(seq 1 40); do pgrep -x tde-nmctl >/dev/null || break; sleep 0.25; done
	if pgrep -x tde-nmctl >/dev/null; then
		echo "FAIL: $name: the editor did not close (validation message?)"
		import -window root "/tmp/rt-fail-${uuid}.png"
		pkill -x tde-nmctl; fail=1; continue
	fi
	after=$(dump "$uuid")
	if compare "$before" "$after"; then
		echo "PASS: $name"
	else
		echo "FAIL: $name"; fail=1
	fi
done < /tmp/conns.txt
exit $fail
