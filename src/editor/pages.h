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

#ifndef PAGES_H
#define PAGES_H

#include "settingpage.h"

#include <tqvaluelist.h>

class TQButtonGroup;
class TQCheckBox;
class TQComboBox;
class TQGroupBox;
class TQLabel;
class TQPushButton;
class TQRadioButton;
class TQSpinBox;
class TQTable;
class TQTextEdit;
class TQWidgetStack;
class KLineEdit;
class KURLRequester;
class EapWidget;
class SecretEdit;

/* ---------------------------------------------------------------------- */

/** connection: name, autoconnect, permissions, zone, metered, VPN */
class GeneralPage : public SettingPage
{
	TQ_OBJECT
public:
	GeneralPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
	TQString connectionName() const;
private slots:
	void slotAllUsers(bool on);
private:
	KLineEdit *m_name;
	TQCheckBox *m_autoconnect;
	TQSpinBox *m_priority;
	TQCheckBox *m_allUsers;
	TQLabel *m_authNote;
	TQCheckBox *m_vpnAuto;
	TQComboBox *m_vpn;
	TQStringList m_vpnUuids;
	TQComboBox *m_zone;
	TQComboBox *m_metered;
	TQComboBox *m_autoconnectRetries;
	KLineEdit *m_uuid;
};

/** interface name only, for dummy and other simple virtual devices */
class InterfacePage : public SettingPage
{
	TQ_OBJECT
public:
	InterfacePage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private:
	KLineEdit *m_iface;
	TQSpinBox *m_mtu;
	TQString m_type;
};

/** proxy: method, PAC URL, browser only */
class ProxyPage : public SettingPage
{
	TQ_OBJECT
public:
	ProxyPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
private slots:
	void slotMethod(int);
private:
	TQComboBox *m_method;
	KLineEdit *m_pacUrl;
	TQCheckBox *m_browserOnly;
};

/** 802-3-ethernet: device, MAC, MTU, link negotiation, Wake-on-LAN */
class EthernetPage : public SettingPage
{
	TQ_OBJECT
public:
	EthernetPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private slots:
	void slotNegotiation(int);
private:
	TQComboBox *m_iface;
	KLineEdit *m_mac;
	TQComboBox *m_cloned;
	TQSpinBox *m_mtu;
	TQComboBox *m_negotiation;
	TQComboBox *m_speed;
	TQComboBox *m_duplex;
	TQComboBox *m_wol;
};

/** 802-1x on wired connections */
class WiredSecurityPage : public SettingPage
{
	TQ_OBJECT
public:
	WiredSecurityPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private:
	TQCheckBox *m_enable;
	EapWidget *m_eap;
};

/** 802-11-wireless */
class WifiPage : public SettingPage
{
	TQ_OBJECT
public:
	WifiPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
	TQString ssid() const;
	bool isHotspot() const;
signals:
	void modeChanged();
private slots:
	void slotBand(int);
	void slotMode(int);
private:
	KLineEdit *m_ssid;
	TQComboBox *m_mode;
	TQComboBox *m_band;
	TQSpinBox *m_channel;
	TQComboBox *m_bssid;
	TQComboBox *m_iface;
	TQComboBox *m_cloned;
	TQSpinBox *m_mtu;
	TQCheckBox *m_hidden;
	TQComboBox *m_powersave;
	TQComboBox *m_randomization;
};

/** 802-11-wireless-security (+ 802-1x for enterprise) */
class WifiSecurityPage : public SettingPage
{
	TQ_OBJECT
public:
	WifiSecurityPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
	/** select the security type for a key-mgmt value ("wpa-psk", ...) */
	void preset(const TQString &keyMgmt);
private slots:
	void slotType(int);
private:
	TQComboBox *m_type;
	TQWidgetStack *m_stack;
	/* personal */
	SecretEdit *m_psk;
	/* WEP */
	SecretEdit *m_wepKey;
	TQComboBox *m_wepType;
	TQComboBox *m_wepIndex;
	TQComboBox *m_wepAuth;
	/* LEAP */
	KLineEdit *m_leapUser;
	SecretEdit *m_leapPassword;
	/* enterprise */
	EapWidget *m_eap;
	TQComboBox *m_pmf;
	TQWidget *m_pskPage, *m_wepPage, *m_leapPage, *m_eapPage, *m_nonePage;
};

