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

#include <tqcheckbox.h>
#include <tqcombobox.h>
#include <tqgroupbox.h>
#include <tqheader.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqpushbutton.h>
#include <tqregexp.h>
#include <tqspinbox.h>
#include <tqtable.h>

#include <kdialogbase.h>
#include <klineedit.h>
#include <tdelocale.h>

#include <arpa/inet.h>

using namespace NMDBus;

/* "24" or "255.255.255.0" -> 24; -1 if invalid */
static int parsePrefix(const TQString &text, bool v6)
{
	TQString t = text.stripWhiteSpace();
	bool ok = false;
	int p = t.toInt(&ok);
	if (ok)
		return (p >= 0 && p <= (v6 ? 128 : 32)) ? p : -1;
	if (v6)
		return -1;
	TQ_UINT32 mask;
	if (!ip4FromString(t, &mask))
		return -1;
	TQ_UINT32 m = ntohl(mask);
	int bits = 0;
	while (m & 0x80000000) {
		++bits;
		m <<= 1;
	}
	return m ? -1 : bits;
}

static bool validAddress(const TQString &text, bool v6)
{
	if (v6) {
		TQByteArray b;
		return ip6FromString(text, &b);
	}
	TQ_UINT32 a;
	return ip4FromString(text, &a);
}

IpPage::IpPage(bool ipv6, TQWidget *parent)
	: SettingPage(parent, ipv6 ? "Ipv6Page" : "Ipv4Page"), m_v6(ipv6), m_routeMetric(-1), m_routeTable(0)
{
	TQGridLayout *g = new TQGridLayout(this, 12, 2, 8, 6);
	int row = 0;

	m_method = new TQComboBox(false, this);
	addRow(g, row++, i18n("&Method:"), m_method);
	connect(m_method, TQ_SIGNAL(activated(int)), TQ_SLOT(slotMethod(int)));

	TQGroupBox *box = new TQGroupBox(1, TQt::Horizontal, i18n("Addresses"), this);
	TQWidget *bw = new TQWidget(box);
	TQGridLayout *bl = new TQGridLayout(bw, 3, 2, 0, 6);
	m_addresses = new TQTable(0, 3, bw);
	m_addresses->horizontalHeader()->setLabel(0, i18n("Address"));
	m_addresses->horizontalHeader()->setLabel(1, ipv6 ? i18n("Prefix") : i18n("Netmask or prefix"));
	m_addresses->horizontalHeader()->setLabel(2, i18n("Gateway"));
	m_addresses->setLeftMargin(0);
	m_addresses->verticalHeader()->hide();
	m_addresses->setColumnStretchable(0, true);
	m_addresses->setColumnStretchable(1, true);
	m_addresses->setColumnStretchable(2, true);
	m_addresses->setMinimumHeight(90);
	bl->addMultiCellWidget(m_addresses, 0, 2, 0, 0);
	m_add = new TQPushButton(i18n("&Add"), bw);
	m_remove = new TQPushButton(i18n("&Delete"), bw);
	bl->addWidget(m_add, 0, 1);
	bl->addWidget(m_remove, 1, 1);
	bl->setRowStretch(2, 1);
	g->addMultiCellWidget(box, row, row, 0, 1);
	row++;
	connect(m_add, TQ_SIGNAL(clicked()), TQ_SLOT(slotAddAddress()));
	connect(m_remove, TQ_SIGNAL(clicked()), TQ_SLOT(slotRemoveAddress()));

	m_dns = new KLineEdit(this);
	addRow(g, row++, i18n("&DNS servers:"), m_dns);
	m_search = new KLineEdit(this);
	addRow(g, row++, i18n("&Search domains:"), m_search);

	m_dhcpId = new KLineEdit(this);
	m_dhcpIdLabel = addRow(g, row++, ipv6 ? i18n("DHCPv6 &DUID:") : i18n("DHCP c&lient ID:"), m_dhcpId);
	m_dhcpHostname = new KLineEdit(this);
	addRow(g, row++, i18n("DHCP &host name:"), m_dhcpHostname);

	m_privacy = new TQComboBox(false, this);
	m_privacy->insertItem(i18n("Default"));
	m_privacy->insertItem(i18n("Disabled"));
	m_privacy->insertItem(i18n("Enabled, prefer public address"));
	m_privacy->insertItem(i18n("Enabled, prefer temporary address"));
	TQLabel *pl = addRow(g, row++, i18n("IPv6 &privacy extensions:"), m_privacy);
	m_addrGen = new TQComboBox(false, this);
	m_addrGen->insertItem(i18n("EUI-64"));
	m_addrGen->insertItem(i18n("Stable privacy"));
	m_addrGen->insertItem(i18n("Default or EUI-64"));
	m_addrGen->insertItem(i18n("Default"));
	TQLabel *al = addRow(g, row++, i18n("Address &generation:"), m_addrGen);
	if (!ipv6) {
		m_privacy->hide();
		pl->hide();
		m_addrGen->hide();
		al->hide();
	}

	m_autoDns = new TQCheckBox(i18n("Use automatically obtained DNS ser&vers"), this);
	g->addWidget(m_autoDns, row++, 1);
	m_autoRoutes = new TQCheckBox(i18n("Use automatically obtained &routes"), this);
	g->addWidget(m_autoRoutes, row++, 1);
	m_neverDefault = new TQCheckBox(i18n("Use this connection &only for resources on its network"), this);
	g->addWidget(m_neverDefault, row++, 1);
	m_required = new TQCheckBox(ipv6 ? i18n("Require IPv&6 addressing for this connection to complete")
	                                 : i18n("Require IPv&4 addressing for this connection to complete"), this);
	g->addWidget(m_required, row++, 1);

	m_routesButton = new TQPushButton(i18n("R&outes..."), this);
	g->addWidget(m_routesButton, row++, 1, TQt::AlignLeft);
	connect(m_routesButton, TQ_SIGNAL(clicked()), TQ_SLOT(slotRoutes()));

	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);
	setVpnMode(false);
}

