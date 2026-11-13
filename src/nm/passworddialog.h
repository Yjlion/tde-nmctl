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

#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <kdialogbase.h>

#include <tqmap.h>
#include <tqstringlist.h>

#include "secretstore.h"

class KLineEdit;
class TQCheckBox;

/** Asks for the secrets of a connection: Wi-Fi password, VPN password, PIN. */
class PasswordDialog : public KDialogBase
{
	TQ_OBJECT
public:
	PasswordDialog(const ConnectionSettings &conn, const TQString &setting,
	               const TQStringList &keys, const SecretMap &current,
	               bool walletStorage, TQWidget *parent = 0, const char *name = 0);

	SecretMap secrets() const;
	/** "remember in TDE Wallet" was ticked */
	bool remember() const;
	TQString setting() const { return m_setting; }

protected slots:
	virtual void slotOk();

private slots:
	void slotShow(bool on);
	void slotTextChanged();

private:
	TQString m_setting;
	TQStringList m_keys;
	TQMap<TQString, KLineEdit *> m_edits;
	TQCheckBox *m_show;
	TQCheckBox *m_remember;
	ConnectionSettings m_conn;
};

#endif
