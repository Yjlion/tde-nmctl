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

#include "secretagent.h"
#include "nmclient.h"
#include "passworddialog.h"
#include "secretstore.h"

#include <tqdbuserror.h>
#include <tqdbusobjectpath.h>

#include <kdebug.h>
#include <twin.h>

/* NMSecretAgentGetSecretsFlags */
enum {
	AGENT_ALLOW_INTERACTION = 0x1,
	AGENT_REQUEST_NEW = 0x2,
	AGENT_USER_REQUESTED = 0x4
};

/* NMSecretAgentCapabilities: VPN hints */
static const TQ_UINT32 AGENT_CAPABILITIES = 0x1;

#define AGENT_ERROR_USER_CANCELED "org.freedesktop.NetworkManager.SecretAgent.UserCanceled"
#define AGENT_ERROR_NO_SECRETS    "org.freedesktop.NetworkManager.SecretAgent.NoSecrets"
#define AGENT_ERROR_FAILED        "org.freedesktop.NetworkManager.SecretAgent.Failed"
#define AGENT_IDENTIFIER          "org.trinitydesktop.tde-nmctl"

SecretAgent::SecretAgent(TQObject *parent, const char *name)
	: TQObject(parent, name), m_registered(false)
{
	m_bus = TQT_DBusConnection::systemBus();
	if (!m_bus.isConnected())
		return;
	m_bus.registerObject(NM_DBUS_PATH_SECRET_AGENT, this);
	connect(NMClient::self(), TQ_SIGNAL(availabilityChanged(bool)),
	        TQ_SLOT(slotAvailabilityChanged(bool)));
	registerAgent();
}

SecretAgent::~SecretAgent()
{
	if (m_bus.isConnected()) {
		if (m_registered) {
			TQT_DBusMessage msg = TQT_DBusMessage::methodCall(NM_DBUS_SERVICE, NM_DBUS_PATH_AGENT_MANAGER,
			                                                  NM_DBUS_INTERFACE_AGENT_MANAGER, "Unregister");
			m_bus.send(msg);
		}
		m_bus.unregisterObject(NM_DBUS_PATH_SECRET_AGENT);
	}
}

void SecretAgent::registerAgent()
{
	if (!m_bus.isConnected() || !NMClient::self()->isRunning())
		return;
	TQT_DBusMessage msg = TQT_DBusMessage::methodCall(NM_DBUS_SERVICE, NM_DBUS_PATH_AGENT_MANAGER,
	                                                  NM_DBUS_INTERFACE_AGENT_MANAGER,
	                                                  "RegisterWithCapabilities");
	msg << TQT_DBusData::fromString(AGENT_IDENTIFIER) << TQT_DBusData::fromUInt32(AGENT_CAPABILITIES);
	m_bus.sendWithAsyncReply(msg, this, TQ_SLOT(slotRegisterReply(const TQT_DBusMessage&)));
}

void SecretAgent::slotRegisterReply(const TQT_DBusMessage &reply)
{
	m_registered = reply.type() == TQT_DBusMessage::ReplyMessage;
	if (!m_registered)
		kdWarning() << "tde-nmctl: registering the secret agent failed: "
		            << reply.error().name() << ": " << reply.error().message() << endl;
}

void SecretAgent::slotAvailabilityChanged(bool running)
{
	m_registered = false;
	if (running)
		registerAgent();
}

bool SecretAgent::handleMethodCall(const TQT_DBusMessage &message)
{
	if (message.interface() != NM_DBUS_INTERFACE_SECRET_AGENT)
		return false;
	/* only NetworkManager may ask for secrets */
	TQT_DBusMessage q = TQT_DBusMessage::methodCall("org.freedesktop.DBus", "/org/freedesktop/DBus",
	                                                "org.freedesktop.DBus", "GetNameOwner");
	q << TQT_DBusData::fromString(NM_DBUS_SERVICE);
	TQT_DBusMessage owner = m_bus.sendWithReply(q);
	if (owner.type() != TQT_DBusMessage::ReplyMessage || owner.count() < 1 ||
	    owner[0].toString() != message.sender()) {
		replyError(message, AGENT_ERROR_FAILED, "Request not sent by NetworkManager");
		return true;
	}

	TQString m = message.member();
	if (m == "GetSecrets")
		getSecrets(message);
	else if (m == "CancelGetSecrets")
		cancelGetSecrets(message);
	else if (m == "SaveSecrets")
		saveSecrets(message);
	else if (m == "DeleteSecrets")
		deleteSecrets(message);
	else
		return false;
	return true;
}