TQStringList IpPage::methods() const
{
	TQStringList m;
	if (m_v6)
		m << "auto" << "dhcp" << "manual" << "link-local" << "shared" << "ignore" << "disabled";
	else
		m << "auto" << "manual" << "link-local" << "shared" << "disabled";
	return m;
}

void IpPage::setVpnMode(bool vpn)
{
	m_method->clear();
	if (m_v6) {
		m_method->insertItem(i18n("Automatic"));
		m_method->insertItem(i18n("Automatic, DHCP only"));
		m_method->insertItem(i18n("Manual"));
		m_method->insertItem(i18n("Link-local only"));
		m_method->insertItem(i18n("Shared to other computers"));
		m_method->insertItem(i18n("Ignore"));
		m_method->insertItem(i18n("Disabled"));
	} else {
		m_method->insertItem(i18n("Automatic (DHCP)"));
		m_method->insertItem(i18n("Manual"));
		m_method->insertItem(i18n("Link-local only"));
		m_method->insertItem(i18n("Shared to other computers"));
		m_method->insertItem(i18n("Disabled"));
	}
	if (vpn) {
		/* VPN plugins only know automatic and manual */
		m_method->setCurrentItem(0);
	}
	slotMethod(m_method->currentItem());
}

void IpPage::slotMethod(int)
{
	TQString m = comboData(m_method, methods());
	bool manual = m == "manual";
	bool autoMethod = m == "auto" || m == "dhcp";
	bool none = m == "disabled" || m == "ignore" || m == "link-local";
	bool shared = m == "shared";
	m_addresses->setEnabled(manual || shared);
	m_add->setEnabled(manual || shared);
	m_remove->setEnabled(manual || shared);
	m_dns->setEnabled(!none);
	m_search->setEnabled(!none);
	m_dhcpId->setEnabled(autoMethod);
	m_dhcpHostname->setEnabled(autoMethod);
	m_autoDns->setEnabled(autoMethod);
	m_autoRoutes->setEnabled(autoMethod);
	m_neverDefault->setEnabled(!none);
	m_required->setEnabled(!none);
	m_routesButton->setEnabled(!none);
	m_privacy->setEnabled(m_v6 && !none);
	m_addrGen->setEnabled(m_v6 && !none);
	emit changed();
}

