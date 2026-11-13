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
#include <tqregexp.h>
#include <tqspinbox.h>
#include <tqwidgetstack.h>

#include <klineedit.h>
#include <tdelocale.h>

/* ---------------------------------------------------------------------- */
/* Wi-Fi                                                                  */
/* ---------------------------------------------------------------------- */

static TQStringList wifiModes()
{
	TQStringList m;
	m << "infrastructure" << "ap" << "adhoc" << "mesh";
	return m;
}

WifiPage::WifiPage(TQWidget *parent)
	: SettingPage(parent, "WifiPage")
{
	TQGridLayout *g = new TQGridLayout(this, 12, 2, 8, 6);
	int row = 0;

	m_ssid = new KLineEdit(this);
	addRow(g, row++, i18n("&SSID:"), m_ssid);
	connect(m_ssid, TQ_SIGNAL(textChanged(const TQString&)), TQ_SIGNAL(changed()));

	m_mode = new TQComboBox(false, this);
	m_mode->insertItem(i18n("Client (infrastructure)"));
	m_mode->insertItem(i18n("Hotspot (access point)"));
	m_mode->insertItem(i18n("Ad-hoc"));
	m_mode->insertItem(i18n("Mesh"));
	addRow(g, row++, i18n("&Mode:"), m_mode);
	connect(m_mode, TQ_SIGNAL(activated(int)), TQ_SLOT(slotMode(int)));

	m_band = new TQComboBox(false, this);
	m_band->insertItem(i18n("Automatic"));
	m_band->insertItem(i18n("5 GHz (A)"));
	m_band->insertItem(i18n("2.4 GHz (B/G)"));
	m_band->insertItem(i18n("6 GHz"));
	addRow(g, row++, i18n("&Band:"), m_band);
	connect(m_band, TQ_SIGNAL(activated(int)), TQ_SLOT(slotBand(int)));

	m_channel = new TQSpinBox(0, 233, 1, this);
	m_channel->setSpecialValueText(i18n("Automatic"));
	addRow(g, row++, i18n("C&hannel:"), m_channel);

	m_bssid = new TQComboBox(true, this);
	addRow(g, row++, i18n("BSS&ID:"), m_bssid);

	m_iface = new TQComboBox(true, this);
	fillInterfaces(m_iface, NM_DEVICE_TYPE_WIFI);
	addRow(g, row++, i18n("&Device:"), m_iface);

	m_cloned = new TQComboBox(true, this);
	fillMacChoices(m_cloned);
	addRow(g, row++, i18n("&Cloned MAC address:"), m_cloned);

	m_randomization = new TQComboBox(false, this);
	m_randomization->insertItem(i18n("Default"));
	m_randomization->insertItem(i18n("Never"));
	m_randomization->insertItem(i18n("Always"));
	addRow(g, row++, i18n("MAC address &randomization:"), m_randomization);

	m_mtu = new TQSpinBox(0, 65535, 1, this);
	m_mtu->setSpecialValueText(i18n("Automatic"));
	m_mtu->setSuffix(i18n(" bytes"));
	addRow(g, row++, i18n("MT&U:"), m_mtu);

	m_powersave = new TQComboBox(false, this);
	m_powersave->insertItem(i18n("Default"));
	m_powersave->insertItem(i18n("Ignore"));
	m_powersave->insertItem(i18n("Disabled"));
	m_powersave->insertItem(i18n("Enabled"));
	addRow(g, row++, i18n("&Power saving:"), m_powersave);

	m_hidden = new TQCheckBox(i18n("This is a hidden net&work (it does not broadcast its name)"), this);
	g->addWidget(m_hidden, row++, 1);

	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
}

void WifiPage::slotBand(int b)
{
	m_channel->setEnabled(b != 0);
	if (b == 0)
		m_channel->setValue(0);
}

void WifiPage::slotMode(int)
{
	emit modeChanged();
	emit changed();
}

TQString WifiPage::ssid() const
{
	return m_ssid->text();
}

bool WifiPage::isHotspot() const
{
	return m_mode->currentItem() == 1 || m_mode->currentItem() == 2;
}