/** ipv4 / ipv6 */
class IpPage : public SettingPage
{
	TQ_OBJECT
public:
	IpPage(bool ipv6, TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
	/** limit the methods for VPN and the like */
	void setVpnMode(bool vpn);
private slots:
	void slotMethod(int);
	void slotAddAddress();
	void slotRemoveAddress();
	void slotRoutes();
private:
	TQString setting() const { return m_v6 ? "ipv6" : "ipv4"; }
	TQStringList methods() const;
	bool parseAddress(const TQString &text, TQString *addr) const;

	bool m_v6;
	TQComboBox *m_method;
	TQTable *m_addresses;
	TQPushButton *m_add;
	TQPushButton *m_remove;
	KLineEdit *m_dns;
	KLineEdit *m_search;
	TQCheckBox *m_autoDns;
	TQCheckBox *m_autoRoutes;
	TQCheckBox *m_neverDefault;
	TQCheckBox *m_required;
	KLineEdit *m_dhcpId;
	KLineEdit *m_dhcpHostname;
	TQComboBox *m_privacy;
	TQComboBox *m_addrGen;
	TQPushButton *m_routesButton;
	TQValueList<NMVariantMap> m_routes;
	int m_routeMetric;
	int m_routeTable;
	TQLabel *m_dhcpIdLabel;
};

/** the ports of a bond, bridge or team */
class PortsWidget : public TQWidget
{
	TQ_OBJECT
public:
	PortsWidget(const TQString &portType, TQWidget *parent = 0);
	void setController(const TQString &uuid, const TQString &iface);
private slots:
	void reload();
	void slotAdd();
	void slotEdit();
	void slotDelete();
private:
	TQString m_portType;
	TQString m_uuid;
	TQString m_iface;
	class TDEListView *m_list;
	TQPushButton *m_edit;
	TQPushButton *m_delete;
};

class BondPage : public SettingPage
{
	TQ_OBJECT
public:
	BondPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private:
	KLineEdit *m_iface;
	TQComboBox *m_mode;
	TQComboBox *m_monitoring;
	TQSpinBox *m_frequency;
	TQSpinBox *m_updelay;
	TQSpinBox *m_downdelay;
	KLineEdit *m_arpTargets;
	KLineEdit *m_primary;
	PortsWidget *m_ports;
	TQMap<TQString, TQString> m_options;
};

class BridgePage : public SettingPage
{
	TQ_OBJECT
public:
	BridgePage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private:
	KLineEdit *m_iface;
	TQCheckBox *m_stp;
	TQSpinBox *m_priority;
	TQSpinBox *m_forwardDelay;
	TQSpinBox *m_helloTime;
	TQSpinBox *m_maxAge;
	TQSpinBox *m_ageing;
	TQCheckBox *m_vlanFiltering;
	TQCheckBox *m_multicastSnooping;
	PortsWidget *m_ports;
};

class TeamPage : public SettingPage
{
	TQ_OBJECT
public:
	TeamPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private:
	KLineEdit *m_iface;
	TQComboBox *m_runner;
	TQTextEdit *m_config;
	PortsWidget *m_ports;
};

class VlanPage : public SettingPage
{
	TQ_OBJECT
public:
	VlanPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private slots:
	void slotUpdateName();
private:
	TQComboBox *m_parent;
	TQSpinBox *m_id;
	KLineEdit *m_iface;
	TQCheckBox *m_reorder;
	TQCheckBox *m_gvrp;
	TQCheckBox *m_mvrp;
	TQCheckBox *m_looseBinding;
	bool m_nameEdited;
};

class InfinibandPage : public SettingPage
{
	TQ_OBJECT
public:
	InfinibandPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
private:
	TQComboBox *m_iface;
	TQComboBox *m_transport;
	TQSpinBox *m_mtu;
	KLineEdit *m_pkey;
	KLineEdit *m_parent;
};

class PppoePage : public SettingPage
{
	TQ_OBJECT
public:
	PppoePage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private:
	TQComboBox *m_parent;
	KLineEdit *m_service;
	KLineEdit *m_user;
	SecretEdit *m_password;
	TQCheckBox *m_mppe;
	TQCheckBox *m_bsdcomp;
	TQCheckBox *m_deflate;
	TQCheckBox *m_lcpEcho;
};

/** gsm and cdma */
class MobilePage : public SettingPage
{
	TQ_OBJECT
public:
	MobilePage(bool gsm, TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private:
	bool m_gsm;
	KLineEdit *m_number;
	KLineEdit *m_apn;
	KLineEdit *m_user;
	SecretEdit *m_password;
	SecretEdit *m_pin;
	KLineEdit *m_networkId;
	TQCheckBox *m_roaming;
	TQSpinBox *m_mtu;
};

/** vpn (NetworkManager VPN plugins) */
class VpnPage : public SettingPage
{
	TQ_OBJECT
public:
	VpnPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
	static TQStringList serviceTypes();
	static TQString serviceName(const TQString &service);
private slots:
	void slotService(int);
	void slotOpenVpnType(int);
	void slotAddData();
	void slotRemoveData();
private:
	void showFields();
	TQString shortService() const;
	void writeTable(const TQMap<TQString, TQString> &data);
	TQMap<TQString, TQString> readTable() const;

	TQComboBox *m_service;
	KLineEdit *m_gateway;
	TQLabel *m_gatewayLabel;
	KLineEdit *m_user;
	TQLabel *m_userLabel;
	SecretEdit *m_password;
	TQLabel *m_passwordLabel;
	/* OpenVPN */
	TQGroupBox *m_ovpn;
	TQComboBox *m_ovpnType;
	KURLRequester *m_ca;
	KURLRequester *m_cert;
	KURLRequester *m_key;
	SecretEdit *m_certPass;
	KURLRequester *m_staticKey;
	KURLRequester *m_ta;
	KLineEdit *m_port;
	TQCheckBox *m_tcp;
	TQComboBox *m_dev;
	/* group password (vpnc) */
	SecretEdit *m_groupPassword;
	KLineEdit *m_groupName;
	TQLabel *m_groupNameLabel;
	TQLabel *m_groupPasswordLabel;
	/* every option */
	TQTable *m_data;
	TQString m_lastService;
	TQMap<TQString, TQString> m_dataMap;
};

/** wireguard */
class WireGuardPage : public SettingPage
{
	TQ_OBJECT
public:
	WireGuardPage(TQWidget *parent = 0);
	virtual void load(const ConnectionSettings &conn);
	virtual void save(ConnectionSettings &conn);
	virtual TQString validate() const;
private slots:
	void slotGenerate();
	void slotAddPeer();
	void slotRemovePeer();
private:
	KLineEdit *m_iface;
	SecretEdit *m_privateKey;
	KLineEdit *m_publicKey;
	TQSpinBox *m_listenPort;
	TQSpinBox *m_fwmark;
	TQSpinBox *m_mtu;
	TQCheckBox *m_peerRoutes;
	TQTable *m_peers;
	TQValueList<NMVariantMap> m_peerData;
};

#endif
