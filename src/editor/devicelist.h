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

#ifndef DEVICELIST_H
#define DEVICELIST_H

#include <tqwidget.h>

#include "nmclient.h"

class TQCheckBox;
class TQPushButton;
class TQSplitter;
class TQTimer;
class TDEListView;
class KTextBrowser;

/** HTML descriptions of devices and their configuration */
namespace DeviceInfo
{
	TQString html(const NMDeviceInfo &dev, bool withNetworks);
	TQString accessPointsHtml(const NMDeviceInfo &dev);
}

/**
 * Network devices with their state and details: driver, addresses, DNS,
 * speed and, for Wi-Fi devices, the networks in range.
 */
class DeviceList : public TQWidget
{
	TQ_OBJECT
public:
	DeviceList(TQWidget *parent = 0, const char *name = 0);

public slots:
	void reload();

private slots:
	void slotSelectionChanged();
	void slotDisconnect();
	void slotManaged(bool on);
	void slotAutoconnect(bool on);
	void slotRescan();
	void slotStatistics();

private:
	TQString selectedPath() const;
	void showDetails();

	TDEListView *m_list;
	KTextBrowser *m_details;
	TQPushButton *m_disconnect;
	TQPushButton *m_rescan;
	TQCheckBox *m_managed;
	TQCheckBox *m_autoconnect;
	TQTimer *m_statsTimer;
	TQString m_statsPath;
	TQ_UINT64 m_lastRx, m_lastTx;
	TQString m_rates;
};

#endif