void WifiPage::load(const ConnectionSettings &conn)
{
	TQByteArray ssid = conn.bytes("802-11-wireless", "ssid");
	m_ssid->setText(NMDBus::ssidToString(ssid));
	setComboData(m_mode, wifiModes(), conn.str("802-11-wireless", "mode", "infrastructure"));
	TQString band = conn.str("802-11-wireless", "band");
	m_band->setCurrentItem(band == "a" ? 1 : band == "bg" ? 2 : band == "6GHz" ? 3 : 0);
	m_channel->setValue(conn.uint("802-11-wireless", "channel"));
	slotBand(m_band->currentItem());

	m_bssid->clear();
	m_bssid->insertItem(TQString::null);
	const NMDeviceList &devs = NMClient::self()->devices();
	for (NMDeviceList::const_iterator d = devs.begin(); d != devs.end(); ++d)
		for (NMAccessPointList::const_iterator a = (*d).accessPoints.begin(); a != (*d).accessPoints.end(); ++a)
			if ((*a).ssid == ssid && !ssid.isEmpty())
				m_bssid->insertItem((*a).bssid);
	m_bssid->setEditText(conn.str("802-11-wireless", "bssid"));

	TQString iface = conn.interfaceName();
	if (iface.isEmpty())
		m_iface->setCurrentItem(0);
	else
		m_iface->setEditText(iface);
	setMacChoice(m_cloned, conn.str("802-11-wireless", "assigned-mac-address",
	                                conn.str("802-11-wireless", "cloned-mac-address")));
	m_randomization->setCurrentItem(TQMIN(2, conn.uint("802-11-wireless", "mac-address-randomization")));
	m_mtu->setValue(conn.uint("802-11-wireless", "mtu"));
	m_powersave->setCurrentItem(TQMIN(3, conn.uint("802-11-wireless", "powersave")));
	m_hidden->setChecked(conn.boolean("802-11-wireless", "hidden"));
}

void WifiPage::save(ConnectionSettings &conn)
{
	conn.ensure("802-11-wireless");
	conn.setBytes("802-11-wireless", "ssid", NMDBus::bytes(m_ssid->text()));
	conn.setStr("802-11-wireless", "mode", comboData(m_mode, wifiModes()));
	static const char *bands[] = { "", "a", "bg", "6GHz" };
	conn.setStr("802-11-wireless", "band", bands[m_band->currentItem()]);
	if (m_band->currentItem() && m_channel->value())
		conn.setUInt("802-11-wireless", "channel", m_channel->value());
	else
		conn.remove("802-11-wireless", "channel");
	conn.setStr("802-11-wireless", "bssid", m_bssid->currentText().stripWhiteSpace().upper());
	conn.setStr("connection", "interface-name", interfaceText(m_iface));
	conn.remove("802-11-wireless", "cloned-mac-address");
	conn.setStr("802-11-wireless", "assigned-mac-address", macChoice(m_cloned));
	if (m_randomization->currentItem())
		conn.setUInt("802-11-wireless", "mac-address-randomization", m_randomization->currentItem());
	else
		conn.remove("802-11-wireless", "mac-address-randomization");
	if (m_mtu->value())
		conn.setUInt("802-11-wireless", "mtu", m_mtu->value());
	else
		conn.remove("802-11-wireless", "mtu");
	conn.setUInt("802-11-wireless", "powersave", m_powersave->currentItem());
	conn.setBool("802-11-wireless", "hidden", m_hidden->isChecked());
}

TQString WifiPage::validate() const
{
	TQCString ssid = m_ssid->text().utf8();
	if (ssid.isEmpty())
		return i18n("The Wi-Fi network needs an SSID (network name).");
	if (ssid.length() > 32)
		return i18n("The SSID can be at most 32 bytes long.");
	TQString bssid = m_bssid->currentText().stripWhiteSpace();
	if (!bssid.isEmpty() && !validMac(bssid))
		return i18n("The BSSID \"%1\" is not valid.").arg(bssid);
	return TQString::null;
}

/* ---------------------------------------------------------------------- */
/* Wi-Fi security                                                         */
/* ---------------------------------------------------------------------- */

enum SecurityType {
	SecNone = 0, SecWepKey, SecWepPassphrase, SecLeap, SecDynamicWep,
	SecWpaPsk, SecSae, SecWpaEap, SecWpa3Eap, SecOwe
};

