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

#ifndef INFODIALOG_H
#define INFODIALOG_H

#include <kdialogbase.h>

#include <tqmap.h>

class KTextBrowser;
class TQTimer;

/** "Connection Information": one tab per active connection, like nm-applet's. */
class InfoDialog : public KDialogBase
{
	TQ_OBJECT
public:
	InfoDialog(TQWidget *parent = 0);
	void showConnection(const TQString &activePath);

private slots:
	void refresh();

private:
	TQString html(const TQString &activePath);

	TQMap<TQString, KTextBrowser *> m_pages;
	TQMap<TQString, int> m_pageIndex;
	TQMap<TQString, TQ_UINT64> m_lastRx;
	TQMap<TQString, TQ_UINT64> m_lastTx;
	TQTimer *m_timer;
};

#endif
