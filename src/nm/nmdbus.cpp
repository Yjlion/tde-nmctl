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

#include "nmdbus.h"

#include <tqfile.h>

#include <tqdbusdatalist.h>
#include <tqdbusdatamap.h>
#include <tqdbusobjectpath.h>
#include <tqdbusvariant.h>

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ---------------------------------------------------------------------- */
/* reading                                                                */
/* ---------------------------------------------------------------------- */

TQT_DBusData NMDBus::unwrap(const TQT_DBusData &data)
{
	TQT_DBusData d = data;
	while (d.type() == TQT_DBusData::Variant)
		d = d.toVariant().value;
	return d;
}

TQT_DBusData NMDBus::variant(const TQT_DBusData &data)
{
	TQT_DBusVariant v;
	v.value = data;
	v.signature = data.buildDBusSignature();
	return TQT_DBusData::fromVariant(v);
}

TQString NMDBus::toStr(const TQT_DBusData &data)
{
	TQT_DBusData d = unwrap(data);
	switch (d.type()) {
	case TQT_DBusData::String:
		return d.toString();
	case TQT_DBusData::ObjectPath:
		return d.toObjectPath();
	case TQT_DBusData::Bool:
		return d.toBool() ? "yes" : "no";
	case TQT_DBusData::Byte:
		return TQString::number(d.toByte());
	case TQT_DBusData::Int16:
		return TQString::number(d.toInt16());
	case TQT_DBusData::UInt16:
		return TQString::number(d.toUInt16());
	case TQT_DBusData::Int32:
		return TQString::number(d.toInt32());
	case TQT_DBusData::UInt32:
		return TQString::number(d.toUInt32());
	case TQT_DBusData::Int64:
		return TQString::number(d.toInt64());
	case TQT_DBusData::UInt64:
		return TQString::number(d.toUInt64());
	case TQT_DBusData::Double:
		return TQString::number(d.toDouble());
	default:
		return TQString::null;
	}
}

bool NMDBus::toBool(const TQT_DBusData &data, bool def)
{
	TQT_DBusData d = unwrap(data);
	if (d.type() == TQT_DBusData::Bool)
		return d.toBool();
	if (d.type() == TQT_DBusData::Invalid)
		return def;
	return toInt(d, def ? 1 : 0) != 0;
}

TQ_UINT32 NMDBus::toUInt(const TQT_DBusData &data, TQ_UINT32 def)
{
	TQT_DBusData d = unwrap(data);
	switch (d.type()) {
	case TQT_DBusData::Byte:   return d.toByte();
	case TQT_DBusData::UInt16: return d.toUInt16();
	case TQT_DBusData::Int16:  return d.toInt16();
	case TQT_DBusData::UInt32: return d.toUInt32();
	case TQT_DBusData::Int32:  return d.toInt32();
	case TQT_DBusData::UInt64: return (TQ_UINT32)d.toUInt64();
	case TQT_DBusData::Int64:  return (TQ_UINT32)d.toInt64();
	case TQT_DBusData::Bool:   return d.toBool() ? 1 : 0;
	default: return def;
	}
}

TQ_INT32 NMDBus::toInt(const TQT_DBusData &data, TQ_INT32 def)
{
	TQT_DBusData d = unwrap(data);
	switch (d.type()) {
	case TQT_DBusData::Int32:  return d.toInt32();
	case TQT_DBusData::Int64:  return (TQ_INT32)d.toInt64();
	case TQT_DBusData::Invalid: return def;
	default: return (TQ_INT32)toUInt(d, (TQ_UINT32)def);
	}
}

TQ_UINT64 NMDBus::toUInt64(const TQT_DBusData &data)
{
	TQT_DBusData d = unwrap(data);
	if (d.type() == TQT_DBusData::UInt64)
		return d.toUInt64();
	if (d.type() == TQT_DBusData::Int64)
		return (TQ_UINT64)d.toInt64();
	return toUInt(d);
}

TQStringList NMDBus::toStrList(const TQT_DBusData &data)
{
	TQStringList result;
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::List)
		return result;
	TQValueList<TQT_DBusData> items = d.toTQValueList();
	for (TQValueList<TQT_DBusData>::const_iterator it = items.begin(); it != items.end(); ++it)
		result << toStr(*it);
	return result;
}

