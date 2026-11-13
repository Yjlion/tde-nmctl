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

#ifndef NMCLIENT_H
#define NMCLIENT_H

#include <tqobject.h>
#include <tqmap.h>
#include <tqstring.h>
#include <tqstringlist.h>
#include <tqvaluelist.h>

#include <tqdbusconnection.h>
#include <tqdbusmessage.h>

#include "nmdbus.h"

class TQTimer;

/* NMDeviceType */
enum {
	NM_DEVICE_TYPE_UNKNOWN = 0, NM_DEVICE_TYPE_ETHERNET = 1, NM_DEVICE_TYPE_WIFI = 2,
	NM_DEVICE_TYPE_BT = 5, NM_DEVICE_TYPE_OLPC_MESH = 6, NM_DEVICE_TYPE_WIMAX = 7,
	NM_DEVICE_TYPE_MODEM = 8, NM_DEVICE_TYPE_INFINIBAND = 9, NM_DEVICE_TYPE_BOND = 10,
	NM_DEVICE_TYPE_VLAN = 11, NM_DEVICE_TYPE_ADSL = 12, NM_DEVICE_TYPE_BRIDGE = 13,
	NM_DEVICE_TYPE_GENERIC = 14, NM_DEVICE_TYPE_TEAM = 15, NM_DEVICE_TYPE_TUN = 16,
	NM_DEVICE_TYPE_IP_TUNNEL = 17, NM_DEVICE_TYPE_MACVLAN = 18, NM_DEVICE_TYPE_VXLAN = 19,
	NM_DEVICE_TYPE_VETH = 20, NM_DEVICE_TYPE_MACSEC = 21, NM_DEVICE_TYPE_DUMMY = 22,
	NM_DEVICE_TYPE_PPP = 23, NM_DEVICE_TYPE_OVS_INTERFACE = 24, NM_DEVICE_TYPE_OVS_PORT = 25,
	NM_DEVICE_TYPE_OVS_BRIDGE = 26, NM_DEVICE_TYPE_WPAN = 27, NM_DEVICE_TYPE_6LOWPAN = 28,
	NM_DEVICE_TYPE_WIREGUARD = 29, NM_DEVICE_TYPE_WIFI_P2P = 30, NM_DEVICE_TYPE_VRF = 31,
	NM_DEVICE_TYPE_LOOPBACK = 32, NM_DEVICE_TYPE_HSR = 33
};

/* NMDeviceState */
enum {
	NM_DEVICE_STATE_UNKNOWN = 0, NM_DEVICE_STATE_UNMANAGED = 10, NM_DEVICE_STATE_UNAVAILABLE = 20,
	NM_DEVICE_STATE_DISCONNECTED = 30, NM_DEVICE_STATE_PREPARE = 40, NM_DEVICE_STATE_CONFIG = 50,
	NM_DEVICE_STATE_NEED_AUTH = 60, NM_DEVICE_STATE_IP_CONFIG = 70, NM_DEVICE_STATE_IP_CHECK = 80,
	NM_DEVICE_STATE_SECONDARIES = 90, NM_DEVICE_STATE_ACTIVATED = 100,
	NM_DEVICE_STATE_DEACTIVATING = 110, NM_DEVICE_STATE_FAILED = 120
};

/* NMState */
enum {
	NM_STATE_UNKNOWN = 0, NM_STATE_ASLEEP = 10, NM_STATE_DISCONNECTED = 20,
	NM_STATE_DISCONNECTING = 30, NM_STATE_CONNECTING = 40, NM_STATE_CONNECTED_LOCAL = 50,
	NM_STATE_CONNECTED_SITE = 60, NM_STATE_CONNECTED_GLOBAL = 70
};

/* NMConnectivityState */
enum {
	NM_CONNECTIVITY_UNKNOWN = 0, NM_CONNECTIVITY_NONE = 1, NM_CONNECTIVITY_PORTAL = 2,
	NM_CONNECTIVITY_LIMITED = 3, NM_CONNECTIVITY_FULL = 4
};