WifiSecurityPage::WifiSecurityPage(TQWidget *parent)
	: SettingPage(parent, "WifiSecurityPage")
{
	TQVBoxLayout *top = new TQVBoxLayout(this, 8, 6);
	TQHBoxLayout *row = new TQHBoxLayout(top, 6);
	TQLabel *l = new TQLabel(i18n("&Security:"), this);
	m_type = new TQComboBox(false, this);
	m_type->insertItem(i18n("None"));
	m_type->insertItem(i18n("WEP 40/128-bit key (hex or ASCII)"));
	m_type->insertItem(i18n("WEP 128-bit passphrase"));
	m_type->insertItem(i18n("LEAP"));
	m_type->insertItem(i18n("Dynamic WEP (802.1X)"));
	m_type->insertItem(i18n("WPA & WPA2 Personal"));
	m_type->insertItem(i18n("WPA3 Personal"));
	m_type->insertItem(i18n("WPA & WPA2 Enterprise"));
	m_type->insertItem(i18n("WPA3 Enterprise (192-bit)"));
	m_type->insertItem(i18n("Enhanced Open (OWE)"));
	l->setBuddy(m_type);
	row->addWidget(l);
	row->addWidget(m_type, 1);
	connect(m_type, TQ_SIGNAL(activated(int)), TQ_SLOT(slotType(int)));

	m_stack = new TQWidgetStack(this);
	top->addWidget(m_stack, 1);

	/* none / OWE */
	m_nonePage = new TQWidget(m_stack);
	m_stack->addWidget(m_nonePage);

	/* personal */
	m_pskPage = new TQWidget(m_stack);
	TQGridLayout *g = new TQGridLayout(m_pskPage, 2, 2, 0, 6);
	m_psk = new SecretEdit(m_pskPage);
	l = new TQLabel(i18n("&Password:"), m_pskPage);
	l->setBuddy(m_psk->lineEdit());
	g->addWidget(l, 0, 0, TQt::AlignTop);
	g->addWidget(m_psk, 0, 1);
	g->setRowStretch(1, 1);
	g->setColStretch(1, 1);
	m_stack->addWidget(m_pskPage);

	/* WEP */
	m_wepPage = new TQWidget(m_stack);
	g = new TQGridLayout(m_wepPage, 4, 2, 0, 6);
	m_wepKey = new SecretEdit(m_wepPage);
	l = new TQLabel(i18n("&Key:"), m_wepPage);
	l->setBuddy(m_wepKey->lineEdit());
	g->addWidget(l, 0, 0, TQt::AlignTop);
	g->addWidget(m_wepKey, 0, 1);
	m_wepIndex = new TQComboBox(false, m_wepPage);
	m_wepIndex->insertItem(i18n("1 (default)"));
	for (int i = 2; i <= 4; ++i)
		m_wepIndex->insertItem(TQString::number(i));
	l = new TQLabel(i18n("WEP inde&x:"), m_wepPage);
	l->setBuddy(m_wepIndex);
	g->addWidget(l, 1, 0);
	g->addWidget(m_wepIndex, 1, 1);
	m_wepAuth = new TQComboBox(false, m_wepPage);
	m_wepAuth->insertItem(i18n("Open System"));
	m_wepAuth->insertItem(i18n("Shared Key"));
	l = new TQLabel(i18n("&Authentication:"), m_wepPage);
	l->setBuddy(m_wepAuth);
	g->addWidget(l, 2, 0);
	g->addWidget(m_wepAuth, 2, 1);
	m_wepType = 0;
	g->setRowStretch(3, 1);
	g->setColStretch(1, 1);
	m_stack->addWidget(m_wepPage);

	/* LEAP */
	m_leapPage = new TQWidget(m_stack);
	g = new TQGridLayout(m_leapPage, 3, 2, 0, 6);
	m_leapUser = new KLineEdit(m_leapPage);
	l = new TQLabel(i18n("&Username:"), m_leapPage);
	l->setBuddy(m_leapUser);
	g->addWidget(l, 0, 0);
	g->addWidget(m_leapUser, 0, 1);
	m_leapPassword = new SecretEdit(m_leapPage);
	l = new TQLabel(i18n("&Password:"), m_leapPage);
	l->setBuddy(m_leapPassword->lineEdit());
	g->addWidget(l, 1, 0, TQt::AlignTop);
	g->addWidget(m_leapPassword, 1, 1);
	g->setRowStretch(2, 1);
	g->setColStretch(1, 1);
	m_stack->addWidget(m_leapPage);

	/* enterprise */
	m_eapPage = new TQWidget(m_stack);
	TQVBoxLayout *v = new TQVBoxLayout(m_eapPage, 0, 6);
	m_eap = new EapWidget(m_eapPage);
	v->addWidget(m_eap);
	m_stack->addWidget(m_eapPage);

	TQHBoxLayout *pmfRow = new TQHBoxLayout(top, 6);
	m_pmf = new TQComboBox(false, this);
	m_pmf->insertItem(i18n("Default"));
	m_pmf->insertItem(i18n("Disabled"));
	m_pmf->insertItem(i18n("Optional"));
	m_pmf->insertItem(i18n("Required"));
	l = new TQLabel(i18n("Protected &management frames (PMF):"), this);
	l->setBuddy(m_pmf);
	pmfRow->addWidget(l);
	pmfRow->addWidget(m_pmf);
	pmfRow->addStretch(1);

	m_type->setCurrentItem(SecWpaPsk);
	slotType(SecWpaPsk);
}

