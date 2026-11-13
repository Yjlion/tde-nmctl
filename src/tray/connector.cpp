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

#include "connector.h"
#include "authorization.h"
#include "connectiondialog.h"
#include "nmclient.h"
#include "nmctlsettings.h"
#include "passworddialog.h"
#include "secretstore.h"

#include <tqcheckbox.h>
#include <tqcombobox.h>
#include <tqlabel.h>
#include <tqlayout.h>

#include <tdeapplication.h>
#include <kiconloader.h>
#include <klineedit.h>
#include <tdelocale.h>
#include <kuser.h>
#include <twin.h>

ConnectionSettings Connector::wifiSettings(const TQByteArray &ssid, const TQString &keyMgmt, bool hidden)
{
	ConnectionSettings c;
	TQString name = NMDBus::ssidToString(ssid);
	c.setStr("connection", "id", name);
	c.setStr("connection", "uuid", ConnectionSettings::newUuid());
	c.setStr("connection", "type", "802-11-wireless");
	c.setBool("connection", "autoconnect", true);
	/* a connection only for this user needs no administrator password */
	if (NMAuthorization::check(NM_AUTH_MODIFY_SYSTEM) != NMAuthorization::Yes)
		c.setStrList("connection", "permissions", TQStringList("user:" + KUser().loginName()));
	c.setBytes("802-11-wireless", "ssid", ssid);
	c.setStr("802-11-wireless", "mode", "infrastructure");
	if (hidden)
		c.setBool("802-11-wireless", "hidden", true);
	if (!keyMgmt.isEmpty())
		c.setStr("802-11-wireless-security", "key-mgmt", keyMgmt);
	c.setStr("ipv4", "method", "auto");
	c.setStr("ipv6", "method", "auto");
	return c;
}

/* adds the password to a new connection and connects */
static void addAndConnect(ConnectionSettings conn, const TQString &devicePath, const TQString &apPath,
                          const TQString &password)
{
	const TQString s = "802-11-wireless-security";
	TQString km = conn.str(s, "key-mgmt");
	uint flags = NmctlSettings::storeInWallet() ? NM_SECRET_FLAG_AGENT_OWNED : NM_SECRET_FLAG_NONE;
	if (km == "wpa-psk" || km == "sae") {
		conn.setStr(s, "psk", password);
		conn.setUInt(s, "psk-flags", flags);
	} else if (km == "none") {
		conn.setStr(s, "wep-key0", password);
		conn.setUInt(s, "wep-key-flags", flags);
		/* 5/13 characters or 10/26 hex digits are keys, anything else a passphrase */
		uint len = password.length();
		bool key = len == 5 || len == 13 || len == 10 || len == 26;
		conn.setUInt(s, "wep-key-type", key ? 1 : 2);
		conn.setUInt(s, "wep-tx-keyidx", 0);
	}
	SecretStore::saveAgentOwned(conn);
	NMClient::self()->addAndActivate(conn, devicePath, apPath);
}

void Connector::connectAccessPoint(const TQString &devicePath, const TQString &apPath)
{
	NMClient *nm = NMClient::self();
	const NMDeviceInfo *dev = nm->device(devicePath);
	if (!dev)
		return;
	NMAccessPoint ap;
	bool found = false;
	for (NMAccessPointList::const_iterator it = dev->accessPoints.begin(); it != dev->accessPoints.end(); ++it) {
		if ((*it).path == apPath) {
			ap = *it;
			found = true;
		}
	}
	if (!found)
		return;

	/* a saved connection for this network: use the most recent one */
	TQValueList<const NMConnection *> saved = nm->connectionsForSsid(ap.ssid);
	const NMConnection *best = 0;
	for (TQValueList<const NMConnection *>::const_iterator it = saved.begin(); it != saved.end(); ++it) {
		if (!dev->availableConnections.isEmpty() && !dev->availableConnections.contains((*it)->path))
			continue;
		if (!best || (*it)->timestamp() > best->timestamp())
			best = *it;
	}
	if (best) {
		nm->activateConnection(best->path, devicePath, apPath);
		return;
	}

	TQString km = ap.keyMgmt();
	ConnectionSettings conn = wifiSettings(ap.ssid, km, false);
	if (km.isEmpty() || km == "owe") {
		nm->addAndActivate(conn, devicePath, apPath);
		return;
	}
	if (km == "wpa-eap") {
		/* enterprise networks need more than a password */
		ConnectionDialog dlg(conn, TQString::null, 0, "ConnectionDialog");
		dlg.setActivateOnSave(devicePath, apPath);
		dlg.showPage(2);
		dlg.exec();
		return;
	}

	TQStringList keys;
	keys << (km == "none" ? "wep-key0" : "psk");
	PasswordDialog dlg(conn, "802-11-wireless-security", keys, SecretMap(),
	                   SecretStore::walletEnabled(), 0, "WifiPasswordDialog");
	KWin::forceActiveWindow(dlg.winId());
	if (dlg.exec() != TQDialog::Accepted)
		return;
	if (!dlg.remember())
		conn.setUInt("802-11-wireless-security", km == "none" ? "wep-key-flags" : "psk-flags",
		             NM_SECRET_FLAG_NOT_SAVED);
	addAndConnect(conn, devicePath, apPath, dlg.secrets()[keys.first()]);
}

