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

#include "eapwidget.h"
#include "settingpage.h"

#include <tqcheckbox.h>
#include <tqcombobox.h>
#include <tqlabel.h>
#include <tqlayout.h>

#include <klineedit.h>
#include <tdelocale.h>
#include <kurlrequester.h>

static TQStringList eapMethods()
{
	TQStringList m;
	m << "tls" << "peap" << "ttls" << "pwd" << "leap" << "fast" << "md5";
	return m;
}

static TQStringList phase2Methods()
{
	TQStringList m;
	m << "mschapv2" << "gtc" << "md5" << "pap" << "chap" << "mschap" << "tls";
	return m;
}

/* certificates are stored as "file://<path>" byte strings with a trailing NUL */
static TQString certPath(const ConnectionSettings &conn, const TQString &key)
{
	TQByteArray b = conn.bytes("802-1x", key);
	TQString s = TQString::fromUtf8(b.data(), b.size());
	while (s.endsWith(TQChar(0)))
		s.truncate(s.length() - 1);
	if (s.startsWith("file://"))
		return s.mid(7);
	return s.startsWith("pkcs11:") ? s : TQString::null;
}

static void setCertPath(ConnectionSettings &conn, const TQString &key, const TQString &path)
{
	if (path.isEmpty()) {
		conn.remove("802-1x", key);
		return;
	}
	TQString v = path.startsWith("pkcs11:") ? path : "file://" + path;
	TQCString c = v.utf8();
	TQByteArray b;
	b.duplicate(c.data(), c.length() + 1);   /* with the NUL */
	conn.setBytes("802-1x", key, b);
}

EapWidget::EapWidget(TQWidget *parent, const char *name)
	: TQWidget(parent, name)
{
	TQGridLayout *g = new TQGridLayout(this, 14, 2, 0, 6);
	int row = 0;

	m_method = new TQComboBox(false, this);
	m_method->insertItem(i18n("TLS"));
	m_method->insertItem(i18n("Protected EAP (PEAP)"));
	m_method->insertItem(i18n("Tunneled TLS (TTLS)"));
	m_method->insertItem(i18n("PWD"));
	m_method->insertItem(i18n("LEAP"));
	m_method->insertItem(i18n("FAST"));
	m_method->insertItem(i18n("MD5"));
	TQLabel *l = new TQLabel(i18n("&Authentication:"), this);
	l->setBuddy(m_method);
	g->addWidget(l, row, 0);
	g->addWidget(m_method, row++, 1);

	m_identity = new KLineEdit(this);
	l = new TQLabel(i18n("&Identity:"), this);
	l->setBuddy(m_identity);
	g->addWidget(l, row, 0);
	g->addWidget(m_identity, row++, 1);

	m_anonymous = new KLineEdit(this);
	m_anonymousLabel = new TQLabel(i18n("Anon&ymous identity:"), this);
	m_anonymousLabel->setBuddy(m_anonymous);
	g->addWidget(m_anonymousLabel, row, 0);
	g->addWidget(m_anonymous, row++, 1);

	m_domain = new KLineEdit(this);
	m_domainLabel = new TQLabel(i18n("Do&main:"), this);
	m_domainLabel->setBuddy(m_domain);
	g->addWidget(m_domainLabel, row, 0);
	g->addWidget(m_domain, row++, 1);

	m_ca = new KURLRequester(this);
	m_ca->setFilter("*.pem *.crt *.cer *.der|" + i18n("Certificates"));
	m_caLabel = new TQLabel(i18n("&CA certificate:"), this);
	m_caLabel->setBuddy(m_ca);
	g->addWidget(m_caLabel, row, 0);
	g->addWidget(m_ca, row++, 1);

	m_noCa = new TQCheckBox(i18n("No CA certificate is &required (insecure)"), this);
	g->addWidget(m_noCa, row++, 1);
	connect(m_noCa, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotNoCa(bool)));

	m_peapVersion = new TQComboBox(false, this);
	m_peapVersion->insertItem(i18n("Automatic"));
	m_peapVersion->insertItem(i18n("Version 0"));
	m_peapVersion->insertItem(i18n("Version 1"));
	m_peapLabel = new TQLabel(i18n("PEAP &version:"), this);
	m_peapLabel->setBuddy(m_peapVersion);
	g->addWidget(m_peapLabel, row, 0);
	g->addWidget(m_peapVersion, row++, 1);

	m_pacFile = new KURLRequester(this);
	m_pacLabel = new TQLabel(i18n("PAC &file:"), this);
	m_pacLabel->setBuddy(m_pacFile);
	g->addWidget(m_pacLabel, row, 0);
	g->addWidget(m_pacFile, row++, 1);

	m_provisioning = new TQComboBox(false, this);
	m_provisioning->insertItem(i18n("Disabled"));
	m_provisioning->insertItem(i18n("Anonymous"));
	m_provisioning->insertItem(i18n("Authenticated"));
	m_provisioning->insertItem(i18n("Both"));
	m_provLabel = new TQLabel(i18n("PAC pro&visioning:"), this);
	m_provLabel->setBuddy(m_provisioning);
	g->addWidget(m_provLabel, row, 0);
	g->addWidget(m_provisioning, row++, 1);

	m_phase2 = new TQComboBox(false, this);
	m_phase2->insertItem("MSCHAPv2");
	m_phase2->insertItem("GTC");
	m_phase2->insertItem("MD5");
	m_phase2->insertItem("PAP");
	m_phase2->insertItem("CHAP");
	m_phase2->insertItem("MSCHAP");
	m_phase2->insertItem("TLS");
	m_phase2Label = new TQLabel(i18n("I&nner authentication:"), this);
	m_phase2Label->setBuddy(m_phase2);
	g->addWidget(m_phase2Label, row, 0);
	g->addWidget(m_phase2, row++, 1);

	m_clientCert = new KURLRequester(this);
	m_certLabel = new TQLabel(i18n("&User certificate:"), this);
	m_certLabel->setBuddy(m_clientCert);
	g->addWidget(m_certLabel, row, 0);
	g->addWidget(m_clientCert, row++, 1);

	m_privateKey = new KURLRequester(this);
	m_keyLabel = new TQLabel(i18n("Private &key:"), this);
	m_keyLabel->setBuddy(m_privateKey);
	g->addWidget(m_keyLabel, row, 0);
	g->addWidget(m_privateKey, row++, 1);

	m_keyPassword = new SecretEdit(this);
	m_keyPasswordLabel = new TQLabel(i18n("Key pass&word:"), this);
	m_keyPasswordLabel->setBuddy(m_keyPassword->lineEdit());
	g->addWidget(m_keyPasswordLabel, row, 0, TQt::AlignTop);
	g->addWidget(m_keyPassword, row++, 1);

	m_password = new SecretEdit(this);
	m_passwordLabel = new TQLabel(i18n("&Password:"), this);
	m_passwordLabel->setBuddy(m_password->lineEdit());
	g->addWidget(m_passwordLabel, row, 0, TQt::AlignTop);
	g->addWidget(m_password, row++, 1);

	g->setRowStretch(row, 1);
	g->setColStretch(1, 1);

	connect(m_method, TQ_SIGNAL(activated(int)), TQ_SLOT(slotMethod(int)));
	m_method->setCurrentItem(1);
	slotMethod(1);
}

