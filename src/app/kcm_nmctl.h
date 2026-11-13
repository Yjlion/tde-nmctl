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

#ifndef KCM_NMCTL_H
#define KCM_NMCTL_H

#include <tdecmodule.h>

class MainWidget;

/** Control Center / System Settings module: Internet & Network > Network Connections */
class KCMNmctl : public TDECModule
{
	TQ_OBJECT
public:
	KCMNmctl(TQWidget *parent = 0, const char *name = 0, const TQStringList &args = TQStringList());
	virtual ~KCMNmctl();

	virtual void load();
	virtual void save();
	virtual void defaults();
	virtual TQString quickHelp() const;

private slots:
	void slotChanged();

private:
	MainWidget *m_main;
};

#endif
