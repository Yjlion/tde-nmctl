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

#include "devicelist.h"
#include "nmtext.h"

#include <tqcheckbox.h>
#include <tqlayout.h>
#include <tqpushbutton.h>
#include <tqsplitter.h>
#include <tqstylesheet.h>
#include <tqtimer.h>

#include <kiconloader.h>
#include <tdelistview.h>
#include <tdelocale.h>
#include <ktextbrowser.h>

static TQString row(const TQString &label, const TQString &value)
{
	if (value.isEmpty())
		return TQString::null;
	return "<tr><td valign=\"top\"><b>" + TQStyleSheet::escape(label) + "</b></td><td>" + value + "</td></tr>";
}

static TQString esc(const TQString &s)
{
	return TQStyleSheet::escape(s);
}

TQString DeviceInfo::accessPointsHtml(const NMDeviceInfo &dev)
{
	TQString h = "<table cellspacing=\"0\" cellpadding=\"2\" width=\"100%\">";
	h += "<tr><th align=\"left\">" + i18n("Network") + "</th><th align=\"left\">" + i18n("Signal") +
	     "</th><th align=\"left\">" + i18n("Security") + "</th><th align=\"left\">" + i18n("Channel") +
	     "</th><th align=\"left\">" + i18n("BSSID") + "</th></tr>";
	/* strongest first */
	NMAccessPointList aps = dev.accessPoints;
	TQValueList<NMAccessPoint> sorted;
	while (!aps.isEmpty()) {
		NMAccessPointList::iterator best = aps.begin();
		for (NMAccessPointList::iterator it = aps.begin(); it != aps.end(); ++it)
			if ((*it).strength > (*best).strength)
				best = it;
		sorted << *best;
		aps.remove(best);
	}
	for (TQValueList<NMAccessPoint>::const_iterator it = sorted.begin(); it != sorted.end(); ++it) {
		TQString name = (*it).ssidText().isEmpty() ? i18n("(hidden)") : esc((*it).ssidText());
		if ((*it).path == dev.activeAccessPoint)
			name = "<b>" + name + "</b>";
		h += TQString("<tr><td>%1</td><td>%2%</td><td>%3</td><td>%4 (%5)</td><td>%6</td></tr>")
		     .arg(name).arg((*it).strength)
		     .arg(esc((*it).securityText().isEmpty() ? i18n("Open") : (*it).securityText()))
		     .arg((*it).channel()).arg(NMText::band((*it).frequency)).arg((*it).bssid);
	}
	h += "</table>";
	return h;
}

