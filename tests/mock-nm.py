#!/usr/bin/python3
# A small NetworkManager imitation on the D-Bus system bus, for tests and
# screenshots in containers without NetworkManager or Wi-Fi hardware.
#
# It provides a wired and a Wi-Fi device with access points, saved
# connections, active connections, IP configurations and the settings,
# agent manager and statistics interfaces that tde-nmctl uses. Connections
# are kept exactly as they are sent, so tests can check what the client
# writes. It asks the registered secret agent for agent-owned secrets the
# way NetworkManager does.
#
# Usage: mock-nm.py [--scenario home|empty]   (run as root on the system bus)
#
# This file is released under GPL >= 2.

import sys
import uuid as uuidlib

import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

NM = "org.freedesktop.NetworkManager"
NM_PATH = "/org/freedesktop/NetworkManager"
PROPS = "org.freedesktop.DBus.Properties"
MOCK = "org.trinitydesktop.NmctlMock"

bus = None
manager = None


def ay(text):
    return dbus.ByteArray(text.encode("utf-8")) if isinstance(text, str) else dbus.ByteArray(text)


def ip4(a):
    parts = [int(x) for x in a.split(".")]
    return dbus.UInt32(parts[0] | parts[1] << 8 | parts[2] << 16 | parts[3] << 24)


def strip_variants(v):
    """dbus values -> plain python for comparisons and printing"""
    if isinstance(v, dbus.ByteArray) or (isinstance(v, dbus.Array) and v.signature == "y"):
        return bytes(bytearray(v))
    if isinstance(v, (dbus.Array, list)):
        return [strip_variants(x) for x in v]
    if isinstance(v, (dbus.Dictionary, dict)):
        return {str(k): strip_variants(x) for k, x in v.items()}
    if isinstance(v, dbus.Boolean):
        return bool(v)
    if isinstance(v, (dbus.String, dbus.ObjectPath)):
        return str(v)
    if isinstance(v, (dbus.Byte, dbus.Int16, dbus.UInt16, dbus.Int32, dbus.UInt32, dbus.Int64, dbus.UInt64)):
        return int(v)
    return v


class PropObject(dbus.service.Object):
    """an object with org.freedesktop.DBus.Properties"""

    def __init__(self, path):
        dbus.service.Object.__init__(self, bus, path)
        self.path = path
        self.props = {}        # interface -> {name: value}

    def set_prop(self, iface, name, value, notify=True):
        self.props.setdefault(iface, {})[name] = value
        if notify:
            self.PropertiesChanged(iface, {name: value}, [])

    @dbus.service.method(PROPS, in_signature="ss", out_signature="v")
    def Get(self, iface, name):
        try:
            return self.props[iface][name]
        except KeyError:
            raise dbus.exceptions.DBusException("No such property %s.%s" % (iface, name),
                                                name="org.freedesktop.DBus.Error.InvalidArgs")

    @dbus.service.method(PROPS, in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface):
        return dbus.Dictionary(self.props.get(iface, {}), signature="sv")

    @dbus.service.method(PROPS, in_signature="ssv")
    def Set(self, iface, name, value):
        self.on_set(iface, name, value)
        self.set_prop(iface, name, value)
        manager.changed()

    def on_set(self, iface, name, value):
        pass

    @dbus.service.signal(PROPS, signature="sa{sv}as")
    def PropertiesChanged(self, iface, changed, invalidated):
        pass


# ---------------------------------------------------------------------------

class AccessPoint(PropObject):
    count = 0

    def __init__(self, ssid, strength, freq, flags, wpa, rsn, bssid, bitrate=270000):
        AccessPoint.count += 1
        PropObject.__init__(self, "%s/AccessPoint/%d" % (NM_PATH, AccessPoint.count))
        self.ssid = ssid
        self.props[NM + ".AccessPoint"] = {
            "Ssid": dbus.Array(ay(ssid), signature="y"),
            "Strength": dbus.Byte(strength),
            "Frequency": dbus.UInt32(freq),
            "Flags": dbus.UInt32(flags),
            "WpaFlags": dbus.UInt32(wpa),
            "RsnFlags": dbus.UInt32(rsn),
            "HwAddress": bssid,
            "Mode": dbus.UInt32(2),
            "MaxBitrate": dbus.UInt32(bitrate),
            "LastSeen": dbus.Int32(1000),
        }


