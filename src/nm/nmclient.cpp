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

#include "nmclient.h"

#include <tqdir.h>
#include <tqfile.h>
#include <tqtimer.h>

#include <tqdbusdatalist.h>
#include <tqdbuserror.h>
#include <tqdbusobjectpath.h>

#include <tdelocale.h>
#include <kdebug.h>

#include <string.h>

/* NetworkManager asks polkit, which may wait for the user to type a
 * password, so wait a long time for calls that change something. */
static const int CALL_TIMEOUT = 125 * 1000;
static const int READ_TIMEOUT = 10 * 1000;

static const char *FD_SERVICE_BUS   = "org.freedesktop.DBus";
static const char *FD_PATH_BUS      = "/org/freedesktop/DBus";
static const char *FD_INTERFACE_BUS = "org.freedesktop.DBus";

using namespace NMDBus;

static TQValueList<TQT_DBusData> args1(const TQT_DBusData &a)
{
	TQValueList<TQT_DBusData> l;
	l << a;
	return l;
}

static TQT_DBusData path(const TQString &p)
{
	return TQT_DBusData::fromObjectPath(TQT_DBusObjectPath(p.isEmpty() ? TQString("/") : p));
}

static bool validPath(const TQString &p)
{
	return !p.isEmpty() && p != "/";
}

/* ---------------------------------------------------------------------- */
/* NMAccessPoint / NMDeviceInfo                                           */
/* ---------------------------------------------------------------------- */

bool NMAccessPoint::isSecure() const
{
	return (flags & NM_802_11_AP_FLAGS_PRIVACY) || wpaFlags || rsnFlags;
}

bool NMAccessPoint::isEnterprise() const
{
	return ((wpaFlags | rsnFlags) & (NM_802_11_AP_SEC_KEY_MGMT_802_1X |
	                                 NM_802_11_AP_SEC_KEY_MGMT_EAP_SUITE_B_192)) != 0;
}

TQString NMAccessPoint::securityText() const
{
	uint all = wpaFlags | rsnFlags;
	TQStringList parts;
	if (all & NM_802_11_AP_SEC_KEY_MGMT_SAE)
		parts << "WPA3";
	if ((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK))
		parts << "WPA2";
	else if (wpaFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK)
		parts << "WPA";
	if (all & (NM_802_11_AP_SEC_KEY_MGMT_802_1X | NM_802_11_AP_SEC_KEY_MGMT_EAP_SUITE_B_192))
		parts << (rsnFlags ? i18n("WPA2 Enterprise") : i18n("WPA Enterprise"));
	if (all & (NM_802_11_AP_SEC_KEY_MGMT_OWE | NM_802_11_AP_SEC_KEY_MGMT_OWE_TM))
		parts << "OWE";
	if (parts.isEmpty() && (flags & NM_802_11_AP_FLAGS_PRIVACY))
		parts << "WEP";
	return parts.join(" / ");
}

TQString NMAccessPoint::keyMgmt() const
{
	uint all = wpaFlags | rsnFlags;
	if (all & (NM_802_11_AP_SEC_KEY_MGMT_802_1X | NM_802_11_AP_SEC_KEY_MGMT_EAP_SUITE_B_192))
		return "wpa-eap";
	if (all & NM_802_11_AP_SEC_KEY_MGMT_PSK)
		return "wpa-psk";
	if (all & NM_802_11_AP_SEC_KEY_MGMT_SAE)
		return "sae";
	if (all & NM_802_11_AP_SEC_KEY_MGMT_OWE)
		return "owe";
	if (flags & NM_802_11_AP_FLAGS_PRIVACY)
		return "none";        /* static WEP */
	return TQString::null;    /* open */
}

int NMAccessPoint::channel() const
{
	if (frequency >= 2412 && frequency <= 2472)
		return (frequency - 2407) / 5;
	if (frequency == 2484)
		return 14;
	if (frequency >= 5000 && frequency < 5950)
		return (frequency - 5000) / 5;
	if (frequency >= 5955 && frequency <= 7115)
		return (frequency - 5950) / 5;
	return 0;
}

const NMAccessPoint *NMDeviceInfo::activeAP() const
{
	if (!validPath(activeAccessPoint))
		return 0;
	for (NMAccessPointList::const_iterator it = accessPoints.begin(); it != accessPoints.end(); ++it)
		if ((*it).path == activeAccessPoint)
			return &(*it);
	return 0;
}

/* ---------------------------------------------------------------------- */

NMClient *NMClient::s_self = 0;

NMClient *NMClient::self()
{
	if (!s_self)
		s_self = new NMClient(0, "NMClient");
	return s_self;
}

