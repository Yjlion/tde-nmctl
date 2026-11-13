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

#include "traypopup.h"
#include "connector.h"
#include "nmclient.h"
#include "nmctlsettings.h"
#include "nmtext.h"

#include <tqapplication.h>
#include <tqcheckbox.h>
#include <tqdesktopwidget.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqpushbutton.h>
#include <tqscrollview.h>
#include <tqstylesheet.h>
#include <tqtimer.h>

#include <kiconloader.h>
#include <tdeglobalsettings.h>
#include <tdelocale.h>
#include <kpushbutton.h>
#include <kseparator.h>

/* ---------------------------------------------------------------------- */

NetworkRow::NetworkRow(Kind k, TQWidget *parent)
	: TQFrame(parent), kind(k)
{
	setFrameStyle(TQFrame::NoFrame);
	TQHBoxLayout *l = new TQHBoxLayout(this, 3, 6);
	m_icon = new TQLabel(this);
	m_icon->setFixedSize(24, 24);
	l->addWidget(m_icon);
	TQVBoxLayout *text = new TQVBoxLayout(l, 0);
	TQHBoxLayout *titleRow = new TQHBoxLayout(text, 4);
	m_title = new TQLabel(this);
	titleRow->addWidget(m_title);
	m_lock = new TQLabel(this);
	m_lock->setPixmap(SmallIcon("encrypted"));
	titleRow->addWidget(m_lock);
	titleRow->addStretch(1);
	m_detail = new TQLabel(this);
	TQFont f = m_detail->font();
	f.setPointSize(TQMAX(6, f.pointSize() - 1));
	m_detail->setFont(f);
	m_detail->setEnabled(false);
	text->addWidget(m_detail);
	m_info = new TQPushButton(this);
	m_info->setPixmap(SmallIcon("messagebox_info"));
	m_info->setFlat(true);
	m_info->setFixedSize(26, 26);
	l->addWidget(m_info);
	m_button = new TQPushButton(this);
	l->addWidget(m_button);
	connect(m_button, TQ_SIGNAL(clicked()), TQ_SLOT(slotClicked()));
	connect(m_info, TQ_SIGNAL(clicked()), TQ_SLOT(slotInfo()));
	m_info->setShown(k == Active);
}

void NetworkRow::set(const TQString &icon, const TQString &title, const TQString &detail,
                     const TQString &button, bool secure)
{
	m_icon->setPixmap(SmallIcon(icon, 22));
	m_title->setText(title);
	m_detail->setText(detail);
	m_detail->setShown(!detail.isEmpty());
	m_lock->setShown(secure);
	m_button->setText(button);
}

void NetworkRow::slotClicked()
{
	emit clicked(this);
}

void NetworkRow::slotInfo()
{
	emit infoClicked(this);
}

void NetworkRow::enterEvent(TQEvent *)
{
	setPaletteBackgroundColor(TDEGlobalSettings::highlightColor().light(180));
}

void NetworkRow::leaveEvent(TQEvent *)
{
	unsetPalette();
}

void NetworkRow::mouseDoubleClickEvent(TQMouseEvent *)
{
	emit clicked(this);
}

/* ---------------------------------------------------------------------- */

static TQLabel *sectionLabel(const TQString &text, TQWidget *parent)
{
	TQLabel *l = new TQLabel("<b>" + text + "</b>", parent);
	return l;
}