TQString DeviceInfo::html(const NMDeviceInfo &d, bool withNetworks)
{
	NMClient *nm = NMClient::self();
	TQString h = "<qt><h3>" + esc(d.iface) + " &mdash; " + esc(NMText::deviceType(d.type)) + "</h3>";
	h += "<table cellspacing=\"0\" cellpadding=\"2\">";
	TQString state = NMText::deviceState(d.state);
	if (d.state == NM_DEVICE_STATE_FAILED || d.state == NM_DEVICE_STATE_UNAVAILABLE ||
	    d.state == NM_DEVICE_STATE_DISCONNECTED) {
		TQString reason = NMText::stateReason(d.stateReason);
		if (!reason.isEmpty())
			state += " (" + reason + ")";
	}
	h += row(i18n("State:"), esc(state));
	const NMActiveConnection *ac = nm->activeConnection(d.activeConnection);
	if (ac)
		h += row(i18n("Connection:"), esc(ac->id) + (ac->isDefault ? " " + i18n("(default route)") : TQString()));
	if (!d.vendor.isEmpty() || !d.product.isEmpty())
		h += row(i18n("Hardware:"), esc((d.vendor + " " + d.product).stripWhiteSpace()));
	h += row(i18n("Driver:"), esc(d.driver + (d.driverVersion.isEmpty() ? TQString() : " " + d.driverVersion)));
	h += row(i18n("Firmware:"), esc(d.firmwareVersion));
	h += row(i18n("Hardware address:"), esc(d.hwAddress));
	if (!d.permHwAddress.isEmpty() && d.permHwAddress != d.hwAddress)
		h += row(i18n("Permanent address:"), esc(d.permHwAddress));
	if (d.speed) {
		if (d.type == NM_DEVICE_TYPE_ETHERNET)
			h += row(i18n("Speed:"), NMText::bitrate(d.speed * 1000));
		else
			h += row(i18n("Bit rate:"), NMText::bitrate(d.speed));
	}
	if (d.mtu)
		h += row(i18n("MTU:"), TQString::number(d.mtu));
	h += row(i18n("Capabilities:"), esc(d.modemCaps));

	if (d.isWireless()) {
		const NMAccessPoint *ap = d.activeAP();
		if (ap) {
			h += row(i18n("Network:"), esc(ap->ssidText()));
			h += row(i18n("Signal strength:"), TQString("%1%").arg(ap->strength));
			h += row(i18n("Security:"), esc(ap->securityText().isEmpty() ? i18n("Open") : ap->securityText()));
			h += row(i18n("Frequency:"), i18n("%1, channel %2").arg(NMText::frequency(ap->frequency)).arg(ap->channel()));
			h += row(i18n("Access point:"), esc(ap->bssid));
		}
	}

	NMIpConfig v4 = nm->ipConfig(d.ip4Config, false);
	if (!v4.isEmpty()) {
		h += "<tr><td colspan=\"2\"><br><b><u>" + i18n("IPv4") + "</u></b></td></tr>";
		h += row(i18n("Address:"), esc(v4.addresses.join(", ")));
		h += row(i18n("Gateway:"), esc(v4.gateway));
		h += row(i18n("DNS:"), esc(v4.dns.join(", ")));
		h += row(i18n("Domains:"), esc(v4.domains.join(", ")));
	}
	NMIpConfig v6 = nm->ipConfig(d.ip6Config, true);
	if (!v6.isEmpty()) {
		h += "<tr><td colspan=\"2\"><br><b><u>" + i18n("IPv6") + "</u></b></td></tr>";
		h += row(i18n("Address:"), esc(v6.addresses.join(", ")));
		h += row(i18n("Gateway:"), esc(v6.gateway));
		h += row(i18n("DNS:"), esc(v6.dns.join(", ")));
	}
	h += "</table>";

	if (withNetworks && d.isWireless() && !d.accessPoints.isEmpty())
		h += "<h4>" + i18n("Wi-Fi networks in range") + "</h4>" + accessPointsHtml(d);
	h += "</qt>";
	return h;
}

/* ---------------------------------------------------------------------- */

DeviceList::DeviceList(TQWidget *parent, const char *name)
	: TQWidget(parent, name), m_lastRx(0), m_lastTx(0)
{
	TQVBoxLayout *top = new TQVBoxLayout(this, 0, 6);
	TQSplitter *split = new TQSplitter(TQt::Vertical, this);
	top->addWidget(split, 1);

	m_list = new TDEListView(split);
	m_list->addColumn(i18n("Device"));
	m_list->addColumn(i18n("Type"));
	m_list->addColumn(i18n("State"));
	m_list->addColumn(i18n("Connection"));
	m_list->setAllColumnsShowFocus(true);
	m_list->setResizeMode(TQListView::LastColumn);
	m_list->setSorting(-1);

	m_details = new KTextBrowser(split);
	split->setResizeMode(m_list, TQSplitter::KeepSize);
	TQValueList<int> sizes;
	sizes << 150 << 300;
	split->setSizes(sizes);

	TQHBoxLayout *buttons = new TQHBoxLayout(top, 6);
	m_disconnect = new TQPushButton(SmallIconSet("connect_no"), i18n("&Disconnect"), this);
	m_rescan = new TQPushButton(SmallIconSet("reload"), i18n("&Search for Networks"), this);
	m_managed = new TQCheckBox(i18n("&Managed by NetworkManager"), this);
	m_autoconnect = new TQCheckBox(i18n("Connect &automatically"), this);
	buttons->addWidget(m_disconnect);
	buttons->addWidget(m_rescan);
	buttons->addStretch(1);
	buttons->addWidget(m_managed);
	buttons->addWidget(m_autoconnect);

	m_statsTimer = new TQTimer(this);
	connect(m_statsTimer, TQ_SIGNAL(timeout()), TQ_SLOT(slotStatistics()));

	connect(m_list, TQ_SIGNAL(selectionChanged()), TQ_SLOT(slotSelectionChanged()));
	connect(m_disconnect, TQ_SIGNAL(clicked()), TQ_SLOT(slotDisconnect()));
	connect(m_rescan, TQ_SIGNAL(clicked()), TQ_SLOT(slotRescan()));
	connect(m_managed, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotManaged(bool)));
	connect(m_autoconnect, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotAutoconnect(bool)));
	connect(NMClient::self(), TQ_SIGNAL(changed()), TQ_SLOT(reload()));
	reload();
}

