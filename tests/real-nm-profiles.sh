#!/bin/bash
# Creates connection profiles of many types with nmcli in a real
# NetworkManager (a test container), for tests/roundtrip.sh with REAL_NM=1.
set -e
add() { nmcli connection add save yes "$@" >/dev/null; }
add type ethernet con-name "Static LAN" ifname veth0 ipv4.method manual \
	ipv4.addresses 192.168.77.5/24 ipv4.gateway 192.168.77.1 ipv4.dns "1.1.1.1 9.9.9.9" \
	ipv4.dns-search example.org ipv4.routes "10.0.0.0/8 192.168.77.254 50" \
	ipv6.method manual ipv6.addresses fd00::5/64 ipv6.dns fd00::1 \
	802-3-ethernet.mtu 1400 802-3-ethernet.cloned-mac-address random connection.zone home
add type ethernet con-name "DHCP LAN" ifname veth1 ipv4.ignore-auto-dns yes ipv4.dns 8.8.8.8 \
	ipv4.dhcp-hostname mybox ipv6.ip6-privacy 2 connection.autoconnect-priority 5
add type ethernet con-name "8021x LAN" 802-1x.eap peap 802-1x.identity bob 802-1x.phase2-auth mschapv2 \
	802-1x.password secret 802-1x.domain-suffix-match corp.example 802-1x.system-ca-certs no
add type wifi con-name "Home Wi-Fi" ssid HomeNet wifi-sec.key-mgmt wpa-psk wifi-sec.psk secret123
add type wifi con-name "WPA3 Wi-Fi" ssid "Neighbor WPA3" wifi-sec.key-mgmt sae wifi-sec.psk secret456 \
	802-11-wireless.band a 802-11-wireless.channel 36 wifi.powersave 2
add type wifi con-name "Open Cafe" ssid "Cafe Libre" wifi.hidden yes
add type wifi con-name "Office" ssid Office wifi-sec.key-mgmt wpa-eap 802-1x.eap ttls \
	802-1x.identity jdoe 802-1x.anonymous-identity anon 802-1x.phase2-auth pap 802-1x.password pw \
	802-1x.password-flags 1
add type wifi con-name "Hotspot" ssid TDE-Hotspot mode ap wifi.band bg ipv4.method shared \
	wifi-sec.key-mgmt wpa-psk wifi-sec.psk hotspot123 ipv6.method ignore
add type vpn con-name "Work VPN" vpn-type openvpn \
	vpn.data "remote=vpn.example.com, connection-type=password-tls, username=jdoe, ca=/tmp/ca.crt, cert=/tmp/u.crt, key=/tmp/u.key, port=1194, cipher=AES-256-GCM, password-flags=1"
add type bond con-name bond0 ifname bond0 bond.options "mode=active-backup,miimon=100,primary=veth1"
add type ethernet con-name "bond0 port" ifname veth1 master bond0 slave-type bond
add type bridge con-name br0 ifname br0 bridge.stp yes bridge.priority 4096
add type vlan con-name vlan10 dev veth0 id 10
add type dummy con-name dummy1 ifname dummy1 ipv4.method manual ipv4.addresses 10.99.0.1/24
add type gsm con-name "Mobile" gsm.apn internet gsm.username user gsm.password pass
add type pppoe con-name "DSL" ifname veth0 pppoe.username dsluser pppoe.password dslpass
add type infiniband con-name ib0 ifname ib0 infiniband.transport-mode connected
cat > /tmp/wg-test.conf <<EOW
[Interface]
PrivateKey = yAnz5TF+lXXJte14tji3zlMNq+hd2rYUIgJBgB3fBmk=
Address = 10.10.0.2/24
ListenPort = 51820

[Peer]
PublicKey = xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=
AllowedIPs = 10.10.0.0/24
Endpoint = 192.0.2.1:51820
PersistentKeepalive = 25
EOW
nmcli connection import type wireguard file /tmp/wg-test.conf >/dev/null
nmcli -t -f NAME,TYPE connection show
