/*
 * tde-nmctl - test of the NetworkManager client against tests/mock-nm.py
 *
 * Copyright (C) 2026 tde-nmctl developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "nmclient.h"
#include "nmdbus.h"
#include "secretagent.h"
#include "secretstore.h"

#include <stdio.h>
#include <stdlib.h>

#include <tqapplication.h>
#include <tqdatetime.h>

#include <tqdbusconnection.h>
#include <tqdbusmessage.h>

#include <tdeaboutdata.h>
#include <tdeconfig.h>
#include <tdeinstance.h>

static int failures = 0;

static void check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok)
		++failures;
}

static void wait(int ms)
{
	TQTime t;
	t.start();
	while (t.elapsed() < ms)
		tqApp->processEvents(50);
}

static TQString mock(const char *method, const TQString &arg = TQString::null)
{
	TQT_DBusMessage msg = TQT_DBusMessage::methodCall("org.freedesktop.NetworkManager",
	                                                  "/org/freedesktop/NetworkManager",
	                                                  "org.trinitydesktop.NmctlMock", method);
	if (!arg.isNull())
		msg << TQT_DBusData::fromString(arg);
	TQT_DBusMessage reply = TQT_DBusConnection::systemBus().sendWithReply(msg);
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQString::null;
	return reply[0].toString();
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);
	TQApplication app(argc, argv, false);
	TDEAboutData about("clienttest", "clienttest", "1");
	TDEInstance instance(&about);
	{
		/* keep secrets in memory, no wallet daemon in the test */
		TDEConfig wallet("tdewalletrc");
		wallet.setGroup("Wallet");
		wallet.writeEntry("Enabled", false);
		wallet.sync();
	}

	NMClient *nm = NMClient::self();
	nm->refresh();
	check(nm->isRunning(), "NetworkManager (mock) is running");
	check(nm->devices().count() == 2, "two devices");
	const NMDeviceInfo *wifi = nm->deviceByInterface("wlp2s0");
	check(wifi && wifi->accessPoints.count() >= 7, "Wi-Fi access points are read");
	check(wifi && wifi->activeAP() && wifi->activeAP()->ssidText() == "HomeNet", "active access point");
	check(wifi && wifi->activeAP() && wifi->activeAP()->securityText() == "WPA2", "WPA2 security text");
	check(nm->state() == NM_STATE_CONNECTED_GLOBAL, "manager state connected");
	NMIpConfig ip = nm->ipConfig(nm->deviceByInterface("enp3s0")->ip4Config, false);
	check(!ip.addresses.isEmpty() && ip.dns.contains("9.9.9.9"), "IPv4 configuration and DNS");

	SecretAgent agent(0, "agent");
	wait(800);
	check(!mock("Agent").isEmpty(), "secret agent registered");

	/* a wired connection with manual addresses, written and read back */
	ConnectionSettings c;
	TQString uuid = ConnectionSettings::newUuid();
	c.setStr("connection", "id", "Test Static");
	c.setStr("connection", "uuid", uuid);
	c.setStr("connection", "type", "802-3-ethernet");
	c.setUInt("802-3-ethernet", "mtu", 1400);
	c.setStr("802-3-ethernet", "assigned-mac-address", "random");
	c.setStr("ipv4", "method", "manual");
	NMVariantMap a;
	a["address"] = TQT_DBusData::fromString("192.168.50.10");
	a["prefix"] = TQT_DBusData::fromUInt32(24);
	TQValueList<NMVariantMap> addrs;
	addrs << a;
	c.setMapList("ipv4", "address-data", addrs);
	c.setStr("ipv4", "gateway", "192.168.50.1");
	TQ_UINT32 dns;
	NMDBus::ip4FromString("1.1.1.1", &dns);
	TQValueList<TQ_UINT32> dnsList;
	dnsList << dns;
	c.set("ipv4", "dns", NMDBus::fromUIntList(dnsList));
	c.setStrList("ipv4", "dns-search", TQStringList("example.org"));
	c.setStr("ipv6", "method", "manual");
	NMVariantMap a6;
	a6["address"] = TQT_DBusData::fromString("fd00::10");
	a6["prefix"] = TQT_DBusData::fromUInt32(64);
	TQValueList<NMVariantMap> addrs6;
	addrs6 << a6;
	c.setMapList("ipv6", "address-data", addrs6);
	TQByteArray dns6;
	NMDBus::ip6FromString("2606:4700:4700::1111", &dns6);
	TQValueList<TQByteArray> dns6List;
	dns6List << dns6;
	c.set("ipv6", "dns", NMDBus::fromBytesList(dns6List));
	c.setMapList("ipv4", "route-data", TQValueList<NMVariantMap>());
	c.setStrList("connection", "permissions", TQStringList());

	TQString path = nm->addConnection(c);
	check(!path.isEmpty(), "AddConnection");
	TQString dump = mock("ConnectionDump", uuid);
	check(dump.contains("'mtu': 1400"), "mtu written as uint32");
	check(dump.contains("'dns': [16843009]"), "IPv4 DNS written as au in network byte order");
	check(dump.contains("'address': '192.168.50.10', 'prefix': 24"), "address-data written as aa{sv}");
	check(dump.contains("'dns-search': ['example.org']"), "dns-search written as as");
	check(dump.contains("'route-data': []"), "empty aa{sv} written");
	check(dump.contains("b'&\\x06G\\x00G\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x11\\x11'"), "IPv6 DNS written as aay");

	nm->refresh();
	const NMConnection *rc = nm->connection(path);
	check(rc && rc->settings.mapList("ipv4", "address-data").count() == 1, "address-data read back");
	check(rc && rc->settings.uint("802-3-ethernet", "mtu") == 1400, "mtu read back");

	ConnectionSettings u = rc->settings;
	u.setStr("connection", "id", "Test Static Renamed");
	check(nm->updateConnection(path, u), "Update");
	check(mock("ConnectionDump", uuid).contains("'id': 'Test Static Renamed'"), "update stored");

	/* Wi-Fi with a password for all users: GetSecrets returns it */
	ConnectionSettings w;
	TQString wuuid = ConnectionSettings::newUuid();
	w.setStr("connection", "id", "Cafe");
	w.setStr("connection", "uuid", wuuid);
	w.setStr("connection", "type", "802-11-wireless");
	w.setBytes("802-11-wireless", "ssid", NMDBus::bytes("Cafe Libre"));
	w.setStr("802-11-wireless-security", "key-mgmt", "wpa-psk");
	w.setStr("802-11-wireless-security", "psk", "system-secret");
	w.setUInt("802-11-wireless-security", "psk-flags", 0);
	TQString wpath = nm->addConnection(w);
	check(!wpath.isEmpty(), "AddConnection Wi-Fi");
	check(mock("ConnectionDump", wuuid).contains("b'Cafe Libre'"), "SSID written as ay");
	NMSettingsMap secrets;
	check(nm->getSecrets(wpath, "802-11-wireless-security", &secrets) &&
	      NMDBus::toStr(secrets["802-11-wireless-security"]["psk"]) == "system-secret", "GetSecrets");

	/* the user's own password goes to the agent (SaveSecrets) */
	ConnectionSettings v;
	TQString vuuid = ConnectionSettings::newUuid();
	v.setStr("connection", "id", "Test VPN");
	v.setStr("connection", "uuid", vuuid);
	v.setStr("connection", "type", "vpn");
	v.setStr("vpn", "service-type", "org.freedesktop.NetworkManager.openvpn");
	TQMap<TQString, TQString> data;
	data["remote"] = "vpn.example.net";
	data["password-flags"] = "1";
	v.setStrMap("vpn", "data", data);
	TQMap<TQString, TQString> vsec;
	vsec["password"] = "vpn-secret";
	v.setStrMap("vpn", "secrets", vsec);
	TQString vpath = nm->addConnection(v);
	wait(800);
	check(!vpath.isEmpty(), "AddConnection VPN with a{ss} data");
	check(mock("ConnectionDump", vuuid).contains("'remote': 'vpn.example.net'"), "vpn.data written as a{ss}");
	TQString vdump = mock("ConnectionDump", vuuid);
	check(!vdump.left(vdump.find("'agent'")).contains("vpn-secret"), "agent-owned VPN secret not stored by NetworkManager");
	SecretMap stored;
	check(SecretStore::load(vuuid, "vpn", &stored) && stored["password"] == "vpn-secret",
	      "SaveSecrets stored the VPN password for the agent");

	/* activation asks the agent for an agent-owned Wi-Fi password */
	ConnectionSettings p;
	TQString puuid = ConnectionSettings::newUuid();
	p.setStr("connection", "id", "Library");
	p.setStr("connection", "uuid", puuid);
	p.setStr("connection", "type", "802-11-wireless");
	p.setBytes("802-11-wireless", "ssid", NMDBus::bytes("Library Guest"));
	p.setStr("802-11-wireless-security", "key-mgmt", "wpa-psk");
	p.setStr("802-11-wireless-security", "psk", "agent-pass-123");
	p.setUInt("802-11-wireless-security", "psk-flags", NM_SECRET_FLAG_AGENT_OWNED);
	SecretStore::saveAgentOwned(p);
	TQString ppath = nm->addConnection(p);
	wait(500);
	nm->refresh();
	/* device pointers do not survive refresh() */
	wifi = nm->deviceByInterface("wlp2s0");
	nm->activateConnection(ppath, wifi ? wifi->path : TQString("/"));
	wait(1500);
	check(mock("LastSecrets").contains("agent-pass-123"), "agent answered GetSecrets from the store");
	nm->refresh();
	const NMActiveConnection *pac = 0;
	for (NMActiveList::const_iterator it = nm->activeConnections().begin(); it != nm->activeConnections().end(); ++it)
		if ((*it).uuid == puuid)
			pac = &(*it);
	check(pac && pac->state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED, "connection activated with the agent's password");

	/* switches */
	nm->setWirelessEnabled(false);
	wait(800);
	nm->refresh();
	check(!nm->wirelessEnabled(), "Wi-Fi switched off");
	nm->setWirelessEnabled(true);
	wait(500);

	check(nm->deleteConnection(path) && nm->deleteConnection(wpath) && nm->deleteConnection(vpath) &&
	      nm->deleteConnection(ppath), "Delete");
	nm->refresh();
	check(!nm->connectionByUuid(uuid) && !nm->connectionByUuid(vuuid), "deleted connections are gone");

	printf("%d failure(s)\n", failures);
	return failures ? 1 : 0;
}