TQByteArray NMDBus::toBytes(const TQT_DBusData &data)
{
	TQByteArray result;
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::List)
		return result;
	TQValueList<TQT_DBusData> items = d.toTQValueList();
	result.resize(items.count());
	uint i = 0;
	for (TQValueList<TQT_DBusData>::const_iterator it = items.begin(); it != items.end(); ++it)
		result[i++] = (char)toUInt(*it);
	return result;
}

TQValueList<TQ_UINT32> NMDBus::toUIntList(const TQT_DBusData &data)
{
	TQValueList<TQ_UINT32> result;
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::List)
		return result;
	TQValueList<TQT_DBusData> items = d.toTQValueList();
	for (TQValueList<TQT_DBusData>::const_iterator it = items.begin(); it != items.end(); ++it)
		result << toUInt(*it);
	return result;
}

TQValueList<TQByteArray> NMDBus::toBytesList(const TQT_DBusData &data)
{
	TQValueList<TQByteArray> result;
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::List)
		return result;
	TQValueList<TQT_DBusData> items = d.toTQValueList();
	for (TQValueList<TQT_DBusData>::const_iterator it = items.begin(); it != items.end(); ++it)
		result << toBytes(*it);
	return result;
}

TQValueList<TQValueList<TQ_UINT32> > NMDBus::toUIntListList(const TQT_DBusData &data)
{
	TQValueList<TQValueList<TQ_UINT32> > result;
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::List)
		return result;
	TQValueList<TQT_DBusData> items = d.toTQValueList();
	for (TQValueList<TQT_DBusData>::const_iterator it = items.begin(); it != items.end(); ++it)
		result << toUIntList(*it);
	return result;
}

NMVariantMap NMDBus::toVariantMap(const TQT_DBusData &data)
{
	NMVariantMap map;
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::Map || d.keyType() != TQT_DBusData::String)
		return map;
	TQT_DBusDataMap<TQString> dm = d.toStringKeyMap();
	for (TQT_DBusDataMap<TQString>::const_iterator it = dm.begin(); it != dm.end(); ++it)
		map[it.key()] = unwrap(it.data());
	return map;
}

TQValueList<NMVariantMap> NMDBus::toMapList(const TQT_DBusData &data)
{
	TQValueList<NMVariantMap> result;
	TQT_DBusData d = unwrap(data);
	if (d.type() != TQT_DBusData::List)
		return result;
	TQValueList<TQT_DBusData> items = d.toTQValueList();
	for (TQValueList<TQT_DBusData>::const_iterator it = items.begin(); it != items.end(); ++it)
		result << toVariantMap(*it);
	return result;
}

TQMap<TQString, TQString> NMDBus::toStringMap(const TQT_DBusData &data)
{
	TQMap<TQString, TQString> result;
	NMVariantMap m = toVariantMap(data);
	for (NMVariantMap::const_iterator it = m.begin(); it != m.end(); ++it)
		result[it.key()] = toStr(it.data());
	return result;
}

NMSettingsMap NMDBus::toSettings(const TQT_DBusData &data)
{
	NMSettingsMap result;
	NMVariantMap outer = toVariantMap(data);
	for (NMVariantMap::const_iterator it = outer.begin(); it != outer.end(); ++it)
		result[it.key()] = toVariantMap(it.data());
	return result;
}

/* ---------------------------------------------------------------------- */
/* writing                                                                */
/* ---------------------------------------------------------------------- */

TQT_DBusData NMDBus::fromStrList(const TQStringList &list)
{
	TQT_DBusDataList l(TQT_DBusData::String);
	for (TQStringList::const_iterator it = list.begin(); it != list.end(); ++it)
		l << TQT_DBusData::fromString(*it);
	return TQT_DBusData::fromList(l);
}

TQT_DBusData NMDBus::fromPathList(const TQStringList &list)
{
	TQT_DBusDataList l(TQT_DBusData::ObjectPath);
	for (TQStringList::const_iterator it = list.begin(); it != list.end(); ++it)
		l << TQT_DBusData::fromObjectPath(TQT_DBusObjectPath(*it));
	return TQT_DBusData::fromList(l);
}

TQT_DBusData NMDBus::fromBytes(const TQByteArray &bytes)
{
	TQT_DBusDataList l(TQT_DBusData::Byte);
	for (uint i = 0; i < bytes.size(); ++i)
		l << TQT_DBusData::fromByte((TQ_UINT8)bytes[i]);
	return TQT_DBusData::fromList(l);
}

