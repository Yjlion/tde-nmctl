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

#include "secretstore.h"
#include "nmclient.h"

#include <tdeapplication.h>
#include <tdelocale.h>
#include <tdewallet.h>
#include <kdebug.h>

static const char *WALLET_FOLDER = "tde-nmctl";

/* used when TDE Wallet is disabled or cannot be opened */
static TQMap<TQString, SecretMap> s_memory;

static TDEWallet::Wallet *wallet()
{
	static TDEWallet::Wallet *w = 0;
	static bool failed = false;
	if (w && w->isOpen())
		return w;
	if (failed || !TDEWallet::Wallet::isEnabled())
		return 0;
	delete w;
	w = TDEWallet::Wallet::openWallet(TDEWallet::Wallet::NetworkWallet(), 0,
	                                  TDEWallet::Wallet::Synchronous);
	if (!w) {
		/* do not ask again in this session after the user said no */
		failed = true;
		kdWarning() << "tde-nmctl: cannot open TDE Wallet; secrets are kept in memory" << endl;
		return 0;
	}
	if (!w->hasFolder(WALLET_FOLDER))
		w->createFolder(WALLET_FOLDER);
	w->setFolder(WALLET_FOLDER);
	return w;
}

static TQString entryKey(const TQString &uuid, const TQString &setting)
{
	return uuid + ";" + setting;
}

bool SecretStore::walletEnabled()
{
	return TDEWallet::Wallet::isEnabled();
}

bool SecretStore::load(const TQString &uuid, const TQString &setting, SecretMap *secrets)
{
	TQString key = entryKey(uuid, setting);
	if (s_memory.contains(key)) {
		*secrets = s_memory[key];
		return true;
	}
	TDEWallet::Wallet *w = wallet();
	if (!w || !w->hasEntry(key))
		return false;
	SecretMap m;
	if (w->readMap(key, m) != 0)
		return false;
	*secrets = m;
	return true;
}

bool SecretStore::save(const TQString &uuid, const TQString &setting, const SecretMap &secrets)
{
	TQString key = entryKey(uuid, setting);
	TDEWallet::Wallet *w = wallet();
	if (!w) {
		s_memory[key] = secrets;
		return false;
	}
	s_memory.remove(key);
	if (secrets.isEmpty()) {
		if (w->hasEntry(key))
			w->removeEntry(key);
		return true;
	}
	return w->writeMap(key, secrets) == 0;
}

void SecretStore::removeConnection(const TQString &uuid)
{
	TQStringList settings = secretSettings();
	for (TQStringList::const_iterator it = settings.begin(); it != settings.end(); ++it)
		s_memory.remove(entryKey(uuid, *it));
	TDEWallet::Wallet *w = wallet();
	if (!w)
		return;
	TQStringList entries = w->entryList();
	for (TQStringList::const_iterator it = entries.begin(); it != entries.end(); ++it)
		if ((*it).startsWith(uuid + ";"))
			w->removeEntry(*it);
}

TQStringList SecretStore::secretSettings()
{
	TQStringList s;
	s << "802-11-wireless-security" << "802-1x" << "vpn" << "wireguard" << "pppoe" << "gsm"
	  << "cdma" << "adsl" << "macsec";
	return s;
}

TQStringList SecretStore::secretKeys(const ConnectionSettings &conn, const TQString &setting)
{
	TQStringList k;
	if (setting == "802-11-wireless-security")
		k << "psk" << "wep-key0" << "wep-key1" << "wep-key2" << "wep-key3" << "leap-password";
	else if (setting == "802-1x")
		k << "password" << "private-key-password" << "phase2-private-key-password" << "pin";
	else if (setting == "wireguard")
		k << "private-key";
	else if (setting == "pppoe" || setting == "cdma" || setting == "adsl")
		k << "password";
	else if (setting == "gsm")
		k << "password" << "pin";
	else if (setting == "macsec")
		k << "mka-cak";
	else if (setting == "vpn") {
		/* every key of vpn.data that has a -flags entry is a secret */
		TQMap<TQString, TQString> data = conn.strMap("vpn", "data");
		for (TQMap<TQString, TQString>::const_iterator it = data.begin(); it != data.end(); ++it)
			if (it.key().endsWith("-flags"))
				k << it.key().left(it.key().length() - 6);
		TQMap<TQString, TQString> secrets = conn.strMap("vpn", "secrets");
		for (TQMap<TQString, TQString>::const_iterator it = secrets.begin(); it != secrets.end(); ++it)
			if (!k.contains(it.key()))
				k << it.key();
		if (k.isEmpty())
			k << "password";
	}
	return k;
}

uint SecretStore::secretFlags(const ConnectionSettings &conn, const TQString &setting, const TQString &key)
{
	if (setting == "vpn")
		return conn.strMap("vpn", "data")[key + "-flags"].toUInt();
	if (setting == "802-11-wireless-security" && key.startsWith("wep-key"))
		return conn.uint(setting, "wep-key-flags");
	return conn.uint(setting, key + "-flags");
}

TQString SecretStore::secretValue(const ConnectionSettings &conn, const TQString &setting, const TQString &key)
{
	if (setting == "vpn")
		return conn.strMap("vpn", "secrets")[key];
	return conn.str(setting, key);
}

