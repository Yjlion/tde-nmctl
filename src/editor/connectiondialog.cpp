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

#include "connectiondialog.h"
#include "pages.h"
#include "nmclient.h"
#include "nmtext.h"
#include "secretstore.h"

#include <tqlayout.h>

#include <tdeapplication.h>
#include <kiconloader.h>
#include <tdelocale.h>
#include <tdemessagebox.h>

ConnectionDialog::ConnectionDialog(const ConnectionSettings &conn, const TQString &path,
                                   TQWidget *parent, const char *name)
	: KDialogBase(IconList, TQString::null, Ok | Cancel | Help, Ok, parent, name, true, true),
	  m_conn(conn), m_path(path), m_activate(false), m_general(0), m_wifi(0), m_ipv4(0)
{
	TQString type = conn.type();
	setCaption(path.isEmpty() ? i18n("New %1 Connection").arg(NMText::connectionType(type))
	                          : i18n("Editing %1").arg(conn.id()));
	setHelp("editing", "tde-nmctl");
	bool port = !conn.str("connection", "slave-type").isEmpty() ||
	            !conn.str("connection", "port-type").isEmpty();

	m_general = new GeneralPage();
	addPage(m_general, i18n("General"), "tde-nmctl");

	if (type == "802-3-ethernet") {
		addPage(new EthernetPage(), i18n("Ethernet"), "tde-nmctl-wired");
		addPage(new WiredSecurityPage(), i18n("802.1X Security"), "encrypted");
	} else if (type == "802-11-wireless") {
		m_wifi = new WifiPage();
		addPage(m_wifi, i18n("Wi-Fi"), "tde-nmctl-signal-75");
		addPage(new WifiSecurityPage(), i18n("Wi-Fi Security"), "encrypted");
		connect(m_wifi, TQ_SIGNAL(modeChanged()), TQ_SLOT(slotWifiMode()));
	} else if (type == "vpn") {
		addPage(new VpnPage(), i18n("VPN"), "tde-nmctl-vpn");
	} else if (type == "wireguard") {
		addPage(new WireGuardPage(), i18n("WireGuard"), "tde-nmctl-vpn");
	} else if (type == "bond") {
		addPage(new BondPage(), i18n("Bond"), "tde-nmctl-virtual");
	} else if (type == "bridge") {
		addPage(new BridgePage(), i18n("Bridge"), "tde-nmctl-virtual");
	} else if (type == "team") {
		addPage(new TeamPage(), i18n("Team"), "tde-nmctl-virtual");
	} else if (type == "vlan") {
		addPage(new VlanPage(), i18n("VLAN"), "tde-nmctl-virtual");
	} else if (type == "gsm" || type == "cdma") {
		addPage(new MobilePage(type == "gsm"), i18n("Mobile Broadband"), "tde-nmctl-modem");
	} else if (type == "pppoe") {
		addPage(new PppoePage(), i18n("DSL/PPPoE"), "tde-nmctl-wired");
		addPage(new EthernetPage(), i18n("Ethernet"), "tde-nmctl-wired");
	} else if (type == "infiniband") {
		addPage(new InfinibandPage(), i18n("InfiniBand"), "tde-nmctl-wired");
	} else {
		addPage(new InterfacePage(), i18n("Interface"), "tde-nmctl-virtual");
	}

	if (!port) {
		m_ipv4 = new IpPage(false);
		addPage(m_ipv4, i18n("IPv4 Settings"), "network");
		IpPage *v6 = new IpPage(true);
		addPage(v6, i18n("IPv6 Settings"), "network");
		if (type == "vpn" || type == "wireguard") {
			m_ipv4->setVpnMode(true);
			v6->setVpnMode(true);
		}
		if (type != "vpn" && type != "wireguard" && type != "gsm" && type != "cdma")
			addPage(new ProxyPage(), i18n("Proxy"), "proxy");
	}

	for (SettingPage *p = m_pages.first(); p; p = m_pages.next())
		p->load(m_conn);

	resize(TQMAX(720, sizeHint().width()), TQMAX(560, sizeHint().height()));
}

void ConnectionDialog::addPage(SettingPage *page, const TQString &title, const TQString &icon)
{
	TQFrame *frame = KDialogBase::addPage(title, title, BarIcon(icon, TDEIcon::SizeMedium));
	TQVBoxLayout *l = new TQVBoxLayout(frame, 0, 0);
	page->reparent(frame, TQPoint(0, 0));
	l->addWidget(page);
	m_pages.append(page);
}

void ConnectionDialog::showPage(int index)
{
	KDialogBase::showPage(index);
}

void ConnectionDialog::setActivateOnSave(const TQString &devicePath, const TQString &specificObject)
{
	m_activate = true;
	m_activateDevice = devicePath;
	m_activateSpecific = specificObject;
}

void ConnectionDialog::slotWifiMode()
{
	/* a hotspot shares this computer's connection */
	if (m_wifi && m_ipv4 && m_path.isEmpty() && m_wifi->isHotspot()) {
		ConnectionSettings tmp;
		tmp.setStr("ipv4", "method", "shared");
		m_ipv4->load(tmp);
	}
}

