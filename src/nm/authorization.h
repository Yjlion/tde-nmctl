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

#ifndef AUTHORIZATION_H
#define AUTHORIZATION_H

#include <tqstring.h>

#define NM_AUTH_NETWORK_CONTROL    "org.freedesktop.NetworkManager.network-control"
#define NM_AUTH_ENABLE_NETWORK     "org.freedesktop.NetworkManager.enable-disable-network"
#define NM_AUTH_ENABLE_WIFI        "org.freedesktop.NetworkManager.enable-disable-wifi"
#define NM_AUTH_ENABLE_WWAN        "org.freedesktop.NetworkManager.enable-disable-wwan"
#define NM_AUTH_MODIFY_SYSTEM      "org.freedesktop.NetworkManager.settings.modify.system"
#define NM_AUTH_MODIFY_OWN         "org.freedesktop.NetworkManager.settings.modify.own"
#define NM_AUTH_MODIFY_HOSTNAME    "org.freedesktop.NetworkManager.settings.modify.hostname"
#define NM_AUTH_WIFI_SHARE_OPEN    "org.freedesktop.NetworkManager.wifi.share.open"
#define NM_AUTH_WIFI_SHARE_PROTECTED "org.freedesktop.NetworkManager.wifi.share.protected"
#define NM_AUTH_RELOAD             "org.freedesktop.NetworkManager.reload"
#define NM_AUTH_CONNECTIVITY_CHECK "org.freedesktop.NetworkManager.enable-disable-connectivity-check"

/**
 * Asks polkit (through polkit-tqt) whether this process may perform one of
 * NetworkManager's actions, without asking the user for a password.
 *
 * NetworkManager does the real check itself and asks the user's polkit
 * agent (polkit-agent-tde) for the password; this class only lets the UI
 * show a lock or explain why something is not possible.
 */
class NMAuthorization
{
public:
	enum Result {
		Unknown,     /* polkit is not available */
		Yes,         /* allowed without a password */
		Challenge,   /* allowed after authentication */
		No           /* not allowed */
	};

	static Result check(const char *action);
	/** a sentence for the UI, or an empty string for Yes and Unknown */
	static TQString explain(Result result);
	static bool available();
};

#endif
