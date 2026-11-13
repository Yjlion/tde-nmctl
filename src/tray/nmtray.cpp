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

#include "nmtray.h"
#include "connector.h"
#include "infodialog.h"
#include "nmclient.h"
#include "nmctlsettings.h"
#include "nmtext.h"
#include "secretagent.h"
#include "traypopup.h"

#include <tqtimer.h>
#include <tqtooltip.h>
#include <tqstylesheet.h>

#include <tdeaboutapplication.h>
#include <tdeaction.h>
#include <tdeapplication.h>
#include <khelpmenu.h>
#include <kiconloader.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <knotifyclient.h>
#include <tdepopupmenu.h>
#include <krun.h>

NmTray::NmTray(TQWidget *parent, const char *name)
	: KSystemTray(parent, name), m_started(false)
{
	NMClient *nm = NMClient::self();
	m_agent = new SecretAgent(this, "SecretAgent");

	m_popup = new TrayPopup(0, "TrayPopup");
	connect(m_popup, TQ_SIGNAL(editConnections()), TQ_SLOT(editConnections()));
	connect(m_popup, TQ_SIGNAL(showInfo(const TQString&)), TQ_SLOT(showInfo(const TQString&)));
	connect(m_popup, TQ_SIGNAL(connectHidden()), TQ_SLOT(connectHidden()));

	m_networking = new TDEToggleAction(i18n("Enable &Networking"), 0, this, TQ_SLOT(slotNetworking()),
	                                   actionCollection(), "networking");
	m_wifi = new TDEToggleAction(i18n("Enable &Wi-Fi"), 0, this, TQ_SLOT(slotWifi()),
	                             actionCollection(), "wifi");
	m_wwan = new TDEToggleAction(i18n("Enable &Mobile Broadband"), 0, this, TQ_SLOT(slotWwan()),
	                             actionCollection(), "wwan");
	m_notifications = new TDEToggleAction(i18n("Show N&otifications"), 0, this, TQ_SLOT(slotNotifications()),
	                                      actionCollection(), "notifications");
	m_autostart = new TDEToggleAction(i18n("Start Automatically at &Login"), 0, this, TQ_SLOT(slotAutostart()),
	                                  actionCollection(), "autostart");

	TDEPopupMenu *menu = contextMenu();
	m_networking->plug(menu);
	m_wifi->plug(menu);
	m_wwan->plug(menu);
	menu->insertSeparator();
	menu->insertItem(SmallIcon("messagebox_info"), i18n("Connection &Information..."), this, TQ_SLOT(showInfo()));
	menu->insertItem(SmallIcon("tde-nmctl-signal-75"), i18n("Connect to &Hidden Wi-Fi Network..."),
	                 this, TQ_SLOT(connectHidden()));
	menu->insertItem(SmallIcon("configure"), i18n("&Edit Connections..."), this, TQ_SLOT(editConnections()));
	menu->insertSeparator();
	m_notifications->plug(menu);
	m_autostart->plug(menu);
	m_helpMenu = new KHelpMenu(this, TDEGlobal::instance()->aboutData(), false);
	menu->insertItem(SmallIcon("help"), i18n("&Help"), m_helpMenu->menu());

	connect(nm, TQ_SIGNAL(changed()), TQ_SLOT(updateState()));
	connect(nm, TQ_SIGNAL(availabilityChanged(bool)), TQ_SLOT(slotAvailability(bool)));
	connect(nm, TQ_SIGNAL(deviceStateChanged(const TQString&, uint, uint, uint)),
	        TQ_SLOT(slotDeviceStateChanged(const TQString&, uint, uint, uint)));
	connect(nm, TQ_SIGNAL(operationFinished(bool, const TQString&, const TQString&)),
	        TQ_SLOT(slotOperationFinished(bool, const TQString&, const TQString&)));

	updateState();
	m_started = true;
	TQTimer::singleShot(3000, this, TQ_SLOT(checkTdeNetworkManager()));
}

NmTray::~NmTray()
{
	delete m_popup;
}

