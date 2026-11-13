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

#include "infodialog.h"
#include "devicelist.h"
#include "nmclient.h"
#include "nmtext.h"

#include <tqlayout.h>
#include <tqstylesheet.h>
#include <tqtimer.h>

#include <kiconloader.h>
#include <tdelocale.h>
#include <ktextbrowser.h>

InfoDialog::InfoDialog(TQWidget *parent)
	: KDialogBase(Tabbed, i18n("Connection Information"), Close, Close, parent, "InfoDialog", false, true)
{
	setWFlags(getWFlags() | WDestructiveClose);
	NMClient *nm = NMClient::self();
	int index = 0;
	for (NMActiveList::const_iterator it = nm->activeConnections().begin(); it != nm->activeConnections().end(); ++it) {
		if ((*it).type == "loopback")
			continue;
		TQFrame *page = addPage((*it).id);
		TQVBoxLayout *l = new TQVBoxLayout(page, 0, 0);
		KTextBrowser *b = new KTextBrowser(page);
		l->addWidget(b);
		m_pages[(*it).path] = b;
		m_pageIndex[(*it).path] = index++;
		for (TQStringList::const_iterator d = (*it).devices.begin(); d != (*it).devices.end(); ++d)
			nm->setStatisticsRefresh(*d, 1000);
	}
	if (m_pages.isEmpty()) {
		TQFrame *page = addPage(i18n("Not Connected"));
		TQVBoxLayout *l = new TQVBoxLayout(page, 0, 0);
		KTextBrowser *b = new KTextBrowser(page);
		b->setText(i18n("<qt><p>There is no active network connection.</p></qt>"));
		l->addWidget(b);
	}
	m_timer = new TQTimer(this);
	connect(m_timer, TQ_SIGNAL(timeout()), TQ_SLOT(refresh()));
	m_timer->start(2000);
	refresh();
	resize(520, 520);
}

void InfoDialog::showConnection(const TQString &activePath)
{
	if (m_pageIndex.contains(activePath))
		showPage(m_pageIndex[activePath]);
}

TQString InfoDialog::html(const TQString &activePath)
{
	NMClient *nm = NMClient::self();
	const NMActiveConnection *ac = nm->activeConnection(activePath);
	if (!ac)
		return i18n("<qt><p>The connection is no longer active.</p></qt>");
	const NMDeviceInfo *dev = ac->devices.isEmpty() ? 0 : nm->device(ac->devices.first());

	TQString h;
	if (dev && !ac->vpn) {
		h = DeviceInfo::html(*dev, false);
		h.truncate(h.findRev("</qt>"));
	} else {
		h = "<qt><h3>" + TQStyleSheet::escape(ac->id) + "</h3><table cellspacing=\"0\" cellpadding=\"2\">";
		h += "<tr><td><b>" + i18n("Type:") + "</b></td><td>" + NMText::connectionType(ac->type) + "</td></tr>";
		h += "<tr><td><b>" + i18n("State:") + "</b></td><td>" +
		     (ac->vpn ? NMText::vpnState(ac->vpnState) : NMText::activeState(ac->state)) + "</td></tr>";
		NMIpConfig v4 = nm->ipConfig(ac->ip4Config, false);
		if (!v4.isEmpty()) {
			h += "<tr><td><b>" + i18n("Address:") + "</b></td><td>" + v4.addresses.join(", ") + "</td></tr>";
			h += "<tr><td><b>" + i18n("Gateway:") + "</b></td><td>" + v4.gateway + "</td></tr>";
			h += "<tr><td><b>" + i18n("DNS:") + "</b></td><td>" + v4.dns.join(", ") + "</td></tr>";
		}
		NMIpConfig v6 = nm->ipConfig(ac->ip6Config, true);
		if (!v6.isEmpty())
			h += "<tr><td><b>" + i18n("IPv6 address:") + "</b></td><td>" + v6.addresses.join(", ") + "</td></tr>";
		h += "</table>";
	}
	if (ac->isDefault)
		h += "<p>" + i18n("This connection carries the default route.") + "</p>";

	if (dev) {
		TQ_UINT64 rx, tx;
		if (nm->deviceStatistics(dev->path, &rx, &tx)) {
			h += "<h4>" + i18n("Traffic") + "</h4><table cellspacing=\"0\" cellpadding=\"2\">";
			h += "<tr><td><b>" + i18n("Received:") + "</b></td><td>" + NMText::bytes(rx) + "</td></tr>";
			h += "<tr><td><b>" + i18n("Sent:") + "</b></td><td>" + NMText::bytes(tx) + "</td></tr>";
			if (m_lastRx.contains(activePath) && rx >= m_lastRx[activePath] && tx >= m_lastTx[activePath])
				h += "<tr><td><b>" + i18n("Rate:") + "</b></td><td>" +
				     i18n("%1/s down, %2/s up").arg(NMText::bytes((rx - m_lastRx[activePath]) / 2))
				                               .arg(NMText::bytes((tx - m_lastTx[activePath]) / 2)) +
				     "</td></tr>";
			h += "</table>";
			m_lastRx[activePath] = rx;
			m_lastTx[activePath] = tx;
		}
	}
	return h + "</qt>";
}

void InfoDialog::refresh()
{
	for (TQMap<TQString, KTextBrowser *>::iterator it = m_pages.begin(); it != m_pages.end(); ++it) {
		int y = it.data()->contentsY();
		it.data()->setText(html(it.key()));
		it.data()->setContentsPos(0, y);
	}
}

#include "infodialog.moc"