NMClient::NMClient(TQObject *parent, const char *name)
	: TQObject(parent, name), m_refreshConnections(true), m_refreshDevices(true),
	  m_running(false), m_state(0), m_connectivity(0), m_networkingEnabled(false),
	  m_wirelessEnabled(false), m_wirelessHwEnabled(false), m_wwanEnabled(false),
	  m_wwanHwEnabled(false), m_connCheckAvailable(false), m_connCheckEnabled(false)
{
	if (!s_self)
		s_self = this;

	m_bus = TQT_DBusConnection::systemBus();
	m_refreshTimer = new TQTimer(this);
	connect(m_refreshTimer, TQ_SIGNAL(timeout()), TQ_SLOT(slotRefreshTimer()));

	if (m_bus.isConnected()) {
		m_bus.connect(this, TQ_SLOT(slotDBusSignal(const TQT_DBusMessage&)));
		addMatchRules();
		refresh();
	} else {
		m_lastError = i18n("Cannot connect to the D-Bus system bus.");
		kdWarning() << "tde-nmctl: no system bus connection" << endl;
	}
}

NMClient::~NMClient()
{
	if (s_self == this)
		s_self = 0;
}

bool NMClient::busConnected() const
{
	return m_bus.isConnected();
}

void NMClient::addMatchRules()
{
	TQStringList rules;
	rules << TQString("type='signal',sender='%1'").arg(NM_DBUS_SERVICE);
	rules << TQString("type='signal',sender='%1',interface='%2',member='NameOwnerChanged',arg0='%3'")
	         .arg(FD_SERVICE_BUS).arg(FD_INTERFACE_BUS).arg(NM_DBUS_SERVICE);
	for (TQStringList::const_iterator it = rules.begin(); it != rules.end(); ++it) {
		TQT_DBusMessage msg = TQT_DBusMessage::methodCall(FD_SERVICE_BUS, FD_PATH_BUS,
		                                                  FD_INTERFACE_BUS, "AddMatch");
		msg << TQT_DBusData::fromString(*it);
		m_bus.send(msg);
	}
}

/* ---------------------------------------------------------------------- */
/* low level                                                              */
/* ---------------------------------------------------------------------- */

TQT_DBusMessage NMClient::call(const TQString &objPath, const TQString &interface,
                               const TQString &method, const TQValueList<TQT_DBusData> &args,
                               int timeout)
{
	bool toBus = objPath == FD_PATH_BUS;
	TQT_DBusMessage msg = TQT_DBusMessage::methodCall(toBus ? FD_SERVICE_BUS : NM_DBUS_SERVICE,
	                                                  objPath, interface, method);
	for (TQValueList<TQT_DBusData>::const_iterator it = args.begin(); it != args.end(); ++it)
		msg << *it;
	msg.setTimeout(timeout > 0 ? timeout : READ_TIMEOUT);

	TQT_DBusError error;
	TQT_DBusMessage reply = m_bus.sendWithReply(msg, &error);
	if (reply.type() == TQT_DBusMessage::ErrorMessage) {
		m_lastErrorName = reply.error().name();
		m_lastError = friendlyError(reply.error().name(), reply.error().message());
	} else if (error.isValid()) {
		m_lastErrorName = error.name();
		m_lastError = friendlyError(error.name(), error.message());
	} else {
		m_lastErrorName = TQString::null;
		m_lastError = TQString::null;
	}
	if (!m_lastErrorName.isEmpty())
		kdDebug() << "tde-nmctl: " << method << " on " << objPath << " failed: "
		          << m_lastErrorName << ": " << m_lastError << endl;
	return reply;
}

bool NMClient::callAsync(const TQString &objPath, const TQString &interface, const TQString &method,
                         const TQValueList<TQT_DBusData> &args, const TQString &description)
{
	TQT_DBusMessage msg = TQT_DBusMessage::methodCall(NM_DBUS_SERVICE, objPath, interface, method);
	for (TQValueList<TQT_DBusData>::const_iterator it = args.begin(); it != args.end(); ++it)
		msg << *it;
	msg.setTimeout(CALL_TIMEOUT);

	int id = m_bus.sendWithAsyncReply(msg, this, TQ_SLOT(slotAsyncReply(const TQT_DBusMessage&)));
	if (id == 0) {
		TQString err = friendlyError(m_bus.lastError().name(), m_bus.lastError().message());
		emit operationFinished(false, description, err);
		return false;
	}
	m_pending[id] = description;
	return true;
}