void Connector::connectHidden(TQWidget *parent)
{
	HiddenNetworkDialog dlg(parent);
	if (dlg.exec() != TQDialog::Accepted)
		return;
	ConnectionSettings conn = wifiSettings(NMDBus::bytes(dlg.ssid()), dlg.keyMgmt(), true);
	if (dlg.keyMgmt() == "wpa-eap") {
		ConnectionDialog edit(conn, TQString::null, parent, "ConnectionDialog");
		edit.setActivateOnSave(dlg.device());
		edit.showPage(2);
		edit.exec();
		return;
	}
	addAndConnect(conn, dlg.device(), "/", dlg.password());
}

/* ---------------------------------------------------------------------- */

static const char *s_keyMgmt[] = { "", "wpa-psk", "sae", "none", "wpa-eap", "owe" };

HiddenNetworkDialog::HiddenNetworkDialog(TQWidget *parent)
	: KDialogBase(parent, "HiddenNetworkDialog", true, i18n("Connect to Hidden Wi-Fi Network"),
	              Ok | Cancel, Ok, true)
{
	TQWidget *page = new TQWidget(this);
	setMainWidget(page);
	TQGridLayout *g = new TQGridLayout(page, 6, 3, 0, spacingHint());
	TQLabel *icon = new TQLabel(page);
	icon->setPixmap(DesktopIcon("tde-nmctl-signal-75", 48));
	icon->setAlignment(TQt::AlignTop);
	g->addMultiCellWidget(icon, 0, 4, 0, 0);
	TQLabel *intro = new TQLabel(i18n("Enter the name and security of the hidden Wi-Fi network."), page);
	g->addMultiCellWidget(intro, 0, 0, 1, 2);

	m_ssid = new KLineEdit(page);
	TQLabel *l = new TQLabel(i18n("Network &name (SSID):"), page);
	l->setBuddy(m_ssid);
	g->addWidget(l, 1, 1);
	g->addWidget(m_ssid, 1, 2);

	m_security = new TQComboBox(false, page);
	m_security->insertItem(i18n("None"));
	m_security->insertItem(i18n("WPA & WPA2 Personal"));
	m_security->insertItem(i18n("WPA3 Personal"));
	m_security->insertItem(i18n("WEP"));
	m_security->insertItem(i18n("WPA & WPA2 Enterprise"));
	m_security->insertItem(i18n("Enhanced Open (OWE)"));
	m_security->setCurrentItem(1);
	l = new TQLabel(i18n("&Security:"), page);
	l->setBuddy(m_security);
	g->addWidget(l, 2, 1);
	g->addWidget(m_security, 2, 2);

	m_password = new KLineEdit(page);
	m_password->setEchoMode(TQLineEdit::Password);
	l = new TQLabel(i18n("&Password:"), page);
	l->setBuddy(m_password);
	g->addWidget(l, 3, 1);
	g->addWidget(m_password, 3, 2);

	m_device = new TQComboBox(false, page);
	const NMDeviceList &devs = NMClient::self()->devices();
	for (NMDeviceList::const_iterator it = devs.begin(); it != devs.end(); ++it) {
		if ((*it).isWireless()) {
			m_device->insertItem((*it).iface);
			m_devices << (*it).path;
		}
	}
	l = new TQLabel(i18n("&Device:"), page);
	l->setBuddy(m_device);
	g->addWidget(l, 4, 1);
	g->addWidget(m_device, 4, 2);
	g->setColStretch(2, 1);

	connect(m_ssid, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotChanged()));
	connect(m_password, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotChanged()));
	connect(m_security, TQ_SIGNAL(activated(int)), TQ_SLOT(slotChanged()));
	m_ssid->setFocus();
	slotChanged();
}

void HiddenNetworkDialog::slotChanged()
{
	TQString km = keyMgmt();
	bool needsPassword = km == "wpa-psk" || km == "sae" || km == "none";
	m_password->setEnabled(needsPassword);
	bool ok = !m_ssid->text().isEmpty() && m_ssid->text().utf8().length() <= 32;
	if (km == "wpa-psk")
		ok = ok && m_password->text().length() >= 8;
	else if (needsPassword)
		ok = ok && !m_password->text().isEmpty();
	enableButtonOK(ok && !m_devices.isEmpty());
}

TQString HiddenNetworkDialog::ssid() const
{
	return m_ssid->text();
}

TQString HiddenNetworkDialog::keyMgmt() const
{
	return s_keyMgmt[m_security->currentItem()];
}

TQString HiddenNetworkDialog::password() const
{
	return m_password->text();
}

TQString HiddenNetworkDialog::device() const
{
	int i = m_device->currentItem();
	return (i >= 0 && i < (int)m_devices.count()) ? m_devices[i] : TQString("/");
}

#include "connector.moc"
