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

#include "pages.h"
#include "authorization.h"
#include "nmclient.h"

#include <tqcheckbox.h>
#include <tqcombobox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqspinbox.h>

#include <tqdbusconnection.h>
#include <tqdbusmessage.h>

#include <klineedit.h>
#include <tdelocale.h>
#include <kurlrequester.h>
#include <kuser.h>

static TQStringList firewallZones()
{
	TQStringList zones;
	TQT_DBusConnection bus = TQT_DBusConnection::systemBus();
	if (bus.isConnected()) {
		TQT_DBusMessage msg = TQT_DBusMessage::methodCall("org.fedoraproject.FirewallD1",
		                                                  "/org/fedoraproject/FirewallD1",
		                                                  "org.fedoraproject.FirewallD1.zone", "getZones");
		msg.setTimeout(2000);
		TQT_DBusMessage reply = bus.sendWithReply(msg);
		if (reply.type() == TQT_DBusMessage::ReplyMessage && reply.count() > 0)
			zones = NMDBus::toStrList(reply[0]);
	}
	if (zones.isEmpty())
		zones << "block" << "dmz" << "drop" << "external" << "home" << "internal" << "public"
		      << "trusted" << "work";
	zones.sort();
	return zones;
}

GeneralPage::GeneralPage(TQWidget *parent)
	: SettingPage(parent, "GeneralPage")
{
	TQGridLayout *g = new TQGridLayout(this, 11, 2, 8, 6);
	int row = 0;

	m_name = new KLineEdit(this);
	addRow(g, row++, i18n("Connection &name:"), m_name);

	m_autoconnect = new TQCheckBox(i18n("Connect &automatically with priority:"), this);
	m_priority = new TQSpinBox(-999, 999, 1, this);
	g->addWidget(m_autoconnect, row, 0);
	g->addWidget(m_priority, row++, 1, TQt::AlignLeft);
	connect(m_autoconnect, TQ_SIGNAL(toggled(bool)), m_priority, TQ_SLOT(setEnabled(bool)));

	m_autoconnectRetries = new TQComboBox(true, this);
	m_autoconnectRetries->insertItem(i18n("Default"));
	m_autoconnectRetries->insertItem(i18n("Forever"));
	for (int i = 1; i <= 10; ++i)
		m_autoconnectRetries->insertItem(TQString::number(i));
	addRow(g, row++, i18n("Connection &attempts:"), m_autoconnectRetries);

	m_allUsers = new TQCheckBox(i18n("All &users may connect to this network"), this);
	g->addMultiCellWidget(m_allUsers, row, row, 0, 1);
	row++;
	connect(m_allUsers, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotAllUsers(bool)));

	m_authNote = new TQLabel(this);
	m_authNote->setAlignment(TQt::WordBreak | TQt::AlignTop);
	m_authNote->hide();
	g->addMultiCellWidget(m_authNote, row, row, 0, 1);
	row++;

	m_vpnAuto = new TQCheckBox(i18n("Automatically connect to &VPN:"), this);
	m_vpn = new TQComboBox(false, this);
	g->addWidget(m_vpnAuto, row, 0);
	g->addWidget(m_vpn, row++, 1);
	connect(m_vpnAuto, TQ_SIGNAL(toggled(bool)), m_vpn, TQ_SLOT(setEnabled(bool)));

	m_zone = new TQComboBox(true, this);
	m_zone->insertItem(i18n("Default"));
	m_zone->insertStringList(firewallZones());
	addRow(g, row++, i18n("&Firewall zone:"), m_zone);

	m_metered = new TQComboBox(false, this);
	m_metered->insertItem(i18n("Automatic"));
	m_metered->insertItem(i18n("Yes"));
	m_metered->insertItem(i18n("No"));
	addRow(g, row++, i18n("&Metered connection:"), m_metered);

	m_uuid = new KLineEdit(this);
	m_uuid->setReadOnly(true);
	addRow(g, row++, i18n("UUID:"), m_uuid);

	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);

	connect(m_name, TQ_SIGNAL(textChanged(const TQString&)), TQ_SIGNAL(changed()));
}

void GeneralPage::slotAllUsers(bool on)
{
	if (!on) {
		m_authNote->hide();
		return;
	}
	TQString text = NMAuthorization::explain(NMAuthorization::check(NM_AUTH_MODIFY_SYSTEM));
	m_authNote->setText(text);
	m_authNote->setShown(!text.isEmpty());
}

TQString GeneralPage::connectionName() const
{
	return m_name->text().stripWhiteSpace();
}

