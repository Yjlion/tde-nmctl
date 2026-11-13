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

#include "mainwindow.h"
#include "mainwidget.h"
#include "connectionlist.h"
#include "nmclient.h"

#include <tqlayout.h>

#include <tdeaction.h>
#include <tdeapplication.h>
#include <kiconloader.h>
#include <tdelocale.h>
#include <tdemessagebox.h>
#include <kpushbutton.h>
#include <krun.h>
#include <kstdaction.h>
#include <kstdguiitem.h>

MainWindow::MainWindow()
	: TDEMainWindow(0, "NmctlMainWindow")
{
	TQWidget *central = new TQWidget(this);
	TQVBoxLayout *l = new TQVBoxLayout(central, 6, 6);
	m_main = new MainWidget(central, "MainWidget");
	l->addWidget(m_main, 1);
	TQHBoxLayout *b = new TQHBoxLayout(l, 6);
	b->addStretch(1);
	m_apply = new KPushButton(KStdGuiItem::apply(), central);
	m_apply->setEnabled(false);
	b->addWidget(m_apply);
	setCentralWidget(central);

	/* Apply is for the Settings tab; everything else applies at once */
	connect(m_apply, TQ_SIGNAL(clicked()), TQ_SLOT(slotApply()));
	connect(m_main->settings(), TQ_SIGNAL(changed()), TQ_SLOT(slotSettingsChanged()));
	connect(m_main, TQ_SIGNAL(currentChanged(TQWidget*)), TQ_SLOT(slotSettingsChanged()));

	KStdAction::quit(kapp, TQ_SLOT(quit()), actionCollection());
	KStdAction::openNew(this, TQ_SLOT(slotNewConnection()), actionCollection());
	KStdAction::redisplay(this, TQ_SLOT(slotReload()), actionCollection());
	new TDEAction(i18n("Open in &Control Center"), "kcontrol", 0, this, TQ_SLOT(slotControlCenter()),
	              actionCollection(), "open_kcontrol");

	setupGUI(Keys | Save | Create, "tde-nmctlui.rc");
	setAutoSaveSettings();
	if (!initialGeometrySet())
		resize(800, 580);
	slotSettingsChanged();
}

void MainWindow::slotSettingsChanged()
{
	m_apply->setShown(m_main->currentPageIndex() == MainWidget::Settings);
	m_apply->setEnabled(true);
}

void MainWindow::slotApply()
{
	m_main->settings()->save();
	m_apply->setEnabled(false);
}

void MainWindow::slotNewConnection()
{
	m_main->showPage(MainWidget::Connections);
	m_main->connectionList()->addConnection("802-3-ethernet");
}

void MainWindow::slotReload()
{
	NMClient::self()->reloadConnections();
	NMClient::self()->refresh();
}

void MainWindow::slotControlCenter()
{
	KRun::runCommand("tdecmshell nmctl", "tdecmshell", "tde-nmctl");
}

#include "mainwindow.moc"