class IPConfig(PropObject):
    count = 0

    def __init__(self, v6, addresses, gateway, dns, domains=()):
        IPConfig.count += 1
        kind = "IP6Config" if v6 else "IP4Config"
        PropObject.__init__(self, "%s/%s/%d" % (NM_PATH, kind, IPConfig.count))
        addr = [dbus.Dictionary({"address": a.split("/")[0], "prefix": dbus.UInt32(int(a.split("/")[1]))},
                                signature="sv") for a in addresses]
        p = {
            "AddressData": dbus.Array(addr, signature="a{sv}"),
            "Gateway": gateway,
            "Domains": dbus.Array(list(domains), signature="s"),
            "Searches": dbus.Array([], signature="s"),
            "RouteData": dbus.Array([dbus.Dictionary({"dest": "0.0.0.0" if not v6 else "::", "prefix": dbus.UInt32(0),
                                                      "next-hop": gateway, "metric": dbus.UInt32(100)}, signature="sv")],
                                    signature="a{sv}"),
        }
        if v6:
            import socket
            p["Nameservers"] = dbus.Array([dbus.ByteArray(socket.inet_pton(socket.AF_INET6, d)) for d in dns],
                                          signature="ay")
        else:
            p["NameserverData"] = dbus.Array([dbus.Dictionary({"address": d}, signature="sv") for d in dns],
                                             signature="a{sv}")
            p["Nameservers"] = dbus.Array([ip4(d) for d in dns], signature="u")
        self.props[NM + "." + kind] = p


class Device(PropObject):
    count = 0

    def __init__(self, iface, dtype, driver, mac, speed=0):
        Device.count += 1
        PropObject.__init__(self, "%s/Devices/%d" % (NM_PATH, Device.count))
        self.iface = iface
        self.dtype = dtype
        self.aps = []
        self.rx = 123456789
        self.tx = 23456789
        self.props[NM + ".Device"] = {
            "Interface": iface, "IpInterface": iface, "Driver": driver, "DriverVersion": "6.12",
            "FirmwareVersion": "", "HwAddress": mac, "Vendor": "", "Product": "",
            "DeviceType": dbus.UInt32(dtype), "State": dbus.UInt32(30),
            "StateReason": dbus.Struct((dbus.UInt32(30), dbus.UInt32(0)), signature="uu"),
            "Managed": True, "Autoconnect": True, "Real": True, "Mtu": dbus.UInt32(1500),
            "Metered": dbus.UInt32(0), "ActiveConnection": dbus.ObjectPath("/"),
            "Ip4Config": dbus.ObjectPath("/"), "Ip6Config": dbus.ObjectPath("/"),
            "AvailableConnections": dbus.Array([], signature="o"),
        }
        self.props[NM + ".Device.Statistics"] = {
            "RefreshRateMs": dbus.UInt32(0), "RxBytes": dbus.UInt64(self.rx), "TxBytes": dbus.UInt64(self.tx)}
        if dtype == 1:
            self.props[NM + ".Device.Wired"] = {"Speed": dbus.UInt32(speed), "PermHwAddress": mac}
        if dtype == 2:
            self.props[NM + ".Device.Wireless"] = {
                "Bitrate": dbus.UInt32(speed), "PermHwAddress": mac, "WirelessCapabilities": dbus.UInt32(0x7ff),
                "ActiveAccessPoint": dbus.ObjectPath("/"), "AccessPoints": dbus.Array([], signature="o")}

    def add_ap(self, ap):
        self.aps.append(ap)
        self.props[NM + ".Device.Wireless"]["AccessPoints"] = dbus.Array([a.path for a in self.aps], signature="o")

    def set_state(self, new, reason=0):
        old = int(self.props[NM + ".Device"]["State"])
        self.set_prop(NM + ".Device", "State", dbus.UInt32(new))
        self.set_prop(NM + ".Device", "StateReason", dbus.Struct((dbus.UInt32(new), dbus.UInt32(reason)), signature="uu"))
        self.StateChanged(dbus.UInt32(new), dbus.UInt32(old), dbus.UInt32(reason))

    def tick(self):
        self.rx += 52000
        self.tx += 9000
        st = self.props[NM + ".Device.Statistics"]
        st["RxBytes"] = dbus.UInt64(self.rx)
        st["TxBytes"] = dbus.UInt64(self.tx)

    @dbus.service.signal(NM + ".Device", signature="uuu")
    def StateChanged(self, new, old, reason):
        pass

    @dbus.service.method(NM + ".Device")
    def Disconnect(self):
        ac = self.props[NM + ".Device"]["ActiveConnection"]
        if ac != "/":
            manager.deactivate(str(ac), 38)

    @dbus.service.method(NM + ".Device.Wireless", in_signature="a{sv}")
    def RequestScan(self, options):
        manager.scans += 1

    @dbus.service.method(NM + ".Device.Wireless", out_signature="ao")
    def GetAccessPoints(self):
        return [a.path for a in self.aps]


SECRET_KEYS = {
    "802-11-wireless-security": ["psk", "wep-key0", "wep-key1", "wep-key2", "wep-key3", "leap-password"],
    "802-1x": ["password", "private-key-password", "phase2-private-key-password", "pin"],
    "pppoe": ["password"], "gsm": ["password", "pin"], "cdma": ["password"], "wireguard": ["private-key"],
}


