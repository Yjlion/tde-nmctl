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

#ifndef TRAYPOPUP_H
#define TRAYPOPUP_H

#include <tqframe.h>
#include <tqptrlist.h>

class TQCheckBox;
class TQLabel;
class TQPushButton;
class TQScrollView;
class TQVBox;
class TQVBoxLayout;
class TQTimer;

/** One line of the popup: icon, name, details and a button. */
class NetworkRow : public TQFrame
{
	TQ_OBJECT
public:
	enum Kind { Active, AccessPoint, Profile };

	NetworkRow(Kind kind, TQWidget *parent);

	void set(const TQString &icon, const TQString &title, const TQString &detail,
	         const TQString &button, bool secure = false);
	TQPushButton *infoButton() const { return m_info; }

	Kind kind;
	TQString path;        /* active connection, access point or profile */
	TQString device;
	TQByteArray ssid;

signals:
	void clicked(NetworkRow *row);
	void infoClicked(NetworkRow *row);

protected:
	virtual void enterEvent(TQEvent *e);
	virtual void leaveEvent(TQEvent *e);
	virtual void mouseDoubleClickEvent(TQMouseEvent *e);

private slots:
	void slotClicked();
	void slotInfo();

private:
	TQLabel *m_icon;
	TQLabel *m_lock;
	TQLabel *m_title;
	TQLabel *m_detail;
	TQPushButton *m_button;
	TQPushButton *m_info;
};

/**
 * The popup of the tray icon, like nm-applet's menu and plasma-nm's
 * applet: active connections, Wi-Fi networks in range, VPN and other
 * connections, and switches for networking, Wi-Fi and mobile broadband.
 */
class TrayPopup : public TQFrame
{
	TQ_OBJECT
public:
	TrayPopup(TQWidget *parent = 0, const char *name = 0);

	void popupNear(const TQRect &anchor);

public slots:
	void sync();

signals:
	void editConnections();
	void showInfo(const TQString &activePath);
	void connectHidden();

protected:
	virtual void showEvent(TQShowEvent *e);
	virtual void hideEvent(TQHideEvent *e);

private slots:
	void slotRow(NetworkRow *row);
	void slotInfo(NetworkRow *row);
	void slotNetworking(bool on);
	void slotWifi(bool on);
	void slotWwan(bool on);
	void slotRescan();
	void slotEdit();
	void slotHidden();
	void slotChanged();

private:
	void clearRows();
	NetworkRow *addRow(NetworkRow::Kind kind, TQWidget *parent, TQVBoxLayout *layout);

	TQLabel *m_stateIcon;
	TQLabel *m_state;
	TQLabel *m_connectivity;
	TQCheckBox *m_networking;
	TQCheckBox *m_wifi;
	TQCheckBox *m_wwan;
	TQLabel *m_activeLabel;
	TQWidget *m_activeBox;
	TQVBoxLayout *m_activeLayout;
	TQLabel *m_wifiLabel;
	TQPushButton *m_rescan;
	TQScrollView *m_wifiScroll;
	TQWidget *m_wifiBox;
	TQVBoxLayout *m_wifiLayout;
	TQPushButton *m_hidden;
	TQLabel *m_otherLabel;
	TQWidget *m_otherBox;
	TQVBoxLayout *m_otherLayout;
	TQPtrList<NetworkRow> m_rows;
	TQTimer *m_syncTimer;
	TQTimer *m_scanTimer;
};

#endif