void NmTray::updateState()
{
	NMClient *nm = NMClient::self();
	const NMActiveConnection *primary = nm->activeConnection(nm->primaryConnection());
	const NMDeviceInfo *pdev = primary && !primary->devices.isEmpty() ? nm->device(primary->devices.first()) : 0;
	uint strength = pdev && pdev->activeAP() ? pdev->activeAP()->strength : 0;
	bool vpn = false;
	for (NMActiveList::const_iterator it = nm->activeConnections().begin(); it != nm->activeConnections().end(); ++it)
		if (((*it).vpn || (*it).type == "wireguard") && (*it).state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
			vpn = true;

	TQString icon = nm->isRunning() ? NMText::trayIcon(pdev, nm->state(), vpn, strength) : TQString("tde-nmctl-offline");
	if (icon != m_iconName) {
		m_iconName = icon;
		setPixmap(KSystemTray::loadIcon(icon));
	}

	/* tooltip */
	TQString tip = "<qt><b>" + i18n("Network") + "</b><br>";
	if (!nm->isRunning()) {
		tip += i18n("NetworkManager is not running");
	} else {
		tip += NMText::managerState(nm->state());
		for (NMActiveList::const_iterator it = nm->activeConnections().begin(); it != nm->activeConnections().end(); ++it) {
			if ((*it).type == "loopback")
				continue;
			const NMDeviceInfo *d = (*it).devices.isEmpty() ? 0 : nm->device((*it).devices.first());
			TQString line = TQStyleSheet::escape((*it).id);
			if (d && d->isWireless() && d->activeAP())
				line += TQString(" (%1%)").arg(d->activeAP()->strength);
			if (d)
				line += " &mdash; " + d->iface;
			if ((*it).state != NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
				line += ", " + NMText::activeState((*it).state);
			tip += "<br>" + line;
		}
	}
	tip += "</qt>";
	TQToolTip::remove(this);
	TQToolTip::add(this, tip);

	checkVpnChanges();
}

void NmTray::checkVpnChanges()
{
	NMClient *nm = NMClient::self();
	TQMap<TQString, uint> states;
	for (NMActiveList::const_iterator it = nm->activeConnections().begin(); it != nm->activeConnections().end(); ++it) {
		if (!(*it).vpn && (*it).type != "wireguard")
			continue;
		states[(*it).path] = (*it).state;
		m_vpnNames[(*it).path] = (*it).id;
		if (m_started && (*it).state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED &&
		    m_vpnStates[(*it).path] != NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
			notify("vpn-connected", i18n("VPN connection \"%1\" established.").arg((*it).id));
	}
	for (TQMap<TQString, uint>::const_iterator it = m_vpnStates.begin(); it != m_vpnStates.end(); ++it)
		if (!states.contains(it.key()) && it.data() == NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
			notify("vpn-disconnected", i18n("VPN connection \"%1\" disconnected.").arg(m_vpnNames[it.key()]));
	m_vpnStates = states;
}

void NmTray::notify(const TQString &event, const TQString &text)
{
	if (!NmctlSettings::notifications())
		return;
	KNotifyClient::event(winId(), event, text);
}

void NmTray::slotDeviceStateChanged(const TQString &path, uint newState, uint oldState, uint reason)
{
	NMClient *nm = NMClient::self();
	const NMDeviceInfo *d = nm->device(path);
	TQString iface = d ? d->iface : path.section('/', -1);
	if (d && d->type == NM_DEVICE_TYPE_LOOPBACK)
		return;

	if (newState == NM_DEVICE_STATE_ACTIVATED) {
		/* the device's connection is known after the next refresh */
		nm->refresh();
		d = nm->device(path);
		const NMActiveConnection *ac = d ? nm->activeConnection(d->activeConnection) : 0;
		TQString name = ac ? ac->id : iface;
		if (d && d->isWireless() && d->activeAP())
			notify("connected", i18n("Connected to the Wi-Fi network \"%1\".").arg(d->activeAP()->ssidText()));
		else
			notify("connected", i18n("Connection \"%1\" established on %2.").arg(name).arg(iface));
	} else if (newState == NM_DEVICE_STATE_FAILED) {
		notify("failed", i18n("The connection on %1 failed: %2").arg(iface).arg(NMText::stateReason(reason)));
	} else if (oldState == NM_DEVICE_STATE_ACTIVATED && newState <= NM_DEVICE_STATE_DISCONNECTED &&
	           newState != NM_DEVICE_STATE_UNKNOWN) {
		TQString why = NMText::stateReason(reason);
		notify("disconnected", why.isEmpty() ? i18n("%1 is disconnected.").arg(iface)
		                                     : i18n("%1 is disconnected: %2").arg(iface).arg(why));
	}
}

void NmTray::slotOperationFinished(bool ok, const TQString &what, const TQString &error)
{
	if (!ok)
		notify("failed", i18n("%1 failed: %2").arg(what).arg(error));
}

void NmTray::slotAvailability(bool running)
{
	notify(running ? "connected" : "disconnected",
	       running ? i18n("NetworkManager is running.") : i18n("NetworkManager has stopped."));
}

void NmTray::mousePressEvent(TQMouseEvent *e)
{
	if (e->button() == TQt::LeftButton) {
		if (m_popup->isVisible())
			m_popup->hide();
		else
			showPopup();
		return;
	}
	KSystemTray::mousePressEvent(e);
}

void NmTray::showPopup()
{
	TQPoint p = mapToGlobal(TQPoint(0, 0));
	m_popup->popupNear(TQRect(p, size()));
}

void NmTray::contextMenuAboutToShow(TDEPopupMenu *)
{
	NMClient *nm = NMClient::self();
	m_networking->setChecked(nm->networkingEnabled());
	m_networking->setEnabled(nm->isRunning());
	m_wifi->setChecked(nm->wirelessEnabled());
	m_wifi->setEnabled(nm->isRunning() && nm->networkingEnabled() && nm->wirelessHardwareEnabled());
	m_wifi->setEnabled(nm->hasWirelessDevice() && m_wifi->isEnabled());
	m_wwan->setChecked(nm->wwanEnabled());
	m_wwan->setEnabled(nm->isRunning() && nm->hasModemDevice() && nm->wwanHardwareEnabled());
	m_notifications->setChecked(NmctlSettings::notifications());
	m_autostart->setChecked(NmctlSettings::trayAutostart());
}

void NmTray::slotNetworking()
{
	NMClient::self()->setNetworkingEnabled(m_networking->isChecked());
}

void NmTray::slotWifi()
{
	NMClient::self()->setWirelessEnabled(m_wifi->isChecked());
}

void NmTray::slotWwan()
{
	NMClient::self()->setWwanEnabled(m_wwan->isChecked());
}

void NmTray::slotAutostart()
{
	NmctlSettings::setTrayAutostart(m_autostart->isChecked());
}

void NmTray::slotNotifications()
{
	NmctlSettings::setNotifications(m_notifications->isChecked());
}

void NmTray::slotHelp()
{
	kapp->invokeHelp(TQString::null, "tde-nmctl");
}

void NmTray::editConnections()
{
	KRun::runCommand("tde-nmctl", "tde-nmctl", "tde-nmctl");
}

void NmTray::showInfo(const TQString &activePath)
{
	if (!m_info)
		m_info = new InfoDialog(0);
	if (!activePath.isEmpty())
		m_info->showConnection(activePath);
	m_info->show();
	m_info->raise();
}

void NmTray::connectHidden()
{
	Connector::connectHidden(0);
}

void NmTray::checkTdeNetworkManager()
{
	/* ask once whether TDE's own network applet should be turned off */
	if (NmctlSettings::tdeNetworkManagerChecked() || !NmctlSettings::tdeNetworkManagerAutostarts())
		return;
	NmctlSettings::setTdeNetworkManagerChecked();
	int r = KMessageBox::questionYesNo(0,
		i18n("<p>TDENetworkManager, TDE's older network applet, also starts at login and shows "
		     "its own icon in the system tray.</p>"
		     "<p>Turn it off, so that only this network icon is shown? It stays installed and "
		     "can be started again from the menu.</p>"),
		i18n("Network"), KGuiItem(i18n("Turn TDENetworkManager Off")), KGuiItem(i18n("Keep It")));
	if (r == KMessageBox::Yes)
		NmctlSettings::disableTdeNetworkManager();
}

#include "nmtray.moc"
