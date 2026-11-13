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
#include "nmclient.h"

#include <tqcheckbox.h>
#include <tqcombobox.h>
#include <tqdir.h>
#include <tqfile.h>
#include <tqgroupbox.h>
#include <tqheader.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqpushbutton.h>
#include <tqregexp.h>
#include <tqspinbox.h>
#include <tqtable.h>

#include <klineedit.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kprocio.h>
#include <kstandarddirs.h>
#include <kurlrequester.h>

/* ---------------------------------------------------------------------- */
/* VPN plugins                                                            */
/* ---------------------------------------------------------------------- */

struct VpnService {
	const char *service;     /* short name, after org.freedesktop.NetworkManager. */
	const char *name;
	const char *gatewayKey;
	const char *userKey;
	const char *passwordKey;
	const char *groupKey;
	const char *groupPasswordKey;
	const char *package;
};

static const VpnService s_services[] = {
	{ "openvpn",     I18N_NOOP("OpenVPN"), "remote", "username", "password", 0, 0, "network-manager-openvpn" },
	{ "vpnc",        I18N_NOOP("Cisco compatible (vpnc)"), "IPSec gateway", "Xauth username", "Xauth password",
	                 "IPSec ID", "IPSec secret", "network-manager-vpnc" },
	{ "openconnect", I18N_NOOP("Cisco AnyConnect, GlobalProtect, Pulse (OpenConnect)"), "gateway", 0, 0, 0, 0,
	                 "network-manager-openconnect" },
	{ "l2tp",        I18N_NOOP("L2TP (with IPsec)"), "gateway", "user", "password", 0, 0, "network-manager-l2tp" },
	{ "pptp",        I18N_NOOP("PPTP"), "gateway", "user", "password", 0, 0, "network-manager-pptp" },
	{ "strongswan",  I18N_NOOP("IPsec/IKEv2 (strongSwan)"), "address", "user", "password", 0, 0,
	                 "network-manager-strongswan" },
	{ "libreswan",   I18N_NOOP("IPsec based VPN (Libreswan)"), "right", "leftxauthusername", "xauthpassword",
	                 "leftid", "pskvalue", "network-manager-libreswan" },
	{ "fortisslvpn", I18N_NOOP("Fortinet SSLVPN"), "gateway", "user", "password", 0, 0,
	                 "network-manager-fortisslvpn" },
	{ "sstp",        I18N_NOOP("SSTP"), "gateway", "user", "password", 0, 0, "network-manager-sstp" },
	{ "ssh",         I18N_NOOP("SSH"), "remote", "remote-username", "password", 0, 0, "network-manager-ssh" },
	{ 0, 0, 0, 0, 0, 0, 0, 0 }
};

static const VpnService *findService(const TQString &shortName)
{
	for (int i = 0; s_services[i].service; ++i)
		if (shortName == s_services[i].service)
			return &s_services[i];
	return 0;
}

/* the services of the VPN plugins that are installed */
static TQStringList installedServices()
{
	TQStringList result;
	TQStringList dirs;
	dirs << "/usr/lib/NetworkManager/VPN" << "/etc/NetworkManager/VPN";
	for (TQStringList::const_iterator d = dirs.begin(); d != dirs.end(); ++d) {
		TQDir dir(*d, "*.name");
		TQStringList files = dir.entryList();
		for (TQStringList::const_iterator f = files.begin(); f != files.end(); ++f) {
			TQFile file(dir.filePath(*f));
			if (!file.open(IO_ReadOnly))
				continue;
			TQString line;
			while (file.readLine(line, 1024) >= 0) {
				line = line.stripWhiteSpace();
				if (line.startsWith("service="))
					result << line.mid(8);
			}
		}
	}
	return result;
}

TQStringList VpnPage::serviceTypes()
{
	TQStringList t;
	for (int i = 0; s_services[i].service; ++i)
		t << TQString("org.freedesktop.NetworkManager.") + s_services[i].service;
	return t;
}