TQString DeviceList::selectedPath() const
{
	TQListViewItem *item = m_list->selectedItem();
	return item ? item->text(4) : TQString::null;
}

void DeviceList::reload()
{
	TQString selected = selectedPath();
	NMClient *nm = NMClient::self();
	m_list->clear();
	TQListViewItem *last = 0;
	const NMDeviceList &devs = nm->devices();
	for (NMDeviceList::const_iterator it = devs.begin(); it != devs.end(); ++it) {
		if ((*it).type == NM_DEVICE_TYPE_LOOPBACK)
			continue;
		const NMActiveConnection *ac = nm->activeConnection((*it).activeConnection);
		TDEListViewItem *item = new TDEListViewItem(m_list, last, (*it).iface,
		                                            NMText::deviceType((*it).type),
		                                            NMText::deviceState((*it).state),
		                                            ac ? ac->id : TQString::null, (*it).path);
		TQString icon = NMText::deviceIcon((*it).type);
		if ((*it).isWireless()) {
			const NMAccessPoint *ap = (*it).activeAP();
			icon = NMText::signalIcon(ap ? ap->strength : 0);
		}
		item->setPixmap(0, SmallIcon(icon));
		if ((*it).path == selected || (selected.isEmpty() && !last))
			m_list->setSelected(item, true);
		last = item;
	}
	showDetails();
}

void DeviceList::slotSelectionChanged()
{
	m_rates = TQString::null;
	m_lastRx = m_lastTx = 0;
	showDetails();
}

void DeviceList::showDetails()
{
	NMClient *nm = NMClient::self();
	const NMDeviceInfo *d = nm->device(selectedPath());
	m_disconnect->setEnabled(d && (d->isConnected() || d->isActivating()));
	m_rescan->setEnabled(d && d->isWireless());
	m_managed->setEnabled(d != 0);
	m_autoconnect->setEnabled(d && d->managed);
	m_managed->blockSignals(true);
	m_autoconnect->blockSignals(true);
	m_managed->setChecked(d && d->managed);
	m_autoconnect->setChecked(d && d->autoconnect);
	m_managed->blockSignals(false);
	m_autoconnect->blockSignals(false);
	if (!d) {
		m_details->setText(TQString::null);
		m_statsTimer->stop();
		return;
	}
	TQString h = DeviceInfo::html(*d, true);
	if (!m_rates.isEmpty())
		h.insert(h.find("</table>"), m_rates);   /* into the first table */
	int y = m_details->contentsY();
	m_details->setText(h);
	m_details->setContentsPos(0, y);

	if (m_statsPath != d->path) {
		if (!m_statsPath.isEmpty())
			nm->setStatisticsRefresh(m_statsPath, 0);
		m_statsPath = d->path;
		nm->setStatisticsRefresh(m_statsPath, 2000);
	}
	if (!m_statsTimer->isActive())
		m_statsTimer->start(2000);
}

void DeviceList::slotStatistics()
{
	if (!isVisible() || m_statsPath.isEmpty())
		return;
	TQ_UINT64 rx, tx;
	if (!NMClient::self()->deviceStatistics(m_statsPath, &rx, &tx))
		return;
	TQString r = row(i18n("Received:"), NMText::bytes(rx));
	r += row(i18n("Sent:"), NMText::bytes(tx));
	if (m_lastRx && rx >= m_lastRx && tx >= m_lastTx)
		r += row(i18n("Rate:"), i18n("%1/s down, %2/s up").arg(NMText::bytes((rx - m_lastRx) / 2))
		                                                 .arg(NMText::bytes((tx - m_lastTx) / 2)));
	m_lastRx = rx;
	m_lastTx = tx;
	if (r != m_rates) {
		m_rates = r;
		showDetails();
	}
}

void DeviceList::slotDisconnect()
{
	TQString p = selectedPath();
	if (!p.isEmpty())
		NMClient::self()->disconnectDevice(p);
}

void DeviceList::slotManaged(bool on)
{
	TQString p = selectedPath();
	if (!p.isEmpty())
		NMClient::self()->setDeviceManaged(p, on);
}

void DeviceList::slotAutoconnect(bool on)
{
	TQString p = selectedPath();
	if (!p.isEmpty())
		NMClient::self()->setDeviceAutoconnect(p, on);
}

void DeviceList::slotRescan()
{
	NMClient::self()->requestScan(selectedPath());
}

#include "devicelist.moc"
