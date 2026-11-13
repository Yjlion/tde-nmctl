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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "nmtray.h"
#include "nmclient.h"

#include <tdeaboutdata.h>
#include <tdecmdlineargs.h>
#include <tdelocale.h>
#include <kuniqueapplication.h>

static const char description[] = I18N_NOOP("Network status and connections in the system tray, for NetworkManager");

static TDECmdLineOptions options[] = {
	{ "popup", I18N_NOOP("Open the network popup after starting"), 0 },
	TDECmdLineLastOption
};

int main(int argc, char **argv)
{
	TDEAboutData about("tde-nmctl-tray", I18N_NOOP("Network"), TDE_NMCTL_VERSION, description,
	                   TDEAboutData::License_GPL, "(C) 2026 tde-nmctl developers",
	                   I18N_NOOP("This program was written with the help of an AI model (\"vibe coded\"). "
	                             "Review it before trusting it with your network."),
	                   "https://github.com/Yjlion/tde-nmctl");
	about.addAuthor("tde-nmctl developers", 0, 0, "https://github.com/Yjlion/tde-nmctl");

	TDECmdLineArgs::init(argc, argv, &about);
	TDECmdLineArgs::addCmdLineOptions(options);
	KUniqueApplication::addCmdLineOptions();

	if (!KUniqueApplication::start())
		return 0;

	KUniqueApplication app;
	app.disableSessionManagement();
	NMClient::self()->refresh();

	NmTray *tray = new NmTray();
	app.setMainWidget(tray);
	tray->show();
	TDECmdLineArgs *args = TDECmdLineArgs::parsedArgs();
	if (args->isSet("popup"))
		tray->showPopup();
	args->clear();

	return app.exec();
}
