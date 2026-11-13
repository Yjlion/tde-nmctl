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

#include "passworddialog.h"
#include "nmclient.h"
#include "nmtext.h"

#include <tqcheckbox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqregexp.h>

#include <kiconloader.h>
#include <klineedit.h>
#include <tdelocale.h>
#include <tdemessagebox.h>

PasswordDialog::PasswordDialog(const ConnectionSettings &conn, const TQString &setting,
                               const TQStringList &keys, const SecretMap &current,
                               bool walletStorage, TQWidget *parent, const char *name)
	: KDialogBase(parent, name, false, i18n("Authentication Required"), Ok | Cancel, Ok, true),
	  m_setting(setting), m_keys(keys), m_conn(conn)
{
	TQWidget *page = new TQWidget(this);
	setMainWidget(page);

	TQGridLayout *grid = new TQGridLayout(page, 1, 3, 0, spacingHint());
	TQLabel *icon = new TQLabel(page);
	icon->setPixmap(DesktopIcon("encrypted", 48));
	icon->setAlignment(TQt::AlignTop);
	grid->addMultiCellWidget(icon, 0, 1, 0, 0);

	TQString type = conn.type();
	TQString text;
	if (type == "802-11-wireless") {
		TQString ssid = NMDBus::ssidToString(conn.bytes("802-11-wireless", "ssid"));
		text = i18n("<qt><b>The Wi-Fi network \"%1\" needs a password.</b><br>"
		            "Enter the password or key to connect.</qt>").arg(ssid);
	} else if (type == "vpn" || type == "wireguard") {
		text = i18n("<qt><b>The VPN connection \"%1\" needs a password.</b></qt>").arg(conn.id());
	} else if (type == "gsm" || type == "cdma") {
		text = i18n("<qt><b>The mobile broadband connection \"%1\" needs a password or PIN.</b></qt>")
		       .arg(conn.id());
	} else if (setting == "802-1x") {
		text = i18n("<qt><b>The network \"%1\" needs 802.1X authentication.</b><br>"
		            "Enter the password for identity \"%2\".</qt>")
		       .arg(conn.id()).arg(conn.str("802-1x", "identity"));
	} else {
		text = i18n("<qt><b>The connection \"%1\" needs a password.</b></qt>").arg(conn.id());
	}
	TQLabel *msg = new TQLabel(text, page);
	grid->addMultiCellWidget(msg, 0, 0, 1, 2);

	TQGridLayout *fields = new TQGridLayout(0, keys.count(), 2, 0, spacingHint());
	grid->addMultiCellLayout(fields, 1, 1, 1, 2);
	int row = 0;
	for (TQStringList::const_iterator it = keys.begin(); it != keys.end(); ++it, ++row) {
		TQLabel *l = new TQLabel(SecretStore::label(setting, *it), page);
		KLineEdit *e = new KLineEdit(page);
		e->setEchoMode(TQLineEdit::Password);
		e->setText(current[*it]);
		e->setMinimumWidth(260);
		l->setBuddy(e);
		fields->addWidget(l, row, 0);
		fields->addWidget(e, row, 1);
		m_edits[*it] = e;
		connect(e, TQ_SIGNAL(textChanged(const TQString&)), TQ_SLOT(slotTextChanged()));
	}

	m_show = new TQCheckBox(i18n("&Show password"), page);
	grid->addWidget(m_show, 2, 1);
	connect(m_show, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotShow(bool)));

	m_remember = new TQCheckBox(walletStorage ? i18n("&Remember in TDE Wallet")
	                                          : i18n("&Remember for this session"), page);
	m_remember->setChecked(true);
	grid->addWidget(m_remember, 3, 1);
	grid->setColStretch(2, 1);

	if (!keys.isEmpty())
		m_edits[keys.first()]->setFocus();
	slotTextChanged();
}

void PasswordDialog::slotShow(bool on)
{
	for (TQMap<TQString, KLineEdit *>::iterator it = m_edits.begin(); it != m_edits.end(); ++it)
		it.data()->setEchoMode(on ? TQLineEdit::Normal : TQLineEdit::Password);
}

void PasswordDialog::slotTextChanged()
{
	bool ok = true;
	for (TQMap<TQString, KLineEdit *>::iterator it = m_edits.begin(); it != m_edits.end(); ++it)
		if (it.data()->text().isEmpty())
			ok = false;
	enableButtonOK(ok);
}

void PasswordDialog::slotOk()
{
	if (m_setting == "802-11-wireless-security" && m_edits.contains("psk")) {
		TQString psk = m_edits["psk"]->text();
		TQString km = m_conn.str("802-11-wireless-security", "key-mgmt");
		bool hex = psk.length() == 64 && TQString(psk).replace(TQRegExp("[0-9a-fA-F]"), "").isEmpty();
		if (km == "wpa-psk" && !hex && (psk.length() < 8 || psk.length() > 63)) {
			KMessageBox::sorry(this, i18n("A WPA password must have 8 to 63 characters."));
			return;
		}
	}
	KDialogBase::slotOk();
}

SecretMap PasswordDialog::secrets() const
{
	SecretMap m;
	for (TQMap<TQString, KLineEdit *>::const_iterator it = m_edits.begin(); it != m_edits.end(); ++it)
		m[it.key()] = it.data()->text();
	return m;
}

bool PasswordDialog::remember() const
{
	return m_remember->isChecked();
}

#include "passworddialog.moc"