TQString VpnPage::serviceName(const TQString &service)
{
	TQString s = service.section('.', -1);
	const VpnService *v = findService(s);
	return v ? i18n(v->name) : service;
}

/* ---------------------------------------------------------------------- */

VpnPage::VpnPage(TQWidget *parent)
	: SettingPage(parent, "VpnPage")
{
	TQGridLayout *g = new TQGridLayout(this, 12, 2, 8, 6);
	int row = 0;

	m_service = new TQComboBox(false, this);
	TQStringList installed = installedServices();
	for (int i = 0; s_services[i].service; ++i) {
		TQString full = TQString("org.freedesktop.NetworkManager.") + s_services[i].service;
		TQString name = i18n(s_services[i].name);
		if (!installed.isEmpty() && !installed.contains(full))
			name = i18n("%1 (not installed: %2)").arg(name).arg(s_services[i].package);
		m_service->insertItem(name);
	}
	addRow(g, row++, i18n("VPN &type:"), m_service);
	connect(m_service, TQ_SIGNAL(activated(int)), TQ_SLOT(slotService(int)));

	m_gateway = new KLineEdit(this);
	m_gatewayLabel = addRow(g, row++, i18n("&Gateway:"), m_gateway);
	m_user = new KLineEdit(this);
	m_userLabel = addRow(g, row++, i18n("&Username:"), m_user);
	m_password = new SecretEdit(this, 0, true);
	m_passwordLabel = addRow(g, row++, i18n("&Password:"), m_password);
	m_passwordLabel->setAlignment(TQt::AlignTop);
	m_groupName = new KLineEdit(this);
	m_groupNameLabel = addRow(g, row++, i18n("Group &name:"), m_groupName);
	m_groupPassword = new SecretEdit(this, 0, true);
	m_groupPasswordLabel = addRow(g, row++, i18n("Group pass&word:"), m_groupPassword);
	m_groupPasswordLabel->setAlignment(TQt::AlignTop);

	/* OpenVPN */
	m_ovpn = new TQGroupBox(2, TQt::Horizontal, i18n("OpenVPN"), this);
	new TQLabel(i18n("Authentication:"), m_ovpn);
	m_ovpnType = new TQComboBox(false, m_ovpn);
	m_ovpnType->insertItem(i18n("Certificates (TLS)"));
	m_ovpnType->insertItem(i18n("Password"));
	m_ovpnType->insertItem(i18n("Password with certificates (TLS)"));
	m_ovpnType->insertItem(i18n("Static key"));
	connect(m_ovpnType, TQ_SIGNAL(activated(int)), TQ_SLOT(slotOpenVpnType(int)));
	new TQLabel(i18n("CA certificate:"), m_ovpn);
	m_ca = new KURLRequester(m_ovpn);
	new TQLabel(i18n("User certificate:"), m_ovpn);
	m_cert = new KURLRequester(m_ovpn);
	new TQLabel(i18n("Private key:"), m_ovpn);
	m_key = new KURLRequester(m_ovpn);
	new TQLabel(i18n("Key password:"), m_ovpn);
	m_certPass = new SecretEdit(m_ovpn, 0, true);
	new TQLabel(i18n("Static key:"), m_ovpn);
	m_staticKey = new KURLRequester(m_ovpn);
	new TQLabel(i18n("TLS auth key:"), m_ovpn);
	m_ta = new KURLRequester(m_ovpn);
	new TQLabel(i18n("Port:"), m_ovpn);
	m_port = new KLineEdit(m_ovpn);
	new TQLabel(i18n("Device type:"), m_ovpn);
	m_dev = new TQComboBox(false, m_ovpn);
	m_dev->insertItem("TUN");
	m_dev->insertItem("TAP");
	new TQLabel(TQString::null, m_ovpn);
	m_tcp = new TQCheckBox(i18n("Use a TCP connection"), m_ovpn);
	g->addMultiCellWidget(m_ovpn, row, row, 0, 1);
	row++;

	/* every option of the plugin */
	TQGroupBox *box = new TQGroupBox(1, TQt::Horizontal, i18n("Other Options"), this);
	TQWidget *bw = new TQWidget(box);
	TQGridLayout *bl = new TQGridLayout(bw, 3, 2, 0, 6);
	m_data = new TQTable(0, 2, bw);
	m_data->horizontalHeader()->setLabel(0, i18n("Option"));
	m_data->horizontalHeader()->setLabel(1, i18n("Value"));
	m_data->setLeftMargin(0);
	m_data->verticalHeader()->hide();
	m_data->setColumnStretchable(0, true);
	m_data->setColumnStretchable(1, true);
	m_data->setMinimumHeight(90);
	bl->addMultiCellWidget(m_data, 0, 2, 0, 0);
	TQPushButton *add = new TQPushButton(i18n("&Add"), bw);
	TQPushButton *remove = new TQPushButton(i18n("&Delete"), bw);
	bl->addWidget(add, 0, 1);
	bl->addWidget(remove, 1, 1);
	bl->setRowStretch(2, 1);
	connect(add, TQ_SIGNAL(clicked()), TQ_SLOT(slotAddData()));
	connect(remove, TQ_SIGNAL(clicked()), TQ_SLOT(slotRemoveData()));
	g->addMultiCellWidget(box, row, row, 0, 1);
	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
}

