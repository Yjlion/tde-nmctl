#!/bin/bash
# Captures the screenshots in screenshots/ under Xvfb with the TDE window
# manager and panel, against the simulated NetworkManager of
# tests/mock-nm.py, and copies the ones the handbook uses to doc/en/.
#
# Prerequisites: a Debian container with the TDE packages (kcontrol, kicker,
# twin, khelpcenter, tde-systemsettings), tde-nmctl installed, Xvfb,
# xdotool, ImageMagick, python3-dbus and python3-gi. Run as root:
#   OUT=/src/screenshots tools/take-screenshots.sh
# TDE Wallet's password prompts are answered with $WALLET_PASSWORD.
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${OUT:-/shots}; mkdir -p "$OUT"
WALLET_PASSWORD=${WALLET_PASSWORD:-walletpw1}
export HOME=${HOME:-/root}
CFG=$HOME/.trinity/share/config

# a fresh simulated NetworkManager
[ -f /tmp/mock-nm.pid ] && kill "$(cat /tmp/mock-nm.pid)" 2>/dev/null
sleep 1
. "$HERE/tools/session.sh"
export DISPLAY=:1 PATH=/opt/trinity/bin:$PATH

for p in tde-nmctl tdecmshell kcontrol systemsettings tde-nmctl-tray khelpcenter; do pkill -x $p; done
sleep 1

# TDE Wallet: enabled, without the first-use wizard
mkdir -p "$CFG"
printf "[Wallet]\nEnabled=true\nFirst Use=false\nClose When Idle=false\nUse One Wallet=true\n" > "$CFG/tdewalletrc"
rm -f "$CFG/tde-nmctlrc" "$CFG/tde-nmctl-trayrc" "$CFG/systemsettingsrc" "$CFG/tde-nmctl-trayrc"

mock() {
	dbus-send --system --print-reply=literal --dest=org.freedesktop.NetworkManager \
		/org/freedesktop/NetworkManager "$@"
}

# answers TDE Wallet's password prompt (twice for a new wallet)
unlock_wallet() {
	local w
	for w in $(xdotool search --onlyvisible --name "TDE Wallet Service|^kded$" 2>/dev/null); do
		xdotool windowactivate --sync "$w" 2>/dev/null
		sleep 0.4
		xdotool type --delay 30 "$WALLET_PASSWORD"
		xdotool key Tab
		xdotool type --delay 30 "$WALLET_PASSWORD"
		xdotool key Return
		sleep 1.5
	done
}
waitwin() { # pattern -> window id
	local id
	for i in $(seq 1 40); do
		unlock_wallet
		id=$(xdotool search --onlyvisible --name "$1" 2>/dev/null | tail -1)
		[ -n "$id" ] && { echo "$id"; return; }
		sleep 0.5
	done
}
shot() { # name window-pattern
	local id
	id=$(waitwin "$2")
	if [ -z "$id" ]; then echo "MISSING window for $1"; return; fi
	sleep 1.5
	timeout 20 import -frame -window "$id" "$OUT/$1.png" && echo "saved $1"
}
key() { xdotool key --clearmodifiers "$@"; sleep 1.2; }
place() { # pattern x y w h
	local id; id=$(waitwin "$1")
	[ -z "$id" ] && return
	xdotool windowmove "$id" "$2" "$3"; xdotool windowsize "$id" "$4" "$5"; sleep 1
}
# the biggest visible window of the tray process (its popup or menu)
traywin() {
	local best="" bh=0 w h
	for w in $(xdotool search --onlyvisible --class "tde-nmctl-tray" 2>/dev/null); do
		eval "$(xdotool getwindowgeometry --shell "$w")"
		[ "$HEIGHT" -gt "$bh" ] && { best=$w; bh=$HEIGHT; }
	done
	echo "$best"
}
shotgeom() { # name window margin: the window and some space around it
	eval "$(xdotool getwindowgeometry --shell "$2")"
	local m=${3:-0} x y
	x=$((X - m)); y=$((Y - m)); [ $x -lt 0 ] && x=0; [ $y -lt 0 ] && y=0
	import -window root -crop $((WIDTH + 2 * m))x$((HEIGHT + 2 * m))+$x+$y +repage "$OUT/$1.png" && echo "saved $1"
}

# ---------------------------------------------------------------- editor
app() { # page selection
	pkill -x tde-nmctl; sleep 1
	(setsid tde-nmctl --page "$1" --select "${2:-HomeNet}" >/dev/null 2>&1 &)
	sleep 4
	place "Network Connections" 40 30 860 600
}
app connections
shot connections "Network Connections"
# the Add menu (the Add button is at the top right of the list)
W=$(waitwin "Network Connections")
eval "$(xdotool getwindowgeometry --shell "$W")"
xdotool windowactivate --sync "$W"; key alt+a; sleep 1
import -window root -crop 540x$((HEIGHT + 8))+$((X + WIDTH - 320))+$((Y - 4)) +repage "$OUT/add-menu.png" && echo "saved add-menu"
key Escape

app devices wlp2s0
shot devices "Network Connections"
app settings
shot settings "Network Connections"
pkill -x tde-nmctl; sleep 1

edit() { # connection tab name
	pkill -x tde-nmctl; sleep 1
	(setsid tde-nmctl --edit "$1" --tab "$2" >/dev/null 2>&1 &)
	sleep 3
	place "^Editing " 60 40 660 640
	shot "$3" "^Editing "
	pkill -x tde-nmctl; sleep 1
}
edit HomeNet 0 editor-general
edit HomeNet 1 editor-wifi
edit Office-5G 2 editor-wifi-security
edit "Office LAN" 3 editor-ipv4
edit "Work VPN" 1 editor-vpn
edit wg-home 1 editor-wireguard
edit "Bridge br0" 1 editor-bridge

