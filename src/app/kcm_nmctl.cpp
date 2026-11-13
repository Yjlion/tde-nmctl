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

#include "kcm_nmctl.h"
#include "mainwidget.h"

#include <tqlayout.h>

#include <tdeaboutdata.h>
#include <kgenericfactory.h>
#include <tdelocale.h>

typedef KGenericFactory<KCMNmctl, TQWidget> KCMNmctlFactory;
K_EXPORT_COMPONENT_FACTORY(kcm_nmctl, KCMNmctlFactory("tde-nmctl"))

KCMNmctl::KCMNmctl(TQWidget *parent, const char *name, const TQStringList &)
	: TDECModule(parent, name)
{
	TDEAboutData *about = new TDEAboutData("kcmnmctl", I18N_NOOP("Network Connections"), TDE_NMCTL_VERSION,
		I18N_NOOP("NetworkManager connection editor"), TDEAboutData::License_GPL,
		"(C) 2026 tde-nmctl developers",
		I18N_NOOP("This program was written with the help of an AI model (\"vibe coded\")."),
		"https://github.com/Yjlion/tde-nmctl");
	about->addAuthor("tde-nmctl developers", 0, 0, "https://github.com/Yjlion/tde-nmctl");
	setAboutData(about);

	/* connections are saved by their own dialogs; Apply is for the Settings tab */
	setButtons(Help | Apply | Default);
	setUseRootOnlyMsg(false);

	TQVBoxLayout *layout = new TQVBoxLayout(this, 0, 0);
	m_main = new MainWidget(this, "MainWidget");
	m_main->setMinimumSize(640, 460);
	layout->addWidget(m_main);
	connect(m_main->settings(), TQ_SIGNAL(changed()), TQ_SLOT(slotChanged()));

	load();
}

KCMNmctl::~KCMNmctl()
{
}

void KCMNmctl::load()
{
	m_main->settings()->load();
	emit changed(false);
}

void KCMNmctl::save()
{
	m_main->settings()->save();
	emit changed(false);
}

void KCMNmctl::defaults()
{
	m_main->settings()->defaults();
}

void KCMNmctl::slotChanged()
{
	emit changed(true);
}

TQString KCMNmctl::quickHelp() const
{
	return i18n("<h1>Network Connections</h1>"
	            "<p>Here you can add, edit and delete the network connections of "
	            "NetworkManager: wired, Wi-Fi, VPN, WireGuard, mobile broadband, DSL, "
	            "bonds, bridges and VLANs.</p>"
	            "<p>The <b>Devices</b> tab shows the network devices with their addresses "
	            "and the Wi-Fi networks in range.</p>"
	            "<p>The <b>Settings</b> tab turns networking, Wi-Fi and mobile broadband on "
	            "and off, sets the host name and chooses where passwords are stored. "
	            "Press <b>Apply</b> to save it.</p>"
	            "<p>Changing connections for all users may ask for the administrator "
	            "password through polkit.</p>");
}

#include "kcm_nmctl.moc"
