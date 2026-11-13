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

#include "nmctlsettings.h"

#include <dcopclient.h>
#include <tdeapplication.h>
#include <tdeconfig.h>
#include <tdeglobal.h>
#include <kstandarddirs.h>

static bool readBool(const char *file, const char *key, bool def)
{
	TDEConfig config(file, true, false);
	config.setGroup("General");
	return config.readBoolEntry(key, def);
}

static void writeBool(const char *file, const char *key, bool value)
{
	TDEConfig config(file, false, false);
	config.setGroup("General");
	config.writeEntry(key, value);
	config.sync();
}

bool NmctlSettings::storeInWallet()       { return readBool("tde-nmctlrc", "StoreInWallet", true); }
void NmctlSettings::setStoreInWallet(bool on) { writeBool("tde-nmctlrc", "StoreInWallet", on); }
bool NmctlSettings::notifications()       { return readBool("tde-nmctlrc", "Notifications", true); }
void NmctlSettings::setNotifications(bool on) { writeBool("tde-nmctlrc", "Notifications", on); }
bool NmctlSettings::showAllNetworks()     { return readBool("tde-nmctlrc", "ShowAllNetworks", true); }
void NmctlSettings::setShowAllNetworks(bool on) { writeBool("tde-nmctlrc", "ShowAllNetworks", on); }
bool NmctlSettings::trayAutostart()       { return readBool("tde-nmctl-trayrc", "Autostart", true); }
void NmctlSettings::setTrayAutostart(bool on) { writeBool("tde-nmctl-trayrc", "Autostart", on); }
bool NmctlSettings::tdeNetworkManagerChecked() { return readBool("tde-nmctl-trayrc", "TdeNetworkManagerChecked", false); }
void NmctlSettings::setTdeNetworkManagerChecked() { writeBool("tde-nmctl-trayrc", "TdeNetworkManagerChecked", true); }

bool NmctlSettings::tdeNetworkManagerAutostarts()
{
	if (TDEStandardDirs::findExe("tdenetworkmanager").isEmpty())
		return false;
	TDEGlobal::dirs()->addResourceType("autostart", "share/autostart");
	if (locate("autostart", "tdenetworkmanager.desktop").isEmpty())
		return false;
	/* tdenetworkmanager.desktop:
	 * X-TDE-autostart-condition=tdenetworkmanagerrc:General:Autostart:true */
	return readBool("tdenetworkmanagerrc", "Autostart", true);
}

void NmctlSettings::disableTdeNetworkManager()
{
	writeBool("tdenetworkmanagerrc", "Autostart", false);
	DCOPClient *dcop = kapp ? kapp->dcopClient() : 0;
	if (dcop && dcop->isApplicationRegistered("tdenetworkmanager"))
		dcop->send("tdenetworkmanager", "MainApplication-Interface", "quit()", TQByteArray());
}
