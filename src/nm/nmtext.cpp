/*
 * tde-nmctl - NetworkManager front-end for the Trinity Desktop Environment
 *
 * Copyright (C) 2026 tde-nmctl developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "nmtext.h"
#include "nmclient.h"

#include <tdelocale.h>

TQString NMText::deviceType(uint type)
{
	switch (type) {
	case NM_DEVICE_TYPE_ETHERNET:   return i18n("Ethernet");
	case NM_DEVICE_TYPE_WIFI:       return i18n("Wi-Fi");
	case NM_DEVICE_TYPE_BT:         return i18n("Bluetooth");
	case NM_DEVICE_TYPE_OLPC_MESH:  return i18n("OLPC Mesh");
	case NM_DEVICE_TYPE_WIMAX:      return i18n("WiMAX");
	case NM_DEVICE_TYPE_MODEM:      return i18n("Mobile Broadband");
	case NM_DEVICE_TYPE_INFINIBAND: return i18n("InfiniBand");
	case NM_DEVICE_TYPE_BOND:       return i18n("Bond");
	case NM_DEVICE_TYPE_VLAN:       return i18n("VLAN");
	case NM_DEVICE_TYPE_ADSL:       return i18n("ADSL");
	case NM_DEVICE_TYPE_BRIDGE:     return i18n("Bridge");
	case NM_DEVICE_TYPE_GENERIC:    return i18n("Generic");
	case NM_DEVICE_TYPE_TEAM:       return i18n("Team");
	case NM_DEVICE_TYPE_TUN:        return i18n("TUN/TAP");
	case NM_DEVICE_TYPE_IP_TUNNEL:  return i18n("IP Tunnel");
	case NM_DEVICE_TYPE_MACVLAN:    return i18n("MACVLAN");
	case NM_DEVICE_TYPE_VXLAN:      return i18n("VXLAN");
	case NM_DEVICE_TYPE_VETH:       return i18n("Virtual Ethernet");
	case NM_DEVICE_TYPE_MACSEC:     return i18n("MACsec");
	case NM_DEVICE_TYPE_DUMMY:      return i18n("Dummy");
	case NM_DEVICE_TYPE_PPP:        return i18n("PPP");
	case NM_DEVICE_TYPE_OVS_INTERFACE: return i18n("Open vSwitch Interface");
	case NM_DEVICE_TYPE_OVS_PORT:   return i18n("Open vSwitch Port");
	case NM_DEVICE_TYPE_OVS_BRIDGE: return i18n("Open vSwitch Bridge");
	case NM_DEVICE_TYPE_WPAN:       return i18n("WPAN");
	case NM_DEVICE_TYPE_6LOWPAN:    return i18n("6LoWPAN");
	case NM_DEVICE_TYPE_WIREGUARD:  return i18n("WireGuard");
	case NM_DEVICE_TYPE_WIFI_P2P:   return i18n("Wi-Fi P2P");
	case NM_DEVICE_TYPE_VRF:        return i18n("VRF");
	case NM_DEVICE_TYPE_LOOPBACK:   return i18n("Loopback");
	case NM_DEVICE_TYPE_HSR:        return i18n("HSR");
	default:                        return i18n("Unknown");
	}
}

TQString NMText::deviceState(uint state)
{
	switch (state) {
	case NM_DEVICE_STATE_UNMANAGED:    return i18n("Unmanaged");
	case NM_DEVICE_STATE_UNAVAILABLE:  return i18n("Unavailable");
	case NM_DEVICE_STATE_DISCONNECTED: return i18n("Disconnected");
	case NM_DEVICE_STATE_PREPARE:      return i18n("Preparing");
	case NM_DEVICE_STATE_CONFIG:       return i18n("Configuring");
	case NM_DEVICE_STATE_NEED_AUTH:    return i18n("Waiting for authentication");
	case NM_DEVICE_STATE_IP_CONFIG:    return i18n("Getting an IP address");
	case NM_DEVICE_STATE_IP_CHECK:     return i18n("Checking the connection");
	case NM_DEVICE_STATE_SECONDARIES:  return i18n("Starting secondary connections");
	case NM_DEVICE_STATE_ACTIVATED:    return i18n("Connected");
	case NM_DEVICE_STATE_DEACTIVATING: return i18n("Disconnecting");
	case NM_DEVICE_STATE_FAILED:       return i18n("Failed");
	default:                           return i18n("Unknown");
	}
}

TQString NMText::stateReason(uint reason)
{
	switch (reason) {
	case 0:  return TQString::null;
	case 2:  return i18n("The device is now managed");
	case 3:  return i18n("The device is no longer managed");
	case 4:  return i18n("The device could not be configured");
	case 5:  return i18n("The IP configuration could not be reserved");
	case 6:  return i18n("The IP configuration is no longer valid");
	case 7:  return i18n("Secrets were required, but not provided");
	case 8:  return i18n("The supplicant disconnected");
	case 9:  return i18n("Configuring the supplicant failed");
	case 10: return i18n("The supplicant failed");
	case 11: return i18n("The supplicant timed out");
	case 12: return i18n("The PPP service failed to start");
	case 13: return i18n("The PPP service disconnected");
	case 14: return i18n("The PPP connection failed");
	case 15: return i18n("The DHCP client failed to start");
	case 16: return i18n("The DHCP client failed");
	case 17: return i18n("The DHCP client timed out");
	case 18: return i18n("The shared connection service failed to start");
	case 19: return i18n("The shared connection service failed");
	case 22: return i18n("The modem is busy");
	case 23: return i18n("The modem has no dial tone");
	case 24: return i18n("The modem has no carrier");
	case 25: return i18n("The modem dial timed out");
	case 26: return i18n("The modem dial failed");
	case 27: return i18n("The modem could not be initialized");
	case 28: return i18n("The APN could not be selected");
	case 29: return i18n("Not searching for networks");
	case 30: return i18n("The network registration was denied");
	case 31: return i18n("The network registration timed out");
	case 32: return i18n("Registering with the network failed");
	case 33: return i18n("The PIN check failed");
	case 34: return i18n("Firmware for the device may be missing");
	case 35: return i18n("The device was removed");
	case 36: return i18n("NetworkManager went to sleep");
	case 37: return i18n("The connection was removed");
	case 38: return i18n("Disconnected by the user");
	case 39: return i18n("The cable was unplugged");
	case 40: return i18n("The existing connection was assumed");
	case 41: return i18n("The supplicant is now available");
	case 42: return i18n("The modem was not found");
	case 43: return i18n("The Bluetooth connection failed or timed out");
	case 44: return i18n("The SIM card is missing");
	case 45: return i18n("The SIM PIN is required");
	case 46: return i18n("The SIM PUK is required");
	case 47: return i18n("The SIM is wrong");
	case 50: return i18n("A dependency of the connection failed");
	case 53: return i18n("The Wi-Fi network could not be found");
	case 54: return i18n("A secondary connection failed");
	case 55: return i18n("DCB or FCoE setup failed");
	case 56: return i18n("teamd control failed");
	case 57: return i18n("The modem failed or is no longer available");
	case 58: return i18n("The modem is now ready and available");
	case 59: return i18n("The SIM PIN was incorrect");
	case 60: return i18n("A new connection activation was enqueued");
	case 61: return i18n("The parent device changed");
	case 62: return i18n("The parent device's management changed");
	case 63: return i18n("Open vSwitch database connection was lost");
	case 64: return i18n("A duplicate IP address was detected");
	case 65: return i18n("The selected IP method is not supported");
	case 66: return i18n("Configuring SR-IOV parameters failed");
	case 67: return i18n("The Wi-Fi P2P peer could not be found");
	default: return i18n("Unknown reason (%1)").arg(reason);
	}
}

TQString NMText::managerState(uint state)
{
	switch (state) {
	case NM_STATE_ASLEEP:           return i18n("Networking is disabled");
	case NM_STATE_DISCONNECTED:     return i18n("Not connected");
	case NM_STATE_DISCONNECTING:    return i18n("Disconnecting");
	case NM_STATE_CONNECTING:       return i18n("Connecting");
	case NM_STATE_CONNECTED_LOCAL:  return i18n("Connected (local only)");
	case NM_STATE_CONNECTED_SITE:   return i18n("Connected (site only)");
	case NM_STATE_CONNECTED_GLOBAL: return i18n("Connected");
	default:                        return i18n("Unknown");
	}
}

TQString NMText::connectivity(uint c)
{
	switch (c) {
	case NM_CONNECTIVITY_NONE:    return i18n("No network");
	case NM_CONNECTIVITY_PORTAL:  return i18n("Behind a captive portal (sign-in required)");
	case NM_CONNECTIVITY_LIMITED: return i18n("Limited, no Internet access");
	case NM_CONNECTIVITY_FULL:    return i18n("Full Internet access");
	default:                      return i18n("Unknown");
	}
}

TQString NMText::activeState(uint state)
{
	switch (state) {
	case NM_ACTIVE_CONNECTION_STATE_ACTIVATING:   return i18n("Connecting");
	case NM_ACTIVE_CONNECTION_STATE_ACTIVATED:    return i18n("Connected");
	case NM_ACTIVE_CONNECTION_STATE_DEACTIVATING: return i18n("Disconnecting");
	case NM_ACTIVE_CONNECTION_STATE_DEACTIVATED:  return i18n("Disconnected");
	default:                                      return i18n("Unknown");
	}
}

TQString NMText::vpnState(uint state)
{
	switch (state) {
	case 1: return i18n("Preparing");
	case 2: return i18n("Waiting for authentication");
	case 3: return i18n("Connecting");
	case 4: return i18n("Getting an IP address");
	case 5: return i18n("Connected");
	case 6: return i18n("Failed");
	case 7: return i18n("Disconnected");
	default: return i18n("Unknown");
	}
}

TQString NMText::connectionType(const TQString &type)
{
	if (type == "802-3-ethernet")  return i18n("Ethernet");
	if (type == "802-11-wireless") return i18n("Wi-Fi");
	if (type == "vpn")             return i18n("VPN");
	if (type == "wireguard")       return i18n("WireGuard");
	if (type == "bond")            return i18n("Bond");
	if (type == "bridge")          return i18n("Bridge");
	if (type == "vlan")            return i18n("VLAN");
	if (type == "team")            return i18n("Team");
	if (type == "gsm")             return i18n("Mobile Broadband (GSM/LTE)");
	if (type == "cdma")            return i18n("Mobile Broadband (CDMA)");
	if (type == "pppoe")           return i18n("DSL/PPPoE");
	if (type == "adsl")            return i18n("ADSL");
	if (type == "infiniband")      return i18n("InfiniBand");
	if (type == "bluetooth")       return i18n("Bluetooth");
	if (type == "ip-tunnel")       return i18n("IP Tunnel");
	if (type == "macvlan")         return i18n("MACVLAN");
	if (type == "vxlan")           return i18n("VXLAN");
	if (type == "tun")             return i18n("TUN/TAP");
	if (type == "dummy")           return i18n("Dummy");
	if (type == "loopback")        return i18n("Loopback");
	if (type == "generic")         return i18n("Generic");
	if (type == "wifi-p2p")        return i18n("Wi-Fi P2P");
	if (type == "veth")            return i18n("Virtual Ethernet");
	if (type == "macsec")          return i18n("MACsec");
	if (type == "vrf")             return i18n("VRF");
	if (type.startsWith("ovs-"))   return i18n("Open vSwitch");
	return type;
}

TQString NMText::connectionGroup(const TQString &type)
{
	if (type == "802-3-ethernet" || type == "pppoe" || type == "infiniband" || type == "veth" ||
	    type == "macsec")
		return i18n("Wired");
	if (type == "802-11-wireless" || type == "wifi-p2p")
		return i18n("Wi-Fi");
	if (type == "vpn" || type == "wireguard")
		return i18n("VPN");
	if (type == "gsm" || type == "cdma" || type == "bluetooth" || type == "adsl")
		return i18n("Mobile and DSL");
	if (type == "loopback")
		return i18n("Other");
	return i18n("Virtual");
}

TQStringList NMText::creatableTypes()
{
	TQStringList t;
	t << "802-3-ethernet" << "802-11-wireless" << "pppoe" << "gsm" << "cdma" << "infiniband"
	  << "bond" << "bridge" << "vlan" << "team" << "dummy"
	  << "vpn" << "wireguard";
	return t;
}

TQString NMText::connectionIcon(const TQString &type)
{
	if (type == "802-11-wireless" || type == "wifi-p2p")
		return "tde-nmctl-signal-75";
	if (type == "vpn" || type == "wireguard")
		return "tde-nmctl-vpn";
	if (type == "gsm" || type == "cdma" || type == "bluetooth")
		return "tde-nmctl-modem";
	if (type == "802-3-ethernet" || type == "pppoe" || type == "adsl" || type == "infiniband")
		return "tde-nmctl-wired";
	return "tde-nmctl-virtual";
}

TQString NMText::deviceIcon(uint type)
{
	switch (type) {
	case NM_DEVICE_TYPE_WIFI:
	case NM_DEVICE_TYPE_WIFI_P2P:
	case NM_DEVICE_TYPE_OLPC_MESH:
		return "tde-nmctl-signal-75";
	case NM_DEVICE_TYPE_MODEM:
	case NM_DEVICE_TYPE_BT:
		return "tde-nmctl-modem";
	case NM_DEVICE_TYPE_WIREGUARD:
		return "tde-nmctl-vpn";
	case NM_DEVICE_TYPE_ETHERNET:
	case NM_DEVICE_TYPE_INFINIBAND:
	case NM_DEVICE_TYPE_ADSL:
		return "tde-nmctl-wired";
	default:
		return "tde-nmctl-virtual";
	}
}

TQString NMText::signalIcon(uint strength)
{
	int level;
	if (strength >= 80)      level = 100;
	else if (strength >= 55) level = 75;
	else if (strength >= 30) level = 50;
	else if (strength >= 5)  level = 25;
	else                     level = 0;
	return TQString("tde-nmctl-signal-%1").arg(level);
}

TQString NMText::trayIcon(const NMDeviceInfo *primary, uint state, bool vpn, uint strength)
{
	TQString icon;
	if (state == NM_STATE_ASLEEP)
		return "tde-nmctl-disabled";
	if (state == NM_STATE_CONNECTING)
		return "tde-nmctl-connecting";
	if (!primary || state < NM_STATE_CONNECTED_LOCAL)
		return "tde-nmctl-offline";
	switch (primary->type) {
	case NM_DEVICE_TYPE_WIFI:
		icon = signalIcon(strength);
		break;
	case NM_DEVICE_TYPE_MODEM:
	case NM_DEVICE_TYPE_BT:
		icon = "tde-nmctl-modem";
		break;
	default:
		icon = "tde-nmctl-wired";
	}
	if (state < NM_STATE_CONNECTED_GLOBAL && icon == "tde-nmctl-wired")
		icon = "tde-nmctl-wired-limited";
	if (vpn)
		icon += "-vpn";
	return icon;
}

TQString NMText::bitrate(uint kbits)
{
	if (kbits >= 1000000)
		return i18n("%1 Gbit/s").arg(TQString::number(kbits / 1000000.0, 'f', 1));
	if (kbits >= 1000)
		return i18n("%1 Mbit/s").arg(kbits / 1000);
	return i18n("%1 kbit/s").arg(kbits);
}

TQString NMText::bytes(TQ_UINT64 b)
{
	if (b >= (TQ_UINT64)1 << 30)
		return i18n("%1 GiB").arg(TQString::number(b / 1073741824.0, 'f', 2));
	if (b >= 1 << 20)
		return i18n("%1 MiB").arg(TQString::number(b / 1048576.0, 'f', 1));
	if (b >= 1 << 10)
		return i18n("%1 KiB").arg(TQString::number(b / 1024.0, 'f', 1));
	return i18n("%1 B").arg((unsigned long)b);
}

TQString NMText::frequency(uint mhz)
{
	return i18n("%1 MHz").arg(mhz);
}

TQString NMText::band(uint mhz)
{
	if (mhz >= 5925)
		return i18n("6 GHz");
	if (mhz >= 4900)
		return i18n("5 GHz");
	if (mhz >= 2400 && mhz < 2500)
		return i18n("2.4 GHz");
	return TQString::null;
}