void WifiSecurityPage::slotType(int t)
{
	switch (t) {
	case SecWepKey:
	case SecWepPassphrase:
		m_stack->raiseWidget(m_wepPage);
		break;
	case SecLeap:
		m_stack->raiseWidget(m_leapPage);
		break;
	case SecDynamicWep:
	case SecWpaEap:
	case SecWpa3Eap:
		m_stack->raiseWidget(m_eapPage);
		break;
	case SecWpaPsk:
	case SecSae:
		m_stack->raiseWidget(m_pskPage);
		break;
	default:
		m_stack->raiseWidget(m_nonePage);
	}
	m_pmf->setEnabled(t >= SecWpaPsk);
	emit changed();
}

void WifiSecurityPage::preset(const TQString &km)
{
	int t = SecNone;
	if (km == "wpa-psk")         t = SecWpaPsk;
	else if (km == "sae")        t = SecSae;
	else if (km == "wpa-eap")    t = SecWpaEap;
	else if (km == "owe")        t = SecOwe;
	else if (km == "none")       t = SecWepKey;
	m_type->setCurrentItem(t);
	slotType(t);
}

void WifiSecurityPage::load(const ConnectionSettings &conn)
{
	int t = SecNone;
	if (conn.has("802-11-wireless-security")) {
		TQString km = conn.str("802-11-wireless-security", "key-mgmt");
		if (km == "none")
			t = conn.uint("802-11-wireless-security", "wep-key-type") == 2 ? SecWepPassphrase : SecWepKey;
		else if (km == "ieee8021x")
			t = conn.str("802-11-wireless-security", "auth-alg") == "leap" ? SecLeap : SecDynamicWep;
		else if (km == "wpa-psk")
			t = SecWpaPsk;
		else if (km == "sae")
			t = SecSae;
		else if (km == "wpa-eap")
			t = SecWpaEap;
		else if (km == "wpa-eap-suite-b-192")
			t = SecWpa3Eap;
		else if (km == "owe")
			t = SecOwe;
	}
	m_type->setCurrentItem(t);
	slotType(t);

	m_psk->load(conn, "802-11-wireless-security", "psk");
	uint idx = conn.uint("802-11-wireless-security", "wep-tx-keyidx");
	m_wepIndex->setCurrentItem(TQMIN(3u, idx));
	uint wf = conn.has("802-11-wireless-security", "wep-key-flags")
	          ? conn.uint("802-11-wireless-security", "wep-key-flags") : SecretEdit::defaultFlags();
	m_wepKey->setSecret(conn.str("802-11-wireless-security", TQString("wep-key%1").arg(idx)), wf);
	m_wepAuth->setCurrentItem(conn.str("802-11-wireless-security", "auth-alg") == "shared" ? 1 : 0);
	m_leapUser->setText(conn.str("802-11-wireless-security", "leap-username"));
	m_leapPassword->load(conn, "802-11-wireless-security", "leap-password");
	m_eap->load(conn);
	m_pmf->setCurrentItem(TQMIN(3, conn.integer("802-11-wireless-security", "pmf")));
}