/* NMActiveConnectionState */
enum {
	NM_ACTIVE_CONNECTION_STATE_UNKNOWN = 0, NM_ACTIVE_CONNECTION_STATE_ACTIVATING = 1,
	NM_ACTIVE_CONNECTION_STATE_ACTIVATED = 2, NM_ACTIVE_CONNECTION_STATE_DEACTIVATING = 3,
	NM_ACTIVE_CONNECTION_STATE_DEACTIVATED = 4
};

/* NM80211ApFlags, NM80211ApSecurityFlags, NM80211Mode */
enum {
	NM_802_11_AP_FLAGS_PRIVACY = 0x1, NM_802_11_AP_FLAGS_WPS = 0x2,
	NM_802_11_AP_SEC_PAIR_WEP40 = 0x1, NM_802_11_AP_SEC_PAIR_WEP104 = 0x2,
	NM_802_11_AP_SEC_KEY_MGMT_PSK = 0x100, NM_802_11_AP_SEC_KEY_MGMT_802_1X = 0x200,
	NM_802_11_AP_SEC_KEY_MGMT_SAE = 0x400, NM_802_11_AP_SEC_KEY_MGMT_OWE = 0x800,
	NM_802_11_AP_SEC_KEY_MGMT_OWE_TM = 0x1000, NM_802_11_AP_SEC_KEY_MGMT_EAP_SUITE_B_192 = 0x2000,
	NM_802_11_MODE_ADHOC = 1, NM_802_11_MODE_INFRA = 2, NM_802_11_MODE_AP = 3, NM_802_11_MODE_MESH = 4
};

/* NMSettingSecretFlags */
enum {
	NM_SECRET_FLAG_NONE = 0, NM_SECRET_FLAG_AGENT_OWNED = 0x1,
	NM_SECRET_FLAG_NOT_SAVED = 0x2, NM_SECRET_FLAG_NOT_REQUIRED = 0x4
};

struct NMAccessPoint
{
	NMAccessPoint() : strength(0), frequency(0), flags(0), wpaFlags(0), rsnFlags(0),
	                  mode(0), maxBitrate(0), lastSeen(-1) {}

	TQString path;
	TQByteArray ssid;
	TQString bssid;
	uint strength;      /* percent */
	uint frequency;     /* MHz */
	uint flags;
	uint wpaFlags;
	uint rsnFlags;
	uint mode;
	uint maxBitrate;    /* kbit/s */
	int lastSeen;

	TQString ssidText() const { return NMDBus::ssidToString(ssid); }
	bool isSecure() const;
	bool isEnterprise() const;
	/** "WPA2", "WPA3", "WEP", "Enterprise", ... or empty for open networks */
	TQString securityText() const;
	/** the key-mgmt value a new connection to this access point needs */
	TQString keyMgmt() const;
	int channel() const;
};
typedef TQValueList<NMAccessPoint> NMAccessPointList;

struct NMIpConfig
{
	TQStringList addresses;   /* address/prefix */
	TQString gateway;
	TQStringList dns;
	TQStringList domains;
	TQStringList routes;      /* dest/prefix via next-hop metric m */
	bool isEmpty() const { return addresses.isEmpty() && gateway.isEmpty() && dns.isEmpty(); }
};

struct NMDeviceInfo
{
	NMDeviceInfo() : type(0), state(0), stateReason(0), managed(false), autoconnect(false),
	                 real(true), speed(0), mtu(0), wirelessCaps(0), metered(0) {}

	TQString path;
	TQString iface;
	TQString ipIface;
	TQString driver;
	TQString driverVersion;
	TQString firmwareVersion;
	TQString hwAddress;
	TQString permHwAddress;
	TQString vendor;
	TQString product;
	uint type;
	uint state;
	uint stateReason;
	bool managed;
	bool autoconnect;
	bool real;
	uint speed;          /* Mbit/s (wired) or kbit/s bitrate (wireless) */
	uint mtu;
	uint wirelessCaps;
	uint metered;
	TQString activeConnection;
	TQString ip4Config;
	TQString ip6Config;
	TQStringList availableConnections;
	TQString activeAccessPoint;
	NMAccessPointList accessPoints;
	TQString modemCaps;

