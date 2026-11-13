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

#include "settingpage.h"
#include "nmclient.h"
#include "secretstore.h"
#include "nmctlsettings.h"

#include <tqcheckbox.h>
#include <tqcombobox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqregexp.h>

#include <klineedit.h>
#include <tdelocale.h>

SettingPage::SettingPage(TQWidget *parent, const char *name)
	: TQWidget(parent, name)
{
}

TQLabel *SettingPage::addRow(TQGridLayout *grid, int row, const TQString &label, TQWidget *widget)
{
	TQLabel *l = new TQLabel(label, this);
	l->setBuddy(widget);
	grid->addWidget(l, row, 0);
	grid->addWidget(widget, row, 1);
	return l;
}

void SettingPage::setComboData(TQComboBox *combo, const TQStringList &values, const TQString &value)
{
	int i = values.findIndex(value);
	combo->setCurrentItem(i < 0 ? 0 : i);
}

TQString SettingPage::comboData(TQComboBox *combo, const TQStringList &values)
{
	int i = combo->currentItem();
	if (i < 0 || i >= (int)values.count())
		return TQString::null;
	return values[i];
}

void SettingPage::fillInterfaces(TQComboBox *combo, uint deviceType, bool withAny)
{
	combo->clear();
	combo->setEditable(true);
	if (withAny)
		combo->insertItem(i18n("(any)"));
	const NMDeviceList &devs = NMClient::self()->devices();
	for (NMDeviceList::const_iterator it = devs.begin(); it != devs.end(); ++it)
		if (deviceType == 0 || (*it).type == deviceType)
			combo->insertItem((*it).iface);
}

TQString SettingPage::interfaceText(TQComboBox *combo)
{
	TQString t = combo->currentText().stripWhiteSpace();
	if (t == i18n("(any)"))
		return TQString::null;
	return t;
}

void SettingPage::fillMacChoices(TQComboBox *combo)
{
	combo->clear();
	combo->setEditable(true);
	combo->insertItem(i18n("(default)"));
	combo->insertItem(i18n("Preserve"));
	combo->insertItem(i18n("Permanent"));
	combo->insertItem(i18n("Random"));
	combo->insertItem(i18n("Stable"));
}

static const char *s_macKeys[] = { "", "preserve", "permanent", "random", "stable", 0 };

void SettingPage::setMacChoice(TQComboBox *combo, const TQString &value)
{
	for (int i = 0; s_macKeys[i]; ++i) {
		if (value == s_macKeys[i]) {
			combo->setCurrentItem(i);
			return;
		}
	}
	combo->setEditText(value);
}

TQString SettingPage::macChoice(TQComboBox *combo)
{
	TQString t = combo->currentText().stripWhiteSpace();
	for (int i = 0; s_macKeys[i]; ++i)
		if (t == combo->text(i))
			return s_macKeys[i];
	return t;
}

bool SettingPage::validMac(const TQString &mac)
{
	return TQRegExp("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$").search(mac) == 0;
}

/* ---------------------------------------------------------------------- */

SecretEdit::SecretEdit(TQWidget *parent, const char *name, bool allowNotRequired)
	: TQWidget(parent, name), m_allowNotRequired(allowNotRequired)
{
	TQGridLayout *l = new TQGridLayout(this, 2, 2, 0, 4);
	m_edit = new KLineEdit(this);
	m_edit->setEchoMode(TQLineEdit::Password);
	l->addWidget(m_edit, 0, 0);

	m_storage = new TQComboBox(false, this);
	m_storage->insertItem(i18n("Store in TDE Wallet (this user)"));
	m_storage->insertItem(i18n("Store for all users (unencrypted)"));
	m_storage->insertItem(i18n("Ask every time"));
	if (allowNotRequired)
		m_storage->insertItem(i18n("Not required"));
	l->addWidget(m_storage, 0, 1);

	m_show = new TQCheckBox(i18n("Show password"), this);
	l->addWidget(m_show, 1, 0);
	l->setColStretch(0, 1);

	connect(m_edit, TQ_SIGNAL(textChanged(const TQString&)), TQ_SIGNAL(changed()));
	connect(m_storage, TQ_SIGNAL(activated(int)), TQ_SLOT(slotStorage(int)));
	connect(m_show, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotShow(bool)));
	setSecret(TQString::null, defaultFlags());
}

uint SecretEdit::defaultFlags()
{
	return NmctlSettings::storeInWallet() ? NM_SECRET_FLAG_AGENT_OWNED : NM_SECRET_FLAG_NONE;
}

void SecretEdit::setSecret(const TQString &value, uint f)
{
	m_edit->setText(value);
	int idx;
	if (f & NM_SECRET_FLAG_NOT_REQUIRED)
		idx = m_allowNotRequired ? NotRequired : Ask;
	else if (f & NM_SECRET_FLAG_NOT_SAVED)
		idx = Ask;
	else if (f & NM_SECRET_FLAG_AGENT_OWNED)
		idx = Wallet;
	else
		idx = System;
	m_storage->setCurrentItem(idx);
	slotStorage(idx);
}

TQString SecretEdit::value() const
{
	int idx = m_storage->currentItem();
	if (idx == Ask || idx == NotRequired)
		return TQString::null;
	return m_edit->text();
}

uint SecretEdit::flags() const
{
	switch (m_storage->currentItem()) {
	case Wallet:      return NM_SECRET_FLAG_AGENT_OWNED;
	case Ask:         return NM_SECRET_FLAG_NOT_SAVED;
	case NotRequired: return NM_SECRET_FLAG_NOT_REQUIRED;
	default:          return NM_SECRET_FLAG_NONE;
	}
}

void SecretEdit::slotStorage(int idx)
{
	m_edit->setEnabled(idx == Wallet || idx == System);
	m_show->setEnabled(idx == Wallet || idx == System);
	emit changed();
}

void SecretEdit::slotShow(bool on)
{
	m_edit->setEchoMode(on ? TQLineEdit::Normal : TQLineEdit::Password);
}

void SecretEdit::load(const ConnectionSettings &conn, const TQString &setting, const TQString &key)
{
	uint f = conn.has(setting, key + "-flags") ? conn.uint(setting, key + "-flags") : defaultFlags();
	setSecret(conn.str(setting, key), f);
}

void SecretEdit::save(ConnectionSettings &conn, const TQString &setting, const TQString &key) const
{
	conn.setUInt(setting, key + "-flags", flags());
	conn.setStr(setting, key, value());
}

void SecretEdit::loadVpn(const ConnectionSettings &conn, const TQString &key)
{
	TQMap<TQString, TQString> data = conn.strMap("vpn", "data");
	uint f = data.contains(key + "-flags") ? data[key + "-flags"].toUInt() : defaultFlags();
	setSecret(conn.strMap("vpn", "secrets")[key], f);
}

void SecretEdit::saveVpn(ConnectionSettings &conn, const TQString &key) const
{
	TQMap<TQString, TQString> data = conn.strMap("vpn", "data");
	data[key + "-flags"] = TQString::number(flags());
	conn.setStrMap("vpn", "data", data);
	SecretStore::setSecretValue(conn, "vpn", key, value());
}

#include "settingpage.moc"