TQString VpnPage::shortService() const
{
	int i = m_service->currentItem();
	return (i >= 0 && s_services[i].service) ? TQString(s_services[i].service) : TQString::null;
}

void VpnPage::showFields()
{
	const VpnService *v = findService(shortService());
	bool user = v && v->userKey;
	bool pw = v && v->passwordKey;
	bool group = v && v->groupKey;
	bool ovpn = shortService() == "openvpn";
	m_gateway->setShown(v && v->gatewayKey);
	m_gatewayLabel->setShown(v && v->gatewayKey);
	m_user->setShown(user);
	m_userLabel->setShown(user);
	m_password->setShown(pw);
	m_passwordLabel->setShown(pw);
	m_groupName->setShown(group);
	m_groupNameLabel->setShown(group);
	m_groupPassword->setShown(group);
	m_groupPasswordLabel->setShown(group);
	m_ovpn->setShown(ovpn);
	if (ovpn)
		slotOpenVpnType(m_ovpnType->currentItem());
}

static const char *s_ovpnTypes[] = { "tls", "password", "password-tls", "static-key" };

void VpnPage::slotOpenVpnType(int t)
{
	bool tls = t == 0 || t == 2;
	bool password = t == 1 || t == 2;
	m_user->setEnabled(password);
	m_password->setEnabled(password);
	m_ca->setEnabled(t != 3);
	m_cert->setEnabled(tls);
	m_key->setEnabled(tls);
	m_certPass->setEnabled(tls);
	m_staticKey->setEnabled(t == 3);
}

void VpnPage::slotService(int)
{
	/* the old plugin's options mean nothing to the new one */
	if (shortService() != m_lastService)
		writeTable(TQMap<TQString, TQString>());
	m_lastService = shortService();
	showFields();
	emit changed();
}

/* the vpn.data keys shown in fields instead of the table */
static TQStringList managedKeys(const TQString &service)
{
	TQStringList k;
	const VpnService *v = findService(service);
	if (!v)
		return k;
	if (v->gatewayKey) k << v->gatewayKey;
	if (v->userKey) k << v->userKey;
	if (v->passwordKey) k << TQString(v->passwordKey) + "-flags";
	if (v->groupKey) k << v->groupKey;
	if (v->groupPasswordKey) k << TQString(v->groupPasswordKey) + "-flags";
	if (service == "openvpn")
		k << "connection-type" << "ca" << "cert" << "key" << "cert-pass-flags" << "static-key"
		  << "ta" << "port" << "proto-tcp" << "dev-type";
	return k;
}