void IpPage::slotAddAddress()
{
	int r = m_addresses->numRows();
	m_addresses->setNumRows(r + 1);
	if (r == 0 && !m_v6)
		m_addresses->setText(r, 1, "24");
	else if (r == 0)
		m_addresses->setText(r, 1, "64");
	m_addresses->setCurrentCell(r, 0);
	m_addresses->editCell(r, 0);
}

void IpPage::slotRemoveAddress()
{
	int r = m_addresses->currentRow();
	if (r >= 0)
		m_addresses->removeRow(r);
}

bool IpPage::parseAddress(const TQString &text, TQString *addr) const
{
	*addr = text.stripWhiteSpace();
	return validAddress(*addr, m_v6);
}

void IpPage::load(const ConnectionSettings &conn)
{
	TQString s = setting();
	setComboData(m_method, methods(), conn.str(s, "method", "auto"));

	TQValueList<NMVariantMap> addrs = conn.mapList(s, "address-data");
	TQString gateway = conn.str(s, "gateway");
	m_addresses->setNumRows(0);
	if (addrs.isEmpty()) {
		/* old NetworkManager: addresses aau (IPv4) / a(ayuay) (IPv6) */
		if (!m_v6) {
			TQValueList<TQValueList<TQ_UINT32> > old = toUIntListList(conn.value(s, "addresses"));
			for (TQValueList<TQValueList<TQ_UINT32> >::const_iterator it = old.begin(); it != old.end(); ++it) {
				if ((*it).count() < 2)
					continue;
				NMVariantMap m;
				m["address"] = TQT_DBusData::fromString(ip4ToString((*it)[0]));
				m["prefix"] = TQT_DBusData::fromUInt32((*it)[1]);
				addrs << m;
				if (gateway.isEmpty() && (*it).count() > 2 && (*it)[2])
					gateway = ip4ToString((*it)[2]);
			}
		}
	}
	int r = 0;
	m_addresses->setNumRows(addrs.count());
	for (TQValueList<NMVariantMap>::const_iterator it = addrs.begin(); it != addrs.end(); ++it, ++r) {
		NMVariantMap m = *it;
		m_addresses->setText(r, 0, toStr(m["address"]));
		m_addresses->setText(r, 1, TQString::number(toUInt(m["prefix"])));
		if (r == 0)
			m_addresses->setText(r, 2, gateway);
	}

	TQStringList dns;
	if (conn.has(s, "dns-data")) {
		dns = conn.strList(s, "dns-data");
	} else if (m_v6) {
		TQValueList<TQByteArray> d = toBytesList(conn.value(s, "dns"));
		for (TQValueList<TQByteArray>::const_iterator it = d.begin(); it != d.end(); ++it)
			dns << ip6ToString(*it);
	} else {
		TQValueList<TQ_UINT32> d = toUIntList(conn.value(s, "dns"));
		for (TQValueList<TQ_UINT32>::const_iterator it = d.begin(); it != d.end(); ++it)
			dns << ip4ToString(*it);
	}
	m_dns->setText(dns.join(", "));
	m_search->setText(conn.strList(s, "dns-search").join(", "));
	m_autoDns->setChecked(!conn.boolean(s, "ignore-auto-dns"));
	m_autoRoutes->setChecked(!conn.boolean(s, "ignore-auto-routes"));
	m_neverDefault->setChecked(conn.boolean(s, "never-default"));
	m_required->setChecked(!conn.boolean(s, "may-fail", true));
	m_dhcpId->setText(conn.str(s, m_v6 ? "dhcp-duid" : "dhcp-client-id"));
	m_dhcpHostname->setText(conn.str(s, "dhcp-hostname"));
	if (m_v6) {
		int priv = conn.integer(s, "ip6-privacy", -1);
		m_privacy->setCurrentItem(priv < 0 ? 0 : TQMIN(3, priv + 1));
		int gen = conn.integer(s, "addr-gen-mode", 3);
		m_addrGen->setCurrentItem(TQMIN(3, TQMAX(0, gen)));
	}

	m_routes = conn.mapList(s, "route-data");
	if (m_routes.isEmpty() && !m_v6) {
		TQValueList<TQValueList<TQ_UINT32> > old = toUIntListList(conn.value(s, "routes"));
		for (TQValueList<TQValueList<TQ_UINT32> >::const_iterator it = old.begin(); it != old.end(); ++it) {
			if ((*it).count() < 4)
				continue;
			NMVariantMap m;
			m["dest"] = TQT_DBusData::fromString(ip4ToString((*it)[0]));
			m["prefix"] = TQT_DBusData::fromUInt32((*it)[1]);
			if ((*it)[2])
				m["next-hop"] = TQT_DBusData::fromString(ip4ToString((*it)[2]));
			m["metric"] = TQT_DBusData::fromUInt32((*it)[3]);
			m_routes << m;
		}
	}
	m_routeMetric = conn.integer(s, "route-metric", -1);
	slotMethod(m_method->currentItem());
}