void NMClient::slotAsyncReply(const TQT_DBusMessage &reply)
{
	int id = reply.replySerialNumber();
	TQString description;
	if (m_pending.contains(id)) {
		description = m_pending[id];
		m_pending.remove(id);
	}
	if (reply.type() == TQT_DBusMessage::ErrorMessage) {
		TQT_DBusError err = reply.error();
		emit operationFinished(false, description, friendlyError(err.name(), err.message()));
	} else {
		emit operationFinished(true, description, TQString::null);
	}
	scheduleRefresh(false);
}

NMVariantMap NMClient::getAll(const TQString &objPath, const TQString &interface)
{
	TQT_DBusMessage reply = call(objPath, DBUS_INTERFACE_PROPERTIES, "GetAll",
	                             args1(TQT_DBusData::fromString(interface)));
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return NMVariantMap();
	return toVariantMap(reply[0]);
}

bool NMClient::setProperty(const TQString &objPath, const TQString &interface, const TQString &name,
                           const TQT_DBusData &value, const TQString &description)
{
	TQValueList<TQT_DBusData> a;
	a << TQT_DBusData::fromString(interface) << TQT_DBusData::fromString(name) << variant(value);
	return callAsync(objPath, DBUS_INTERFACE_PROPERTIES, "Set", a, description);
}

/* ---------------------------------------------------------------------- */
/* signals and refresh                                                    */
/* ---------------------------------------------------------------------- */

void NMClient::slotDBusSignal(const TQT_DBusMessage &message)
{
	if (message.member() == "NameOwnerChanged" && message.interface() == FD_INTERFACE_BUS) {
		if (message.count() >= 3 && message[0].toString() == NM_DBUS_SERVICE) {
			bool running = !message[2].toString().isEmpty();
			if (running != m_running) {
				m_running = running;
				emit availabilityChanged(running);
			}
			scheduleRefresh(true);
		}
		return;
	}
	if (message.sender() != NM_DBUS_SERVICE && !message.path().startsWith(NM_DBUS_PATH))
		return;

	TQString iface = message.interface();
	TQString member = message.member();

	if (iface == NM_DBUS_INTERFACE_DEVICE && member == "StateChanged" && message.count() >= 3) {
		emit deviceStateChanged(message.path(), toUInt(message[0]), toUInt(message[1]),
		                        toUInt(message[2]));
	}

	bool settings = iface == NM_DBUS_INTERFACE_SETTINGS || iface == NM_DBUS_INTERFACE_CONNECTION ||
	                message.path().startsWith(NM_DBUS_PATH_SETTINGS);
	if (member == "PropertiesChanged" && message.count() >= 1 &&
	    message[0].type() == TQT_DBusData::String &&
	    message[0].toString().startsWith(NM_DBUS_INTERFACE_SETTINGS))
		settings = true;
	scheduleRefresh(settings);
}

void NMClient::scheduleRefresh(bool connections)
{
	m_refreshDevices = true;
	if (connections)
		m_refreshConnections = true;
	if (!m_refreshTimer->isActive())
		m_refreshTimer->start(250, true);
}

void NMClient::slotRefreshTimer()
{
	bool conns = m_refreshConnections;
	m_refreshConnections = false;
	m_refreshDevices = false;

	checkRunning();
	if (m_running) {
		readManager();
		readDevices();
		readActive();
		if (conns)
			readConnections();
	} else {
		m_devices.clear();
		m_active.clear();
		m_connections.clear();
		m_state = NM_STATE_UNKNOWN;
	}
	if (conns)
		emit connectionsChanged();
	emit changed();
}

void NMClient::refresh()
{
	m_refreshConnections = true;
	m_refreshTimer->stop();
	slotRefreshTimer();
}

void NMClient::checkRunning()
{
	TQT_DBusMessage reply = call(FD_PATH_BUS, FD_INTERFACE_BUS, "NameHasOwner",
	                             args1(TQT_DBusData::fromString(NM_DBUS_SERVICE)));
	bool running = reply.type() == TQT_DBusMessage::ReplyMessage && reply.count() >= 1 &&
	               reply[0].toBool();
	if (running != m_running) {
		m_running = running;
		emit availabilityChanged(running);
	}
}

void NMClient::readManager()
{
	NMVariantMap p = getAll(NM_DBUS_PATH, NM_DBUS_INTERFACE);
	m_version = toStr(p["Version"]);
	m_state = toUInt(p["State"]);
	m_connectivity = toUInt(p["Connectivity"]);
	m_networkingEnabled = toBool(p["NetworkingEnabled"], true);
	m_wirelessEnabled = toBool(p["WirelessEnabled"]);
	m_wirelessHwEnabled = toBool(p["WirelessHardwareEnabled"], true);
	m_wwanEnabled = toBool(p["WwanEnabled"]);
	m_wwanHwEnabled = toBool(p["WwanHardwareEnabled"], true);
	m_connCheckAvailable = toBool(p["ConnectivityCheckAvailable"]);
	m_connCheckEnabled = toBool(p["ConnectivityCheckEnabled"]);
	m_primaryConnection = toStr(p["PrimaryConnection"]);

	NMVariantMap s = getAll(NM_DBUS_PATH_SETTINGS, NM_DBUS_INTERFACE_SETTINGS);
	m_hostname = toStr(s["Hostname"]);
}