void VpnPage::writeTable(const TQMap<TQString, TQString> &data)
{
	TQStringList managed = managedKeys(shortService());
	m_data->setNumRows(0);
	for (TQMap<TQString, TQString>::const_iterator it = data.begin(); it != data.end(); ++it) {
		if (managed.contains(it.key()))
			continue;
		int r = m_data->numRows();
		m_data->setNumRows(r + 1);
		m_data->setText(r, 0, it.key());
		m_data->setText(r, 1, it.data());
	}
}

TQMap<TQString, TQString> VpnPage::readTable() const
{
	TQMap<TQString, TQString> data;
	for (int r = 0; r < m_data->numRows(); ++r) {
		TQString k = m_data->text(r, 0).stripWhiteSpace();
		if (!k.isEmpty())
			data[k] = m_data->text(r, 1);
	}
	return data;
}

void VpnPage::slotAddData()
{
	int r = m_data->numRows();
	m_data->setNumRows(r + 1);
	m_data->setCurrentCell(r, 0);
	m_data->editCell(r, 0);
}

void VpnPage::slotRemoveData()
{
	if (m_data->currentRow() >= 0)
		m_data->removeRow(m_data->currentRow());
}

void VpnPage::load(const ConnectionSettings &conn)
{
	TQString service = conn.str("vpn", "service-type").section('.', -1);
	m_service->setCurrentItem(0);
	for (int i = 0; s_services[i].service; ++i)
		if (service == s_services[i].service)
			m_service->setCurrentItem(i);
	m_lastService = shortService();
	m_dataMap = conn.strMap("vpn", "data");
	const VpnService *v = findService(shortService());

	if (v && v->gatewayKey)
		m_gateway->setText(m_dataMap[v->gatewayKey]);
	if (v && v->userKey)
		m_user->setText(m_dataMap[v->userKey]);
	else
		m_user->setText(conn.str("vpn", "user-name"));
	if (v && v->passwordKey)
		m_password->loadVpn(conn, v->passwordKey);
	if (v && v->groupKey) {
		m_groupName->setText(m_dataMap[v->groupKey]);
		m_groupPassword->loadVpn(conn, v->groupPasswordKey);
	}
	if (shortService() == "openvpn") {
		TQString t = m_dataMap["connection-type"];
		m_ovpnType->setCurrentItem(0);
		for (int i = 0; i < 4; ++i)
			if (t == s_ovpnTypes[i])
				m_ovpnType->setCurrentItem(i);
		m_ca->setURL(m_dataMap["ca"]);
		m_cert->setURL(m_dataMap["cert"]);
		m_key->setURL(m_dataMap["key"]);
		m_certPass->loadVpn(conn, "cert-pass");
		m_staticKey->setURL(m_dataMap["static-key"]);
		m_ta->setURL(m_dataMap["ta"]);
		m_port->setText(m_dataMap["port"]);
		m_tcp->setChecked(m_dataMap["proto-tcp"] == "yes");
		m_dev->setCurrentItem(m_dataMap["dev-type"] == "tap" ? 1 : 0);
	}
	writeTable(m_dataMap);
	showFields();
}