def secret_flags(settings, setting, key):
    s = settings.get(setting, {})
    if setting == "vpn":
        return int(s.get("data", {}).get(key + "-flags", "0"))
    if key.startswith("wep-key"):
        return int(s.get("wep-key-flags", 0))
    return int(s.get(key + "-flags", 0))


def split_secrets(settings):
    """removes agent-owned and not-saved secrets; returns (stored, agent_secrets)"""
    stored = dbus.Dictionary({}, signature="sa{sv}")
    agent = {}
    for sname, s in settings.items():
        s = dbus.Dictionary(s, signature="sv")
        keys = SECRET_KEYS.get(str(sname), [])
        for k in keys:
            if k in s and secret_flags(settings, sname, k) & 0x3:
                if secret_flags(settings, sname, k) & 0x1:
                    agent.setdefault(str(sname), {})[k] = s[k]
                del s[k]
        if sname == "vpn" and "secrets" in s:
            sec = dict(s["secrets"])
            for k in list(sec.keys()):
                if secret_flags(settings, "vpn", k) & 0x3:
                    if secret_flags(settings, "vpn", k) & 0x1:
                        agent.setdefault("vpn", {})[k] = sec[k]
                    del sec[k]
            s["secrets"] = dbus.Dictionary(sec, signature="ss")
        stored[sname] = s
    return stored, agent


def public_settings(settings):
    out = dbus.Dictionary({}, signature="sa{sv}")
    for sname, s in settings.items():
        s = dbus.Dictionary(s, signature="sv")
        for k in SECRET_KEYS.get(str(sname), []):
            s.pop(k, None)
        if sname == "vpn":
            s.pop("secrets", None)
        out[sname] = s
    return out


class Connection(PropObject):
    count = 0

    def __init__(self, settings):
        Connection.count += 1
        PropObject.__init__(self, "%s/Settings/%d" % (NM_PATH, Connection.count))
        self.settings, self.agent_secrets = split_secrets(settings)
        self.props[NM + ".Settings.Connection"] = {
            "Unsaved": False, "Flags": dbus.UInt32(0),
            "Filename": "/etc/NetworkManager/system-connections/%s.nmconnection" % self.id()}

    def id(self):
        return str(self.settings["connection"]["id"])

    def uuid(self):
        return str(self.settings["connection"]["uuid"])

    def type(self):
        return str(self.settings["connection"]["type"])

    @dbus.service.method(NM + ".Settings.Connection", out_signature="a{sa{sv}}")
    def GetSettings(self):
        return public_settings(self.settings)

    @dbus.service.method(NM + ".Settings.Connection", in_signature="s", out_signature="a{sa{sv}}",
                         async_callbacks=("ok", "err"), sender_keyword="sender")
    def GetSecrets(self, setting, ok, err, sender=None):
        s = self.settings.get(setting, {})
        result = {}
        if setting == "vpn":
            if "secrets" in s:
                result["secrets"] = s["secrets"]
        else:
            for k in SECRET_KEYS.get(setting, []):
                if k in s:
                    result[k] = s[k]
        reply = dbus.Dictionary({setting: dbus.Dictionary(result, signature="sv")}, signature="sa{sv}")
        agent = manager.agent
        # a client that is its own agent would wait for itself
        if agent is None or agent == sender:
            ok(reply)
            return

        def done(secrets):
            for k, v in secrets.get(setting, {}).items():
                reply[setting][k] = v
            ok(reply)

        manager.ask_agent(self, setting, [], 0, done, lambda e: ok(reply))

    @dbus.service.method(NM + ".Settings.Connection", in_signature="a{sa{sv}}")
    def Update(self, settings):
        self.settings, agent = split_secrets(settings)
        self.agent_secrets = agent
        manager.log.append(("Update", self.uuid(), strip_variants(settings)))
        if agent:
            manager.save_agent_secrets(self, settings)
        self.Updated()
        manager.changed()

    @dbus.service.method(NM + ".Settings.Connection")
    def Delete(self):
        manager.log.append(("Delete", self.uuid(), None))
        manager.settings.remove(self)

    @dbus.service.signal(NM + ".Settings.Connection")
    def Updated(self):
        pass

    @dbus.service.signal(NM + ".Settings.Connection")
    def Removed(self):
        pass