NMAccessPoint NMClient::readAccessPoint(const TQString &apPath)
{
	NMVariantMap p = getAll(apPath, NM_DBUS_INTERFACE_AP);
	NMAccessPoint ap;
	ap.path = apPath;
	ap.ssid = toBytes(p["Ssid"]);
	ap.bssid = toStr(p["HwAddress"]);
	ap.strength = toUInt(p["Strength"]);
	ap.frequency = toUInt(p["Frequency"]);
	ap.flags = toUInt(p["Flags"]);
	ap.wpaFlags = toUInt(p["WpaFlags"]);
	ap.rsnFlags = toUInt(p["RsnFlags"]);
	ap.mode = toUInt(p["Mode"]);
	ap.maxBitrate = toUInt(p["MaxBitrate"]);
	ap.lastSeen = toInt(p["LastSeen"], -1);
	return ap;
}

void NMClient::readDevices()
{
	NMDeviceList devices;
	NMVariantMap mgr = getAll(NM_DBUS_PATH, NM_DBUS_INTERFACE);
	TQStringList paths = toStrList(mgr["Devices"]);

	for (TQStringList::const_iterator it = paths.begin(); it != paths.end(); ++it) {
		NMVariantMap p = getAll(*it, NM_DBUS_INTERFACE_DEVICE);
		if (p.isEmpty())
			continue;
		NMDeviceInfo d;
		d.path = *it;
		d.iface = toStr(p["Interface"]);
		d.ipIface = toStr(p["IpInterface"]);
		d.driver = toStr(p["Driver"]);
		d.driverVersion = toStr(p["DriverVersion"]);
		d.firmwareVersion = toStr(p["FirmwareVersion"]);
		d.hwAddress = toStr(p["HwAddress"]);
		d.vendor = toStr(p["Vendor"]);
		d.product = toStr(p["Product"]);
		d.type = toUInt(p["DeviceType"]);
		d.state = toUInt(p["State"]);
		TQT_DBusData reason = unwrap(p["StateReason"]);
		if (reason.type() == TQT_DBusData::Struct && reason.toStruct().count() >= 2)
			d.stateReason = toUInt(reason.toStruct()[1]);
		d.managed = toBool(p["Managed"]);
		d.autoconnect = toBool(p["Autoconnect"]);
		d.real = toBool(p["Real"], true);
		d.mtu = toUInt(p["Mtu"]);
		d.metered = toUInt(p["Metered"]);
		d.activeConnection = toStr(p["ActiveConnection"]);
		d.ip4Config = toStr(p["Ip4Config"]);
		d.ip6Config = toStr(p["Ip6Config"]);
		d.availableConnections = toStrList(p["AvailableConnections"]);

		if (d.type == NM_DEVICE_TYPE_ETHERNET) {
			NMVariantMap w = getAll(*it, NM_DBUS_INTERFACE_WIRED);
			d.speed = toUInt(w["Speed"]);
			d.permHwAddress = toStr(w["PermHwAddress"]);
		} else if (d.type == NM_DEVICE_TYPE_WIFI) {
			NMVariantMap w = getAll(*it, NM_DBUS_INTERFACE_WIRELESS);
			d.speed = toUInt(w["Bitrate"]);
			d.permHwAddress = toStr(w["PermHwAddress"]);
			d.wirelessCaps = toUInt(w["WirelessCapabilities"]);
			d.activeAccessPoint = toStr(w["ActiveAccessPoint"]);
			TQStringList aps = toStrList(w["AccessPoints"]);
			for (TQStringList::const_iterator a = aps.begin(); a != aps.end(); ++a) {
				NMAccessPoint ap = readAccessPoint(*a);
				if (!ap.bssid.isEmpty() || !ap.ssid.isEmpty())
					d.accessPoints << ap;
			}
		} else if (d.type == NM_DEVICE_TYPE_MODEM) {
			NMVariantMap m = getAll(*it, NM_DBUS_INTERFACE_MODEM);
			uint caps = toUInt(m["CurrentCapabilities"]);
			TQStringList c;
			if (caps & 0x1) c << "POTS";
			if (caps & 0x2) c << "CDMA/EVDO";
			if (caps & 0x4) c << "GSM/UMTS";
			if (caps & 0x8) c << "LTE";
			if (caps & 0x20) c << "5G";
			d.modemCaps = c.join(", ");
		}
		devices << d;
	}
	m_devices = devices;
}

