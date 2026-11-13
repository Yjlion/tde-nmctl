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
#include "eapwidget.h"
#include "nmclient.h"

#include <tqcheckbox.h>
#include <tqcombobox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqspinbox.h>

#include <klineedit.h>
#include <tdelocale.h>

/* ---------------------------------------------------------------------- */
/* Ethernet                                                               */
/* ---------------------------------------------------------------------- */

EthernetPage::EthernetPage(TQWidget *parent)
	: SettingPage(parent, "EthernetPage")
{
	TQGridLayout *g = new TQGridLayout(this, 9, 2, 8, 6);
	int row = 0;

	m_iface = new TQComboBox(true, this);
	fillInterfaces(m_iface, NM_DEVICE_TYPE_ETHERNET);
	addRow(g, row++, i18n("&Device:"), m_iface);

	m_mac = new KLineEdit(this);
	addRow(g, row++, i18n("Restrict to &MAC address:"), m_mac);

	m_cloned = new TQComboBox(true, this);
	fillMacChoices(m_cloned);
	addRow(g, row++, i18n("&Cloned MAC address:"), m_cloned);

	m_mtu = new TQSpinBox(0, 65535, 1, this);
	m_mtu->setSpecialValueText(i18n("Automatic"));
	m_mtu->setSuffix(i18n(" bytes"));
	addRow(g, row++, i18n("MT&U:"), m_mtu);

	m_negotiation = new TQComboBox(false, this);
	m_negotiation->insertItem(i18n("Ignore (keep the device's setting)"));
	m_negotiation->insertItem(i18n("Automatic"));
	m_negotiation->insertItem(i18n("Manual"));
	addRow(g, row++, i18n("&Link negotiation:"), m_negotiation);
	connect(m_negotiation, TQ_SIGNAL(activated(int)), TQ_SLOT(slotNegotiation(int)));

	m_speed = new TQComboBox(false, this);
	m_speed->insertItem(i18n("10 Mbit/s"));
	m_speed->insertItem(i18n("100 Mbit/s"));
	m_speed->insertItem(i18n("1 Gbit/s"));
	m_speed->insertItem(i18n("2.5 Gbit/s"));
	m_speed->insertItem(i18n("5 Gbit/s"));
	m_speed->insertItem(i18n("10 Gbit/s"));
	addRow(g, row++, i18n("&Speed:"), m_speed);

	m_duplex = new TQComboBox(false, this);
	m_duplex->insertItem(i18n("Full"));
	m_duplex->insertItem(i18n("Half"));
	addRow(g, row++, i18n("Du&plex:"), m_duplex);

	m_wol = new TQComboBox(false, this);
	m_wol->insertItem(i18n("Default"));
	m_wol->insertItem(i18n("Ignore"));
	m_wol->insertItem(i18n("Disabled"));
	m_wol->insertItem(i18n("Magic packet"));
	m_wol->insertItem(i18n("Any activity (PHY)"));
	addRow(g, row++, i18n("&Wake on LAN:"), m_wol);

	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
}

static const uint s_speeds[] = { 10, 100, 1000, 2500, 5000, 10000 };

/* NMSettingWiredWakeOnLan */
static const uint s_wol[] = { 0x1 /* default */, 0x8000 /* ignore */, 0 /* none */, 0x40 /* magic */, 0x2 /* phy */ };

void EthernetPage::slotNegotiation(int n)
{
	m_speed->setEnabled(n == 2);
	m_duplex->setEnabled(n == 2);
}

void EthernetPage::load(const ConnectionSettings &conn)
{
	TQString iface = conn.interfaceName();
	if (iface.isEmpty())
		m_iface->setCurrentItem(0);
	else
		m_iface->setEditText(iface);
	m_mac->setText(conn.str("802-3-ethernet", "mac-address"));
	setMacChoice(m_cloned, conn.str("802-3-ethernet", "assigned-mac-address",
	                                conn.str("802-3-ethernet", "cloned-mac-address")));
	m_mtu->setValue(conn.uint("802-3-ethernet", "mtu"));

	bool autoNeg = conn.boolean("802-3-ethernet", "auto-negotiate");
	uint speed = conn.uint("802-3-ethernet", "speed");
	if (autoNeg)
		m_negotiation->setCurrentItem(1);
	else if (speed)
		m_negotiation->setCurrentItem(2);
	else
		m_negotiation->setCurrentItem(0);
	m_speed->setCurrentItem(2);
	for (int i = 0; i < 6; ++i)
		if (s_speeds[i] == speed)
			m_speed->setCurrentItem(i);
	m_duplex->setCurrentItem(conn.str("802-3-ethernet", "duplex") == "half" ? 1 : 0);
	slotNegotiation(m_negotiation->currentItem());

	uint wol = conn.uint("802-3-ethernet", "wake-on-lan", 1);
	m_wol->setCurrentItem(0);
	for (int i = 0; i < 5; ++i)
		if (s_wol[i] == wol)
			m_wol->setCurrentItem(i);
}

