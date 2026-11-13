#!/usr/bin/python3
# Compares two ConnectionDump results of mock-nm.py and prints what an
# editor round trip changed. Keys the editor writes with their default
# value are not counted as changes.
import ast
import sys

DEFAULTS = {
    ("connection", "autoconnect"): True, ("connection", "autoconnect-priority"): 0,
    ("connection", "autoconnect-retries"): -1, ("connection", "permissions"): [],
    ("connection", "metered"): 0,
    ("ipv4", "ignore-auto-dns"): False, ("ipv4", "ignore-auto-routes"): False,
    ("ipv4", "never-default"): False, ("ipv4", "may-fail"): True,
    ("ipv6", "ignore-auto-dns"): False, ("ipv6", "ignore-auto-routes"): False,
    ("ipv6", "never-default"): False, ("ipv6", "may-fail"): True,
    ("ipv6", "ip6-privacy"): -1, ("ipv6", "addr-gen-mode"): 3,
    ("ipv4", "address-data"): [], ("ipv6", "address-data"): [],
    ("ipv4", "route-data"): [], ("ipv6", "route-data"): [],
    ("ipv4", "dns"): [], ("ipv6", "dns"): [], ("ipv4", "dns-search"): [], ("ipv6", "dns-search"): [],
    ("802-11-wireless", "hidden"): False, ("802-11-wireless", "powersave"): 0,
    ("802-11-wireless-security", "pmf"): 0,
    ("802-3-ethernet", "auto-negotiate"): False, ("802-3-ethernet", "wake-on-lan"): 1,
    ("proxy", "method"): 0, ("proxy", "browser-only"): False,
    ("wireguard", "fwmark"): 0, ("wireguard", "mtu"): 0, ("wireguard", "peer-routes"): True,
    ("bridge", "ageing-time"): 300, ("bridge", "multicast-snooping"): True, ("bridge", "priority"): 32768,
    ("bridge", "forward-delay"): 15, ("bridge", "hello-time"): 2, ("bridge", "max-age"): 20,
    ("bridge", "vlan-filtering"): False, ("gsm", "home-only"): False, ("gsm", "password-flags"): 1,
    ("802-1x", "password-flags"): 1, ("wireguard", "private-key-flags"): 1,
}


def flat(d):
    out = {}
    for s, keys in d.get("stored", {}).items():
        if not keys:
            out[(s, None)] = "present"
        for k, v in keys.items():
            out[(s, k)] = v
    return out


before = flat(ast.literal_eval(sys.argv[1]))
after = flat(ast.literal_eval(sys.argv[2]))
bad = []
for key in sorted(set(before) | set(after), key=str):
    b, a = before.get(key, "<unset>"), after.get(key, "<unset>")
    if b == a:
        continue
    if key[1] is None:
        continue
    if b == "<unset>" and DEFAULTS.get(key, object()) == a:
        continue
    if key[0] == "vpn" and key[1] == "data":
        b = {k: v for k, v in b.items()} if isinstance(b, dict) else b
        a = {k: v for k, v in a.items()} if isinstance(a, dict) else a
        if b == a:
            continue
    bad.append("  %s.%s: %r -> %r" % (key[0], key[1], b, a))
if bad:
    print("\n".join(bad))
    sys.exit(1)
