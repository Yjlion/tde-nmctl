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

#include "mainwindow.h"
#include "mainwidget.h"
#include "connectionlist.h"
#include "connectiondialog.h"
#include "nmclient.h"
#include "nmtext.h"

#include <stdio.h>

#include <tqapplication.h>

#include <tdeaboutdata.h>
#include <tdeapplication.h>
#include <tdecmdlineargs.h>
#include <tdeinstance.h>
#include <tdelocale.h>

static const char description[] = I18N_NOOP("Network connection editor for NetworkManager");

static TDECmdLineOptions options[] = {
	{ "page <page>", I18N_NOOP("Initial page: connections, devices or settings"), "connections" },
	{ "edit <connection>", I18N_NOOP("Edit the connection with this name or UUID and exit"), 0 },
	{ "new <type>", I18N_NOOP("Create a new connection of this type (ethernet, wifi, vpn, wireguard, "
	                          "bond, bridge, vlan, team, gsm, cdma, pppoe, infiniband, dummy) and exit"), 0 },
	{ "tab <number>", I18N_NOOP("With --edit or --new: the tab of the editor to show first"), "0" },
	{ "select <connection>", I18N_NOOP("Select this connection in the list"), 0 },
	{ "list", I18N_NOOP("Print the devices, Wi-Fi networks and connections and exit"), 0 },
	{ "show <connection>", I18N_NOOP("Print the settings of a connection and exit"), 0 },
	TDECmdLineLastOption
};

static const char *s_typeAliases[][2] = {
	{ "ethernet", "802-3-ethernet" }, { "wired", "802-3-ethernet" },
	{ "wifi", "802-11-wireless" }, { "wireless", "802-11-wireless" },
	{ "dsl", "pppoe" }, { "mobile", "gsm" }, { 0, 0 }
};

static TQString typeFromArg(const TQString &arg)
{
	for (int i = 0; s_typeAliases[i][0]; ++i)
		if (arg == s_typeAliases[i][0])
			return s_typeAliases[i][1];
	return arg;
}

static const NMConnection *findConnection(const TQString &key)
{
	NMClient *nm = NMClient::self();
	const NMConnection *c = nm->connectionByUuid(key);
	if (c)
		return c;
	const NMConnectionList &all = nm->connections();
	for (NMConnectionList::const_iterator it = all.begin(); it != all.end(); ++it)
		if ((*it).id() == key)
			return &(*it);
	return 0;
}

static void out(const TQString &s)
{
	fputs(s.local8Bit().data(), stdout);
	fputc('\n', stdout);
}

static int runList()
{
	NMClient *nm = NMClient::self();
	if (!nm->isRunning()) {
		fprintf(stderr, "tde-nmctl: NetworkManager is not running\n");
		return 1;
	}
	out(TQString("NetworkManager %1: %2, connectivity: %3")
	    .arg(nm->version()).arg(NMText::managerState(nm->state())).arg(NMText::connectivity(nm->connectivity())));
	out(TQString("networking: %1, wifi: %2, wwan: %3")
	    .arg(nm->networkingEnabled() ? "on" : "off").arg(nm->wirelessEnabled() ? "on" : "off")
	    .arg(nm->wwanEnabled() ? "on" : "off"));
	out("\nDevices:");
	const NMDeviceList &devs = nm->devices();
	for (NMDeviceList::const_iterator it = devs.begin(); it != devs.end(); ++it) {
		const NMActiveConnection *ac = nm->activeConnection((*it).activeConnection);
		out(TQString("  %1\t%2\t%3\t%4").arg((*it).iface).arg(NMText::deviceType((*it).type))
		    .arg(NMText::deviceState((*it).state)).arg(ac ? ac->id : TQString("-")));
		for (NMAccessPointList::const_iterator a = (*it).accessPoints.begin(); a != (*it).accessPoints.end(); ++a)
			out(TQString("    %1 %2\t%3%\t%4\t%5").arg((*a).path == (*it).activeAccessPoint ? "*" : " ")
			    .arg((*a).ssidText()).arg((*a).strength)
			    .arg((*a).securityText().isEmpty() ? TQString("open") : (*a).securityText()).arg((*a).bssid));
	}
	out("\nConnections:");
	const NMConnectionList &all = nm->connections();
	for (NMConnectionList::const_iterator it = all.begin(); it != all.end(); ++it) {
		const NMActiveConnection *ac = nm->activeForConnection((*it).path);
		out(TQString("  %1\t%2\t%3\t%4").arg((*it).id()).arg((*it).uuid()).arg((*it).type())
		    .arg(ac ? NMText::activeState(ac->state) : TQString("-")));
	}
	return 0;
}