TQT_DBusData NMDBus::fromUIntList(const TQValueList<TQ_UINT32> &list)
{
	TQT_DBusDataList l(TQT_DBusData::UInt32);
	for (TQValueList<TQ_UINT32>::const_iterator it = list.begin(); it != list.end(); ++it)
		l << TQT_DBusData::fromUInt32(*it);
	return TQT_DBusData::fromList(l);
}

TQT_DBusData NMDBus::fromBytesList(const TQValueList<TQByteArray> &list)
{
	TQT_DBusDataList l(fromBytes(TQByteArray()));
	for (TQValueList<TQByteArray>::const_iterator it = list.begin(); it != list.end(); ++it)
		l << fromBytes(*it);
	return TQT_DBusData::fromList(l);
}

TQT_DBusData NMDBus::fromVariantMap(const NMVariantMap &map)
{
	TQT_DBusDataMap<TQString> m(TQT_DBusData::Variant);
	for (NMVariantMap::const_iterator it = map.begin(); it != map.end(); ++it) {
		if (it.data().type() == TQT_DBusData::Invalid)
			continue;
		m.insert(it.key(), variant(it.data()));
	}
	return TQT_DBusData::fromStringKeyMap(m);
}

TQT_DBusData NMDBus::fromMapList(const TQValueList<NMVariantMap> &list)
{
	TQT_DBusDataList l(fromVariantMap(NMVariantMap()));
	for (TQValueList<NMVariantMap>::const_iterator it = list.begin(); it != list.end(); ++it)
		l << fromVariantMap(*it);
	return TQT_DBusData::fromList(l);
}

TQT_DBusData NMDBus::fromStringMap(const TQMap<TQString, TQString> &map)
{
	TQT_DBusDataMap<TQString> m(TQT_DBusData::String);
	for (TQMap<TQString, TQString>::const_iterator it = map.begin(); it != map.end(); ++it)
		m.insert(it.key(), TQT_DBusData::fromString(it.data()));
	return TQT_DBusData::fromStringKeyMap(m);
}

TQT_DBusData NMDBus::fromSettings(const NMSettingsMap &settings)
{
	TQT_DBusDataMap<TQString> m(fromVariantMap(NMVariantMap()));
	for (NMSettingsMap::const_iterator it = settings.begin(); it != settings.end(); ++it)
		m.insert(it.key(), fromVariantMap(it.data()));
	return TQT_DBusData::fromStringKeyMap(m);
}

/* ---------------------------------------------------------------------- */

TQString NMDBus::dump(const TQT_DBusData &data)
{
	TQT_DBusData d = unwrap(data);
	switch (d.type()) {
	case TQT_DBusData::List: {
		TQStringList parts;
		TQValueList<TQT_DBusData> items = d.toTQValueList();
		for (TQValueList<TQT_DBusData>::const_iterator it = items.begin(); it != items.end(); ++it)
			parts << dump(*it);
		return "[" + parts.join(", ") + "]";
	}
	case TQT_DBusData::Struct: {
		TQStringList parts;
		TQValueList<TQT_DBusData> items = d.toStruct();
		for (TQValueList<TQT_DBusData>::const_iterator it = items.begin(); it != items.end(); ++it)
			parts << dump(*it);
		return "(" + parts.join(", ") + ")";
	}
	case TQT_DBusData::Map: {
		TQStringList parts;
		if (d.keyType() == TQT_DBusData::String) {
			TQT_DBusDataMap<TQString> m = d.toStringKeyMap();
			for (TQT_DBusDataMap<TQString>::const_iterator it = m.begin(); it != m.end(); ++it)
				parts << it.key() + ": " + dump(it.data());
		}
		return "{" + parts.join(", ") + "}";
	}
	case TQT_DBusData::String:
		return "'" + d.toString() + "'";
	default:
		return toStr(d);
	}
}

TQByteArray NMDBus::bytes(const TQString &utf8)
{
	TQCString c = utf8.utf8();
	TQByteArray b;
	b.duplicate(c.data(), c.length());
	return b;
}

TQString NMDBus::ssidToString(const TQByteArray &ssid)
{
	if (ssid.isEmpty())
		return TQString::null;
	return TQString::fromUtf8(ssid.data(), ssid.size());
}