void IpPage::save(ConnectionSettings &conn)
{
	TQString s = setting();
	TQString method = comboData(m_method, methods());
	conn.ensure(s);
	conn.setStr(s, "method", method);

	/* the deprecated array forms would override the new ones */
	conn.remove(s, "addresses");
	conn.remove(s, "routes");
	conn.remove(s, "address-labels");
	conn.remove(s, "dns-data");

	TQValueList<NMVariantMap> addrs;
	TQString gateway;
	if (method == "manual" || method == "shared") {
		for (int r = 0; r < m_addresses->numRows(); ++r) {
			TQString a = m_addresses->text(r, 0).stripWhiteSpace();
			if (a.isEmpty())
				continue;
			NMVariantMap m;
			m["address"] = TQT_DBusData::fromString(a);
			m["prefix"] = TQT_DBusData::fromUInt32(parsePrefix(m_addresses->text(r, 1), m_v6));
			addrs << m;
			TQString gw = m_addresses->text(r, 2).stripWhiteSpace();
			if (gateway.isEmpty() && !gw.isEmpty())
				gateway = gw;
		}
	}
	conn.setMapList(s, "address-data", addrs);
	conn.setStr(s, "gateway", gateway);

	TQStringList dnsList = TQStringList::split(TQRegExp("[,;\\s]+"), m_dns->text());
	if (m_v6) {
		TQValueList<TQByteArray> d;
		for (TQStringList::const_iterator it = dnsList.begin(); it != dnsList.end(); ++it) {
			TQByteArray b;
			if (ip6FromString(*it, &b))
				d << b;
		}
		conn.set(s, "dns", fromBytesList(d));
	} else {
		TQValueList<TQ_UINT32> d;
		for (TQStringList::const_iterator it = dnsList.begin(); it != dnsList.end(); ++it) {
			TQ_UINT32 a;
			if (ip4FromString(*it, &a))
				d << a;
		}
		conn.set(s, "dns", fromUIntList(d));
	}
	conn.setStrList(s, "dns-search", TQStringList::split(TQRegExp("[,;\\s]+"), m_search->text()));

	conn.setBool(s, "ignore-auto-dns", !m_autoDns->isChecked());
	conn.setBool(s, "ignore-auto-routes", !m_autoRoutes->isChecked());
	conn.setBool(s, "never-default", m_neverDefault->isChecked());
	conn.setBool(s, "may-fail", !m_required->isChecked());
	conn.setStr(s, m_v6 ? "dhcp-duid" : "dhcp-client-id", m_dhcpId->text().stripWhiteSpace());
	conn.setStr(s, "dhcp-hostname", m_dhcpHostname->text().stripWhiteSpace());
	if (m_v6) {
		conn.setInt(s, "ip6-privacy", m_privacy->currentItem() - 1);
		conn.setInt(s, "addr-gen-mode", m_addrGen->currentItem());
	}
	conn.setMapList(s, "route-data", m_routes);
}