# ------------------------------------------------- Control Center module
(setsid kcontrol >/dev/null 2>&1 &); sleep 8
W=$(waitwin "Control Center")
xdotool windowmove "$W" 0 0; xdotool windowsize "$W" 1150 780; sleep 2
xdotool mousemove 16 149 click 1; sleep 1.5     # Internet & Network
xdotool mousemove 143 209 click 1; sleep 6      # Network Connections
shot kcontrol "Control Center"
pkill -x kcontrol; sleep 1

# System Settings: Network & Connectivity -> Network Settings
(setsid systemsettings >/dev/null 2>&1 &); sleep 8
W=$(waitwin "System Settings")
xdotool windowmove "$W" 0 0; sleep 1
shot systemsettings-overview "System Settings"
xdotool mousemove 65 530 click 1; sleep 4       # Network Settings
xdotool mousemove 133 370 click 1; sleep 5      # Network Connections
W=$(waitwin "System Settings")
xdotool windowsize "$W" 1100 720; sleep 2
shot systemsettings "System Settings"
pkill -x systemsettings; sleep 1

# the handbook in KHelpCenter
(setsid khelpcenter help:/tde-nmctl/tray.html >/dev/null 2>&1 &); sleep 8
place "Help Center" 40 30 1000 760; sleep 2
shot handbook-khelpcenter "Help Center"
pkill -x khelpcenter; sleep 1

# ------------------------------------------------------------- the tray
pgrep -x kicker >/dev/null || (setsid kicker >/dev/null 2>&1 &); sleep 8
(setsid tde-nmctl-tray >/dev/null 2>&1 &); sleep 6
TRAY=$(xdotool search --onlyvisible --name "^Network$" | head -1)
eval "$(xdotool getwindowgeometry --shell "$TRAY")"
TX=$((X + WIDTH / 2)); TY=$((Y + HEIGHT / 2))

xdotool mousemove $((TX - 3)) $((TY - 3)); sleep 0.3; xdotool mousemove $TX $TY; sleep 3
import -window root -crop 420x170+$((TX - 340))+$((TY - 150)) +repage "$OUT/tray-tooltip.png" && echo "saved tray-tooltip"

xdotool mousemove $TX $TY click 1; sleep 3
P=$(traywin)
[ -n "$P" ] && shotgeom tray-popup "$P" 8
key Escape

xdotool mousemove $TX $TY click 3; sleep 2
P=$(traywin)
[ -n "$P" ] && shotgeom tray-menu "$P" 8
key Escape Escape

# Connection Information, from the popup's info button of the first row
xdotool mousemove $TX $TY click 1; sleep 3
P=$(traywin)
eval "$(xdotool getwindowgeometry --shell "$P")"
# the (i) button of the first active connection: left of "Disconnect"
xdotool mousemove $((X + WIDTH - 133)) $((Y + 122)) click 1; sleep 3
place "Connection Information" 100 60 560 600
shot info-dialog "Connection Information"
W=$(waitwin "Connection Information"); [ -n "$W" ] && xdotool windowclose "$W" 2>/dev/null
sleep 1

# Connect to Hidden Network, from the popup
xdotool mousemove $TX $TY click 1; sleep 3
P=$(traywin)
eval "$(xdotool getwindowgeometry --shell "$P")"
# the button is below the list of networks; find it by trying the
# "Connect to Hidden Network" line, 3/4 down the popup
for dy in $(seq $((HEIGHT * 55 / 100)) 20 $((HEIGHT - 60))); do
	xdotool mousemove $((X + 90)) $((Y + dy)) click 1; sleep 1.5
	W=$(xdotool search --onlyvisible --name "Hidden Wi-Fi" 2>/dev/null | head -1)
	[ -n "$W" ] && break
	[ -z "$(traywin)" ] && { xdotool mousemove $TX $TY click 1; sleep 2; }
done
if [ -n "$W" ]; then
	xdotool windowactivate --sync "$W"; sleep 0.5
	xdotool type --delay 40 "Basement AP"; sleep 0.5
	shot hidden-network "Hidden Wi-Fi"
	key Escape
else
	echo "MISSING window for hidden-network"
	key Escape
fi

# the password dialog of the secret agent: NetworkManager asks for a new
# password of HomeNet, as after a wrong one
mock org.trinitydesktop.NmctlMock.ActivateByIdFlags string:"HomeNet" uint32:3 >/dev/null
sleep 3
W=$(waitwin "Authentication Required")
if [ -n "$W" ]; then
	xdotool windowactivate --sync "$W"; sleep 0.5
	xdotool key ctrl+a BackSpace; xdotool type --delay 60 "correct horse"; sleep 0.5
	shot password-dialog "Authentication Required"
	key Return
	sleep 3; unlock_wallet; sleep 2
fi

# the popup again, while connecting to a new network
xdotool mousemove $TX $TY click 1; sleep 3
P=$(traywin)
[ -n "$P" ] && shotgeom tray-popup-connected "$P" 8
key Escape

# the handbook's pictures
for f in tray-popup password-dialog info-dialog tray-menu connections editor-general \
         editor-wifi-security editor-ipv4 editor-vpn editor-wireguard devices settings kcontrol; do
	[ -f "$OUT/$f.png" ] && [ -d "$HERE/doc/en" ] && cp "$OUT/$f.png" "$HERE/doc/en/$f.png"
done
echo done
