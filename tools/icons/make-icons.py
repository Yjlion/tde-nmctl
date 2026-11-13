#!/usr/bin/python3
# Writes the SVG sources of tde-nmctl's icons into this directory.
# Then run ./render.sh to make the PNG and SVGZ files in ../../icons.
# This file is released under GPL >= 2.

import math

DARK = "#1f4f8f"
DARK_STROKE = "#123461"
LIGHT = "#c3ccd8"
LIGHT_STROKE = "#9aa6b6"
GREEN = "#3c9b3c"


def svg(body, defs=""):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">'
            '<defs>%s</defs>%s</svg>\n' % (defs, body))


def wedge(r0, r1, a0=-135, a1=-45, cx=24, cy=40):
    """an annular sector of the Wi-Fi fan, angles in degrees (0 = right)"""
    def pt(r, a):
        return cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))
    p0, p1 = pt(r1, a0), pt(r1, a1)
    p2, p3 = pt(r0, a1), pt(r0, a0)
    if r0 == 0:
        return "M%.2f %.2fL%.2f %.2fA%g %g 0 0 1 %.2f %.2fZ" % (cx, cy, p0[0], p0[1], r1, r1, p1[0], p1[1])
    return ("M%.2f %.2fA%g %g 0 0 1 %.2f %.2fL%.2f %.2fA%g %g 0 0 0 %.2f %.2fZ"
            % (p0[0], p0[1], r1, r1, p1[0], p1[1], p2[0], p2[1], r0, r0, p3[0], p3[1]))


def fan(level, on=DARK, on_stroke=DARK_STROKE, off=LIGHT, off_stroke=LIGHT_STROKE):
    """level 0..4 filled segments"""
    rings = [(0, 9), (12, 18), (21, 27), (30, 36)]
    out = ""
    for i, (r0, r1) in enumerate(rings):
        lit = i < level
        out += '<path d="%s" fill="%s" stroke="%s" stroke-width="1.2" stroke-linejoin="round"/>' % (
            wedge(r0, r1), on if lit else off, on_stroke if lit else off_stroke)
    return out


def ethernet(color="#2f6fb5", stroke="#163a66"):
    return ('<rect x="7" y="8" width="34" height="28" rx="3" fill="%s" stroke="%s" stroke-width="2"/>'
            '<path d="M14 14h20v12h-4v4h-12v-4h-4z" fill="#f4f6f9" stroke="%s" stroke-width="1.5" stroke-linejoin="round"/>'
            '<path d="M18 14v5M21 14v5M24 14v5M27 14v5M30 14v5" stroke="#d8a21a" stroke-width="1.6"/>'
            '<path d="M24 36v6M14 42h20" stroke="%s" stroke-width="3" stroke-linecap="round"/>'
            % (color, stroke, stroke, stroke))


def lock_badge(x=29, y=27, s=1.0):
    return ('<g transform="translate(%g %g) scale(%g)">'
            '<path d="M5 8V5.5a5 5 0 0 1 10 0V8" fill="none" stroke="#5a4300" stroke-width="2.6"/>'
            '<rect x="1.5" y="8" width="17" height="12" rx="2" fill="#f2c230" stroke="#5a4300" stroke-width="1.6"/>'
            '<circle cx="10" cy="13" r="1.8" fill="#5a4300"/><path d="M10 14v3" stroke="#5a4300" stroke-width="1.6"/>'
            '</g>' % (x, y, s))


def warn_badge():
    return ('<path d="M36 26l10 18H26z" fill="#f2c230" stroke="#6b4e00" stroke-width="1.6" stroke-linejoin="round"/>'
            '<path d="M36 31.5v6" stroke="#3a2a00" stroke-width="2.4" stroke-linecap="round"/>'
            '<circle cx="36" cy="40.8" r="1.4" fill="#3a2a00"/>')


def cross_badge():
    return ('<circle cx="37" cy="37" r="9" fill="#d23c3c" stroke="#7a1414" stroke-width="1.5"/>'
            '<path d="M33 33l8 8M41 33l-8 8" stroke="#fff" stroke-width="2.6" stroke-linecap="round"/>')


