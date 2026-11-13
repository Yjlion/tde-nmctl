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

#ifndef EAPWIDGET_H
#define EAPWIDGET_H

#include <tqwidget.h>

#include "nmdbus.h"

class TQCheckBox;
class TQComboBox;
class TQLabel;
class KLineEdit;
class KURLRequester;
class SecretEdit;

/**
 * The 802.1X (802-1x) settings: TLS, PEAP, TTLS, PWD, LEAP and FAST with
 * the inner authentication, certificates and password.
 */
class EapWidget : public TQWidget
{
	TQ_OBJECT
public:
	EapWidget(TQWidget *parent = 0, const char *name = 0);

	void load(const ConnectionSettings &conn);
	void save(ConnectionSettings &conn);
	TQString validate() const;

private slots:
	void slotMethod(int);
	void slotNoCa(bool);

private:
	TQString method() const;

	TQComboBox *m_method;
	KLineEdit *m_identity;
	KLineEdit *m_anonymous;
	KLineEdit *m_domain;
	KURLRequester *m_ca;
	TQCheckBox *m_noCa;
	KURLRequester *m_clientCert;
	KURLRequester *m_privateKey;
	SecretEdit *m_keyPassword;
	TQComboBox *m_phase2;
	TQComboBox *m_peapVersion;
	KURLRequester *m_pacFile;
	TQComboBox *m_provisioning;
	SecretEdit *m_password;

	TQLabel *m_anonymousLabel, *m_domainLabel, *m_caLabel, *m_certLabel, *m_keyLabel,
	        *m_keyPasswordLabel, *m_phase2Label, *m_peapLabel, *m_pacLabel, *m_provLabel,
	        *m_passwordLabel;
};

#endif