static int runShow(const TQString &key)
{
	const NMConnection *c = findConnection(key);
	if (!c) {
		fprintf(stderr, "tde-nmctl: no connection %s\n", key.local8Bit().data());
		return 1;
	}
	const NMSettingsMap &m = c->settings.map();
	for (NMSettingsMap::const_iterator s = m.begin(); s != m.end(); ++s)
		for (NMVariantMap::const_iterator k = s.data().begin(); k != s.data().end(); ++k)
			out(TQString("%1.%2: %3").arg(s.key()).arg(k.key()).arg(NMDBus::dump(k.data())));
	return 0;
}

int main(int argc, char **argv)
{
	TDEAboutData about("tde-nmctl", I18N_NOOP("Network Connections"), TDE_NMCTL_VERSION, description,
	                   TDEAboutData::License_GPL, "(C) 2026 tde-nmctl developers",
	                   I18N_NOOP("This program was written with the help of an AI model (\"vibe coded\"). "
	                             "Review it before trusting it with your network."),
	                   "https://github.com/Yjlion/tde-nmctl");
	about.addAuthor("tde-nmctl developers", 0, 0, "https://github.com/Yjlion/tde-nmctl");

	TDECmdLineArgs::init(argc, argv, &about);
	TDECmdLineArgs::addCmdLineOptions(options);
	TDECmdLineArgs *args = TDECmdLineArgs::parsedArgs();

	if (args->isSet("list") || args->isSet("show")) {
		/* no X display needed */
		int qargc = 1;
		char *qargv[] = { argv[0], 0 };
		TQApplication app(qargc, qargv, false);
		TDEInstance instance(&about);
		NMClient::self()->refresh();
		if (args->isSet("list"))
			return runList();
		return runShow(TQString::fromLocal8Bit(args->getOption("show")));
	}

	TDEApplication app;
	NMClient::self()->refresh();
	int tab = TQString(args->getOption("tab")).toInt();

	if (args->isSet("edit")) {
		TQString key = TQString::fromLocal8Bit(args->getOption("edit"));
		const NMConnection *c = findConnection(key);
		if (!c) {
			fprintf(stderr, "tde-nmctl: no connection %s\n", key.local8Bit().data());
			return 1;
		}
		return ConnectionDialog::editConnection(c->path, 0, tab) ? 0 : 1;
	}
	if (args->isSet("new")) {
		TQString type = typeFromArg(TQString::fromLocal8Bit(args->getOption("new")));
		ConnectionSettings conn = ConnectionDialog::newConnection(type);
		ConnectionDialog dlg(conn, TQString::null, 0, "ConnectionDialog");
		if (tab)
			dlg.showPage(tab);
		return dlg.exec() == TQDialog::Accepted ? 0 : 1;
	}

	MainWindow *win = new MainWindow();
	win->mainWidget()->showPage(MainWidget::pageFromString(args->getOption("page")));
	if (args->isSet("select"))
		win->mainWidget()->connectionList()->select(TQString::fromLocal8Bit(args->getOption("select")));
	app.setMainWidget(win);
	win->show();
	args->clear();
	return app.exec();
}