	bool isWireless() const { return type == NM_DEVICE_TYPE_WIFI; }
	bool isConnected() const { return state == NM_DEVICE_STATE_ACTIVATED; }
	bool isActivating() const { return state > NM_DEVICE_STATE_DISCONNECTED && state < NM_DEVICE_STATE_ACTIVATED; }
	const NMAccessPoint *activeAP() const;
};
typedef TQValueList<NMDeviceInfo> NMDeviceList;

struct NMActiveConnection
{
	NMActiveConnection() : state(0), isDefault(false), isDefault6(false), vpn(false), vpnState(0) {}

	TQString path;
	TQString id;
	TQString uuid;
	TQString type;
	TQString connection;
	TQString specificObject;
	TQStringList devices;
	uint state;
	bool isDefault;
	bool isDefault6;
	bool vpn;
	uint vpnState;
	TQString ip4Config;
	TQString ip6Config;
};
typedef TQValueList<NMActiveConnection> NMActiveList;

struct NMConnection
{
	NMConnection() : unsaved(false), flags(0) {}

	TQString path;
	ConnectionSettings settings;
	bool unsaved;
	uint flags;
	TQString filename;

	TQString id() const { return settings.id(); }
	TQString uuid() const { return settings.uuid(); }
	TQString type() const { return settings.type(); }
	TQ_UINT64 timestamp() const { return settings.uint64("connection", "timestamp"); }
	/** true if only the listed users may use it (connection.permissions) */
	bool isPrivate() const { return !settings.strList("connection", "permissions").isEmpty(); }
};
typedef TQValueList<NMConnection> NMConnectionList;

/**
 * Client for the NetworkManager D-Bus API on the system bus.
 *
 * Keeps a copy of NetworkManager's state (devices, access points, active
 * connections, connection profiles) that is refreshed whenever
 * NetworkManager sends a signal; changed() is emitted afterwards.
 *
 * NetworkManager checks the caller's permissions with polkit and asks the
 * user's polkit agent (polkit-agent-tde) for a password when needed, so
 * calls that change something use a long timeout.
 */
class NMClient : public TQObject
{
	TQ_OBJECT

public:
	/** the client shared by everything in this process */
	static NMClient *self();

	NMClient(TQObject *parent = 0, const char *name = 0);
	virtual ~NMClient();

	bool busConnected() const;
	bool isRunning() const { return m_running; }
	TQString lastError() const { return m_lastError; }
	TQString lastErrorName() const { return m_lastErrorName; }

	/* manager state */
	TQString version() const { return m_version; }
	uint state() const { return m_state; }
	uint connectivity() const { return m_connectivity; }
	bool networkingEnabled() const { return m_networkingEnabled; }
	bool wirelessEnabled() const { return m_wirelessEnabled; }
	bool wirelessHardwareEnabled() const { return m_wirelessHwEnabled; }
	bool wwanEnabled() const { return m_wwanEnabled; }
	bool wwanHardwareEnabled() const { return m_wwanHwEnabled; }
	bool connectivityCheckAvailable() const { return m_connCheckAvailable; }
	bool connectivityCheckEnabled() const { return m_connCheckEnabled; }
	TQString primaryConnection() const { return m_primaryConnection; }
	TQString hostname() const { return m_hostname; }
	bool hasWirelessDevice() const;
	bool hasModemDevice() const;

	const NMDeviceList &devices() const { return m_devices; }
	const NMActiveList &activeConnections() const { return m_active; }
	const NMConnectionList &connections() const { return m_connections; }

