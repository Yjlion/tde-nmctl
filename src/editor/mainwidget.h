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

#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <tqtabwidget.h>

class TQCheckBox;
class TQLabel;
class TQPushButton;
class KLineEdit;
class ConnectionList;
class DeviceList;

/**
 * NetworkManager's global settings and tde-nmctl's own options: radio
 * switches, connectivity check, host name, password storage and tray.
 */
class GlobalSettings : public TQWidget
{
	TQ_OBJECT
public:
	GlobalSettings(TQWidget *parent = 0, const char *name = 0);

	void load();
	void save();
	void defaults();

signals:
	void changed();

private slots:
	void updateState();
	void slotNetworking(bool on);
	void slotWifi(bool on);
	void slotWwan(bool on);
	void slotConnectivityCheck(bool on);
	void slotCheckNow();

private:
	TQCheckBox *m_networking;
	TQCheckBox *m_wifi;
	TQCheckBox *m_wwan;
	TQCheckBox *m_connCheck;
	TQLabel *m_connectivity;
	TQLabel *m_version;
	KLineEdit *m_hostname;
	TQString m_savedHostname;
	TQCheckBox *m_wallet;
	TQCheckBox *m_notifications;
	TQCheckBox *m_autostart;
	TQCheckBox *m_showAll;
};

/** connections, devices and settings in tabs; the app's and the KCM's main widget */
class MainWidget : public TQTabWidget
{
	TQ_OBJECT
public:
	enum Page { Connections = 0, Devices = 1, Settings = 2 };

	MainWidget(TQWidget *parent = 0, const char *name = 0);

	ConnectionList *connectionList() const { return m_connections; }
	DeviceList *deviceList() const { return m_devices; }
	GlobalSettings *settings() const { return m_settings; }
	void showPage(Page page);
	static Page pageFromString(const TQString &name);

private:
	ConnectionList *m_connections;
	DeviceList *m_devices;
	GlobalSettings *m_settings;
};

#endif