TrayPopup::TrayPopup(TQWidget *parent, const char *name)
	: TQFrame(parent, name, WType_Popup)
{
	setFrameStyle(TQFrame::PopupPanel | TQFrame::Raised);
	setLineWidth(1);
	m_rows.setAutoDelete(false);

	TQVBoxLayout *l = new TQVBoxLayout(this, 8, 4);

	TQHBoxLayout *head = new TQHBoxLayout(l, 8);
	m_stateIcon = new TQLabel(this);
	head->addWidget(m_stateIcon);
	TQVBoxLayout *headText = new TQVBoxLayout(head, 0);
	m_state = new TQLabel(this);
	m_state->setTextFormat(TQt::PlainText);
	TQFont bold = m_state->font();
	bold.setBold(true);
	m_state->setFont(bold);
	m_connectivity = new TQLabel(this);
	m_connectivity->setEnabled(false);
	headText->addWidget(m_state);
	headText->addWidget(m_connectivity);
	head->setStretchFactor(headText, 1);

	TQHBoxLayout *switches = new TQHBoxLayout(l, 12);
	m_networking = new TQCheckBox(i18n("&Networking"), this);
	m_wifi = new TQCheckBox(i18n("&Wi-Fi"), this);
	m_wwan = new TQCheckBox(i18n("&Mobile"), this);
	switches->addWidget(m_networking);
	switches->addWidget(m_wifi);
	switches->addWidget(m_wwan);
	switches->addStretch(1);
	connect(m_networking, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotNetworking(bool)));
	connect(m_wifi, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotWifi(bool)));
	connect(m_wwan, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotWwan(bool)));

	l->addWidget(new KSeparator(this));
	m_activeLabel = sectionLabel(i18n("Active Connections"), this);
	l->addWidget(m_activeLabel);
	m_activeBox = new TQWidget(this);
	m_activeLayout = new TQVBoxLayout(m_activeBox, 0, 0);
	l->addWidget(m_activeBox);

	TQHBoxLayout *wifiHead = new TQHBoxLayout(l, 6);
	m_wifiLabel = sectionLabel(i18n("Wi-Fi Networks"), this);
	wifiHead->addWidget(m_wifiLabel, 1);
	m_rescan = new TQPushButton(this);
	m_rescan->setPixmap(SmallIcon("reload"));
	m_rescan->setFlat(true);
	m_rescan->setFixedSize(24, 24);
	wifiHead->addWidget(m_rescan);
	connect(m_rescan, TQ_SIGNAL(clicked()), TQ_SLOT(slotRescan()));

	m_wifiScroll = new TQScrollView(this);
	m_wifiScroll->setResizePolicy(TQScrollView::AutoOneFit);
	m_wifiScroll->setHScrollBarMode(TQScrollView::AlwaysOff);
	m_wifiScroll->setFrameStyle(TQFrame::NoFrame);
	m_wifiBox = new TQWidget(m_wifiScroll->viewport());
	m_wifiLayout = new TQVBoxLayout(m_wifiBox, 0, 0);
	m_wifiScroll->addChild(m_wifiBox);
	l->addWidget(m_wifiScroll);
	m_hidden = new TQPushButton(i18n("Connect to &Hidden Network..."), this);
	m_hidden->setFlat(true);
	l->addWidget(m_hidden, 0, TQt::AlignLeft);
	connect(m_hidden, TQ_SIGNAL(clicked()), TQ_SLOT(slotHidden()));

	m_otherLabel = sectionLabel(i18n("VPN and Other Connections"), this);
	l->addWidget(m_otherLabel);
	m_otherBox = new TQWidget(this);
	m_otherLayout = new TQVBoxLayout(m_otherBox, 0, 0);
	l->addWidget(m_otherBox);

	l->addWidget(new KSeparator(this));
	TQHBoxLayout *buttons = new TQHBoxLayout(l, 6);
	buttons->addStretch(1);
	KPushButton *edit = new KPushButton(SmallIconSet("configure"), i18n("&Edit Connections..."), this);
	buttons->addWidget(edit);
	connect(edit, TQ_SIGNAL(clicked()), TQ_SLOT(slotEdit()));

	m_syncTimer = new TQTimer(this);
	connect(m_syncTimer, TQ_SIGNAL(timeout()), TQ_SLOT(sync()));
	m_scanTimer = new TQTimer(this);
	connect(m_scanTimer, TQ_SIGNAL(timeout()), TQ_SLOT(slotRescan()));
	connect(NMClient::self(), TQ_SIGNAL(changed()), TQ_SLOT(slotChanged()));
}

void TrayPopup::slotChanged()
{
	if (isVisible() && !m_syncTimer->isActive())
		m_syncTimer->start(500, true);
}

void TrayPopup::showEvent(TQShowEvent *e)
{
	TQFrame::showEvent(e);
	/* keep the list of networks fresh while the popup is open */
	NMClient::self()->requestScan();
	m_scanTimer->start(20000);
}