	const NMDeviceInfo *device(const TQString &path) const;
	const NMDeviceInfo *deviceByInterface(const TQString &iface) const;
	const NMActiveConnection *activeConnection(const TQString &path) const;
	const NMActiveConnection *activeForConnection(const TQString &connectionPath) const;
	const NMConnection *connection(const TQString &path) const;
	const NMConnection *connectionByUuid(const TQString &uuid) const;
	/** the profiles that can be used for an access point (by SSID) */
	TQValueList<const NMConnection *> connectionsForSsid(const TQByteArray &ssid) const;
	NMIpConfig ipConfig(const TQString &path, bool ipv6);
	bool deviceStatistics(const TQString &devicePath, TQ_UINT64 *rx, TQ_UINT64 *tx);
	void setStatisticsRefresh(const TQString &devicePath, uint ms);

	/* actions without an immediate result; operationFinished() reports
	 * failures */
	void activateConnection(const TQString &connectionPath, const TQString &devicePath = "/",
	                        const TQString &specificObject = "/");
	void addAndActivate(const ConnectionSettings &settings, const TQString &devicePath,
	                    const TQString &specificObject = "/");
	void deactivate(const TQString &activePath);
	void disconnectDevice(const TQString &devicePath);
	void requestScan(const TQString &devicePath = TQString::null);
	void setNetworkingEnabled(bool on);
	void setWirelessEnabled(bool on);
	void setWwanEnabled(bool on);
	void setConnectivityCheckEnabled(bool on);
	void setDeviceManaged(const TQString &devicePath, bool on);
	void setDeviceAutoconnect(const TQString &devicePath, bool on);

	/* synchronous actions; false and lastError() on failure */
	TQString addConnection(const ConnectionSettings &settings, bool persist = true);
	bool updateConnection(const TQString &path, const ConnectionSettings &settings);
	bool deleteConnection(const TQString &path);
	bool getSecrets(const TQString &path, const TQString &setting, NMSettingsMap *secrets);
	bool saveHostname(const TQString &hostname);
	bool reloadConnections();
	uint checkConnectivity();

	/** a polkit authentication agent is running in this session */
	static bool polkitAgentRunning();
	static TQString friendlyError(const TQString &name, const TQString &message);

public slots:
	void refresh();

signals:
	/** devices, access points or active connections changed */
	void changed();
	/** the list of connection profiles changed */
	void connectionsChanged();
	/** NetworkManager started or stopped */
	void availabilityChanged(bool running);
	void deviceStateChanged(const TQString &devicePath, uint newState, uint oldState, uint reason);
	void operationFinished(bool ok, const TQString &description, const TQString &error);

private slots:
	void slotDBusSignal(const TQT_DBusMessage &message);
	void slotAsyncReply(const TQT_DBusMessage &reply);
	void slotRefreshTimer();

private:
	TQT_DBusMessage call(const TQString &path, const TQString &interface, const TQString &method,
	                     const TQValueList<TQT_DBusData> &args = TQValueList<TQT_DBusData>(),
	                     int timeout = -1);
	bool callAsync(const TQString &path, const TQString &interface, const TQString &method,
	               const TQValueList<TQT_DBusData> &args, const TQString &description);
	NMVariantMap getAll(const TQString &path, const TQString &interface);
	bool setProperty(const TQString &path, const TQString &interface, const TQString &name,
	                 const TQT_DBusData &value, const TQString &description);
	void addMatchRules();
	void checkRunning();
	void readManager();
	void readDevices();
	void readActive();
	void readConnections();
	NMAccessPoint readAccessPoint(const TQString &path);
	void scheduleRefresh(bool connections);

	TQT_DBusConnection m_bus;
	TQTimer *m_refreshTimer;
	bool m_refreshConnections;
	bool m_refreshDevices;
	TQMap<int, TQString> m_pending;
	TQString m_lastError;
	TQString m_lastErrorName;

	bool m_running;
	TQString m_version;
	uint m_state;
	uint m_connectivity;
	bool m_networkingEnabled;
	bool m_wirelessEnabled;
	bool m_wirelessHwEnabled;
	bool m_wwanEnabled;
	bool m_wwanHwEnabled;
	bool m_connCheckAvailable;
	bool m_connCheckEnabled;
	TQString m_primaryConnection;
	TQString m_hostname;
	NMDeviceList m_devices;
	NMActiveList m_active;
	NMConnectionList m_connections;

	static NMClient *s_self;
};

#endif
