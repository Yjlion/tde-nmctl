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

#ifndef NMDBUS_H
#define NMDBUS_H

#include <tqcstring.h>
#include <tqmap.h>
#include <tqstring.h>
#include <tqstringlist.h>
#include <tqvaluelist.h>

#include <tqdbusdata.h>

/* NetworkManager D-Bus names, see NetworkManager's introspection/*.xml */
#define NM_DBUS_SERVICE              "org.freedesktop.NetworkManager"
#define NM_DBUS_PATH                 "/org/freedesktop/NetworkManager"
#define NM_DBUS_PATH_SETTINGS        "/org/freedesktop/NetworkManager/Settings"
#define NM_DBUS_PATH_AGENT_MANAGER   "/org/freedesktop/NetworkManager/AgentManager"
#define NM_DBUS_PATH_SECRET_AGENT    "/org/freedesktop/NetworkManager/SecretAgent"
#define NM_DBUS_INTERFACE            "org.freedesktop.NetworkManager"
#define NM_DBUS_INTERFACE_SETTINGS   "org.freedesktop.NetworkManager.Settings"
#define NM_DBUS_INTERFACE_CONNECTION "org.freedesktop.NetworkManager.Settings.Connection"
#define NM_DBUS_INTERFACE_DEVICE     "org.freedesktop.NetworkManager.Device"
#define NM_DBUS_INTERFACE_WIRELESS   "org.freedesktop.NetworkManager.Device.Wireless"
#define NM_DBUS_INTERFACE_WIRED      "org.freedesktop.NetworkManager.Device.Wired"
#define NM_DBUS_INTERFACE_MODEM      "org.freedesktop.NetworkManager.Device.Modem"
#define NM_DBUS_INTERFACE_STATISTICS "org.freedesktop.NetworkManager.Device.Statistics"
#define NM_DBUS_INTERFACE_AP         "org.freedesktop.NetworkManager.AccessPoint"
#define NM_DBUS_INTERFACE_ACTIVE     "org.freedesktop.NetworkManager.Connection.Active"
#define NM_DBUS_INTERFACE_VPN_ACTIVE "org.freedesktop.NetworkManager.VPN.Connection"
#define NM_DBUS_INTERFACE_IP4CONFIG  "org.freedesktop.NetworkManager.IP4Config"
#define NM_DBUS_INTERFACE_IP6CONFIG  "org.freedesktop.NetworkManager.IP6Config"
#define NM_DBUS_INTERFACE_DHCP4      "org.freedesktop.NetworkManager.DHCP4Config"
#define NM_DBUS_INTERFACE_AGENT_MANAGER "org.freedesktop.NetworkManager.AgentManager"
#define NM_DBUS_INTERFACE_SECRET_AGENT  "org.freedesktop.NetworkManager.SecretAgent"
#define DBUS_INTERFACE_PROPERTIES    "org.freedesktop.DBus.Properties"

/** a{sv} with the variants unwrapped */
typedef TQMap<TQString, TQT_DBusData> NMVariantMap;
/** a{sa{sv}}: setting name -> property -> value */
typedef TQMap<TQString, NMVariantMap> NMSettingsMap;

/**
 * Conversion between dbus-1-tqt data and plain TQt types.
 *
 * dbus-1-tqt needs the element type of every list and map, including
 * empty ones, so the from*() helpers always build typed containers.
 */
namespace NMDBus
{
	TQT_DBusData unwrap(const TQT_DBusData &data);
	TQT_DBusData variant(const TQT_DBusData &data);

	TQString toStr(const TQT_DBusData &data);
	bool toBool(const TQT_DBusData &data, bool def = false);
	TQ_UINT32 toUInt(const TQT_DBusData &data, TQ_UINT32 def = 0);
	TQ_INT32 toInt(const TQT_DBusData &data, TQ_INT32 def = 0);
	TQ_UINT64 toUInt64(const TQT_DBusData &data);
	TQStringList toStrList(const TQT_DBusData &data);   /* as and ao */
	TQByteArray toBytes(const TQT_DBusData &data);      /* ay */
	TQValueList<TQ_UINT32> toUIntList(const TQT_DBusData &data);
	TQValueList<TQByteArray> toBytesList(const TQT_DBusData &data);   /* aay */
	TQValueList<TQValueList<TQ_UINT32> > toUIntListList(const TQT_DBusData &data); /* aau */
	NMVariantMap toVariantMap(const TQT_DBusData &data);
	TQValueList<NMVariantMap> toMapList(const TQT_DBusData &data);    /* aa{sv} */
	TQMap<TQString, TQString> toStringMap(const TQT_DBusData &data);   /* a{ss} */
	NMSettingsMap toSettings(const TQT_DBusData &data);