void NMClient::readActive()
{
	NMActiveList list;
	NMVariantMap mgr = getAll(NM_DBUS_PATH, NM_DBUS_INTERFACE);
	TQStringList paths = toStrList(mgr["ActiveConnections"]);
	for (TQStringList::const_iterator it = paths.begin(); it != paths.end(); ++it) {
		NMVariantMap p = getAll(*it, NM_DBUS_INTERFACE_ACTIVE);
		if (p.isEmpty())
			continue;
		NMActiveConnection a;
		a.path = *it;
		a.id = toStr(p["Id"]);
		a.uuid = toStr(p["Uuid"]);
		a.type = toStr(p["Type"]);
		a.connection = toStr(p["Connection"]);
		a.specificObject = toStr(p["SpecificObject"]);
		a.devices = toStrList(p["Devices"]);
		a.state = toUInt(p["State"]);
		a.isDefault = toBool(p["Default"]);
		a.isDefault6 = toBool(p["Default6"]);
		a.vpn = toBool(p["Vpn"]);
		a.ip4Config = toStr(p["Ip4Config"]);
		a.ip6Config = toStr(p["Ip6Config"]);
		if (a.vpn) {
			NMVariantMap v = getAll(*it, NM_DBUS_INTERFACE_VPN_ACTIVE);
			a.vpnState = toUInt(v["VpnState"]);
		}
		list << a;
	}
	m_active = list;
}

void NMClient::readConnections()
{
	NMConnectionList list;
	TQT_DBusMessage reply = call(NM_DBUS_PATH_SETTINGS, NM_DBUS_INTERFACE_SETTINGS, "ListConnections");
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1) {
		m_connections.clear();
		return;
	}
	TQStringList paths = toStrList(reply[0]);
	for (TQStringList::const_iterator it = paths.begin(); it != paths.end(); ++it) {
		TQT_DBusMessage r = call(*it, NM_DBUS_INTERFACE_CONNECTION, "GetSettings");
		if (r.type() != TQT_DBusMessage::ReplyMessage || r.count() < 1)
			continue;
		NMConnection c;
		c.path = *it;
		c.settings = ConnectionSettings(toSettings(r[0]));
		NMVariantMap p = getAll(*it, NM_DBUS_INTERFACE_CONNECTION);
		c.unsaved = toBool(p["Unsaved"]);
		c.flags = toUInt(p["Flags"]);
		c.filename = toStr(p["Filename"]);
		list << c;
	}
	m_connections = list;
}

/* ---------------------------------------------------------------------- */
/* lookups                                                                */
/* ---------------------------------------------------------------------- */

bool NMClient::hasWirelessDevice() const
{
	for (NMDeviceList::const_iterator it = m_devices.begin(); it != m_devices.end(); ++it)
		if ((*it).type == NM_DEVICE_TYPE_WIFI)
			return true;
	return false;
}

bool NMClient::hasModemDevice() const
{
	for (NMDeviceList::const_iterator it = m_devices.begin(); it != m_devices.end(); ++it)
		if ((*it).type == NM_DEVICE_TYPE_MODEM)
			return true;
	return false;
}

const NMDeviceInfo *NMClient::device(const TQString &p) const
{
	for (NMDeviceList::const_iterator it = m_devices.begin(); it != m_devices.end(); ++it)
		if ((*it).path == p)
			return &(*it);
	return 0;
}

const NMDeviceInfo *NMClient::deviceByInterface(const TQString &iface) const
{
	for (NMDeviceList::const_iterator it = m_devices.begin(); it != m_devices.end(); ++it)
		if ((*it).iface == iface)
			return &(*it);
	return 0;
}

const NMActiveConnection *NMClient::activeConnection(const TQString &p) const
{
	for (NMActiveList::const_iterator it = m_active.begin(); it != m_active.end(); ++it)
		if ((*it).path == p)
			return &(*it);
	return 0;
}

const NMActiveConnection *NMClient::activeForConnection(const TQString &connectionPath) const
{
	for (NMActiveList::const_iterator it = m_active.begin(); it != m_active.end(); ++it)
		if ((*it).connection == connectionPath)
			return &(*it);
	return 0;
}

const NMConnection *NMClient::connection(const TQString &p) const
{
	for (NMConnectionList::const_iterator it = m_connections.begin(); it != m_connections.end(); ++it)
		if ((*it).path == p)
			return &(*it);
	return 0;
}

