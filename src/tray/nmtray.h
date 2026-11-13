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

#ifndef NMTRAY_H
#define NMTRAY_H

#include <ksystemtray.h>

#include <tqguardedptr.h>
#include <tqmap.h>

class TrayPopup;
class InfoDialog;
class KHelpMenu;
class SecretAgent;
class TDEToggleAction;

/**
 * Network status in kicker's system tray, like nm-applet: the icon shows
 * the primary connection and its signal strength, left click opens the
 * popup with networks, the context menu has the switches. The tray also
 * runs the secret agent that hands passwords from TDE Wallet to
 * NetworkManager.
 */
class NmTray : public KSystemTray
{
	TQ_OBJECT
public:
	NmTray(TQWidget *parent = 0, const char *name = 0);
	virtual ~NmTray();

public slots:
	void updateState();
	void editConnections();
	void showInfo(const TQString &activePath = TQString::null);
	void connectHidden();
	void showPopup();

protected:
	virtual void mousePressEvent(TQMouseEvent *e);
	virtual void contextMenuAboutToShow(TDEPopupMenu *menu);

private slots:
	void slotNetworking();
	void slotWifi();
	void slotWwan();
	void slotAutostart();
	void slotNotifications();
	void slotHelp();
	void slotDeviceStateChanged(const TQString &path, uint newState, uint oldState, uint reason);
	void slotOperationFinished(bool ok, const TQString &what, const TQString &error);
	void slotAvailability(bool running);
	void checkTdeNetworkManager();

private:
	void notify(const TQString &event, const TQString &text);
	void checkVpnChanges();

	TrayPopup *m_popup;
	TQGuardedPtr<InfoDialog> m_info;
	SecretAgent *m_agent;
	KHelpMenu *m_helpMenu;
	TDEToggleAction *m_networking;
	TDEToggleAction *m_wifi;
	TDEToggleAction *m_wwan;
	TDEToggleAction *m_autostart;
	TDEToggleAction *m_notifications;
	TQString m_iconName;
	TQMap<TQString, uint> m_vpnStates;   /* active path -> state */
	TQMap<TQString, TQString> m_vpnNames;
	bool m_started;
};

#endif