void VpnPage::save(ConnectionSettings &conn)
{
	TQString service = shortService();
	const VpnService *v = findService(service);
	conn.setStr("vpn", "service-type", "org.freedesktop.NetworkManager." + service);

	TQMap<TQString, TQString> data = readTable();
	if (v && v->gatewayKey && !m_gateway->text().stripWhiteSpace().isEmpty())
		data[v->gatewayKey] = m_gateway->text().stripWhiteSpace();
	if (v && v->userKey && !m_user->text().stripWhiteSpace().isEmpty())
		data[v->userKey] = m_user->text().stripWhiteSpace();
	if (v && v->groupKey && !m_groupName->text().stripWhiteSpace().isEmpty())
		data[v->groupKey] = m_groupName->text().stripWhiteSpace();

	if (service == "openvpn") {
		int t = m_ovpnType->currentItem();
		data["connection-type"] = s_ovpnTypes[t];
		if (t != 3 && !m_ca->url().isEmpty())
			data["ca"] = m_ca->url();
		if (t == 0 || t == 2) {
			if (!m_cert->url().isEmpty())
				data["cert"] = m_cert->url();
			if (!m_key->url().isEmpty())
				data["key"] = m_key->url();
		}
		if (t == 3 && !m_staticKey->url().isEmpty())
			data["static-key"] = m_staticKey->url();
		if (!m_ta->url().isEmpty())
			data["ta"] = m_ta->url();
		if (!m_port->text().stripWhiteSpace().isEmpty())
			data["port"] = m_port->text().stripWhiteSpace();
		if (m_tcp->isChecked())
			data["proto-tcp"] = "yes";
		if (m_dev->currentItem() == 1)
			data["dev-type"] = "tap";
	}
	conn.setStrMap("vpn", "data", data);

	if (v && v->passwordKey)
		m_password->saveVpn(conn, v->passwordKey);
	if (v && v->groupPasswordKey)
		m_groupPassword->saveVpn(conn, v->groupPasswordKey);
	if (service == "openvpn" && (m_ovpnType->currentItem() == 0 || m_ovpnType->currentItem() == 2))
		m_certPass->saveVpn(conn, "cert-pass");
}

TQString VpnPage::validate() const
{
	const VpnService *v = findService(shortService());
	if (v && v->gatewayKey && m_gateway->text().stripWhiteSpace().isEmpty())
		return i18n("Enter the VPN gateway (server address).");
	if (shortService() == "openvpn") {
		int t = m_ovpnType->currentItem();
		if ((t == 0 || t == 2) && (m_cert->url().isEmpty() || m_key->url().isEmpty()))
			return i18n("OpenVPN certificate authentication needs a user certificate and a private key.");
		if (t == 3 && m_staticKey->url().isEmpty())
			return i18n("Choose the OpenVPN static key file.");
	}
	return TQString::null;
}

/* ---------------------------------------------------------------------- */
/* WireGuard                                                              */
/* ---------------------------------------------------------------------- */

WireGuardPage::WireGuardPage(TQWidget *parent)
	: SettingPage(parent, "WireGuardPage")
{
	TQGridLayout *g = new TQGridLayout(this, 9, 2, 8, 6);
	int row = 0;
	m_iface = new KLineEdit(this);
	addRow(g, row++, i18n("&Interface name:"), m_iface);

	TQWidget *keyRow = new TQWidget(this);
	TQHBoxLayout *kl = new TQHBoxLayout(keyRow, 0, 6);
	m_privateKey = new SecretEdit(keyRow);
	kl->addWidget(m_privateKey, 1);
	TQPushButton *gen = new TQPushButton(i18n("&Generate"), keyRow);
	kl->addWidget(gen, 0, TQt::AlignTop);
	connect(gen, TQ_SIGNAL(clicked()), TQ_SLOT(slotGenerate()));
	addRow(g, row++, i18n("&Private key:"), keyRow)->setAlignment(TQt::AlignTop);

	m_publicKey = new KLineEdit(this);
	m_publicKey->setReadOnly(true);
	addRow(g, row++, i18n("Public key:"), m_publicKey);

	m_listenPort = new TQSpinBox(0, 65535, 1, this);
	m_listenPort->setSpecialValueText(i18n("Automatic"));
	addRow(g, row++, i18n("&Listen port:"), m_listenPort);
	m_fwmark = new TQSpinBox(0, 2147483647, 1, this);
	m_fwmark->setSpecialValueText(i18n("Off"));
	addRow(g, row++, i18n("Firewall &mark:"), m_fwmark);
	m_mtu = new TQSpinBox(0, 65535, 1, this);
	m_mtu->setSpecialValueText(i18n("Automatic"));
	addRow(g, row++, i18n("MT&U:"), m_mtu);
	m_peerRoutes = new TQCheckBox(i18n("Add &routes for the peers' allowed IPs"), this);
	g->addWidget(m_peerRoutes, row++, 1);

	TQGroupBox *box = new TQGroupBox(1, TQt::Horizontal, i18n("Peers"), this);
	TQWidget *bw = new TQWidget(box);
	TQGridLayout *bl = new TQGridLayout(bw, 3, 2, 0, 6);
	m_peers = new TQTable(0, 5, bw);
	m_peers->horizontalHeader()->setLabel(0, i18n("Public key"));
	m_peers->horizontalHeader()->setLabel(1, i18n("Allowed IPs"));
	m_peers->horizontalHeader()->setLabel(2, i18n("Endpoint"));
	m_peers->horizontalHeader()->setLabel(3, i18n("Keepalive (s)"));
	m_peers->horizontalHeader()->setLabel(4, i18n("Preshared key"));
	m_peers->setLeftMargin(0);
	m_peers->verticalHeader()->hide();
	for (int c = 0; c < 5; ++c)
		m_peers->setColumnStretchable(c, true);
	m_peers->setMinimumHeight(110);
	bl->addMultiCellWidget(m_peers, 0, 2, 0, 0);
	TQPushButton *add = new TQPushButton(i18n("&Add"), bw);
	TQPushButton *remove = new TQPushButton(i18n("&Delete"), bw);
	bl->addWidget(add, 0, 1);
	bl->addWidget(remove, 1, 1);
	bl->setRowStretch(2, 1);
	connect(add, TQ_SIGNAL(clicked()), TQ_SLOT(slotAddPeer()));
	connect(remove, TQ_SIGNAL(clicked()), TQ_SLOT(slotRemovePeer()));
	g->addMultiCellWidget(box, row, row, 0, 1);
	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
}

