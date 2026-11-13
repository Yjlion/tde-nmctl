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

#include "mainwidget.h"
#include "connectionlist.h"
#include "devicelist.h"
#include "authorization.h"
#include "nmclient.h"
#include "nmctlsettings.h"
#include "nmtext.h"
#include "secretstore.h"

#include <tqcheckbox.h>
#include <tqgroupbox.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <tqpushbutton.h>

#include <kiconloader.h>
#include <klineedit.h>
#include <tdelocale.h>
#include <tdemessagebox.h>

GlobalSettings::GlobalSettings(TQWidget *parent, const char *name)
	: TQWidget(parent, name)
{
	TQVBoxLayout *top = new TQVBoxLayout(this, 8, 8);

	TQGroupBox *radio = new TQGroupBox(1, TQt::Horizontal, i18n("NetworkManager"), this);
	m_networking = new TQCheckBox(i18n("Enable &networking"), radio);
	m_wifi = new TQCheckBox(i18n("Enable &Wi-Fi"), radio);
	m_wwan = new TQCheckBox(i18n("Enable &mobile broadband"), radio);
	TQWidget *cc = new TQWidget(radio);
	TQHBoxLayout *ccl = new TQHBoxLayout(cc, 0, 6);
	m_connCheck = new TQCheckBox(i18n("Check &Internet access (connectivity check)"), cc);
	TQPushButton *checkNow = new TQPushButton(i18n("Check N&ow"), cc);
	ccl->addWidget(m_connCheck);
	ccl->addWidget(checkNow);
	ccl->addStretch(1);
	m_connectivity = new TQLabel(radio);
	m_version = new TQLabel(radio);
	top->addWidget(radio);
	connect(m_networking, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotNetworking(bool)));
	connect(m_wifi, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotWifi(bool)));
	connect(m_wwan, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotWwan(bool)));
	connect(m_connCheck, TQ_SIGNAL(toggled(bool)), TQ_SLOT(slotConnectivityCheck(bool)));
	connect(checkNow, TQ_SIGNAL(clicked()), TQ_SLOT(slotCheckNow()));

	TQGroupBox *host = new TQGroupBox(2, TQt::Horizontal, i18n("Computer"), this);
	TQLabel *hl = new TQLabel(i18n("&Host name:"), host);
	m_hostname = new KLineEdit(host);
	hl->setBuddy(m_hostname);
	top->addWidget(host);
	connect(m_hostname, TQ_SIGNAL(textChanged(const TQString&)), TQ_SIGNAL(changed()));

	TQGroupBox *own = new TQGroupBox(1, TQt::Horizontal, i18n("Passwords and Tray"), this);
	m_wallet = new TQCheckBox(i18n("Store new passwords in TDE &Wallet (only for this user)"), own);
	TQLabel *wl = new TQLabel(i18n("<qt><small>When this is off, new passwords are stored by "
	                               "NetworkManager for all users, unencrypted, in "
	                               "/etc/NetworkManager/system-connections.</small></qt>"), own);
	wl->setAlignment(TQt::WordBreak);
	m_notifications = new TQCheckBox(i18n("Show &notifications when connections change"), own);
	m_showAll = new TQCheckBox(i18n("Show all Wi-Fi networks in range in the &tray popup"), own);
	m_autostart = new TQCheckBox(i18n("Start the tray icon at &login"), own);
	top->addWidget(own);
	connect(m_wallet, TQ_SIGNAL(toggled(bool)), TQ_SIGNAL(changed()));
	connect(m_notifications, TQ_SIGNAL(toggled(bool)), TQ_SIGNAL(changed()));
	connect(m_showAll, TQ_SIGNAL(toggled(bool)), TQ_SIGNAL(changed()));
	connect(m_autostart, TQ_SIGNAL(toggled(bool)), TQ_SIGNAL(changed()));

	top->addStretch(1);

	connect(NMClient::self(), TQ_SIGNAL(changed()), TQ_SLOT(updateState()));
	load();
}