void ConnectionDialog::slotOk()
{
	int index = 0;
	for (SettingPage *p = m_pages.first(); p; p = m_pages.next(), ++index) {
		TQString error = p->validate();
		if (!error.isEmpty()) {
			showPage(index);
			KMessageBox::sorry(this, error, i18n("Invalid Setting"));
			return;
		}
	}

	ConnectionSettings conn = m_conn;
	for (SettingPage *p = m_pages.first(); p; p = m_pages.next())
		p->save(conn);
	if (conn.uuid().isEmpty())
		conn.setStr("connection", "uuid", ConnectionSettings::newUuid());

	/* the user's own secrets also go to TDE Wallet right away, so they are
	 * there even if the tray (the secret agent) is not running */
	SecretStore::saveAgentOwned(conn);

	NMClient *nm = NMClient::self();
	if (m_path.isEmpty()) {
		m_savedPath = nm->addConnection(conn);
		if (m_savedPath.isEmpty()) {
			KMessageBox::error(this, i18n("<qt>The connection could not be saved:<br>%1</qt>")
			                         .arg(nm->lastError()));
			return;
		}
	} else {
		if (!nm->updateConnection(m_path, conn)) {
			KMessageBox::error(this, i18n("<qt>The connection could not be saved:<br>%1</qt>")
			                         .arg(nm->lastError()));
			return;
		}
		m_savedPath = m_path;
	}
	if (m_activate)
		nm->activateConnection(m_savedPath, m_activateDevice, m_activateSpecific);
	accept();
}

/* ---------------------------------------------------------------------- */

ConnectionSettings ConnectionDialog::newConnection(const TQString &type)
{
	ConnectionSettings c;
	c.setStr("connection", "type", type);
	c.setStr("connection", "uuid", ConnectionSettings::newUuid());
	c.setBool("connection", "autoconnect", type != "vpn" && type != "wireguard");

	/* "Wi-Fi connection 2" etc. */
	TQString base = i18n("%1 connection").arg(NMText::connectionType(type));
	TQString id = base + " 1";
	const NMConnectionList &all = NMClient::self()->connections();
	for (int n = 1; ; ++n) {
		id = TQString("%1 %2").arg(base).arg(n);
		bool used = false;
		for (NMConnectionList::const_iterator it = all.begin(); it != all.end(); ++it)
			if ((*it).id() == id)
				used = true;
		if (!used)
			break;
	}
	c.setStr("connection", "id", id);

	c.setStr("ipv4", "method", "auto");
	c.setStr("ipv6", "method", "auto");

	if (type == "802-3-ethernet") {
		c.ensure("802-3-ethernet");
	} else if (type == "802-11-wireless") {
		c.setStr("802-11-wireless", "mode", "infrastructure");
		c.setStr("802-11-wireless-security", "key-mgmt", "wpa-psk");
	} else if (type == "vpn") {
		c.setStr("vpn", "service-type", "org.freedesktop.NetworkManager.openvpn");
	} else if (type == "wireguard") {
		c.ensure("wireguard");
		c.setStr("connection", "interface-name", "wg0");
		c.setStr("ipv4", "method", "manual");
		c.setStr("ipv6", "method", "ignore");
	} else if (type == "bond") {
		TQMap<TQString, TQString> opts;
		opts["mode"] = "balance-rr";
		c.setStrMap("bond", "options", opts);
		c.setStr("connection", "interface-name", "bond0");
	} else if (type == "bridge") {
		c.ensure("bridge");
		c.setStr("connection", "interface-name", "br0");
	} else if (type == "team") {
		c.ensure("team");
		c.setStr("connection", "interface-name", "team0");
	} else if (type == "vlan") {
		c.setUInt("vlan", "id", 1);
	} else if (type == "dummy") {
		c.ensure("dummy");
		c.setStr("connection", "interface-name", "dummy0");
		c.setStr("ipv4", "method", "manual");
		c.setStr("ipv6", "method", "ignore");
	} else if (type == "gsm") {
		c.ensure("gsm");
		c.ensure("ppp");
	} else if (type == "cdma") {
		c.setStr("cdma", "number", "#777");
		c.ensure("ppp");
	} else if (type == "pppoe") {
		c.ensure("pppoe");
		c.ensure("802-3-ethernet");
		c.ensure("ppp");
	} else if (type == "infiniband") {
		c.setStr("infiniband", "transport-mode", "datagram");
	} else {
		c.ensure(type);
	}
	return c;
}

ConnectionSettings ConnectionDialog::withSecrets(const TQString &path)
{
	NMClient *nm = NMClient::self();
	const NMConnection *c = nm->connection(path);
	if (!c)
		return ConnectionSettings();
	ConnectionSettings conn = c->settings;
	TQStringList settings = SecretStore::secretSettings();
	for (TQStringList::const_iterator it = settings.begin(); it != settings.end(); ++it) {
		if (!conn.has(*it))
			continue;
		NMSettingsMap secrets;
		if (nm->getSecrets(path, *it, &secrets))
			conn.mergeSecrets(secrets);
	}
	SecretStore::fillAgentOwned(conn);
	return conn;
}

bool ConnectionDialog::editConnection(const TQString &path, TQWidget *parent, int page)
{
	ConnectionSettings conn = withSecrets(path);
	if (conn.uuid().isEmpty())
		return false;
	ConnectionDialog dlg(conn, path, parent, "ConnectionDialog");
	if (page)
		dlg.showPage(page);
	return dlg.exec() == TQDialog::Accepted;
}

TQString ConnectionDialog::createConnection(const TQString &type, TQWidget *parent,
                                           const ConnectionSettings *preset)
{
	ConnectionSettings conn = preset ? *preset : newConnection(type);
	ConnectionDialog dlg(conn, TQString::null, parent, "ConnectionDialog");
	if (dlg.exec() != TQDialog::Accepted)
		return TQString::null;
	return dlg.savedPath();
}

#include "connectiondialog.moc"