TQString EapWidget::method() const
{
	return eapMethods()[m_method->currentItem()];
}

void EapWidget::slotMethod(int)
{
	TQString m = method();
	bool tls = m == "tls";
	bool tunnel = m == "peap" || m == "ttls" || m == "fast";
	bool ca = tls || m == "peap" || m == "ttls";

	m_anonymous->setShown(tunnel);
	m_anonymousLabel->setShown(tunnel);
	m_domain->setShown(ca);
	m_domainLabel->setShown(ca);
	m_ca->setShown(ca);
	m_caLabel->setShown(ca);
	m_noCa->setShown(ca);
	m_peapVersion->setShown(m == "peap");
	m_peapLabel->setShown(m == "peap");
	m_pacFile->setShown(m == "fast");
	m_pacLabel->setShown(m == "fast");
	m_provisioning->setShown(m == "fast");
	m_provLabel->setShown(m == "fast");
	m_phase2->setShown(tunnel);
	m_phase2Label->setShown(tunnel);
	m_clientCert->setShown(tls);
	m_certLabel->setShown(tls);
	m_privateKey->setShown(tls);
	m_keyLabel->setShown(tls);
	m_keyPassword->setShown(tls);
	m_keyPasswordLabel->setShown(tls);
	m_password->setShown(!tls);
	m_passwordLabel->setShown(!tls);
}

void EapWidget::slotNoCa(bool on)
{
	m_ca->setEnabled(!on);
}

