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

#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <kdialogbase.h>

#include <tqptrlist.h>

#include "nmdbus.h"

#include "settingpage.h"
class GeneralPage;
class WifiPage;
class IpPage;

/**
 * The connection editor dialog, with one tab per group of settings, like
 * nm-connection-editor's and plasma-nm's.
 */
class ConnectionDialog : public KDialogBase
{
	TQ_OBJECT
public:
	/** path is empty for a new connection */
	ConnectionDialog(const ConnectionSettings &conn, const TQString &path,
	                 TQWidget *parent = 0, const char *name = 0);

	/** connect after saving (new connections from the tray) */
	void setActivateOnSave(const TQString &devicePath, const TQString &specificObject = "/");
	TQString savedPath() const { return m_savedPath; }
	void showPage(int index);

	/** default settings of a new connection of a type */
	static ConnectionSettings newConnection(const TQString &type);
	/** edits an existing connection, with its secrets; true if saved */
	static bool editConnection(const TQString &path, TQWidget *parent, int page = 0);
	/** asks for and saves a new connection; returns its D-Bus path */
	static TQString createConnection(const TQString &type, TQWidget *parent,
	                                 const ConnectionSettings *preset = 0);
	/** the settings of a connection including its secrets */
	static ConnectionSettings withSecrets(const TQString &path);

protected slots:
	virtual void slotOk();

private slots:
	void slotWifiMode();

private:
	void addPage(SettingPage *page, const TQString &title, const TQString &icon);

	ConnectionSettings m_conn;
	TQString m_path;
	TQString m_savedPath;
	TQString m_activateDevice;
	TQString m_activateSpecific;
	bool m_activate;
	TQPtrList<SettingPage> m_pages;
	GeneralPage *m_general;
	WifiPage *m_wifi;
	IpPage *m_ipv4;
};

#endif