class ActiveConnection(PropObject):
    count = 0

    def __init__(self, conn, device, specific="/", vpn=False):
        ActiveConnection.count += 1
        PropObject.__init__(self, "%s/ActiveConnection/%d" % (NM_PATH, ActiveConnection.count))
        self.conn = conn
        self.device = device
        self.props[NM + ".Connection.Active"] = {
            "Id": conn.id(), "Uuid": conn.uuid(), "Type": conn.type(),
            "Connection": dbus.ObjectPath(conn.path), "SpecificObject": dbus.ObjectPath(specific),
            "Devices": dbus.Array([device.path] if device else [], signature="o"),
            "State": dbus.UInt32(2), "Default": False, "Default6": False, "Vpn": vpn,
            "Ip4Config": dbus.ObjectPath("/"), "Ip6Config": dbus.ObjectPath("/"),
        }
        if vpn:
            self.props[NM + ".VPN.Connection"] = {"VpnState": dbus.UInt32(5), "Banner": ""}


class Settings(PropObject):
    def __init__(self):
        PropObject.__init__(self, NM_PATH + "/Settings")
        self.connections = []
        self.props[NM + ".Settings"] = {"Hostname": "trinity-pc", "CanModify": True,
                                        "Connections": dbus.Array([], signature="o")}

    def add(self, settings, notify=True):
        c = Connection(settings)
        self.connections.append(c)
        self.props[NM + ".Settings"]["Connections"] = dbus.Array([x.path for x in self.connections], signature="o")
        if notify:
            self.NewConnection(c.path)
            manager.changed()
        return c

    def remove(self, c):
        self.connections.remove(c)
        self.props[NM + ".Settings"]["Connections"] = dbus.Array([x.path for x in self.connections], signature="o")
        for ac in list(manager.active):
            if ac.conn is c:
                manager.deactivate(ac.path, 37)
        c.Removed()
        self.ConnectionRemoved(c.path)
        c.remove_from_connection()
        manager.changed()

    def by_path(self, path):
        for c in self.connections:
            if c.path == path:
                return c
        return None

    @dbus.service.method(NM + ".Settings", out_signature="ao")
    def ListConnections(self):
        return dbus.Array([c.path for c in self.connections], signature="o")

    @dbus.service.method(NM + ".Settings", in_signature="a{sa{sv}}", out_signature="o")
    def AddConnection(self, settings):
        manager.log.append(("AddConnection", str(settings["connection"]["uuid"]), strip_variants(settings)))
        c = self.add(settings)
        if c.agent_secrets:
            manager.save_agent_secrets(c, settings)
        return c.path

    @dbus.service.method(NM + ".Settings", in_signature="a{sa{sv}}", out_signature="o")
    def AddConnectionUnsaved(self, settings):
        return self.AddConnection(settings)

    @dbus.service.method(NM + ".Settings", out_signature="b")
    def ReloadConnections(self):
        return True

    @dbus.service.method(NM + ".Settings", in_signature="s")
    def SaveHostname(self, hostname):
        self.set_prop(NM + ".Settings", "Hostname", hostname)

    @dbus.service.signal(NM + ".Settings", signature="o")
    def NewConnection(self, path):
        pass

    @dbus.service.signal(NM + ".Settings", signature="o")
    def ConnectionRemoved(self, path):
        pass


class AgentManager(dbus.service.Object):
    def __init__(self):
        dbus.service.Object.__init__(self, bus, NM_PATH + "/AgentManager")

    @dbus.service.method(NM + ".AgentManager", in_signature="s", sender_keyword="sender")
    def Register(self, identifier, sender=None):
        manager.agent = sender
        manager.log.append(("RegisterAgent", identifier, sender))

    @dbus.service.method(NM + ".AgentManager", in_signature="su", sender_keyword="sender")
    def RegisterWithCapabilities(self, identifier, caps, sender=None):
        self.Register(identifier, sender=sender)

    @dbus.service.method(NM + ".AgentManager", sender_keyword="sender")
    def Unregister(self, sender=None):
        if manager.agent == sender:
            manager.agent = None


