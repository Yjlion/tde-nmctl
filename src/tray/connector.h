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

#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <kdialogbase.h>

#include "nmdbus.h"

class TQComboBox;
class KLineEdit;
class SecretEdit;

/**
 * Connecting to a Wi-Fi network from the tray: uses a saved connection if
 * there is one, otherwise asks for the password and creates a connection
 * whose password is kept in TDE Wallet.
 */
namespace Connector
{
	void connectAccessPoint(const TQString &devicePath, const TQString &apPath);
	void connectHidden(TQWidget *parent = 0);
	/** a new Wi-Fi connection for an SSID with the given security */
	ConnectionSettings wifiSettings(const TQByteArray &ssid, const TQString &keyMgmt, bool hidden);
}

/** "Connect to Hidden Network": SSID, security and password */
class HiddenNetworkDialog : public KDialogBase
{
	TQ_OBJECT
public:
	HiddenNetworkDialog(TQWidget *parent = 0);

	TQString ssid() const;
	TQString keyMgmt() const;
	TQString password() const;
	TQString device() const;

private slots:
	void slotChanged();

private:
	KLineEdit *m_ssid;
	TQComboBox *m_security;
	KLineEdit *m_password;
	TQComboBox *m_device;
	TQStringList m_devices;
};

#endif
