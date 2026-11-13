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

#ifndef SECRETAGENT_H
#define SECRETAGENT_H

#include <tqobject.h>
#include <tqguardedptr.h>
#include <tqvaluelist.h>

#include <tqdbusconnection.h>
#include <tqdbusmessage.h>
#include <tqdbusobject.h>

#include "nmdbus.h"

class PasswordDialog;

/**
 * NetworkManager secret agent (org.freedesktop.NetworkManager.SecretAgent).
 *
 * NetworkManager asks the agent for the passwords of connections whose
 * secrets belong to the user. The agent answers from TDE Wallet (see
 * SecretStore) or asks the user with a PasswordDialog. It runs in the
 * tray applet, which is started at login.
 */
class SecretAgent : public TQObject, public TQT_DBusObjectBase
{
	TQ_OBJECT
public:
	SecretAgent(TQObject *parent = 0, const char *name = 0);
	virtual ~SecretAgent();

	bool isRegistered() const { return m_registered; }

public slots:
	void registerAgent();

signals:
	/** a connection needs secrets and the user is being asked */
	void secretsRequested(const TQString &connectionId);

protected:
	virtual bool handleMethodCall(const TQT_DBusMessage &message);

private slots:
	void slotAvailabilityChanged(bool running);
	void slotDialogFinished();
	void slotRegisterReply(const TQT_DBusMessage &reply);

private:
	struct Request {
		TQT_DBusMessage call;
		TQString connectionPath;
		TQString setting;
		ConnectionSettings conn;
		TQGuardedPtr<PasswordDialog> dialog;
	};

	void getSecrets(const TQT_DBusMessage &message);
	void cancelGetSecrets(const TQT_DBusMessage &message);
	void saveSecrets(const TQT_DBusMessage &message);
	void deleteSecrets(const TQT_DBusMessage &message);
	void replySecrets(const TQT_DBusMessage &call, const NMSettingsMap &secrets);
	void replyError(const TQT_DBusMessage &call, const TQString &name, const TQString &text);

	TQT_DBusConnection m_bus;
	bool m_registered;
	TQValueList<Request> m_requests;
};

#endif