class Manager(PropObject):
    def __init__(self):
        PropObject.__init__(self, NM_PATH)
        self.devices = []
        self.active = []
        self.agent = None
        self.log = []
        self.scans = 0
        self.last_secrets = {}
        self.timer = None
        self.props[NM] = {
            "Version": "1.52.0 (mock)", "State": dbus.UInt32(20), "Connectivity": dbus.UInt32(1),
            "NetworkingEnabled": True, "WirelessEnabled": True, "WirelessHardwareEnabled": True,
            "WwanEnabled": False, "WwanHardwareEnabled": True, "ConnectivityCheckAvailable": True,
            "ConnectivityCheckEnabled": True, "PrimaryConnection": dbus.ObjectPath("/"),
            "Devices": dbus.Array([], signature="o"), "AllDevices": dbus.Array([], signature="o"),
            "ActiveConnections": dbus.Array([], signature="o"), "Metered": dbus.UInt32(4),
        }

    def changed(self):
        """recompute the derived properties and tell the clients"""
        p = self.props[NM]
        p["Devices"] = dbus.Array([d.path for d in self.devices], signature="o")
        p["AllDevices"] = p["Devices"]
        p["ActiveConnections"] = dbus.Array([a.path for a in self.active], signature="o")
        primary = next((a for a in self.active if a.props[NM + ".Connection.Active"]["Default"]), None)
        if primary is None:
            primary = next((a for a in self.active if a.device is not None), None)
            if primary is not None:
                primary.props[NM + ".Connection.Active"]["Default"] = True
        p["PrimaryConnection"] = dbus.ObjectPath(primary.path if primary else "/")
        if not p["NetworkingEnabled"]:
            state, conn = 10, 1
        elif primary is not None:
            state, conn = 70, 4
        else:
            state, conn = 20, 1
        p["State"] = dbus.UInt32(state)
        p["Connectivity"] = dbus.UInt32(conn)
        for d in self.devices:
            d.props[NM + ".Device"]["AvailableConnections"] = dbus.Array(
                [c.path for c in self.settings.connections if self.fits(c, d)], signature="o")
        self.PropertiesChanged(NM, {"State": p["State"], "ActiveConnections": p["ActiveConnections"]}, [])
        self.StateChanged(p["State"])

    @staticmethod
    def fits(c, d):
        t = c.type()
        iface = str(c.settings["connection"].get("interface-name", ""))
        if iface and iface != d.iface:
            return False
        return (t == "802-3-ethernet" and d.dtype == 1) or (t == "802-11-wireless" and d.dtype == 2)

    def device_for(self, c):
        for d in self.devices:
            if self.fits(c, d):
                return d
        return None

    def ask_agent(self, conn, setting, hints, flags, ok, err):
        full = dbus.Dictionary(public_settings(conn.settings), signature="sa{sv}")
        bus.call_async(self.agent, NM_PATH + "/SecretAgent", NM + ".SecretAgent", "GetSecrets",
                       "a{sa{sv}}osasu", (full, dbus.ObjectPath(conn.path), setting, hints, dbus.UInt32(flags)),
                       ok, err, timeout=600)

    def save_agent_secrets(self, conn, settings):
        if self.agent is None:
            return
        bus.call_async(self.agent, NM_PATH + "/SecretAgent", NM + ".SecretAgent", "SaveSecrets",
                       "a{sa{sv}}o", (settings, dbus.ObjectPath(conn.path)),
                       lambda *a: None, lambda e: None)

    def activate(self, conn, device=None, specific="/", agent_flags=0x1):
        if device is None and conn.type() in ("vpn", "wireguard"):
            ac = ActiveConnection(conn, None, specific, vpn=conn.type() == "vpn")
            cfg = IPConfig(False, ["10.8.0.6/24"], "10.8.0.1", ["10.8.0.1"])
            ac.props[NM + ".Connection.Active"]["Ip4Config"] = dbus.ObjectPath(cfg.path)
            self.active.append(ac)
            self.changed()
            return ac
        if device is None:
            device = self.device_for(conn)
        if device is None:
            raise dbus.exceptions.DBusException("No suitable device found for this connection",
                                                name=NM + ".Error.UnknownDevice")
        old = device.props[NM + ".Device"]["ActiveConnection"]
        if old != "/":
            self.deactivate(str(old), 38)
        ac = ActiveConnection(conn, device, specific)
        self.active.append(ac)
        device.set_prop(NM + ".Device", "ActiveConnection", dbus.ObjectPath(ac.path))
        if device.dtype == 2:
            ap = next((a for a in device.aps if a.path == specific), None)
            if ap is None:
                ssid = bytes(bytearray(conn.settings.get("802-11-wireless", {}).get("ssid", [])))
                ap = next((a for a in device.aps if a.ssid.encode() == ssid), None)
            device.set_prop(NM + ".Device.Wireless", "ActiveAccessPoint",
                            dbus.ObjectPath(ap.path if ap else "/"))
            ac.props[NM + ".Connection.Active"]["SpecificObject"] = dbus.ObjectPath(ap.path if ap else "/")

        def finish():
            v4 = IPConfig(False, ["192.168.1.%d/24" % (20 + device.dtype)], "192.168.1.1",
                          ["192.168.1.1", "9.9.9.9"], ["home.lan"])
            v6 = IPConfig(True, ["fd00::%d/64" % (20 + device.dtype)], "fe80::1", ["fd00::1"])
            device.set_prop(NM + ".Device", "Ip4Config", dbus.ObjectPath(v4.path))
            device.set_prop(NM + ".Device", "Ip6Config", dbus.ObjectPath(v6.path))
            ac.props[NM + ".Connection.Active"]["Ip4Config"] = dbus.ObjectPath(v4.path)
            ac.props[NM + ".Connection.Active"]["Ip6Config"] = dbus.ObjectPath(v6.path)
            ac.set_prop(NM + ".Connection.Active", "State", dbus.UInt32(2))
            device.set_state(100)
            conn.settings["connection"]["timestamp"] = dbus.UInt64(1767225600)
            self.changed()

        # like NetworkManager: ask the agent for secrets that are not stored
        sec = conn.settings.get("802-11-wireless-security")
        need = None
        if sec is not None and str(sec.get("key-mgmt", "")) in ("wpa-psk", "sae"):
            if "psk" not in sec and "psk" not in conn.agent_secrets.get("802-11-wireless-security", {}):
                need = "psk"
            elif secret_flags(conn.settings, "802-11-wireless-security", "psk") & 0x1:
                need = "psk"
        if need and self.agent is not None:
            ac.props[NM + ".Connection.Active"]["State"] = dbus.UInt32(1)
            device.set_state(60)

            def got(secrets):
                self.last_secrets = strip_variants(secrets)
                finish()

            def failed(e):
                self.last_secrets = {"error": e.get_dbus_name()}
                device.set_state(120, 7)
                self.deactivate(ac.path, 7)

            self.ask_agent(conn, "802-11-wireless-security", ["psk"], agent_flags, got, failed)
        else:
            finish()
        self.changed()
        return ac

    def deactivate(self, path, reason=38):
        ac = next((a for a in self.active if a.path == path), None)
        if ac is None:
            raise dbus.exceptions.DBusException("Not active", name=NM + ".Error.ConnectionNotActive")
        self.active.remove(ac)
        if ac.device is not None:
            ac.device.set_prop(NM + ".Device", "ActiveConnection", dbus.ObjectPath("/"))
            ac.device.set_prop(NM + ".Device", "Ip4Config", dbus.ObjectPath("/"))
            ac.device.set_prop(NM + ".Device", "Ip6Config", dbus.ObjectPath("/"))
            if ac.device.dtype == 2:
                ac.device.set_prop(NM + ".Device.Wireless", "ActiveAccessPoint", dbus.ObjectPath("/"))
            if int(ac.device.props[NM + ".Device"]["State"]) != 120:
                ac.device.set_state(30, reason)
        ac.remove_from_connection()
        self.changed()

    def on_set(self, iface, name, value):
        if name == "WirelessEnabled" and not value:
            for ac in list(self.active):
                if ac.device is not None and ac.device.dtype == 2:
                    self.deactivate(ac.path, 36)

    # -- D-Bus API --------------------------------------------------------

    @dbus.service.method(NM, out_signature="ao")
    def GetDevices(self):
        return dbus.Array([d.path for d in self.devices], signature="o")

    @dbus.service.method(NM, out_signature="ao")
    def GetAllDevices(self):
        return self.GetDevices()

    @dbus.service.method(NM, in_signature="ooo", out_signature="o")
    def ActivateConnection(self, conn_path, dev_path, specific):
        conn = self.settings.by_path(str(conn_path))
        if conn is None:
            raise dbus.exceptions.DBusException("Unknown connection", name=NM + ".Error.UnknownConnection")
        dev = next((d for d in self.devices if d.path == dev_path), None)
        self.log.append(("ActivateConnection", conn.uuid(), str(dev_path)))
        return self.activate(conn, dev, str(specific)).path

    @dbus.service.method(NM, in_signature="a{sa{sv}}oo", out_signature="oo")
    def AddAndActivateConnection(self, settings, dev_path, specific):
        settings = dbus.Dictionary(settings, signature="sa{sv}")
        if "connection" not in settings:
            settings["connection"] = dbus.Dictionary({}, signature="sv")
        c = settings["connection"]
        if "uuid" not in c:
            c["uuid"] = str(uuidlib.uuid4())
        if "id" not in c:
            c["id"] = "New connection"
        if "type" not in c:
            c["type"] = "802-11-wireless"
        self.log.append(("AddAndActivateConnection", str(c["uuid"]), strip_variants(settings)))
        conn = self.settings.add(settings)
        if conn.agent_secrets:
            self.save_agent_secrets(conn, settings)
        dev = next((d for d in self.devices if d.path == dev_path), None)
        ac = self.activate(conn, dev, str(specific))
        return (dbus.ObjectPath(conn.path), dbus.ObjectPath(ac.path))

    @dbus.service.method(NM, in_signature="o")
    def DeactivateConnection(self, path):
        self.log.append(("DeactivateConnection", str(path), None))
        self.deactivate(str(path))

    @dbus.service.method(NM, in_signature="b")
    def Enable(self, on):
        self.props[NM]["NetworkingEnabled"] = bool(on)
        if not on:
            for ac in list(self.active):
                self.deactivate(ac.path, 36)
        self.changed()

    @dbus.service.method(NM, out_signature="u")
    def CheckConnectivity(self):
        return self.props[NM]["Connectivity"]

    @dbus.service.signal(NM, signature="u")
    def StateChanged(self, state):
        pass

    # -- for tests ----------------------------------------------------------

    @dbus.service.method(MOCK, out_signature="s")
    def Log(self):
        return repr(self.log)

    @dbus.service.method(MOCK, out_signature="s")
    def LastSecrets(self):
        return repr(self.last_secrets)

    @dbus.service.method(MOCK, out_signature="s")
    def Agent(self):
        return self.agent or ""

    @dbus.service.method(MOCK, in_signature="s", out_signature="s")
    def ConnectionDump(self, uuid):
        for c in self.settings.connections:
            if c.uuid() == uuid:
                return repr({"stored": strip_variants(c.settings), "agent": strip_variants(c.agent_secrets)})
        return ""

    @dbus.service.method(MOCK, in_signature="s")
    def ActivateById(self, cid):
        self.ActivateByIdFlags(cid, 0x1)

    @dbus.service.method(MOCK, in_signature="su")
    def ActivateByIdFlags(self, cid, flags):
        """flags 0x3: ask the agent for new secrets, as after a wrong password"""
        for c in self.settings.connections:
            if c.id() == cid:
                self.activate(c, agent_flags=int(flags))
                return
        raise dbus.exceptions.DBusException("no connection " + cid)


