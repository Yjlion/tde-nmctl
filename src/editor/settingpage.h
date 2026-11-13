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

#ifndef SETTINGPAGE_H
#define SETTINGPAGE_H

#include <tqwidget.h>

#include "nmdbus.h"

class TQComboBox;
class TQCheckBox;
class TQGridLayout;
class TQLabel;
class KLineEdit;

/**
 * One tab of the connection editor. load() fills the widgets from the
 * connection's settings, save() writes them back; keys a page does not
 * know about stay as they are.
 */
class SettingPage : public TQWidget
{
	TQ_OBJECT
public:
	SettingPage(TQWidget *parent = 0, const char *name = 0);

	virtual void load(const ConnectionSettings &conn) = 0;
	virtual void save(ConnectionSettings &conn) = 0;
	/** an error message, or an empty string if the input is valid */
	virtual TQString validate() const { return TQString::null; }

	/* helpers for subclasses */
	static void setComboData(TQComboBox *combo, const TQStringList &values, const TQString &value);
	static TQString comboData(TQComboBox *combo, const TQStringList &values);
	/** a combo box with the interfaces of devices of a type (editable) */
	static void fillInterfaces(TQComboBox *combo, uint deviceType, bool withAny = true);
	static TQString interfaceText(TQComboBox *combo);
	/** a combo box with MAC address choices (cloned-mac-address) */
	static void fillMacChoices(TQComboBox *combo);
	static void setMacChoice(TQComboBox *combo, const TQString &value);
	static TQString macChoice(TQComboBox *combo);
	static bool validMac(const TQString &mac);

signals:
	void changed();

protected:
	/** a label + widget row in a two column grid */
	TQLabel *addRow(TQGridLayout *grid, int row, const TQString &label, TQWidget *widget);
};

/**
 * A password with the choice where NetworkManager keeps it, like the
 * password fields of nm-connection-editor and plasma-nm.
 */
class SecretEdit : public TQWidget
{
	TQ_OBJECT
public:
	enum Storage { Wallet = 0, System = 1, Ask = 2, NotRequired = 3 };

	SecretEdit(TQWidget *parent = 0, const char *name = 0, bool allowNotRequired = false);

	void setSecret(const TQString &value, uint nmFlags);
	TQString value() const;
	uint flags() const;
	/** sets setting.key and setting.key-flags */
	void load(const ConnectionSettings &conn, const TQString &setting, const TQString &key);
	void save(ConnectionSettings &conn, const TQString &setting, const TQString &key) const;
	/** for vpn: vpn.secrets[key] and vpn.data[key-flags] */
	void loadVpn(const ConnectionSettings &conn, const TQString &key);
	void saveVpn(ConnectionSettings &conn, const TQString &key) const;

	KLineEdit *lineEdit() const { return m_edit; }

	/** the storage for new connections: wallet (default) or all users */
	static uint defaultFlags();
	/** NetworkManager leaves out *-flags that are 0, so a missing value
	 *  means "all users" for an existing connection, but the user's
	 *  default for a new one; the connection editor sets this */
	static void setEditingNewConnection(bool isNew);
	/** the flags to assume when a connection has no *-flags value */
	static uint missingFlags();

signals:
	void changed();

private slots:
	void slotStorage(int index);
	void slotShow(bool on);

private:
	KLineEdit *m_edit;
	TQComboBox *m_storage;
	TQCheckBox *m_show;
	bool m_allowNotRequired;
};

#endif