void GlobalSettings::updateState()
{
	NMClient *nm = NMClient::self();
	bool running = nm->isRunning();
	m_networking->blockSignals(true);
	m_wifi->blockSignals(true);
	m_wwan->blockSignals(true);
	m_connCheck->blockSignals(true);
	m_networking->setChecked(nm->networkingEnabled());
	m_wifi->setChecked(nm->wirelessEnabled());
	m_wwan->setChecked(nm->wwanEnabled());
	m_connCheck->setChecked(nm->connectivityCheckEnabled());
	m_networking->blockSignals(false);
	m_wifi->blockSignals(false);
	m_wwan->blockSignals(false);
	m_connCheck->blockSignals(false);

	m_networking->setEnabled(running);
	m_wifi->setEnabled(running && nm->wirelessHardwareEnabled() && nm->networkingEnabled());
	m_wwan->setEnabled(running && nm->wwanHardwareEnabled() && nm->networkingEnabled());
	m_connCheck->setEnabled(running && nm->connectivityCheckAvailable());
	if (running && !nm->wirelessHardwareEnabled())
		m_wifi->setText(i18n("Enable &Wi-Fi (turned off by a hardware switch)"));
	else
		m_wifi->setText(i18n("Enable &Wi-Fi"));

	m_connectivity->setText(i18n("State: %1. Connectivity: %2.")
	                        .arg(NMText::managerState(nm->state()))
	                        .arg(NMText::connectivity(nm->connectivity())));
	m_version->setText(running ? i18n("NetworkManager version %1").arg(nm->version())
	                           : i18n("NetworkManager is not running."));
}

void GlobalSettings::load()
{
	updateState();
	m_savedHostname = NMClient::self()->hostname();
	m_hostname->blockSignals(true);
	m_hostname->setText(m_savedHostname);
	m_hostname->blockSignals(false);
	m_wallet->setChecked(NmctlSettings::storeInWallet());
	m_wallet->setEnabled(SecretStore::walletEnabled());
	m_notifications->setChecked(NmctlSettings::notifications());
	m_showAll->setChecked(NmctlSettings::showAllNetworks());
	m_autostart->setChecked(NmctlSettings::trayAutostart());
}

void GlobalSettings::save()
{
	NmctlSettings::setStoreInWallet(m_wallet->isChecked());
	NmctlSettings::setNotifications(m_notifications->isChecked());
	NmctlSettings::setShowAllNetworks(m_showAll->isChecked());
	NmctlSettings::setTrayAutostart(m_autostart->isChecked());

	TQString h = m_hostname->text().stripWhiteSpace();
	if (h != m_savedHostname && !h.isEmpty()) {
		if (NMClient::self()->saveHostname(h))
			m_savedHostname = h;
		else
			KMessageBox::error(this, i18n("<qt>The host name could not be changed:<br>%1</qt>")
			                         .arg(NMClient::self()->lastError()));
	}
}

void GlobalSettings::defaults()
{
	m_wallet->setChecked(true);
	m_notifications->setChecked(true);
	m_showAll->setChecked(true);
	m_autostart->setChecked(true);
}

void GlobalSettings::slotNetworking(bool on)
{
	NMClient::self()->setNetworkingEnabled(on);
}

void GlobalSettings::slotWifi(bool on)
{
	NMClient::self()->setWirelessEnabled(on);
}

void GlobalSettings::slotWwan(bool on)
{
	NMClient::self()->setWwanEnabled(on);
}

void GlobalSettings::slotConnectivityCheck(bool on)
{
	NMClient::self()->setConnectivityCheckEnabled(on);
}

void GlobalSettings::slotCheckNow()
{
	uint c = NMClient::self()->checkConnectivity();
	m_connectivity->setText(i18n("State: %1. Connectivity: %2.")
	                        .arg(NMText::managerState(NMClient::self()->state()))
	                        .arg(NMText::connectivity(c)));
}

/* ---------------------------------------------------------------------- */

MainWidget::MainWidget(TQWidget *parent, const char *name)
	: TQTabWidget(parent, name)
{
	setMargin(6);
	m_connections = new ConnectionList(this, "ConnectionList");
	addTab(m_connections, SmallIconSet("tde-nmctl"), i18n("&Connections"));
	m_devices = new DeviceList(this, "DeviceList");
	addTab(m_devices, SmallIconSet("tde-nmctl-wired"), i18n("&Devices"));
	m_settings = new GlobalSettings(this, "GlobalSettings");
	addTab(m_settings, SmallIconSet("configure"), i18n("&Settings"));
}

void MainWidget::showPage(Page page)
{
	setCurrentPage((int)page);
}

MainWidget::Page MainWidget::pageFromString(const TQString &name)
{
	if (name == "devices")
		return Devices;
	if (name == "settings")
		return Settings;
	return Connections;
}

#include "mainwidget.moc"