void EthernetPage::save(ConnectionSettings &conn)
{
	conn.ensure("802-3-ethernet");
	conn.setStr("connection", "interface-name", interfaceText(m_iface));
	conn.setStr("802-3-ethernet", "mac-address", m_mac->text().stripWhiteSpace().upper());
	conn.remove("802-3-ethernet", "cloned-mac-address");
	conn.setStr("802-3-ethernet", "assigned-mac-address", macChoice(m_cloned));
	if (m_mtu->value())
		conn.setUInt("802-3-ethernet", "mtu", m_mtu->value());
	else
		conn.remove("802-3-ethernet", "mtu");

	switch (m_negotiation->currentItem()) {
	case 1:
		conn.setBool("802-3-ethernet", "auto-negotiate", true);
		conn.remove("802-3-ethernet", "speed");
		conn.remove("802-3-ethernet", "duplex");
		break;
	case 2:
		conn.setBool("802-3-ethernet", "auto-negotiate", false);
		conn.setUInt("802-3-ethernet", "speed", s_speeds[m_speed->currentItem()]);
		conn.setStr("802-3-ethernet", "duplex", m_duplex->currentItem() ? "half" : "full");
		break;
	default:
		conn.setBool("802-3-ethernet", "auto-negotiate", false);
		conn.remove("802-3-ethernet", "speed");
		conn.remove("802-3-ethernet", "duplex");
	}
	conn.setUInt("802-3-ethernet", "wake-on-lan", s_wol[m_wol->currentItem()]);
}

TQString EthernetPage::validate() const
{
	TQString mac = m_mac->text().stripWhiteSpace();
	if (!mac.isEmpty() && !validMac(mac))
		return i18n("The MAC address \"%1\" is not valid.").arg(mac);
	TQString cloned = macChoice(m_cloned);
	if (cloned.contains(':') && !validMac(cloned))
		return i18n("The cloned MAC address \"%1\" is not valid.").arg(cloned);
	return TQString::null;
}

/* ---------------------------------------------------------------------- */
/* 802.1X on wired connections                                            */
/* ---------------------------------------------------------------------- */

WiredSecurityPage::WiredSecurityPage(TQWidget *parent)
	: SettingPage(parent, "WiredSecurityPage")
{
	TQVBoxLayout *l = new TQVBoxLayout(this, 8, 6);
	m_enable = new TQCheckBox(i18n("&Use 802.1X security for this connection"), this);
	l->addWidget(m_enable);
	m_eap = new EapWidget(this);
	l->addWidget(m_eap);
	l->addStretch(1);
	connect(m_enable, TQ_SIGNAL(toggled(bool)), m_eap, TQ_SLOT(setEnabled(bool)));
}

void WiredSecurityPage::load(const ConnectionSettings &conn)
{
	m_enable->setChecked(conn.has("802-1x"));
	m_eap->setEnabled(m_enable->isChecked());
	m_eap->load(conn);
}

void WiredSecurityPage::save(ConnectionSettings &conn)
{
	if (m_enable->isChecked())
		m_eap->save(conn);
	else
		conn.removeSetting("802-1x");
}

TQString WiredSecurityPage::validate() const
{
	return m_enable->isChecked() ? m_eap->validate() : TQString::null;
}

/* ---------------------------------------------------------------------- */
/* PPPoE / DSL                                                            */
/* ---------------------------------------------------------------------- */

PppoePage::PppoePage(TQWidget *parent)
	: SettingPage(parent, "PppoePage")
{
	TQGridLayout *g = new TQGridLayout(this, 9, 2, 8, 6);
	int row = 0;
	m_parent = new TQComboBox(true, this);
	fillInterfaces(m_parent, NM_DEVICE_TYPE_ETHERNET, false);
	addRow(g, row++, i18n("&Parent device:"), m_parent);
	m_user = new KLineEdit(this);
	addRow(g, row++, i18n("&Username:"), m_user);
	m_password = new SecretEdit(this);
	addRow(g, row++, i18n("Pass&word:"), m_password)->setAlignment(TQt::AlignTop);
	m_service = new KLineEdit(this);
	addRow(g, row++, i18n("&Service:"), m_service);

	m_mppe = new TQCheckBox(i18n("Use point-to-point &encryption (MPPE)"), this);
	g->addWidget(m_mppe, row++, 1);
	m_bsdcomp = new TQCheckBox(i18n("Allow &BSD data compression"), this);
	g->addWidget(m_bsdcomp, row++, 1);
	m_deflate = new TQCheckBox(i18n("Allow &Deflate data compression"), this);
	g->addWidget(m_deflate, row++, 1);
	m_lcpEcho = new TQCheckBox(i18n("Send PPP &echo packets"), this);
	g->addWidget(m_lcpEcho, row++, 1);
	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
}