void TrayPopup::hideEvent(TQHideEvent *e)
{
	m_scanTimer->stop();
	TQFrame::hideEvent(e);
}

void TrayPopup::clearRows()
{
	/* hidden at once, so they no longer count in the layouts */
	for (NetworkRow *r = m_rows.first(); r; r = m_rows.next()) {
		r->hide();
		r->deleteLater();
	}
	m_rows.clear();
}

NetworkRow *TrayPopup::addRow(NetworkRow::Kind kind, TQWidget *parent, TQVBoxLayout *layout)
{
	NetworkRow *r = new NetworkRow(kind, parent);
	layout->addWidget(r);
	connect(r, TQ_SIGNAL(clicked(NetworkRow*)), TQ_SLOT(slotRow(NetworkRow*)));
	connect(r, TQ_SIGNAL(infoClicked(NetworkRow*)), TQ_SLOT(slotInfo(NetworkRow*)));
	m_rows.append(r);
	r->show();
	return r;
}

static const TQString SEP = TQString(" ") + TQChar(0x00b7) + " ";

static TQString esc(const TQString &s)
{
	return TQStyleSheet::escape(s);
}

void TrayPopup::sync()
{
	NMClient *nm = NMClient::self();
	clearRows();

	/* header */
	const NMActiveConnection *primary = nm->activeConnection(nm->primaryConnection());
	const NMDeviceInfo *pdev = primary && !primary->devices.isEmpty() ? nm->device(primary->devices.first()) : 0;
	uint strength = 0;
	if (pdev && pdev->activeAP())
		strength = pdev->activeAP()->strength;
	bool vpn = false;
	for (NMActiveList::const_iterator it = nm->activeConnections().begin(); it != nm->activeConnections().end(); ++it)
		if (((*it).vpn || (*it).type == "wireguard") && (*it).state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
			vpn = true;
	m_stateIcon->setPixmap(DesktopIcon(NMText::trayIcon(pdev, nm->state(), vpn, strength), 32));
	if (!nm->isRunning()) {
		m_state->setText(i18n("NetworkManager is not running"));
		m_connectivity->setText(TQString::null);
	} else if (primary && nm->state() >= NM_STATE_CONNECTED_LOCAL) {
		m_state->setText(i18n("Connected to %1").arg(primary->id));
		m_connectivity->setText(NMText::connectivity(nm->connectivity()));
	} else {
		m_state->setText(NMText::managerState(nm->state()));
		m_connectivity->setText(TQString::null);
	}

	m_networking->blockSignals(true);
	m_wifi->blockSignals(true);
	m_wwan->blockSignals(true);
	m_networking->setChecked(nm->networkingEnabled());
	m_wifi->setChecked(nm->wirelessEnabled());
	m_wwan->setChecked(nm->wwanEnabled());
	m_networking->blockSignals(false);
	m_wifi->blockSignals(false);
	m_wwan->blockSignals(false);
	m_networking->setEnabled(nm->isRunning());
	m_wifi->setEnabled(nm->isRunning() && nm->networkingEnabled() && nm->wirelessHardwareEnabled());
	m_wifi->setShown(nm->hasWirelessDevice());
	m_wwan->setEnabled(nm->isRunning() && nm->networkingEnabled() && nm->wwanHardwareEnabled());
	m_wwan->setShown(nm->hasModemDevice());

	/* active connections */
	TQStringList activeProfiles;
	int activeCount = 0;
	for (NMActiveList::const_iterator it = nm->activeConnections().begin(); it != nm->activeConnections().end(); ++it) {
		const NMActiveConnection &ac = *it;
		if (ac.type == "loopback")
			continue;
		activeProfiles << ac.connection;
		const NMDeviceInfo *dev = ac.devices.isEmpty() ? 0 : nm->device(ac.devices.first());
		TQString icon = NMText::connectionIcon(ac.type);
		TQStringList detail;
		bool secure = false;
		if (dev && dev->isWireless() && dev->activeAP()) {
			icon = NMText::signalIcon(dev->activeAP()->strength);
			detail << i18n("Signal %1%").arg(dev->activeAP()->strength);
			secure = dev->activeAP()->isSecure();
		}
		if (ac.state != NM_ACTIVE_CONNECTION_STATE_ACTIVATED) {
			detail.prepend(NMText::activeState(ac.state));
			if (dev && dev->isActivating())
				detail[0] = NMText::deviceState(dev->state);
			icon = "tde-nmctl-connecting";
		} else {
			TQString cfg = !ac.ip4Config.isEmpty() && ac.ip4Config != "/" ? ac.ip4Config
			               : (dev ? dev->ip4Config : TQString::null);
			NMIpConfig ip = nm->ipConfig(cfg, false);
			if (!ip.addresses.isEmpty())
				detail << ip.addresses.first().section('/', 0, 0);
			if (dev && dev->speed && dev->type == NM_DEVICE_TYPE_ETHERNET)
				detail << NMText::bitrate(dev->speed * 1000);
		}
		if (dev)
			detail.prepend(dev->iface);
		NetworkRow *r = addRow(NetworkRow::Active, m_activeBox, m_activeLayout);
		r->path = ac.path;
		r->set(icon, esc(ac.id), detail.join(SEP), i18n("Disconnect"), secure);
		++activeCount;
	}
	m_activeLabel->setShown(activeCount > 0);
	m_activeBox->setShown(activeCount > 0);

	/* Wi-Fi networks: the strongest access point of each SSID */
	struct Net { NMAccessPoint ap; TQString device; bool known; bool active; };
	TQValueList<Net> nets;
	bool showAll = NmctlSettings::showAllNetworks();
	for (NMDeviceList::const_iterator d = nm->devices().begin(); d != nm->devices().end(); ++d) {
		if (!(*d).isWireless() || (*d).state <= NM_DEVICE_STATE_UNAVAILABLE)
			continue;
		for (NMAccessPointList::const_iterator a = (*d).accessPoints.begin(); a != (*d).accessPoints.end(); ++a) {
			if ((*a).ssid.isEmpty())
				continue;
			bool active = (*a).path == (*d).activeAccessPoint;
			bool known = !nm->connectionsForSsid((*a).ssid).isEmpty();
			if (!showAll && !known && !active)
				continue;
			bool merged = false;
			for (TQValueList<Net>::iterator n = nets.begin(); n != nets.end(); ++n) {
				if ((*n).ap.ssid == (*a).ssid) {
					if (active || (!(*n).active && (*a).strength > (*n).ap.strength)) {
						(*n).ap = *a;
						(*n).device = (*d).path;
						(*n).active = (*n).active || active;
					}
					merged = true;
				}
			}
			if (!merged) {
				Net n;
				n.ap = *a;
				n.device = (*d).path;
				n.known = known;
				n.active = active;
				nets << n;
			}
		}
	}
	/* active first, then saved networks, then by signal strength */
	TQValueList<Net> sorted;
	while (!nets.isEmpty()) {
		TQValueList<Net>::iterator best = nets.begin();
		for (TQValueList<Net>::iterator n = nets.begin(); n != nets.end(); ++n) {
			int sb = ((*best).active ? 1000 : 0) + ((*best).known ? 200 : 0) + (*best).ap.strength;
			int sn = ((*n).active ? 1000 : 0) + ((*n).known ? 200 : 0) + (*n).ap.strength;
			if (sn > sb)
				best = n;
		}
		sorted << *best;
		nets.remove(best);
	}
	int wifiCount = 0;
	for (TQValueList<Net>::const_iterator n = sorted.begin(); n != sorted.end(); ++n) {
		if ((*n).active)
			continue;   /* already listed above */
		NetworkRow *r = addRow(NetworkRow::AccessPoint, m_wifiBox, m_wifiLayout);
		r->path = (*n).ap.path;
		r->device = (*n).device;
		r->ssid = (*n).ap.ssid;
		TQString sec = (*n).ap.securityText();
		TQString detail = sec.isEmpty() ? i18n("Open") : sec;
		if ((*n).known)
			detail = i18n("Saved") + SEP + detail;
		detail += SEP + NMText::band((*n).ap.frequency);
		r->set(NMText::signalIcon((*n).ap.strength), esc((*n).ap.ssidText()), detail, i18n("Connect"),
		       (*n).ap.isSecure());
		++wifiCount;
	}
	bool wifiOn = nm->hasWirelessDevice() && nm->wirelessEnabled() && nm->networkingEnabled();
	m_wifiLabel->setShown(wifiOn);
	m_rescan->setShown(wifiOn);
	m_hidden->setShown(wifiOn);
	m_wifiScroll->setShown(wifiOn && wifiCount > 0);
	m_wifiBox->adjustSize();
	int rowHeight = wifiCount ? m_wifiBox->sizeHint().height() / wifiCount : 30;
	m_wifiScroll->setFixedHeight(TQMIN(wifiCount, 7) * rowHeight + 2);

	/* VPN and other saved connections that can be started now */
	int otherCount = 0;
	for (NMConnectionList::const_iterator c = nm->connections().begin(); c != nm->connections().end(); ++c) {
		TQString type = (*c).type();
		if (activeProfiles.contains((*c).path) || type == "802-11-wireless" || type == "loopback")
			continue;
		if (!(*c).settings.str("connection", "slave-type").isEmpty())
			continue;
		bool available = type == "vpn" || type == "wireguard" || type == "bond" || type == "bridge" ||
		                 type == "vlan" || type == "dummy" || type == "team";
		for (NMDeviceList::const_iterator d = nm->devices().begin(); d != nm->devices().end() && !available; ++d)
			if ((*d).availableConnections.contains((*c).path))
				available = true;
		if (!available)
			continue;
		NetworkRow *r = addRow(NetworkRow::Profile, m_otherBox, m_otherLayout);
		r->path = (*c).path;
		r->set(NMText::connectionIcon(type), esc((*c).id()), NMText::connectionType(type), i18n("Connect"));
		++otherCount;
	}
	m_otherLabel->setShown(otherCount > 0);
	m_otherBox->setShown(otherCount > 0);

	if (isVisible()) {
		layout()->activate();
		resize(TQMAX(minimumSizeHint().width(), 360), sizeHint().height());
	}
}

void TrayPopup::popupNear(const TQRect &anchor)
{
	sync();
	layout()->activate();
	resize(TQMAX(minimumSizeHint().width(), 360), sizeHint().height());
	TQDesktopWidget *desktop = TQApplication::desktop();
	TQRect screen = desktop->screenGeometry(desktop->screenNumber(anchor.center()));
	int x = anchor.center().x() - width() / 2;
	int y = anchor.top() - height() - 2;
	if (y < screen.top())
		y = anchor.bottom() + 2;
	x = TQMAX(screen.left(), TQMIN(x, screen.right() - width()));
	y = TQMAX(screen.top(), TQMIN(y, screen.bottom() - height()));
	move(x, y);
	show();
}

void TrayPopup::slotRow(NetworkRow *row)
{
	NMClient *nm = NMClient::self();
	switch (row->kind) {
	case NetworkRow::Active:
		nm->deactivate(row->path);
		break;
	case NetworkRow::AccessPoint: {
		TQString dev = row->device, ap = row->path;
		hide();
		Connector::connectAccessPoint(dev, ap);
		return;
	}
	case NetworkRow::Profile:
		nm->activateConnection(row->path);
		break;
	}
	hide();
}

void TrayPopup::slotInfo(NetworkRow *row)
{
	TQString path = row->path;
	hide();
	emit showInfo(path);
}

void TrayPopup::slotNetworking(bool on)
{
	NMClient::self()->setNetworkingEnabled(on);
}

void TrayPopup::slotWifi(bool on)
{
	NMClient::self()->setWirelessEnabled(on);
}

void TrayPopup::slotWwan(bool on)
{
	NMClient::self()->setWwanEnabled(on);
}

void TrayPopup::slotRescan()
{
	NMClient::self()->requestScan();
}

void TrayPopup::slotEdit()
{
	hide();
	emit editConnections();
}

void TrayPopup::slotHidden()
{
	hide();
	emit connectHidden();
}

#include "traypopup.moc"
