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

#ifndef NMCTLSETTINGS_H
#define NMCTLSETTINGS_H

/**
 * The user's settings: tde-nmctlrc, and tde-nmctl-trayrc for the tray's
 * autostart condition.
 */
class NmctlSettings
{
public:
	/** new passwords go to TDE Wallet (agent-owned) instead of the system */
	static bool storeInWallet();
	static void setStoreInWallet(bool on);

	/** notify when connections are established or lost */
	static bool notifications();
	static void setNotifications(bool on);

	/** show Wi-Fi networks without a saved connection in the tray popup */
	static bool showAllNetworks();
	static void setShowAllNetworks(bool on);

	/** start the tray applet at login */
	static bool trayAutostart();
	static void setTrayAutostart(bool on);

	/** TDE's own tdenetworkmanager is installed and starts at login */
	static bool tdeNetworkManagerAutostarts();
	/** turns tdenetworkmanager's autostart off and quits it */
	static void disableTdeNetworkManager();
	static bool tdeNetworkManagerChecked();
	static void setTdeNetworkManagerChecked();
};

#endif