void SecretAgent::getSecrets(const TQT_DBusMessage &message)
{
	if (message.count() < 5) {
		replyError(message, AGENT_ERROR_FAILED, "Invalid arguments");
		return;
	}
	ConnectionSettings conn(NMDBus::toSettings(message[0]));
	TQString path = message[1].toObjectPath();
	TQString setting = message[2].toString();
	TQStringList hints = NMDBus::toStrList(message[3]);
	TQ_UINT32 flags = message[4].toUInt32();

	TQStringList keys = SecretStore::requiredSecrets(conn, setting, hints);
	SecretMap stored;
	SecretStore::load(conn.uuid(), setting, &stored);

	if (!(flags & AGENT_REQUEST_NEW)) {
		SecretMap found;
		bool complete = !keys.isEmpty();
		for (TQStringList::const_iterator it = keys.begin(); it != keys.end(); ++it) {
			if (stored.contains(*it) && !stored[*it].isEmpty())
				found[*it] = stored[*it];
			else
				complete = false;
		}
		/* also hand out stored secrets that are not strictly required */
		for (SecretMap::const_iterator it = stored.begin(); it != stored.end(); ++it)
			if (!found.contains(it.key()) && !it.data().isEmpty())
				found[it.key()] = it.data();
		if (complete || (keys.isEmpty() && !found.isEmpty())) {
			replySecrets(message, SecretStore::toSettings(setting, found));
			return;
		}
	}

	if (!(flags & AGENT_ALLOW_INTERACTION) || keys.isEmpty()) {
		replyError(message, AGENT_ERROR_NO_SECRETS, "No secrets stored for this connection");
		return;
	}

	Request r;
	r.call = message;
	r.connectionPath = path;
	r.setting = setting;
	r.conn = conn;
	r.dialog = new PasswordDialog(conn, setting, keys, stored, SecretStore::walletEnabled(), 0,
	                              "PasswordDialog");
	connect(r.dialog, TQ_SIGNAL(finished()), TQ_SLOT(slotDialogFinished()));
	m_requests << r;
	emit secretsRequested(conn.id());
	r.dialog->show();
	KWin::forceActiveWindow(r.dialog->winId());
}

void SecretAgent::slotDialogFinished()
{
	const PasswordDialog *dlg = dynamic_cast<const PasswordDialog *>(sender());
	for (TQValueList<Request>::iterator it = m_requests.begin(); it != m_requests.end(); ++it) {
		if ((PasswordDialog *)(*it).dialog != dlg)
			continue;
		Request r = *it;
		m_requests.remove(it);
		if (dlg->result() == TQDialog::Accepted) {
			SecretMap secrets = dlg->secrets();
			if (dlg->remember()) {
				/* keep the user's secrets in the wallet; secrets owned by
				 * the system are saved by NetworkManager itself */
				SecretMap toStore;
				SecretStore::load(r.conn.uuid(), r.setting, &toStore);
				for (SecretMap::const_iterator s = secrets.begin(); s != secrets.end(); ++s) {
					uint f = SecretStore::secretFlags(r.conn, r.setting, s.key());
					if ((f & NM_SECRET_FLAG_AGENT_OWNED) && !(f & NM_SECRET_FLAG_NOT_SAVED))
						toStore[s.key()] = s.data();
				}
				SecretStore::save(r.conn.uuid(), r.setting, toStore);
			}
			replySecrets(r.call, SecretStore::toSettings(r.setting, secrets));
		} else {
			replyError(r.call, AGENT_ERROR_USER_CANCELED, "The user cancelled the request");
		}
		r.dialog->deleteLater();
		return;
	}
}

void SecretAgent::cancelGetSecrets(const TQT_DBusMessage &message)
{
	TQString path = message.count() > 0 ? TQString(message[0].toObjectPath()) : TQString::null;
	TQString setting = message.count() > 1 ? message[1].toString() : TQString::null;
	for (TQValueList<Request>::iterator it = m_requests.begin(); it != m_requests.end(); ) {
		if ((*it).connectionPath == path && (*it).setting == setting) {
			replyError((*it).call, AGENT_ERROR_USER_CANCELED, "Cancelled by NetworkManager");
			if ((*it).dialog)
				(*it).dialog->deleteLater();
			it = m_requests.remove(it);
		} else {
			++it;
		}
	}
	m_bus.send(TQT_DBusMessage::methodReply(message));
}

void SecretAgent::saveSecrets(const TQT_DBusMessage &message)
{
	if (message.count() >= 1) {
		ConnectionSettings conn(NMDBus::toSettings(message[0]));
		SecretStore::saveAgentOwned(conn);
	}
	m_bus.send(TQT_DBusMessage::methodReply(message));
}

void SecretAgent::deleteSecrets(const TQT_DBusMessage &message)
{
	if (message.count() >= 1) {
		ConnectionSettings conn(NMDBus::toSettings(message[0]));
		if (!conn.uuid().isEmpty())
			SecretStore::removeConnection(conn.uuid());
	}
	m_bus.send(TQT_DBusMessage::methodReply(message));
}

void SecretAgent::replySecrets(const TQT_DBusMessage &call, const NMSettingsMap &secrets)
{
	TQT_DBusMessage reply = TQT_DBusMessage::methodReply(call);
	reply << NMDBus::fromSettings(secrets);
	m_bus.send(reply);
}

void SecretAgent::replyError(const TQT_DBusMessage &call, const TQString &name, const TQString &text)
{
	m_bus.send(TQT_DBusMessage::methodError(call, TQT_DBusError(name, text)));
}

#include "secretagent.moc"