void WifiSecurityPage::save(ConnectionSettings &conn)
{
	const TQString s = "802-11-wireless-security";
	int t = m_type->currentItem();
	if (t == SecNone) {
		conn.removeSetting(s);
		conn.removeSetting("802-1x");
		return;
	}
	/* start from a clean setting, so keys of another type do not stay */
	NMVariantMap old = conn.map()[s];
	conn.removeSetting(s);
	conn.ensure(s);
	if (old.contains("proto"))
		conn.set(s, "proto", old["proto"]);

	bool eap = false;
	switch (t) {
	case SecWepKey:
	case SecWepPassphrase: {
		conn.setStr(s, "key-mgmt", "none");
		conn.setUInt(s, "wep-key-type", t == SecWepPassphrase ? 2 : 1);
		uint idx = m_wepIndex->currentItem();
		conn.setUInt(s, "wep-tx-keyidx", idx);
		conn.setStr(s, TQString("wep-key%1").arg(idx), m_wepKey->value());
		conn.setUInt(s, "wep-key-flags", m_wepKey->flags());
		conn.setStr(s, "auth-alg", m_wepAuth->currentItem() ? "shared" : "open");
		break;
	}
	case SecLeap:
		conn.setStr(s, "key-mgmt", "ieee8021x");
		conn.setStr(s, "auth-alg", "leap");
		conn.setStr(s, "leap-username", m_leapUser->text().stripWhiteSpace());
		m_leapPassword->save(conn, s, "leap-password");
		break;
	case SecDynamicWep:
		conn.setStr(s, "key-mgmt", "ieee8021x");
		eap = true;
		break;
	case SecWpaPsk:
		conn.setStr(s, "key-mgmt", "wpa-psk");
		m_psk->save(conn, s, "psk");
		break;
	case SecSae:
		conn.setStr(s, "key-mgmt", "sae");
		m_psk->save(conn, s, "psk");
		break;
	case SecWpaEap:
		conn.setStr(s, "key-mgmt", "wpa-eap");
		eap = true;
		break;
	case SecWpa3Eap:
		conn.setStr(s, "key-mgmt", "wpa-eap-suite-b-192");
		eap = true;
		break;
	case SecOwe:
		conn.setStr(s, "key-mgmt", "owe");
		break;
	}
	if (t >= SecWpaPsk)
		conn.setInt(s, "pmf", m_pmf->currentItem());
	if (eap)
		m_eap->save(conn);
	else
		conn.removeSetting("802-1x");
}

TQString WifiSecurityPage::validate() const
{
	int t = m_type->currentItem();
	if (t == SecWpaPsk || t == SecSae) {
		TQString p = m_psk->value();
		if (m_psk->flags() & (NM_SECRET_FLAG_NOT_SAVED | NM_SECRET_FLAG_NOT_REQUIRED))
			return TQString::null;
		bool hex = p.length() == 64 && TQString(p).replace(TQRegExp("[0-9a-fA-F]"), "").isEmpty();
		if (t == SecWpaPsk && !hex && (p.length() < 8 || p.length() > 63))
			return i18n("A WPA password must have 8 to 63 characters, or be 64 hexadecimal digits.");
		if (t == SecSae && p.isEmpty())
			return i18n("Enter the WPA3 password.");
	}
	if (t == SecWepKey && !(m_wepKey->flags() & NM_SECRET_FLAG_NOT_SAVED)) {
		TQString k = m_wepKey->value();
		bool hex = TQString(k).replace(TQRegExp("[0-9a-fA-F]"), "").isEmpty();
		if (!((k.length() == 5 || k.length() == 13) || (hex && (k.length() == 10 || k.length() == 26))))
			return i18n("A WEP key has 5 or 13 characters, or 10 or 26 hexadecimal digits.");
	}
	if (t == SecDynamicWep || t == SecWpaEap || t == SecWpa3Eap)
		return m_eap->validate();
	return TQString::null;
}
