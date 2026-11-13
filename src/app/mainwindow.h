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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <tdemainwindow.h>

class MainWidget;
class KPushButton;

/** The standalone connection editor window. */
class MainWindow : public TDEMainWindow
{
	TQ_OBJECT
public:
	MainWindow();
	MainWidget *mainWidget() const { return m_main; }

private slots:
	void slotControlCenter();
	void slotApply();
	void slotSettingsChanged();
	void slotNewConnection();
	void slotReload();

private:
	MainWidget *m_main;
	KPushButton *m_apply;
};

#endif