const NMConnection *NMClient::connectionByUuid(const TQString &uuid) const
{
	for (NMConnectionList::const_iterator it = m_connections.begin(); it != m_connections.end(); ++it)
		if ((*it).uuid() == uuid)
			return &(*it);
	return 0;
}

TQValueList<const NMConnection *> NMClient::connectionsForSsid(const TQByteArray &ssid) const
{
	TQValueList<const NMConnection *> result;
	for (NMConnectionList::const_iterator it = m_connections.begin(); it != m_connections.end(); ++it) {
		if ((*it).type() != "802-11-wireless")
			continue;
		if ((*it).settings.bytes("802-11-wireless", "ssid") == ssid)
			result << &(*it);
	}
	return result;
}

NMIpConfig NMClient::ipConfig(const TQString &cfgPath, bool ipv6)
{
	NMIpConfig cfg;
	if (!validPath(cfgPath))
		return cfg;
	NMVariantMap p = getAll(cfgPath, ipv6 ? NM_DBUS_INTERFACE_IP6CONFIG : NM_DBUS_INTERFACE_IP4CONFIG);

	TQValueList<NMVariantMap> addrs = toMapList(p["AddressData"]);
	for (TQValueList<NMVariantMap>::const_iterator it = addrs.begin(); it != addrs.end(); ++it)
		cfg.addresses << TQString("%1/%2").arg(toStr((*it)["address"])).arg(toUInt((*it)["prefix"]));
	cfg.gateway = toStr(p["Gateway"]);

	if (ipv6) {
		TQValueList<TQByteArray> ns = toBytesList(p["Nameservers"]);
		for (TQValueList<TQByteArray>::const_iterator it = ns.begin(); it != ns.end(); ++it)
			cfg.dns << ip6ToString(*it);
	} else if (p.contains("NameserverData")) {
		TQValueList<NMVariantMap> ns = toMapList(p["NameserverData"]);
		for (TQValueList<NMVariantMap>::const_iterator it = ns.begin(); it != ns.end(); ++it)
			cfg.dns << toStr((*it)["address"]);
	} else {
		TQValueList<TQ_UINT32> ns = toUIntList(p["Nameservers"]);
		for (TQValueList<TQ_UINT32>::const_iterator it = ns.begin(); it != ns.end(); ++it)
			cfg.dns << ip4ToString(*it);
	}
	cfg.domains = toStrList(p["Domains"]) + toStrList(p["Searches"]);

	TQValueList<NMVariantMap> routes = toMapList(p["RouteData"]);
	for (TQValueList<NMVariantMap>::const_iterator it = routes.begin(); it != routes.end(); ++it) {
		TQString r = TQString("%1/%2").arg(toStr((*it)["dest"])).arg(toUInt((*it)["prefix"]));
		if ((*it).contains("next-hop"))
			r += " via " + toStr((*it)["next-hop"]);
		if ((*it).contains("metric"))
			r += TQString(" metric %1").arg(toUInt((*it)["metric"]));
		cfg.routes << r;
	}
	return cfg;
}

bool NMClient::deviceStatistics(const TQString &devicePath, TQ_UINT64 *rx, TQ_UINT64 *tx)
{
	NMVariantMap p = getAll(devicePath, NM_DBUS_INTERFACE_STATISTICS);
	if (p.isEmpty())
		return false;
	*rx = toUInt64(p["RxBytes"]);
	*tx = toUInt64(p["TxBytes"]);
	return true;
}

void NMClient::setStatisticsRefresh(const TQString &devicePath, uint ms)
{
	TQValueList<TQT_DBusData> a;
	a << TQT_DBusData::fromString(NM_DBUS_INTERFACE_STATISTICS)
	  << TQT_DBusData::fromString("RefreshRateMs") << variant(TQT_DBusData::fromUInt32(ms));
	TQT_DBusMessage msg = TQT_DBusMessage::methodCall(NM_DBUS_SERVICE, devicePath,
	                                                  DBUS_INTERFACE_PROPERTIES, "Set");
	for (TQValueList<TQT_DBusData>::const_iterator it = a.begin(); it != a.end(); ++it)
		msg << *it;
	m_bus.send(msg);
}

/* ---------------------------------------------------------------------- */
/* actions                                                                */
/* ---------------------------------------------------------------------- */