def modem():
    out = ""
    for i, h in enumerate([10, 17, 24, 31]):
        x = 8 + i * 9
        out += '<rect x="%d" y="%d" width="6.5" height="%d" rx="1" fill="%s" stroke="%s" stroke-width="1.2"/>' % (
            x, 42 - h, h, DARK, DARK_STROKE)
    return out


def vpn():
    return ('<path d="M24 4l16 6v11c0 10-7 17-16 23C15 38 8 31 8 21V10z" fill="#2f6fb5" stroke="#163a66" '
            'stroke-width="2" stroke-linejoin="round"/>' + lock_badge(14, 13, 1.0))


def virtual():
    return ('<rect x="4" y="15" width="16" height="18" rx="3" fill="#6c8fc7" stroke="#1d335a" stroke-width="2"/>'
            '<rect x="28" y="15" width="16" height="18" rx="3" fill="#6c8fc7" stroke="#1d335a" stroke-width="2"/>'
            '<path d="M20 24h8" stroke="#1d335a" stroke-width="3"/>'
            '<path d="M12 33v7h24v-7" fill="none" stroke="#1d335a" stroke-width="2.4"/>'
            '<circle cx="24" cy="40" r="3" fill="#f2c230" stroke="#5a4300" stroke-width="1.4"/>')


def connecting():
    dots = "".join('<circle cx="%d" cy="42" r="3.2" fill="%s"/>' % (x, c)
                   for x, c in [(30, "#1f4f8f"), (37, "#4f7fbf"), (44, "#8fb0dd")])
    return fan(4, on=LIGHT, on_stroke=LIGHT_STROKE) + dots


def app_icon():
    defs = ('<linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4f86c6"/>'
            '<stop offset="1" stop-color="#1f4f8f"/></linearGradient>')
    body = ('<rect x="3" y="3" width="42" height="42" rx="8" fill="url(#g)" stroke="#123461" stroke-width="2"/>')
    rings = [(0, 6), (9, 13), (16, 20), (23, 27)]
    for r0, r1 in rings:
        body += '<path d="%s" fill="#ffffff" fill-opacity="0.95"/>' % wedge(r0, r1, cx=24, cy=36)
    body += '<circle cx="38" cy="38" r="6" fill="%s" stroke="#fff" stroke-width="1.6"/>' % GREEN
    return svg(body, defs)


icons = {
    "tde-nmctl": app_icon(),
    "tde-nmctl-wired": svg(ethernet()),
    "tde-nmctl-wired-limited": svg(ethernet() + warn_badge()),
    "tde-nmctl-offline": svg(ethernet("#9aa6b6", "#5f6b7a") + cross_badge()),
    "tde-nmctl-disabled": svg(fan(4, on=LIGHT, on_stroke=LIGHT_STROKE) +
                              '<path d="M8 8l32 32" stroke="#7a1414" stroke-width="3.5" stroke-linecap="round"/>'),
    "tde-nmctl-connecting": svg(connecting()),
    "tde-nmctl-modem": svg(modem()),
    "tde-nmctl-vpn": svg(vpn()),
    "tde-nmctl-virtual": svg(virtual()),
}
for level, n in [(0, 0), (25, 1), (50, 2), (75, 3), (100, 4)]:
    icons["tde-nmctl-signal-%d" % level] = svg(fan(n))
# with an active VPN connection
for base in ["tde-nmctl-wired", "tde-nmctl-wired-limited", "tde-nmctl-modem"] + \
        ["tde-nmctl-signal-%d" % l for l in (0, 25, 50, 75, 100)]:
    icons[base + "-vpn"] = icons[base].replace("</svg>", lock_badge(27, 25, 1.0) + "</svg>")

for name, data in icons.items():
    with open(name + ".svg", "w") as f:
        f.write(data)
print("\n".join(sorted(icons)))