	TQT_DBusData fromStrList(const TQStringList &list);
	TQT_DBusData fromPathList(const TQStringList &list);
	TQT_DBusData fromBytes(const TQByteArray &bytes);
	TQT_DBusData fromUIntList(const TQValueList<TQ_UINT32> &list);
	TQT_DBusData fromBytesList(const TQValueList<TQByteArray> &list);
	TQT_DBusData fromVariantMap(const NMVariantMap &map);
	TQT_DBusData fromMapList(const TQValueList<NMVariantMap> &list);
	TQT_DBusData fromStringMap(const TQMap<TQString, TQString> &map);
	TQT_DBusData fromSettings(const NMSettingsMap &settings);

	/** a printable form, for --list and debugging */
	TQString dump(const TQT_DBusData &data);

	TQByteArray bytes(const TQString &utf8);
	TQString ssidToString(const TQByteArray &ssid);

	/* IPv4 address in network byte order as used by ipv4.dns (au) */
	TQString ip4ToString(TQ_UINT32 addr);
	bool ip4FromString(const TQString &text, TQ_UINT32 *addr);
	TQString ip6ToString(const TQByteArray &addr);
	bool ip6FromString(const TQString &text, TQByteArray *addr);
}

/**
 * The settings of one connection profile (a{sa{sv}}), with typed access.
 * Keys the editor does not know are kept unchanged.
 */
class ConnectionSettings
{
public:
	ConnectionSettings() {}
	ConnectionSettings(const NMSettingsMap &map) : m_map(map) {}

	NMSettingsMap &map() { return m_map; }
	const NMSettingsMap &map() const { return m_map; }

	bool has(const TQString &setting) const { return m_map.contains(setting); }
	bool has(const TQString &setting, const TQString &key) const;
	TQT_DBusData value(const TQString &setting, const TQString &key) const;

	TQString str(const TQString &setting, const TQString &key, const TQString &def = TQString::null) const;
	bool boolean(const TQString &setting, const TQString &key, bool def = false) const;
	TQ_UINT32 uint(const TQString &setting, const TQString &key, TQ_UINT32 def = 0) const;
	TQ_INT32 integer(const TQString &setting, const TQString &key, TQ_INT32 def = 0) const;
	TQ_UINT64 uint64(const TQString &setting, const TQString &key, TQ_UINT64 def = 0) const;
	TQByteArray bytes(const TQString &setting, const TQString &key) const;
	TQStringList strList(const TQString &setting, const TQString &key) const;
	TQMap<TQString, TQString> strMap(const TQString &setting, const TQString &key) const;
	TQValueList<NMVariantMap> mapList(const TQString &setting, const TQString &key) const;

	void set(const TQString &setting, const TQString &key, const TQT_DBusData &value);
	/** sets the key, or removes it if the text is empty */
	void setStr(const TQString &setting, const TQString &key, const TQString &value);
	void setBool(const TQString &setting, const TQString &key, bool value);
	void setUInt(const TQString &setting, const TQString &key, TQ_UINT32 value);
	void setInt(const TQString &setting, const TQString &key, TQ_INT32 value);
	void setUInt64(const TQString &setting, const TQString &key, TQ_UINT64 value);
	void setBytes(const TQString &setting, const TQString &key, const TQByteArray &value);
	void setStrList(const TQString &setting, const TQString &key, const TQStringList &value);
	void setStrMap(const TQString &setting, const TQString &key, const TQMap<TQString, TQString> &value);
	void setMapList(const TQString &setting, const TQString &key, const TQValueList<NMVariantMap> &value);
	void remove(const TQString &setting, const TQString &key);
	void ensure(const TQString &setting);
	void removeSetting(const TQString &setting);

	TQString id() const { return str("connection", "id"); }
	TQString uuid() const { return str("connection", "uuid"); }
	TQString type() const { return str("connection", "type"); }
	TQString interfaceName() const { return str("connection", "interface-name"); }

	/** merges the secrets returned by GetSecrets() into this */
	void mergeSecrets(const NMSettingsMap &secrets);

	static TQString newUuid();

private:
	NMSettingsMap m_map;
};

#endif