void NMClient::activateConnection(const TQString &connectionPath, const TQString &devicePath,
                                  const TQString &specificObject)
{
	const NMConnection *c = connection(connectionPath);
	TQValueList<TQT_DBusData> a;
	a << path(connectionPath) << path(devicePath) << path(specificObject);
	callAsync(NM_DBUS_PATH, NM_DBUS_INTERFACE, "ActivateConnection", a,
	          i18n("Connect to %1").arg(c ? c->id() : connectionPath));
}

void NMClient::addAndActivate(const ConnectionSettings &settings, const TQString &devicePath,
                              const TQString &specificObject)
{
	TQValueList<TQT_DBusData> a;
	a << fromSettings(settings.map()) << path(devicePath) << path(specificObject);
	callAsync(NM_DBUS_PATH, NM_DBUS_INTERFACE, "AddAndActivateConnection", a,
	          i18n("Connect to %1").arg(settings.id()));
}

void NMClient::deactivate(const TQString &activePath)
{
	const NMActiveConnection *ac = activeConnection(activePath);
	callAsync(NM_DBUS_PATH, NM_DBUS_INTERFACE, "DeactivateConnection", args1(path(activePath)),
	          i18n("Disconnect %1").arg(ac ? ac->id : activePath));
}

void NMClient::disconnectDevice(const TQString &devicePath)
{
	const NMDeviceInfo *d = device(devicePath);
	callAsync(devicePath, NM_DBUS_INTERFACE_DEVICE, "Disconnect", TQValueList<TQT_DBusData>(),
	          i18n("Disconnect %1").arg(d ? d->iface : devicePath));
}

void NMClient::requestScan(const TQString &devicePath)
{
	for (NMDeviceList::const_iterator it = m_devices.begin(); it != m_devices.end(); ++it) {
		if ((*it).type != NM_DEVICE_TYPE_WIFI)
			continue;
		if (!devicePath.isEmpty() && (*it).path != devicePath)
			continue;
		/* NetworkManager refuses scans that come too often; that is not
		 * worth an error message, so do not wait for the answer */
		TQT_DBusMessage msg = TQT_DBusMessage::methodCall(NM_DBUS_SERVICE, (*it).path,
		                                                  NM_DBUS_INTERFACE_WIRELESS, "RequestScan");
		msg << fromVariantMap(NMVariantMap());
		m_bus.send(msg);
	}
}

void NMClient::setNetworkingEnabled(bool on)
{
	callAsync(NM_DBUS_PATH, NM_DBUS_INTERFACE, "Enable", args1(TQT_DBusData::fromBool(on)),
	          on ? i18n("Enable networking") : i18n("Disable networking"));
}

void NMClient::setWirelessEnabled(bool on)
{
	setProperty(NM_DBUS_PATH, NM_DBUS_INTERFACE, "WirelessEnabled", TQT_DBusData::fromBool(on),
	            on ? i18n("Enable Wi-Fi") : i18n("Disable Wi-Fi"));
}

void NMClient::setWwanEnabled(bool on)
{
	setProperty(NM_DBUS_PATH, NM_DBUS_INTERFACE, "WwanEnabled", TQT_DBusData::fromBool(on),
	            on ? i18n("Enable mobile broadband") : i18n("Disable mobile broadband"));
}

void NMClient::setConnectivityCheckEnabled(bool on)
{
	setProperty(NM_DBUS_PATH, NM_DBUS_INTERFACE, "ConnectivityCheckEnabled", TQT_DBusData::fromBool(on),
	            i18n("Change the connectivity check"));
}

void NMClient::setDeviceManaged(const TQString &devicePath, bool on)
{
	setProperty(devicePath, NM_DBUS_INTERFACE_DEVICE, "Managed", TQT_DBusData::fromBool(on),
	            on ? i18n("Manage device") : i18n("Stop managing device"));
}

void NMClient::setDeviceAutoconnect(const TQString &devicePath, bool on)
{
	setProperty(devicePath, NM_DBUS_INTERFACE_DEVICE, "Autoconnect", TQT_DBusData::fromBool(on),
	            i18n("Change automatic connection"));
}

TQString NMClient::addConnection(const ConnectionSettings &settings, bool persist)
{
	TQT_DBusMessage reply = call(NM_DBUS_PATH_SETTINGS, NM_DBUS_INTERFACE_SETTINGS,
	                             persist ? "AddConnection" : "AddConnectionUnsaved",
	                             args1(fromSettings(settings.map())), CALL_TIMEOUT);
	scheduleRefresh(true);
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return TQString::null;
	return reply[0].toObjectPath();
}

bool NMClient::updateConnection(const TQString &connPath, const ConnectionSettings &settings)
{
	TQT_DBusMessage reply = call(connPath, NM_DBUS_INTERFACE_CONNECTION, "Update",
	                             args1(fromSettings(settings.map())), CALL_TIMEOUT);
	scheduleRefresh(true);
	return reply.type() == TQT_DBusMessage::ReplyMessage;
}