static TQString runWg(const TQStringList &args, const TQString &input)
{
	TQString wg = TDEStandardDirs::findExe("wg");
	if (wg.isEmpty())
		wg = TDEStandardDirs::findExe("wg", "/usr/bin:/usr/sbin:/sbin:/bin");
	if (wg.isEmpty())
		return TQString::null;
	KProcIO proc;
	proc << wg << args;
	if (!proc.start(TDEProcess::NotifyOnExit, true))
		return TQString::null;
	if (!input.isEmpty())
		proc.writeStdin(input, true);
	proc.closeWhenDone();
	proc.wait(5);
	TQString line, out;
	while (proc.readln(line) >= 0)
		out += line;
	return out.stripWhiteSpace();
}

void WireGuardPage::slotGenerate()
{
	TQString key = runWg(TQStringList("genkey"), TQString::null);
	if (key.isEmpty()) {
		KMessageBox::sorry(this, i18n("Generating a key needs the \"wg\" program from the "
		                              "wireguard-tools package."));
		return;
	}
	m_privateKey->setSecret(key, m_privateKey->flags());
	m_publicKey->setText(runWg(TQStringList("pubkey"), key));
	emit changed();
}

void WireGuardPage::slotAddPeer()
{
	int r = m_peers->numRows();
	m_peers->setNumRows(r + 1);
	m_peers->setText(r, 1, "0.0.0.0/0, ::/0");
	m_peers->setText(r, 3, "25");
	m_peerData << NMVariantMap();
	m_peers->setCurrentCell(r, 0);
	m_peers->editCell(r, 0);
}

void WireGuardPage::slotRemovePeer()
{
	int r = m_peers->currentRow();
	if (r < 0)
		return;
	m_peers->removeRow(r);
	TQValueList<NMVariantMap>::iterator it = m_peerData.at(r);
	if (it != m_peerData.end())
		m_peerData.remove(it);
}