TQString IpPage::validate() const
{
	TQString method = comboData(m_method, methods());
	if (method == "manual") {
		int count = 0;
		for (int r = 0; r < m_addresses->numRows(); ++r) {
			TQString a = m_addresses->text(r, 0).stripWhiteSpace();
			if (a.isEmpty())
				continue;
			++count;
			TQString addr;
			if (!parseAddress(a, &addr))
				return i18n("\"%1\" is not a valid %2 address.").arg(a).arg(m_v6 ? "IPv6" : "IPv4");
			if (parsePrefix(m_addresses->text(r, 1), m_v6) < 0)
				return i18n("The prefix or netmask of %1 is not valid.").arg(a);
			TQString gw = m_addresses->text(r, 2).stripWhiteSpace();
			if (!gw.isEmpty() && !validAddress(gw, m_v6))
				return i18n("The gateway \"%1\" is not valid.").arg(gw);
		}
		if (count == 0)
			return i18n("The manual %1 method needs at least one address.").arg(m_v6 ? "IPv6" : "IPv4");
	}
	TQStringList dnsList = TQStringList::split(TQRegExp("[,;\\s]+"), m_dns->text());
	for (TQStringList::const_iterator it = dnsList.begin(); it != dnsList.end(); ++it)
		if (!validAddress(*it, m_v6))
			return i18n("The DNS server \"%1\" is not a valid %2 address.").arg(*it).arg(m_v6 ? "IPv6" : "IPv4");
	return TQString::null;
}

/* ---------------------------------------------------------------------- */
/* static routes                                                          */
/* ---------------------------------------------------------------------- */

void IpPage::slotRoutes()
{
	KDialogBase dlg(this, "RoutesDialog", true, m_v6 ? i18n("IPv6 Routes") : i18n("IPv4 Routes"),
	                KDialogBase::Ok | KDialogBase::Cancel);
	TQWidget *page = new TQWidget(&dlg);
	dlg.setMainWidget(page);
	TQVBoxLayout *l = new TQVBoxLayout(page, 0, 6);
	TQLabel *hint = new TQLabel(i18n("Static routes used in addition to the automatic ones. "
	                                 "Empty rows are ignored."), page);
	hint->setAlignment(TQt::WordBreak);
	l->addWidget(hint);
	const int rows = TQMAX(12, (int)m_routes.count() + 4);
	TQTable *table = new TQTable(rows, 4, page);
	table->horizontalHeader()->setLabel(0, i18n("Destination"));
	table->horizontalHeader()->setLabel(1, m_v6 ? i18n("Prefix") : i18n("Netmask or prefix"));
	table->horizontalHeader()->setLabel(2, i18n("Gateway"));
	table->horizontalHeader()->setLabel(3, i18n("Metric"));
	table->setLeftMargin(0);
	table->verticalHeader()->hide();
	for (int c = 0; c < 4; ++c)
		table->setColumnStretchable(c, true);
	table->setMinimumSize(520, 260);
	l->addWidget(table);

	int r = 0;
	for (TQValueList<NMVariantMap>::const_iterator it = m_routes.begin(); it != m_routes.end(); ++it, ++r) {
		NMVariantMap m = *it;
		table->setText(r, 0, toStr(m["dest"]));
		table->setText(r, 1, TQString::number(toUInt(m["prefix"])));
		table->setText(r, 2, toStr(m["next-hop"]));
		if (m.contains("metric"))
			table->setText(r, 3, TQString::number(toUInt(m["metric"])));
	}

	while (dlg.exec() == TQDialog::Accepted) {
		TQValueList<NMVariantMap> routes;
		TQString error;
		for (int i = 0; i < table->numRows(); ++i) {
			TQString dest = table->text(i, 0).stripWhiteSpace();
			if (dest.isEmpty())
				continue;
			int prefix = parsePrefix(table->text(i, 1), m_v6);
			TQString gw = table->text(i, 2).stripWhiteSpace();
			if (!validAddress(dest, m_v6) || prefix < 0 || (!gw.isEmpty() && !validAddress(gw, m_v6))) {
				error = i18n("Row %1 is not a valid route.").arg(i + 1);
				break;
			}
			NMVariantMap m;
			m["dest"] = TQT_DBusData::fromString(dest);
			m["prefix"] = TQT_DBusData::fromUInt32(prefix);
			if (!gw.isEmpty())
				m["next-hop"] = TQT_DBusData::fromString(gw);
			bool ok = false;
			uint metric = table->text(i, 3).stripWhiteSpace().toUInt(&ok);
			if (ok)
				m["metric"] = TQT_DBusData::fromUInt32(metric);
			routes << m;
		}
		if (!error.isEmpty()) {
			hint->setText("<qt><b>" + error + "</b></qt>");
			continue;
		}
		m_routes = routes;
		emit changed();
		break;
	}
}