TQString NMDBus::ip4ToString(TQ_UINT32 addr)
{
	struct in_addr a;
	a.s_addr = addr;
	char buf[INET_ADDRSTRLEN];
	if (!inet_ntop(AF_INET, &a, buf, sizeof(buf)))
		return TQString::null;
	return TQString::fromLatin1(buf);
}

bool NMDBus::ip4FromString(const TQString &text, TQ_UINT32 *addr)
{
	struct in_addr a;
	if (inet_pton(AF_INET, text.stripWhiteSpace().latin1(), &a) != 1)
		return false;
	*addr = a.s_addr;
	return true;
}

TQString NMDBus::ip6ToString(const TQByteArray &addr)
{
	if (addr.size() != 16)
		return TQString::null;
	char buf[INET6_ADDRSTRLEN];
	if (!inet_ntop(AF_INET6, addr.data(), buf, sizeof(buf)))
		return TQString::null;
	return TQString::fromLatin1(buf);
}

bool NMDBus::ip6FromString(const TQString &text, TQByteArray *addr)
{
	struct in6_addr a;
	if (inet_pton(AF_INET6, text.stripWhiteSpace().latin1(), &a) != 1)
		return false;
	addr->duplicate((const char *)&a, 16);
	return true;
}

/* ---------------------------------------------------------------------- */
/* ConnectionSettings                                                     */
/* ---------------------------------------------------------------------- */

bool ConnectionSettings::has(const TQString &setting, const TQString &key) const
{
	NMSettingsMap::const_iterator it = m_map.find(setting);
	if (it == m_map.end())
		return false;
	return it.data().contains(key);
}

TQT_DBusData ConnectionSettings::value(const TQString &setting, const TQString &key) const
{
	NMSettingsMap::const_iterator it = m_map.find(setting);
	if (it == m_map.end())
		return TQT_DBusData();
	NMVariantMap::const_iterator v = it.data().find(key);
	if (v == it.data().end())
		return TQT_DBusData();
	return v.data();
}

TQString ConnectionSettings::str(const TQString &setting, const TQString &key, const TQString &def) const
{
	TQT_DBusData d = value(setting, key);
	if (d.type() == TQT_DBusData::Invalid)
		return def;
	return NMDBus::toStr(d);
}

bool ConnectionSettings::boolean(const TQString &setting, const TQString &key, bool def) const
{
	return NMDBus::toBool(value(setting, key), def);
}

TQ_UINT32 ConnectionSettings::uint(const TQString &setting, const TQString &key, TQ_UINT32 def) const
{
	return NMDBus::toUInt(value(setting, key), def);
}

TQ_INT32 ConnectionSettings::integer(const TQString &setting, const TQString &key, TQ_INT32 def) const
{
	return NMDBus::toInt(value(setting, key), def);
}

TQ_UINT64 ConnectionSettings::uint64(const TQString &setting, const TQString &key, TQ_UINT64 def) const
{
	TQT_DBusData d = value(setting, key);
	if (d.type() == TQT_DBusData::Invalid)
		return def;
	return NMDBus::toUInt64(d);
}

TQByteArray ConnectionSettings::bytes(const TQString &setting, const TQString &key) const
{
	return NMDBus::toBytes(value(setting, key));
}

TQStringList ConnectionSettings::strList(const TQString &setting, const TQString &key) const
{
	return NMDBus::toStrList(value(setting, key));
}

TQMap<TQString, TQString> ConnectionSettings::strMap(const TQString &setting, const TQString &key) const
{
	return NMDBus::toStringMap(value(setting, key));
}

TQValueList<NMVariantMap> ConnectionSettings::mapList(const TQString &setting, const TQString &key) const
{
	return NMDBus::toMapList(value(setting, key));
}

void ConnectionSettings::set(const TQString &setting, const TQString &key, const TQT_DBusData &value)
{
	m_map[setting][key] = value;
}

void ConnectionSettings::setStr(const TQString &setting, const TQString &key, const TQString &value)
{
	if (value.isEmpty())
		remove(setting, key);
	else
		set(setting, key, TQT_DBusData::fromString(value));
}

void ConnectionSettings::setBool(const TQString &setting, const TQString &key, bool value)
{
	set(setting, key, TQT_DBusData::fromBool(value));
}

