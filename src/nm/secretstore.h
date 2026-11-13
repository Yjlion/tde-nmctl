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

#ifndef SECRETSTORE_H
#define SECRETSTORE_H

#include <tqmap.h>
#include <tqstring.h>
#include <tqstringlist.h>

#include "nmdbus.h"

typedef TQMap<TQString, TQString> SecretMap;

/**
 * Keeps the secrets of connections whose secrets belong to the user
 * ("agent-owned", the *-flags of the secret contain 0x1) in TDE Wallet,
 * in the folder "tde-nmctl" of the network wallet. The entry of each
 * setting is a map named "<connection uuid>;<setting name>".
 *
 * If TDE Wallet is disabled the secrets are only kept in memory for the
 * running session.
 */
class SecretStore
{
public:
	static bool walletEnabled();

	static bool load(const TQString &uuid, const TQString &setting, SecretMap *secrets);
	static bool save(const TQString &uuid, const TQString &setting, const SecretMap &secrets);
	static void removeConnection(const TQString &uuid);

	/** the settings that can hold secrets */
	static TQStringList secretSettings();
	/** the secret keys a setting can have */
	static TQStringList secretKeys(const ConnectionSettings &conn, const TQString &setting);
	/** flags of one secret (key-flags, or vpn.data[key-flags]) */
	static uint secretFlags(const ConnectionSettings &conn, const TQString &setting, const TQString &key);
	/** the secrets the connection needs to connect */
	static TQStringList requiredSecrets(const ConnectionSettings &conn, const TQString &setting,
	                                    const TQStringList &hints);
	/** the current value of one secret in the connection */
	static TQString secretValue(const ConnectionSettings &conn, const TQString &setting, const TQString &key);
	static void setSecretValue(ConnectionSettings &conn, const TQString &setting, const TQString &key,
	                           const TQString &value);

	/** the agent-owned secrets that are set in the connection */
	static SecretMap agentOwned(const ConnectionSettings &conn, const TQString &setting);
	/** saves all agent-owned secrets of the connection; true if any */
	static bool saveAgentOwned(const ConnectionSettings &conn);
	/** fills the agent-owned secrets of the connection from the wallet */
	static void fillAgentOwned(ConnectionSettings &conn);

	/** {setting: {key: value}}, or {vpn: {secrets: {key: value}}} */
	static NMSettingsMap toSettings(const TQString &setting, const SecretMap &secrets);

	/** a translated label for a secret, "Password", "PIN", ... */
	static TQString label(const TQString &setting, const TQString &key);
};

#endif