void GeneralPage::load(const ConnectionSettings &conn)
{
	m_name->setText(conn.id());
	m_autoconnect->setChecked(conn.boolean("connection", "autoconnect", true));
	m_priority->setValue(conn.integer("connection", "autoconnect-priority"));
	m_priority->setEnabled(m_autoconnect->isChecked());
	int retries = conn.integer("connection", "autoconnect-retries", -1);
	if (retries < 0)
		m_autoconnectRetries->setCurrentItem(0);
	else if (retries == 0)
		m_autoconnectRetries->setCurrentItem(1);
	else
		m_autoconnectRetries->setEditText(TQString::number(retries));
	m_allUsers->setChecked(conn.strList("connection", "permissions").isEmpty());
	slotAllUsers(m_allUsers->isChecked());

	/* secondaries: the VPN started with this connection */
	m_vpn->clear();
	m_vpnUuids.clear();
	const NMConnectionList &all = NMClient::self()->connections();
	for (NMConnectionList::const_iterator it = all.begin(); it != all.end(); ++it) {
		if ((*it).type() == "vpn" || (*it).type() == "wireguard") {
			m_vpn->insertItem((*it).id());
			m_vpnUuids << (*it).uuid();
		}
	}
	TQStringList sec = conn.strList("connection", "secondaries");
	bool isVpn = conn.type() == "vpn" || conn.type() == "wireguard";
	m_vpnAuto->setEnabled(!m_vpnUuids.isEmpty() && !isVpn);
	m_vpnAuto->setChecked(false);
	for (uint i = 0; i < m_vpnUuids.count(); ++i) {
		if (sec.contains(m_vpnUuids[i])) {
			m_vpn->setCurrentItem(i);
			m_vpnAuto->setChecked(true);
		}
	}
	m_vpn->setEnabled(m_vpnAuto->isChecked());

	TQString zone = conn.str("connection", "zone");
	if (zone.isEmpty())
		m_zone->setCurrentItem(0);
	else
		m_zone->setEditText(zone);
	m_metered->setCurrentItem(TQMIN(2, TQMAX(0, conn.integer("connection", "metered"))));
	m_uuid->setText(conn.uuid());
}

void GeneralPage::save(ConnectionSettings &conn)
{
	conn.setStr("connection", "id", connectionName());
	conn.setBool("connection", "autoconnect", m_autoconnect->isChecked());
	conn.setInt("connection", "autoconnect-priority", m_priority->value());
	if (m_autoconnectRetries->currentItem() == 0 &&
	    m_autoconnectRetries->currentText() == m_autoconnectRetries->text(0))
		conn.setInt("connection", "autoconnect-retries", -1);
	else if (m_autoconnectRetries->currentText() == m_autoconnectRetries->text(1))
		conn.setInt("connection", "autoconnect-retries", 0);
	else
		conn.setInt("connection", "autoconnect-retries", m_autoconnectRetries->currentText().toInt());

	if (m_allUsers->isChecked())
		conn.setStrList("connection", "permissions", TQStringList());
	else
		conn.setStrList("connection", "permissions", TQStringList("user:" + KUser().loginName()));

	TQStringList sec = conn.strList("connection", "secondaries");
	for (TQStringList::const_iterator it = m_vpnUuids.begin(); it != m_vpnUuids.end(); ++it)
		sec.remove(*it);
	if (m_vpnAuto->isChecked() && m_vpn->currentItem() >= 0)
		sec << m_vpnUuids[m_vpn->currentItem()];
	if (sec.isEmpty())
		conn.remove("connection", "secondaries");
	else
		conn.setStrList("connection", "secondaries", sec);

	TQString zone = m_zone->currentText().stripWhiteSpace();
	conn.setStr("connection", "zone", zone == m_zone->text(0) ? TQString::null : zone);
	conn.setInt("connection", "metered", m_metered->currentItem());
}

TQString GeneralPage::validate() const
{
	if (connectionName().isEmpty())
		return i18n("The connection needs a name.");
	return TQString::null;
}

/* ---------------------------------------------------------------------- */

ProxyPage::ProxyPage(TQWidget *parent)
	: SettingPage(parent, "ProxyPage")
{
	TQGridLayout *g = new TQGridLayout(this, 5, 2, 8, 6);
	m_method = new TQComboBox(false, this);
	m_method->insertItem(i18n("None"));
	m_method->insertItem(i18n("Automatic (proxy auto-configuration)"));
	addRow(g, 0, i18n("&Method:"), m_method);
	m_pacUrl = new KLineEdit(this);
	addRow(g, 1, i18n("PAC &URL:"), m_pacUrl);
	m_browserOnly = new TQCheckBox(i18n("Use the proxy for web &browsers only"), this);
	g->addWidget(m_browserOnly, 3, 1);
	TQLabel *note = new TQLabel(i18n("<qt>NetworkManager hands the proxy configuration to "
	                                 "applications that ask for it (for example through "
	                                 "PacRunner). TDE's own proxy settings are in the Control "
	                                 "Center under Internet &amp; Network &gt; Proxy.</qt>"), this);
	note->setAlignment(TQt::WordBreak | TQt::AlignTop);
	g->addMultiCellWidget(note, 4, 4, 0, 1);
	g->setRowStretch(5, 1);
	g->setColStretch(1, 1);
	connect(m_method, TQ_SIGNAL(activated(int)), TQ_SLOT(slotMethod(int)));
}

void ProxyPage::slotMethod(int m)
{
	m_pacUrl->setEnabled(m == 1);
	m_browserOnly->setEnabled(m == 1);
}

void ProxyPage::load(const ConnectionSettings &conn)
{
	m_method->setCurrentItem(conn.integer("proxy", "method") == 1 ? 1 : 0);
	m_pacUrl->setText(conn.str("proxy", "pac-url"));
	m_browserOnly->setChecked(conn.boolean("proxy", "browser-only"));
	slotMethod(m_method->currentItem());
}

void ProxyPage::save(ConnectionSettings &conn)
{
	if (m_method->currentItem() == 0 && !conn.has("proxy"))
		return;
	conn.setInt("proxy", "method", m_method->currentItem());
	conn.setStr("proxy", "pac-url", m_method->currentItem() == 1 ? m_pacUrl->text().stripWhiteSpace() : TQString::null);
	conn.setBool("proxy", "browser-only", m_browserOnly->isChecked());
}

#include "pages.moc"