void ConnectionSettings::setUInt(const TQString &setting, const TQString &key, TQ_UINT32 value)
{
	set(setting, key, TQT_DBusData::fromUInt32(value));
}

void ConnectionSettings::setInt(const TQString &setting, const TQString &key, TQ_INT32 value)
{
	set(setting, key, TQT_DBusData::fromInt32(value));
}

void ConnectionSettings::setUInt64(const TQString &setting, const TQString &key, TQ_UINT64 value)
{
	set(setting, key, TQT_DBusData::fromUInt64(value));
}

void ConnectionSettings::setBytes(const TQString &setting, const TQString &key, const TQByteArray &value)
{
	if (value.isEmpty())
		remove(setting, key);
	else
		set(setting, key, NMDBus::fromBytes(value));
}

void ConnectionSettings::setStrList(const TQString &setting, const TQString &key, const TQStringList &value)
{
	set(setting, key, NMDBus::fromStrList(value));
}

void ConnectionSettings::setStrMap(const TQString &setting, const TQString &key,
                                   const TQMap<TQString, TQString> &value)
{
	set(setting, key, NMDBus::fromStringMap(value));
}

void ConnectionSettings::setMapList(const TQString &setting, const TQString &key,
                                    const TQValueList<NMVariantMap> &value)
{
	set(setting, key, NMDBus::fromMapList(value));
}

void ConnectionSettings::remove(const TQString &setting, const TQString &key)
{
	NMSettingsMap::iterator it = m_map.find(setting);
	if (it != m_map.end())
		it.data().remove(key);
}

void ConnectionSettings::ensure(const TQString &setting)
{
	if (!m_map.contains(setting))
		m_map[setting] = NMVariantMap();
}

void ConnectionSettings::removeSetting(const TQString &setting)
{
	m_map.remove(setting);
}

void ConnectionSettings::mergeSecrets(const NMSettingsMap &secrets)
{
	for (NMSettingsMap::const_iterator s = secrets.begin(); s != secrets.end(); ++s) {
		for (NMVariantMap::const_iterator k = s.data().begin(); k != s.data().end(); ++k) {
			if (k.key() == "secrets" && s.key() == "vpn") {
				/* vpn.secrets is a{ss}; merge per key */
				TQMap<TQString, TQString> old = strMap("vpn", "secrets");
				TQMap<TQString, TQString> add = NMDBus::toStringMap(k.data());
				for (TQMap<TQString, TQString>::const_iterator a = add.begin(); a != add.end(); ++a)
					old[a.key()] = a.data();
				setStrMap("vpn", "secrets", old);
			} else if (k.key() == "peers" && s.key() == "wireguard") {
				/* the preshared keys come as a list of peers keyed by public-key */
				TQValueList<NMVariantMap> peers = mapList("wireguard", "peers");
				TQValueList<NMVariantMap> add = NMDBus::toMapList(k.data());
				for (TQValueList<NMVariantMap>::iterator p = peers.begin(); p != peers.end(); ++p) {
					for (TQValueList<NMVariantMap>::const_iterator a = add.begin(); a != add.end(); ++a) {
						if (NMDBus::toStr((*a)["public-key"]) == NMDBus::toStr((*p)["public-key"]) &&
						    (*a).contains("preshared-key"))
							(*p)["preshared-key"] = (*a)["preshared-key"];
					}
				}
				setMapList("wireguard", "peers", peers);
			} else {
				m_map[s.key()][k.key()] = k.data();
			}
		}
	}
}

TQString ConnectionSettings::newUuid()
{
	TQFile f("/proc/sys/kernel/random/uuid");
	if (f.open(IO_ReadOnly)) {
		TQString line;
		f.readLine(line, 64);
		line = line.stripWhiteSpace();
		if (line.length() == 36)
			return line;
	}
	/* RFC 4122 version 4 */
	static bool seeded = false;
	if (!seeded) {
		srandom(time(0) ^ getpid());
		seeded = true;
	}
	unsigned char b[16];
	for (int i = 0; i < 16; ++i)
		b[i] = random() & 0xff;
	b[6] = (b[6] & 0x0f) | 0x40;
	b[8] = (b[8] & 0x3f) | 0x80;
	TQString s;
	for (int i = 0; i < 16; ++i) {
		if (i == 4 || i == 6 || i == 8 || i == 10)
			s += '-';
		s += TQString().sprintf("%02x", b[i]);
	}
	return s;
}