void WireGuardPage::load(const ConnectionSettings &conn)
{
	m_iface->setText(conn.interfaceName());
	m_privateKey->load(conn, "wireguard", "private-key");
	TQString priv = conn.str("wireguard", "private-key");
	m_publicKey->setText(priv.isEmpty() ? TQString::null : runWg(TQStringList("pubkey"), priv));
	m_listenPort->setValue(conn.uint("wireguard", "listen-port"));
	m_fwmark->setValue(conn.uint("wireguard", "fwmark"));
	m_mtu->setValue(conn.uint("wireguard", "mtu"));
	m_peerRoutes->setChecked(conn.boolean("wireguard", "peer-routes", true));

	m_peerData = conn.mapList("wireguard", "peers");
	m_peers->setNumRows(m_peerData.count());
	int r = 0;
	for (TQValueList<NMVariantMap>::const_iterator it = m_peerData.begin(); it != m_peerData.end(); ++it, ++r) {
		NMVariantMap p = *it;
		m_peers->setText(r, 0, NMDBus::toStr(p["public-key"]));
		m_peers->setText(r, 1, NMDBus::toStrList(p["allowed-ips"]).join(", "));
		m_peers->setText(r, 2, NMDBus::toStr(p["endpoint"]));
		if (p.contains("persistent-keepalive"))
			m_peers->setText(r, 3, TQString::number(NMDBus::toUInt(p["persistent-keepalive"])));
		m_peers->setText(r, 4, NMDBus::toStr(p["preshared-key"]));
	}
}

void WireGuardPage::save(ConnectionSettings &conn)
{
	conn.ensure("wireguard");
	conn.setStr("connection", "interface-name", m_iface->text().stripWhiteSpace());
	m_privateKey->save(conn, "wireguard", "private-key");
	conn.setUInt("wireguard", "listen-port", m_listenPort->value());
	conn.setUInt("wireguard", "fwmark", m_fwmark->value());
	conn.setUInt("wireguard", "mtu", m_mtu->value());
	conn.setBool("wireguard", "peer-routes", m_peerRoutes->isChecked());

	TQValueList<NMVariantMap> peers;
	for (int r = 0; r < m_peers->numRows(); ++r) {
		TQString key = m_peers->text(r, 0).stripWhiteSpace();
		if (key.isEmpty())
			continue;
		NMVariantMap p = r < (int)m_peerData.count() ? m_peerData[r] : NMVariantMap();
		p["public-key"] = TQT_DBusData::fromString(key);
		TQStringList ips = TQStringList::split(TQRegExp("[,;\\s]+"), m_peers->text(r, 1));
		p["allowed-ips"] = NMDBus::fromStrList(ips);
		TQString ep = m_peers->text(r, 2).stripWhiteSpace();
		if (ep.isEmpty())
			p.remove("endpoint");
		else
			p["endpoint"] = TQT_DBusData::fromString(ep);
		bool ok = false;
		uint ka = m_peers->text(r, 3).stripWhiteSpace().toUInt(&ok);
		if (ok)
			p["persistent-keepalive"] = TQT_DBusData::fromUInt32(ka);
		else
			p.remove("persistent-keepalive");
		TQString psk = m_peers->text(r, 4).stripWhiteSpace();
		if (psk.isEmpty()) {
			p.remove("preshared-key");
			p.remove("preshared-key-flags");
		} else {
			p["preshared-key"] = TQT_DBusData::fromString(psk);
			p["preshared-key-flags"] = TQT_DBusData::fromUInt32(NM_SECRET_FLAG_NONE);
		}
		peers << p;
	}
	conn.setMapList("wireguard", "peers", peers);
}

TQString WireGuardPage::validate() const
{
	if (m_iface->text().stripWhiteSpace().isEmpty())
		return i18n("The WireGuard interface needs a name, for example wg0.");
	/* an empty key of the user's own is asked for when connecting */
	TQString key = m_privateKey->value().stripWhiteSpace();
	bool optional = (m_privateKey->flags() & (NM_SECRET_FLAG_NOT_SAVED | NM_SECRET_FLAG_NOT_REQUIRED)) ||
	                (key.isEmpty() && (m_privateKey->flags() & NM_SECRET_FLAG_AGENT_OWNED));
	if (!optional && key.length() != 44)
		return i18n("The WireGuard private key must be 44 characters long (base64). "
		            "Use Generate to create one.");
	for (int r = 0; r < m_peers->numRows(); ++r) {
		TQString key = m_peers->text(r, 0).stripWhiteSpace();
		if (!key.isEmpty() && key.length() != 44)
			return i18n("The public key of peer %1 is not valid.").arg(r + 1);
	}
	return TQString::null;
}

