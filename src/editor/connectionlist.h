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

#ifndef CONNECTIONLIST_H
#define CONNECTIONLIST_H

#include <tqwidget.h>

class TQLabel;
class TQListViewItem;
class TQPushButton;
class TDEListView;
class TDEPopupMenu;

/**
 * The list of connection profiles grouped by type, with buttons to add,
 * edit, delete, duplicate, import, export and (dis)connect them, like
 * nm-connection-editor's main window.
 */
class ConnectionList : public TQWidget
{
	TQ_OBJECT
public:
	ConnectionList(TQWidget *parent = 0, const char *name = 0);

	/** selects a connection by uuid or name; false if not found */
	bool select(const TQString &uuidOrId);
	TQString selectedPath() const;

public slots:
	void reload();
	void updateStates();
	void addConnection(const TQString &type);
	void editSelected();
	void deleteSelected();
	void duplicateSelected();
	void toggleSelected();
	void importVpn();
	void exportSelected();

private slots:
	void slotSelectionChanged();
	void slotAddMenu(int id);
	void slotContextMenu(TQListViewItem *item, const TQPoint &pos, int col);
	void slotOperationFinished(bool ok, const TQString &what, const TQString &error);

private:
	void updateAuthorization();

	TDEListView *m_list;
	TQLabel *m_authIcon;
	TQLabel *m_authText;
	TQPushButton *m_add;
	TQPushButton *m_edit;
	TQPushButton *m_delete;
	TQPushButton *m_duplicate;
	TQPushButton *m_connect;
	TQPushButton *m_import;
	TQPushButton *m_export;
	TDEPopupMenu *m_addMenu;
	TQStringList m_addTypes;
	TQString m_pendingSelect;
};

#endif
