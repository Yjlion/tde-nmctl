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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "authorization.h"
#include "nmclient.h"

#include <tdelocale.h>
#include <kdebug.h>

#ifdef HAVE_POLKIT_TQT
#include <polkit-tqt-authority.h>
#include <polkit-tqt-subject.h>
#endif

#include <unistd.h>

bool NMAuthorization::available()
{
#ifdef HAVE_POLKIT_TQT
	PolkitTQt::Authority *authority = PolkitTQt::Authority::instance();
	return authority && !authority->hasError();
#else
	return false;
#endif
}

NMAuthorization::Result NMAuthorization::check(const char *action)
{
#ifdef HAVE_POLKIT_TQT
	PolkitTQt::Authority *authority = PolkitTQt::Authority::instance();
	if (!authority)
		return Unknown;
	authority->clearError();
	PolkitTQt::UnixProcessSubject subject((TQ_LONG)getpid());
	PolkitTQt::Authority::Result r =
		authority->checkAuthorizationSync(action, subject, PolkitTQt::Authority::None);
	if (authority->hasError()) {
		kdDebug() << "tde-nmctl: polkit check of " << action << " failed: "
		          << authority->errorDetails() << endl;
		authority->clearError();
		return Unknown;
	}
	switch (r) {
	case PolkitTQt::Authority::Yes:       return Yes;
	case PolkitTQt::Authority::Challenge: return Challenge;
	case PolkitTQt::Authority::No:        return No;
	default:                              return Unknown;
	}
#else
	Q_UNUSED(action);
	return Unknown;
#endif
}

TQString NMAuthorization::explain(Result result)
{
	switch (result) {
	case Challenge:
		if (!NMClient::polkitAgentRunning())
			return i18n("Changing system connections needs administrator authentication, "
			            "but no polkit authentication agent is running. "
			            "Start polkit-agent-tde to be asked for the password.");
		return i18n("Changing connections for all users needs administrator authentication; "
		            "you will be asked for the password.");
	case No:
		return i18n("You are not allowed to change connections for all users. "
		            "You can still create connections that only you can use.");
	default:
		return TQString::null;
	}
}