void EapWidget::load(const ConnectionSettings &conn)
{
	TQStringList eap = conn.strList("802-1x", "eap");
	TQString m = eap.isEmpty() ? TQString("peap") : eap.first();
	int idx = eapMethods().findIndex(m);
	m_method->setCurrentItem(idx < 0 ? 1 : idx);

	m_identity->setText(conn.str("802-1x", "identity"));
	m_anonymous->setText(conn.str("802-1x", "anonymous-identity"));
	m_domain->setText(conn.str("802-1x", "domain-suffix-match"));
	TQString ca = certPath(conn, "ca-cert");
	m_ca->setURL(ca);
	bool system = conn.boolean("802-1x", "system-ca-certs");
	m_noCa->setChecked(ca.isEmpty() && !system && conn.has("802-1x"));
	slotNoCa(m_noCa->isChecked());
	m_clientCert->setURL(certPath(conn, "client-cert"));
	m_privateKey->setURL(certPath(conn, "private-key"));
	m_keyPassword->load(conn, "802-1x", "private-key-password");
	m_password->load(conn, "802-1x", "password");

	TQString p2 = conn.str("802-1x", "phase2-auth");
	if (p2.isEmpty())
		p2 = conn.str("802-1x", "phase2-autheap");
	idx = phase2Methods().findIndex(p2);
	m_phase2->setCurrentItem(idx < 0 ? 0 : idx);

	TQString ver = conn.str("802-1x", "phase1-peapver");
	m_peapVersion->setCurrentItem(ver == "0" ? 1 : ver == "1" ? 2 : 0);
	m_pacFile->setURL(conn.str("802-1x", "pac-file"));
	m_provisioning->setCurrentItem(conn.str("802-1x", "phase1-fast-provisioning", "0").toInt());
	slotMethod(0);
}

void EapWidget::save(ConnectionSettings &conn)
{
	TQString m = method();
	conn.ensure("802-1x");
	conn.setStrList("802-1x", "eap", TQStringList(m));
	conn.setStr("802-1x", "identity", m_identity->text().stripWhiteSpace());

	bool tunnel = m == "peap" || m == "ttls" || m == "fast";
	bool ca = m == "tls" || m == "peap" || m == "ttls";
	conn.setStr("802-1x", "anonymous-identity", tunnel ? m_anonymous->text().stripWhiteSpace() : TQString::null);
	conn.setStr("802-1x", "domain-suffix-match", ca ? m_domain->text().stripWhiteSpace() : TQString::null);
	if (ca && !m_noCa->isChecked())
		setCertPath(conn, "ca-cert", m_ca->url());
	else
		conn.remove("802-1x", "ca-cert");

	if (tunnel) {
		TQString p2 = phase2Methods()[m_phase2->currentItem()];
		/* TTLS uses non-EAP methods by name; md5/gtc/tls are EAP methods */
		if (m == "ttls" && (p2 == "md5" || p2 == "gtc" || p2 == "tls")) {
			conn.setStr("802-1x", "phase2-autheap", p2);
			conn.remove("802-1x", "phase2-auth");
		} else {
			conn.setStr("802-1x", "phase2-auth", p2);
			conn.remove("802-1x", "phase2-autheap");
		}
	} else {
		conn.remove("802-1x", "phase2-auth");
		conn.remove("802-1x", "phase2-autheap");
	}

	if (m == "peap") {
		int v = m_peapVersion->currentItem();
		conn.setStr("802-1x", "phase1-peapver", v == 1 ? "0" : v == 2 ? "1" : TQString::null);
	} else {
		conn.remove("802-1x", "phase1-peapver");
	}
	if (m == "fast") {
		conn.setStr("802-1x", "pac-file", m_pacFile->url());
		conn.setStr("802-1x", "phase1-fast-provisioning", TQString::number(m_provisioning->currentItem()));
	}

	if (m == "tls") {
		setCertPath(conn, "client-cert", m_clientCert->url());
		setCertPath(conn, "private-key", m_privateKey->url());
		m_keyPassword->save(conn, "802-1x", "private-key-password");
		conn.remove("802-1x", "password");
		conn.remove("802-1x", "password-flags");
	} else {
		conn.remove("802-1x", "client-cert");
		conn.remove("802-1x", "private-key");
		conn.remove("802-1x", "private-key-password");
		conn.remove("802-1x", "private-key-password-flags");
		m_password->save(conn, "802-1x", "password");
	}
}

TQString EapWidget::validate() const
{
	TQString m = method();
	if (m_identity->text().stripWhiteSpace().isEmpty())
		return i18n("The 802.1X identity is missing.");
	if (m == "tls" && (m_clientCert->url().isEmpty() || m_privateKey->url().isEmpty()))
		return i18n("TLS authentication needs a user certificate and a private key.");
	if ((m == "tls" || m == "peap" || m == "ttls") && !m_noCa->isChecked() && m_ca->url().isEmpty())
		return i18n("Choose a CA certificate, or tick \"No CA certificate is required\".");
	return TQString::null;
}

#include "eapwidget.moc"