# ---------------------------------------------------------------------------

def conn_settings(cid, ctype, extra, timestamp=0, uid=None):
    s = {"connection": {"id": cid, "uuid": uid or str(uuidlib.uuid4()), "type": ctype,
                        "autoconnect": dbus.Boolean(True)},
         "ipv4": {"method": "auto"}, "ipv6": {"method": "auto"}}
    if timestamp:
        s["connection"]["timestamp"] = dbus.UInt64(timestamp)
    for k, v in extra.items():
        s.setdefault(k, {}).update(v)
    return dbus.Dictionary({k: dbus.Dictionary(v, signature="sv") for k, v in s.items()}, signature="sa{sv}")


def setup(scenario):
    global manager
    manager = Manager()
    manager.settings = Settings()
    AgentManager()

    eth = Device("enp3s0", 1, "r8169", "3C:7C:3F:12:34:56", speed=1000)
    wifi = Device("wlp2s0", 2, "iwlwifi", "A4:C3:F0:65:43:21", speed=866000)
    manager.devices = [eth, wifi]
    if scenario == "empty":
        manager.changed()
        return

    for args in [("HomeNet", 82, 5180, 0x1, 0, 0x188, "B0:BE:76:11:22:33"),
                 ("HomeNet", 61, 2437, 0x1, 0, 0x188, "B0:BE:76:11:22:34"),
                 ("Cafe Libre", 54, 2412, 0x0, 0, 0, "00:1A:2B:3C:4D:5E"),
                 ("Office-5G", 71, 5500, 0x1, 0, 0x288, "F4:F2:6D:AA:BB:CC"),
                 ("Library Guest", 47, 2462, 0x1, 0x188, 0x188, "C8:3A:35:01:02:03"),
                 ("Neighbor WPA3", 33, 5745, 0x1, 0, 0x488, "9C:53:22:44:55:66"),
                 ("PrinterSetup", 18, 2422, 0x1, 0, 0x188, "E4:6F:13:77:88:99"),
                 ("", 25, 2442, 0x1, 0, 0x188, "12:34:56:78:9A:BC")]:
        wifi.add_ap(AccessPoint(*args))

    st = manager.settings
    wired = st.add(conn_settings("Wired connection 1", "802-3-ethernet", {"802-3-ethernet": {}},
                                 1767200000, "9e1f4c2a-6a37-4c27-9d7d-2f7b1c0a4b11"), False)
    home = st.add(conn_settings("HomeNet", "802-11-wireless", {
        "802-11-wireless": {"ssid": dbus.Array(ay("HomeNet"), signature="y"), "mode": "infrastructure"},
        "802-11-wireless-security": {"key-mgmt": "wpa-psk", "psk-flags": dbus.UInt32(1)}},
        1767220000, "4b8e6f0d-3f4c-4a6e-8b3c-0d9a1e2f3c44"), False)
    st.add(conn_settings("Office-5G", "802-11-wireless", {
        "802-11-wireless": {"ssid": dbus.Array(ay("Office-5G"), signature="y"), "mode": "infrastructure"},
        "802-11-wireless-security": {"key-mgmt": "wpa-eap"},
        "802-1x": {"eap": dbus.Array(["peap"], signature="s"), "identity": "jdoe", "phase2-auth": "mschapv2",
                   "domain-suffix-match": "example.com", "password-flags": dbus.UInt32(1),
                   "ca-cert": dbus.Array(ay("file:///etc/ssl/certs/ISRG_Root_X1.pem\0"), signature="y")}},
        1766000000), False)
    st.add(conn_settings("Work VPN", "vpn", {
        "vpn": {"service-type": "org.freedesktop.NetworkManager.openvpn",
                "data": dbus.Dictionary({"remote": "vpn.example.com", "connection-type": "password-tls",
                                         "username": "jdoe", "ca": "/home/jdoe/vpn/ca.crt",
                                         "cert": "/home/jdoe/vpn/jdoe.crt", "key": "/home/jdoe/vpn/jdoe.key",
                                         "password-flags": "1", "cert-pass-flags": "4",
                                         "port": "1194", "cipher": "AES-256-GCM", "tls-crypt": "/home/jdoe/vpn/tc.key"},
                                        signature="ss")},
        "connection": {"autoconnect": dbus.Boolean(False)}}, 1765000000), False)
    st.add(conn_settings("wg-home", "wireguard", {
        "wireguard": {"listen-port": dbus.UInt32(51820), "private-key-flags": dbus.UInt32(1),
                      "peers": dbus.Array([dbus.Dictionary({"public-key": "xTIBA5rboUvnH4htodjb6e697QjLERt1NAB4mZqp8Dg=",
                                                            "endpoint": "home.example.org:51820",
                                                            "allowed-ips": dbus.Array(["10.10.0.0/24"], signature="s"),
                                                            "persistent-keepalive": dbus.UInt32(25)}, signature="sv")],
                                          signature="a{sv}")},
        "connection": {"interface-name": "wg0", "autoconnect": dbus.Boolean(False)},
        "ipv4": {"method": "manual", "address-data": dbus.Array([dbus.Dictionary(
            {"address": "10.10.0.2", "prefix": dbus.UInt32(24)}, signature="sv")], signature="a{sv}")},
        "ipv6": {"method": "ignore"}}), False)
    st.add(conn_settings("Office LAN", "802-3-ethernet", {
        "802-3-ethernet": {"mtu": dbus.UInt32(1500)},
        "connection": {"autoconnect": dbus.Boolean(False), "zone": "work"},
        "ipv4": {"method": "manual",
                 "address-data": dbus.Array([dbus.Dictionary({"address": "10.20.30.44", "prefix": dbus.UInt32(24)},
                                                             signature="sv")], signature="a{sv}"),
                 "gateway": "10.20.30.1",
                 "dns": dbus.Array([ip4("10.20.30.2"), ip4("10.20.30.3")], signature="u"),
                 "dns-search": dbus.Array(["corp.example.com"], signature="s")}}, 1764000000), False)
    st.add(conn_settings("Mobile (LTE)", "gsm", {"gsm": {"apn": "internet", "pin-flags": dbus.UInt32(1)}}), False)
    st.add(conn_settings("Bridge br0", "bridge", {"bridge": {"stp": dbus.Boolean(True)},
                                                  "connection": {"interface-name": "br0",
                                                                 "autoconnect": dbus.Boolean(False)}}), False)

    manager.agent_secret_store = {}
    manager.changed()
    manager.activate(wired, eth)
    # HomeNet's password is the user's (agent-owned): connected at startup
    # without asking, as if it had been given earlier
    ac = ActiveConnection(home, wifi, wifi.aps[0].path)
    manager.active.append(ac)
    wifi.set_prop(NM + ".Device", "ActiveConnection", dbus.ObjectPath(ac.path), False)
    wifi.set_prop(NM + ".Device.Wireless", "ActiveAccessPoint", dbus.ObjectPath(wifi.aps[0].path), False)
    v4 = IPConfig(False, ["192.168.1.42/24"], "192.168.1.1", ["192.168.1.1"], ["home.lan"])
    wifi.set_prop(NM + ".Device", "Ip4Config", dbus.ObjectPath(v4.path), False)
    ac.props[NM + ".Connection.Active"]["Ip4Config"] = dbus.ObjectPath(v4.path)
    wifi.props[NM + ".Device"]["State"] = dbus.UInt32(100)
    manager.active[0].props[NM + ".Connection.Active"]["Default"] = True
    manager.changed()


def tick():
    for d in manager.devices:
        d.tick()
    return True


def main():
    global bus
    scenario = "home"
    if "--scenario" in sys.argv:
        scenario = sys.argv[sys.argv.index("--scenario") + 1]
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    name = dbus.service.BusName(NM, bus)   # noqa: F841 (keeps the name)
    setup(scenario)
    GLib.timeout_add(1000, tick)
    print("mock NetworkManager ready", flush=True)
    GLib.MainLoop().run()


if __name__ == "__main__":
    main()