void SecretStore::setSecretValue(ConnectionSettings &conn, const TQString &setting, const TQString &key,
                                 const TQString &value)
{
	if (setting == "vpn") {
		TQMap<TQString, TQString> s = conn.strMap("vpn", "secrets");
		if (value.isEmpty())
			s.remove(key);
		else
			s[key] = value;
		conn.setStrMap("vpn", "secrets", s);
		return;
	}
	conn.setStr(setting, key, value);
}

TQStringList SecretStore::requiredSecrets(const ConnectionSettings &conn, const TQString &setting,
                                          const TQStringList &hints)
{
	TQStringList candidates = secretKeys(conn, setting);
	TQStringList result;

	/* NetworkManager names the secrets it wants in the hints */
	for (TQStringList::const_iterator it = hints.begin(); it != hints.end(); ++it)
		if (candidates.contains(*it) && !result.contains(*it))
			result << *it;
	if (!result.isEmpty())
		return result;

	if (setting == "802-11-wireless-security") {
		TQString km = conn.str(setting, "key-mgmt");
		if (km == "wpa-psk" || km == "sae")
			result << "psk";
		else if (km == "none")
			result << TQString("wep-key%1").arg(conn.uint(setting, "wep-tx-keyidx"));
		else if (km == "ieee8021x" && conn.str(setting, "auth-alg") == "leap")
			result << "leap-password";
	} else if (setting == "802-1x") {
		TQStringList eap = conn.strList(setting, "eap");
		if (eap.contains("tls") && eap.count() == 1)
			result << "private-key-password";
		else
			result << "password";
	} else if (setting == "gsm") {
		result << "password";
	} else {
		for (TQStringList::const_iterator it = candidates.begin(); it != candidates.end(); ++it) {
			uint f = secretFlags(conn, setting, *it);
			if (!(f & NM_SECRET_FLAG_NOT_REQUIRED))
				result << *it;
		}
	}

	/* drop secrets that are marked "not required" */
	TQStringList filtered;
	for (TQStringList::const_iterator it = result.begin(); it != result.end(); ++it)
		if (!(secretFlags(conn, setting, *it) & NM_SECRET_FLAG_NOT_REQUIRED))
			filtered << *it;
	return filtered;
}

SecretMap SecretStore::agentOwned(const ConnectionSettings &conn, const TQString &setting)
{
	SecretMap m;
	TQStringList keys = secretKeys(conn, setting);
	for (TQStringList::const_iterator it = keys.begin(); it != keys.end(); ++it) {
		uint f = secretFlags(conn, setting, *it);
		if (!(f & NM_SECRET_FLAG_AGENT_OWNED) || (f & NM_SECRET_FLAG_NOT_SAVED))
			continue;
		TQString v = secretValue(conn, setting, *it);
		if (!v.isEmpty())
			m[*it] = v;
	}
	return m;
}

bool SecretStore::saveAgentOwned(const ConnectionSettings &conn)
{
	bool any = false;
	TQStringList settings = secretSettings();
	for (TQStringList::const_iterator it = settings.begin(); it != settings.end(); ++it) {
		if (!conn.has(*it))
			continue;
		SecretMap m = agentOwned(conn, *it);
		if (m.isEmpty())
			continue;
		SecretMap old;
		load(conn.uuid(), *it, &old);
		for (SecretMap::const_iterator k = m.begin(); k != m.end(); ++k)
			old[k.key()] = k.data();
		save(conn.uuid(), *it, old);
		any = true;
	}
	return any;
}

void SecretStore::fillAgentOwned(ConnectionSettings &conn)
{
	TQStringList settings = secretSettings();
	for (TQStringList::const_iterator it = settings.begin(); it != settings.end(); ++it) {
		if (!conn.has(*it))
			continue;
		SecretMap m;
		if (!load(conn.uuid(), *it, &m))
			continue;
		for (SecretMap::const_iterator k = m.begin(); k != m.end(); ++k) {
			uint f = secretFlags(conn, *it, k.key());
			if ((f & NM_SECRET_FLAG_AGENT_OWNED) && secretValue(conn, *it, k.key()).isEmpty())
				setSecretValue(conn, *it, k.key(), k.data());
		}
	}
}

NMSettingsMap SecretStore::toSettings(const TQString &setting, const SecretMap &secrets)
{
	NMSettingsMap result;
	NMVariantMap values;
	if (setting == "vpn") {
		values["secrets"] = NMDBus::fromStringMap(secrets);
	} else {
		for (SecretMap::const_iterator it = secrets.begin(); it != secrets.end(); ++it)
			values[it.key()] = TQT_DBusData::fromString(it.data());
	}
	result[setting] = values;
	return result;
}

TQString SecretStore::label(const TQString &setting, const TQString &key)
{
	if (key == "psk" || key == "leap-password" || key == "password")
		return i18n("Password:");
	if (key.startsWith("wep-key"))
		return i18n("WEP key:");
	if (key == "private-key-password" || key == "phase2-private-key-password")
		return i18n("Private key password:");
	if (key == "pin")
		return setting == "gsm" ? i18n("SIM PIN:") : i18n("PIN:");
	if (key == "private-key")
		return i18n("Private key:");
	if (key == "cert-pass")
		return i18n("Certificate password:");
	if (key == "Xauth password")
		return i18n("Group password:");
	if (key == "mka-cak")
		return i18n("MKA CAK:");
	TQString k = key;
	k.replace('-', ' ');
	if (!k.isEmpty())
		k[0] = k[0].upper();
	return k + ":";
}