bool NMClient::deleteConnection(const TQString &connPath)
{
	TQT_DBusMessage reply = call(connPath, NM_DBUS_INTERFACE_CONNECTION, "Delete",
	                             TQValueList<TQT_DBusData>(), CALL_TIMEOUT);
	scheduleRefresh(true);
	return reply.type() == TQT_DBusMessage::ReplyMessage;
}

bool NMClient::getSecrets(const TQString &connPath, const TQString &setting, NMSettingsMap *secrets)
{
	TQT_DBusMessage reply = call(connPath, NM_DBUS_INTERFACE_CONNECTION, "GetSecrets",
	                             args1(TQT_DBusData::fromString(setting)), CALL_TIMEOUT);
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return false;
	*secrets = toSettings(reply[0]);
	return true;
}

bool NMClient::saveHostname(const TQString &hostname)
{
	TQT_DBusMessage reply = call(NM_DBUS_PATH_SETTINGS, NM_DBUS_INTERFACE_SETTINGS, "SaveHostname",
	                             args1(TQT_DBusData::fromString(hostname)), CALL_TIMEOUT);
	scheduleRefresh(false);
	return reply.type() == TQT_DBusMessage::ReplyMessage;
}

bool NMClient::reloadConnections()
{
	TQT_DBusMessage reply = call(NM_DBUS_PATH_SETTINGS, NM_DBUS_INTERFACE_SETTINGS,
	                             "ReloadConnections", TQValueList<TQT_DBusData>(), CALL_TIMEOUT);
	scheduleRefresh(true);
	return reply.type() == TQT_DBusMessage::ReplyMessage;
}

uint NMClient::checkConnectivity()
{
	TQT_DBusMessage reply = call(NM_DBUS_PATH, NM_DBUS_INTERFACE, "CheckConnectivity",
	                             TQValueList<TQT_DBusData>(), CALL_TIMEOUT);
	scheduleRefresh(false);
	if (reply.type() != TQT_DBusMessage::ReplyMessage || reply.count() < 1)
		return NM_CONNECTIVITY_UNKNOWN;
	return toUInt(reply[0]);
}

/* ---------------------------------------------------------------------- */

bool NMClient::polkitAgentRunning()
{
	static const char *agents[] = {
		"polkit-agent-tde",
		"polkit-kde-authentication-agent-1",
		"polkit-gnome-authentication-agent-1",
		"polkit-mate-authentication-agent-1",
		"lxpolkit",
		"lxqt-policykit-agent",
		"xfce-polkit",
		"mate-polkit",
		"polkit-1-auth-agent",
		0
	};

	TQDir proc("/proc");
	TQStringList pids = proc.entryList(TQDir::Dirs);
	for (TQStringList::const_iterator it = pids.begin(); it != pids.end(); ++it) {
		bool isPid = false;
		(*it).toInt(&isPid);
		if (!isPid)
			continue;
		TQFile f(TQString("/proc/%1/cmdline").arg(*it));
		if (!f.open(IO_ReadOnly))
			continue;
		TQByteArray raw = f.readAll();
		f.close();
		if (raw.isEmpty())
			continue;
		TQString cmd = TQString::fromLocal8Bit(raw.data(), strnlen(raw.data(), raw.size()));
		cmd = cmd.section('/', -1);
		for (int i = 0; agents[i]; ++i)
			if (cmd == agents[i])
				return true;
	}
	return false;
}

TQString NMClient::friendlyError(const TQString &name, const TQString &message)
{
	if (name.endsWith(".PermissionDenied") || name == "org.freedesktop.DBus.Error.AccessDenied" ||
	    name == "org.freedesktop.DBus.Error.InteractiveAuthorizationRequired")
		return i18n("Not authorized. Authentication failed or was cancelled. "
		            "Make sure a polkit authentication agent such as polkit-agent-tde is running.");
	if (name == "org.freedesktop.DBus.Error.ServiceUnknown" ||
	    name == "org.freedesktop.DBus.Error.NameHasNoOwner")
		return i18n("NetworkManager is not running.");
	if (name == "org.freedesktop.DBus.Error.NoReply" || name == "org.freedesktop.DBus.Error.Timeout")
		return i18n("NetworkManager did not answer in time.");
	if (name.endsWith(".NoSecrets"))
		return i18n("No password or key was given.");
	if (name.endsWith(".UserCanceled"))
		return i18n("The password request was cancelled.");
	if (message.isEmpty())
		return name;
	return message;
}

#include "nmclient.moc"