/* ---------------------------------------------------------------------- */
/* mobile broadband                                                       */
/* ---------------------------------------------------------------------- */

MobilePage::MobilePage(bool gsm, TQWidget *parent)
	: SettingPage(parent, "MobilePage"), m_gsm(gsm)
{
	TQGridLayout *g = new TQGridLayout(this, 9, 2, 8, 6);
	int row = 0;
	m_apn = new KLineEdit(this);
	TQLabel *apnLabel = addRow(g, row++, i18n("&APN:"), m_apn);
	m_number = new KLineEdit(this);
	addRow(g, row++, i18n("&Number:"), m_number);
	m_user = new KLineEdit(this);
	addRow(g, row++, i18n("&Username:"), m_user);
	m_password = new SecretEdit(this, 0, true);
	addRow(g, row++, i18n("&Password:"), m_password)->setAlignment(TQt::AlignTop);
	m_pin = new SecretEdit(this, 0, true);
	TQLabel *pinLabel = addRow(g, row++, i18n("SIM P&IN:"), m_pin);
	pinLabel->setAlignment(TQt::AlignTop);
	m_networkId = new KLineEdit(this);
	TQLabel *netLabel = addRow(g, row++, i18n("Net&work ID:"), m_networkId);
	m_roaming = new TQCheckBox(i18n("Allow &roaming"), this);
	g->addWidget(m_roaming, row++, 1);
	m_mtu = new TQSpinBox(0, 65535, 1, this);
	m_mtu->setSpecialValueText(i18n("Automatic"));
	addRow(g, row++, i18n("MT&U:"), m_mtu);
	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
	if (!gsm) {
		m_apn->hide();
		apnLabel->hide();
		m_pin->hide();
		pinLabel->hide();
		m_networkId->hide();
		netLabel->hide();
		m_roaming->hide();
	}
}

void MobilePage::load(const ConnectionSettings &conn)
{
	TQString s = m_gsm ? "gsm" : "cdma";
	m_number->setText(conn.str(s, "number"));
	m_user->setText(conn.str(s, "username"));
	m_password->load(conn, s, "password");
	m_mtu->setValue(conn.uint(s, "mtu"));
	if (m_gsm) {
		m_apn->setText(conn.str(s, "apn"));
		m_pin->load(conn, s, "pin");
		m_networkId->setText(conn.str(s, "network-id"));
		m_roaming->setChecked(!conn.boolean(s, "home-only"));
	}
}

void MobilePage::save(ConnectionSettings &conn)
{
	TQString s = m_gsm ? "gsm" : "cdma";
	conn.ensure(s);
	conn.setStr(s, "number", m_number->text().stripWhiteSpace());
	conn.setStr(s, "username", m_user->text().stripWhiteSpace());
	m_password->save(conn, s, "password");
	if (m_mtu->value())
		conn.setUInt(s, "mtu", m_mtu->value());
	else
		conn.remove(s, "mtu");
	if (m_gsm) {
		conn.setStr(s, "apn", m_apn->text().stripWhiteSpace());
		m_pin->save(conn, s, "pin");
		conn.setStr(s, "network-id", m_networkId->text().stripWhiteSpace());
		conn.setBool(s, "home-only", !m_roaming->isChecked());
	}
}

TQString MobilePage::validate() const
{
	if (!m_gsm && m_number->text().stripWhiteSpace().isEmpty())
		return i18n("A CDMA connection needs a number to dial, usually #777.");
	return TQString::null;
}