void PppoePage::load(const ConnectionSettings &conn)
{
	TQString parent = conn.str("pppoe", "parent", conn.interfaceName());
	if (!parent.isEmpty())
		m_parent->setEditText(parent);
	m_user->setText(conn.str("pppoe", "username"));
	m_password->load(conn, "pppoe", "password");
	m_service->setText(conn.str("pppoe", "service"));
	m_mppe->setChecked(conn.boolean("ppp", "require-mppe"));
	m_bsdcomp->setChecked(!conn.boolean("ppp", "nobsdcomp"));
	m_deflate->setChecked(!conn.boolean("ppp", "nodeflate"));
	m_lcpEcho->setChecked(conn.uint("ppp", "lcp-echo-interval") > 0);
}

void PppoePage::save(ConnectionSettings &conn)
{
	conn.ensure("pppoe");
	conn.ensure("ppp");
	TQString parent = m_parent->currentText().stripWhiteSpace();
	conn.setStr("pppoe", "parent", parent);
	conn.setStr("pppoe", "username", m_user->text().stripWhiteSpace());
	m_password->save(conn, "pppoe", "password");
	conn.setStr("pppoe", "service", m_service->text().stripWhiteSpace());
	conn.setBool("ppp", "require-mppe", m_mppe->isChecked());
	conn.setBool("ppp", "nobsdcomp", !m_bsdcomp->isChecked());
	conn.setBool("ppp", "nodeflate", !m_deflate->isChecked());
	conn.setUInt("ppp", "lcp-echo-interval", m_lcpEcho->isChecked() ? 30 : 0);
	conn.setUInt("ppp", "lcp-echo-failure", m_lcpEcho->isChecked() ? 5 : 0);
}

TQString PppoePage::validate() const
{
	if (m_user->text().stripWhiteSpace().isEmpty())
		return i18n("The DSL connection needs a username.");
	if (m_parent->currentText().stripWhiteSpace().isEmpty())
		return i18n("Choose the Ethernet device the DSL modem is connected to.");
	return TQString::null;
}

/* ---------------------------------------------------------------------- */
/* InfiniBand                                                             */
/* ---------------------------------------------------------------------- */

InfinibandPage::InfinibandPage(TQWidget *parent)
	: SettingPage(parent, "InfinibandPage")
{
	TQGridLayout *g = new TQGridLayout(this, 6, 2, 8, 6);
	int row = 0;
	m_iface = new TQComboBox(true, this);
	fillInterfaces(m_iface, NM_DEVICE_TYPE_INFINIBAND);
	addRow(g, row++, i18n("&Device:"), m_iface);
	m_transport = new TQComboBox(false, this);
	m_transport->insertItem(i18n("Datagram"));
	m_transport->insertItem(i18n("Connected"));
	addRow(g, row++, i18n("&Transport mode:"), m_transport);
	m_mtu = new TQSpinBox(0, 65520, 1, this);
	m_mtu->setSpecialValueText(i18n("Automatic"));
	addRow(g, row++, i18n("MT&U:"), m_mtu);
	m_pkey = new KLineEdit(this);
	addRow(g, row++, i18n("&P_Key (hex):"), m_pkey);
	m_parent = new KLineEdit(this);
	addRow(g, row++, i18n("P&arent interface:"), m_parent);
	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
}

void InfinibandPage::load(const ConnectionSettings &conn)
{
	TQString iface = conn.interfaceName();
	if (iface.isEmpty())
		m_iface->setCurrentItem(0);
	else
		m_iface->setEditText(iface);
	m_transport->setCurrentItem(conn.str("infiniband", "transport-mode") == "connected" ? 1 : 0);
	m_mtu->setValue(conn.uint("infiniband", "mtu"));
	int pkey = conn.integer("infiniband", "p-key", -1);
	m_pkey->setText(pkey >= 0 ? TQString("0x%1").arg(pkey, 4, 16) : TQString::null);
	m_parent->setText(conn.str("infiniband", "parent"));
}

void InfinibandPage::save(ConnectionSettings &conn)
{
	conn.ensure("infiniband");
	conn.setStr("connection", "interface-name", interfaceText(m_iface));
	conn.setStr("infiniband", "transport-mode", m_transport->currentItem() ? "connected" : "datagram");
	if (m_mtu->value())
		conn.setUInt("infiniband", "mtu", m_mtu->value());
	else
		conn.remove("infiniband", "mtu");
	bool ok = false;
	int pkey = m_pkey->text().stripWhiteSpace().toInt(&ok, 0);
	if (ok)
		conn.setInt("infiniband", "p-key", pkey);
	else
		conn.remove("infiniband", "p-key");
	conn.setStr("infiniband", "parent", m_parent->text().stripWhiteSpace());
}
