"""Panel art for PLEAT and Slope AC: SVG sources and the PNGs the editors load.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

    python3 tools/art/make_panels.py            # write SVG and PNG
    python3 tools/art/make_panels.py --svg-only # write SVG only

The SVGs are the source. Lettering is outlined from a Helvetica-class sans
(Liberation Sans; Arial is the metric twin on macOS), so the files carry no
<text> and render the same in any SVG renderer. The standalone plates match
SergeMiddle/PleatView.h and SlopeAC/SlopeView.h. The combined plate is one
1600 by 960 paperface grid; jack fractions match SergeMiddleCombined/PatchView.h.

Needs fontTools. PNG output also needs cairosvg and Pillow.
The plate PNGs are RGB (colour type 2). The knob and switch PNGs are RGBA.
"""

from __future__ import annotations

import argparse
import csv
import io
import math
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[2]
W, H = 416, 624

import sys

if str(Path(__file__).resolve().parent) not in sys.path:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
from paper_plate import PLATE_H, PLATE_W, render as render_paper_plate
WHITE = "#f3f5f8"

FONT_CANDIDATES = {
    "bold": [
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/Library/Fonts/LiberationSans-Bold.ttf",
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    ],
    "regular": [
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/Library/Fonts/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    ],
}


def load_font(kind: str, override: str | None) -> TTFont:
    paths = [override] if override else FONT_CANDIDATES[kind]
    for path in paths:
        if path and Path(path).is_file():
            return TTFont(path)
    raise SystemExit(f"no {kind} font found; pass --font-{kind}")


BOLD: TTFont
REG: TTFont


def text_path(s, x, y, size, font=None, spacing=0.0, anchor="middle"):
    font = font or BOLD
    gs = font.getGlyphSet()
    cmap = font.getBestCmap()
    k = size / font["head"].unitsPerEm
    names = [cmap[ord(c)] for c in s]
    adv = [gs[n].width * k for n in names]
    total = sum(adv) + spacing * (len(s) - 1)
    x0 = x - total / 2 if anchor == "middle" else (x - total if anchor == "end" else x)
    pen = SVGPathPen(gs)
    cx = x0
    for n, a in zip(names, adv):
        gs[n].draw(TransformPen(pen, (k, 0, 0, -k, cx, y)))
        cx += a + spacing
    return pen.getCommands()


def text_width(s, size, font, spacing=0.0):
    gs = font.getGlyphSet()
    cmap = font.getBestCmap()
    k = size / font["head"].unitsPerEm
    return sum(gs[cmap[ord(c)]].width * k for c in s) + spacing * (len(s) - 1)


def f(v):
    return f"{v:.2f}".rstrip("0").rstrip(".")


# Shared layout fractions.
KNOB_LX, KNOB_RX, KNOB_Y = 0.30 * W, 0.70 * W, 0.38 * H
JACK_Y = 0.785 * H
JACK_X = [0.18 * W, 0.39 * W, 0.61 * W, 0.82 * W]
CAP_T, CAP_B = 0.495 * H, 0.545 * H
JACK_COLLARS = {
    "black": ("#17181c", "#3b3e45"),
    "blue": ("#2557c9", "#13317a"),
    "white": ("#e9eaec", "#8d9097"),
    "red": ("#cc2a2c", "#6d1213"),
}

DEFS = ('<defs>'
        '<linearGradient id="panel" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#141b2b"/><stop offset="0.55" stop-color="#0e1422"/><stop offset="1" stop-color="#0a0f1a"/></linearGradient>'
        '<radialGradient id="screw" cx="0.38" cy="0.32" r="0.75">'
        '<stop offset="0" stop-color="#d9dde3"/><stop offset="0.6" stop-color="#8b9099"/><stop offset="1" stop-color="#4b5059"/></radialGradient>'
        '<radialGradient id="metal" cx="0.4" cy="0.35" r="0.7">'
        '<stop offset="0" stop-color="#eef0f3"/><stop offset="0.7" stop-color="#a3a8b0"/><stop offset="1" stop-color="#5d626b"/></radialGradient>'
        '<radialGradient id="readout" cx="0.5" cy="0.3" r="0.9">'
        '<stop offset="0" stop-color="#0b101b"/><stop offset="1" stop-color="#05080e"/></radialGradient>'
        '</defs>')


def panel_base(o):
    o.append(DEFS)
    o.append(f'<rect width="{W}" height="{H}" fill="url(#panel)"/>')
    o.append(f'<rect x="1.5" y="1.5" width="{W-3}" height="{H-3}" fill="none" stroke="#2a3348" stroke-width="1"/>')
    for sx, sy, a in [(22, 22, 35), (W - 22, 22, -20), (22, H - 22, 70), (W - 22, H - 22, 10)]:
        o.append(f'<circle cx="{sx}" cy="{sy}" r="7.5" fill="url(#screw)" stroke="#05070c" stroke-width="1"/>')
        dx, dy = 5.2 * math.cos(math.radians(a)), 5.2 * math.sin(math.radians(a))
        o.append(f'<line x1="{f(sx-dx)}" y1="{f(sy-dy)}" x2="{f(sx+dx)}" y2="{f(sy+dy)}" stroke="#2b2f36" stroke-width="1.8" stroke-linecap="round"/>')


def readouts(o):
    for cx in (KNOB_LX, KNOB_RX):
        o.append(f'<rect x="{f(cx-44)}" y="{f(CAP_T+2)}" width="88" height="{f(CAP_B-CAP_T-4)}" rx="3" fill="url(#readout)" stroke="{WHITE}" stroke-opacity="0.45" stroke-width="0.8"/>')


def jacks(o, legends, colours, xs=None, prefix=""):
    xs = JACK_X if xs is None else xs
    o.append(f'<rect x="24" y="430" width="{W-48}" height="112" rx="4" fill="none" stroke="{WHITE}" stroke-width="1.2"/>')
    for jx, lab, colour in zip(xs, legends, colours):
        c0, c1 = JACK_COLLARS[colour]
        o.append(f'<path fill="{WHITE}" d="{text_path(lab, jx, 455, 11.5, BOLD, 1.6)}"/>')
        gid = f"col{prefix}{lab.replace(' ', '')}"
        o.append(f'<defs><radialGradient id="{gid}" cx="0.38" cy="0.32" r="0.8"><stop offset="0" stop-color="{c0}"/><stop offset="1" stop-color="{c1}"/></radialGradient></defs>')
        o.append(f'<circle cx="{f(jx)}" cy="{f(JACK_Y)}" r="18.5" fill="none" stroke="{WHITE}" stroke-opacity="0.55" stroke-width="0.9"/>')
        o.append(f'<circle cx="{f(jx)}" cy="{f(JACK_Y)}" r="15" fill="url(#{gid})" stroke="#000" stroke-width="1"/>')
        o.append(f'<circle cx="{f(jx)}" cy="{f(JACK_Y)}" r="7.5" fill="url(#metal)" stroke="#202329" stroke-width="0.8"/>')
        o.append(f'<circle cx="{f(jx)}" cy="{f(JACK_Y)}" r="3.6" fill="#040508"/>')


# ---------------------------------------------------------------- PLEAT

def pleat_plate_svg():
    o = []
    o.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
    o.append('<!-- PLEAT plate, Serge style. Copyright (c) 2026 Martial Systems LLC. All rights reserved. -->')
    o.append('<!-- Layout fractions match SergeMiddle/PleatView.h: knobs at x 0.30 W and 0.70 W, y 0.38 H, diameter 0.29 W; jacks at y 0.785 H, x 0.18/0.39/0.61/0.82 W. Text is outlined (Liberation Sans). -->')
    pleat_body(o)
    o.append('</svg>')
    return "\n".join(o) + "\n"


def pleat_body(o, combined=False):
    panel_base(o)
    o.append(f'<path fill="{WHITE}" d="{text_path("PLEAT", W/2, 68, 38, BOLD, 9)}"/>')
    o.append(f'<path fill="#c7cdd8" d="{text_path("SERGE MIDDLE", W/2, 90, 10.5, BOLD, 3.6)}"/>')
    o.append(f'<line x1="24" y1="106" x2="{W-24}" y2="106" stroke="{WHITE}" stroke-width="1.2"/>')

    def scale(cx, cy, label, marks, minor):
        o.append(f'<path fill="{WHITE}" d="{text_path(label, cx, 134, 13, BOLD, 2.2)}"/>')
        r_in, r_out, r_maj = 66, 72, 76
        for i in range(minor + 1):
            th = math.radians(-130 + 260 * i / minor)
            s, c = math.sin(th), math.cos(th)
            o.append(f'<line x1="{f(cx+r_in*s)}" y1="{f(cy-r_in*c)}" x2="{f(cx+r_out*s)}" y2="{f(cy-r_out*c)}" stroke="{WHITE}" stroke-width="1" stroke-linecap="round"/>')
        for n, txt in marks:
            th = math.radians(-130 + 260 * n)
            s, c = math.sin(th), math.cos(th)
            o.append(f'<line x1="{f(cx+r_in*s)}" y1="{f(cy-r_in*c)}" x2="{f(cx+r_maj*s)}" y2="{f(cy-r_maj*c)}" stroke="{WHITE}" stroke-width="1.6" stroke-linecap="round"/>')
            rr = 87
            tx, ty = cx + rr * s, cy - rr * c + 3.3
            o.append(f'<path fill="#dfe3ea" d="{text_path(txt, tx, ty, 9.5, REG, 0.3)}"/>')

    # Fold is an exponential taper 0.5..8, so 1, 2, 4 land on the quarter marks.
    scale(KNOB_LX, KNOB_Y, "FOLD", [(0, "0.5"), (0.25, "1"), (0.5, "2"), (0.75, "4"), (1, "8")], 16)
    # The right knob is a DC voltage normalled into IN 2. The real middle
    # section has no symmetry control; a DC cord into the DC-coupled IN 2
    # does that job, and this knob stands in for the cord.
    scale(KNOB_RX, KNOB_Y, "IN 2 DC", [(0, "-2.5"), (0.5, "0"), (1, "+2.5")], 10)
    readouts(o)
    o.append(f'<line x1="24" y1="362" x2="{W-24}" y2="362" stroke="{WHITE}" stroke-width="1.2"/>')
    # Transfer glyph: the six-cell map at g = 1 on [-6, 6] (tests/transfer_g1.csv).
    with (ROOT / "tests" / "transfer_g1.csv").open(newline="") as fh:
        rows = list(csv.reader(fh))[1:]
    gx0, gx1, gyc, gh = (132, 284, 396, 26) if not combined else (44, 176, 396, 24)
    pts = []
    for i, (vi, vo) in enumerate(rows):
        if i % 6:
            continue
        x = gx0 + (float(vi) + 6) / 12 * (gx1 - gx0)
        y = gyc - float(vo) / 0.6 * gh
        pts.append(f"{f(x)},{f(y)}")
    o.append(f'<line x1="{gx0}" y1="{gyc}" x2="{gx1}" y2="{gyc}" stroke="{WHITE}" stroke-opacity="0.35" stroke-width="0.8"/>')
    o.append(f'<line x1="{(gx0+gx1)/2}" y1="{gyc-gh-3}" x2="{(gx0+gx1)/2}" y2="{gyc+gh+3}" stroke="{WHITE}" stroke-opacity="0.35" stroke-width="0.8"/>')
    o.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{WHITE}" stroke-width="1.3" stroke-linejoin="round"/>')
    if not combined:
        o.append(f'<path fill="#c7cdd8" d="{text_path("6 CELLS", 64, 400, 9, BOLD, 1.6)}"/>')
        o.append(f'<path fill="#c7cdd8" d="{text_path(chr(0xb1) + "6 V", W-64, 400, 9, BOLD, 1.0)}"/>')
    else:
        o.append(f'<path fill="#c7cdd8" d="{text_path("6 CELLS", (gx0+gx1)/2, 424, 7.5, BOLD, 1.4)}"/>')
        # VC DEPTH: the small attenuator above the VC jack. The original
        # middle section had none; current Serge panels have one.
        cx, cy, r = JACK_X[2], 396.0, 22.0
        for i in range(11):
            th = math.radians(-130 + 26 * i)
            s, c = math.sin(th), math.cos(th)
            r_out = r + (7 if i in (0, 10) else 4.5)
            o.append(f'<line x1="{f(cx+(r+1.5)*s)}" y1="{f(cy-(r+1.5)*c)}" x2="{f(cx+r_out*s)}" y2="{f(cy-r_out*c)}" stroke="{WHITE}" stroke-width="{1.4 if i in (0, 10) else 0.9}" stroke-linecap="round"/>')
        o.append(f'<path fill="{WHITE}" d="{text_path("VC", cx + 40, 392, 11, BOLD, 1.4, "start")}"/>')
        o.append(f'<path fill="#c7cdd8" d="{text_path("DEPTH", cx + 40, 405, 8, BOLD, 1.2, "start")}"/>')
        o.append(f'<path fill="#aab2c0" d="{text_path("0", cx - 25, cy + 27, 7.5, REG, 0, "middle")}"/>')
        o.append(f'<path fill="#aab2c0" d="{text_path("1", cx + 25, cy + 27, 7.5, REG, 0, "middle")}"/>')
    # Original middle-section jacks: IN 1 black (AC coupled), IN 2 blue (DC
    # coupled), VC blue, OUT black (AC coupled).
    jacks(o, ["IN 1", "IN 2", "VC", "OUT"], ["black", "blue", "blue", "black"], prefix="P" if combined else "")
    for jx, txt in ((JACK_X[0], "AC"), (JACK_X[1], "DC"), (JACK_X[3], "AC")):
        o.append(f'<path fill="#aab2c0" d="{text_path(txt, jx + 27, JACK_Y + 3, 8, BOLD, 0.6, "start")}"/>')
    # Normal mark: a dot by the IN 2 legend matches the dot by the IN 2 DC
    # knob legend. With no cord in IN 2, the knob's voltage is what it reads.
    o.append(f'<circle cx="{f(JACK_X[1] - text_width("IN 2", 11.5, BOLD, 1.6) / 2 - 7)}" cy="451" r="2.6" fill="none" stroke="{WHITE}" stroke-width="1.1"/>')
    o.append(f'<circle cx="{f(KNOB_RX - text_width("IN 2 DC", 13, BOLD, 2.2) / 2 - 8)}" cy="129.5" r="2.6" fill="none" stroke="{WHITE}" stroke-width="1.1"/>')
    if not combined:
        ay = 526
        o.append(f'<line x1="{f(JACK_X[0])}" y1="{ay}" x2="{f(JACK_X[3]-8)}" y2="{ay}" stroke="{WHITE}" stroke-opacity="0.7" stroke-width="1"/>')
        o.append(f'<path d="M{f(JACK_X[3]-9)},{ay-3.5} L{f(JACK_X[3])},{ay} L{f(JACK_X[3]-9)},{ay+3.5} Z" fill="{WHITE}" fill-opacity="0.7"/>')
        o.append(f'<line x1="{f(JACK_X[0])}" y1="{ay-6}" x2="{f(JACK_X[0])}" y2="{ay}" stroke="{WHITE}" stroke-opacity="0.7" stroke-width="1"/>')
        o.append(f'<line x1="{f(JACK_X[1])}" y1="{ay-6}" x2="{f(JACK_X[1])}" y2="{ay}" stroke="{WHITE}" stroke-opacity="0.7" stroke-width="1"/>')
    else:
        normals(o, [(JACK_X[0], "\u2190 HOST"), (JACK_X[1], "\u2190 KNOB"), (JACK_X[2], "\u2190 SLOPE"), (JACK_X[3], "\u2192 HOST")])


def normals(o, items):
    # Normal legends under the jacks: where an unpatched jack listens or sends.
    for jx, txt in items:
        o.append(f'<path fill="#9fb0c8" d="{text_path(txt, jx, 528, 7.5, BOLD, 0.8)}"/>')


def knob_svg():
    S = 240
    c = S / 2
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{S}" height="{S}" viewBox="0 0 {S} {S}">',
         '<!-- PLEAT knob: black knurled cap, white pointer at 12 o\'clock. Rendered 120 px at 1x, 240 px at @2x. Copyright (c) 2026 Martial Systems LLC. All rights reserved. -->',
         '<defs>'
         '<radialGradient id="skirt" cx="0.36" cy="0.3" r="0.8"><stop offset="0" stop-color="#34373e"/><stop offset="0.55" stop-color="#141519"/><stop offset="1" stop-color="#060607"/></radialGradient>'
         '<radialGradient id="cap" cx="0.38" cy="0.3" r="0.85"><stop offset="0" stop-color="#3a3d45"/><stop offset="0.6" stop-color="#16171b"/><stop offset="1" stop-color="#0a0a0c"/></radialGradient>'
         '</defs>']
    teeth, r0, r1 = 60, 113, 106.5
    pts = []
    for i in range(teeth * 2):
        th = 2 * math.pi * i / (teeth * 2)
        r = r0 if i % 2 == 0 else r1
        pts.append(f"{f(c + r*math.sin(th))},{f(c - r*math.cos(th))}")
    o.append(f'<polygon points="{" ".join(pts)}" fill="#08080a" stroke="#2c2f35" stroke-width="0.9" stroke-linejoin="round"/>')
    o.append(f'<circle cx="{c}" cy="{c}" r="106" fill="url(#skirt)"/>')
    o.append(f'<circle cx="{c}" cy="{c}" r="84" fill="#000" fill-opacity="0.55"/>')
    o.append(f'<circle cx="{c}" cy="{c}" r="81" fill="url(#cap)" stroke="#4a4e57" stroke-opacity="0.7" stroke-width="1.4"/>')
    o.append(f'<rect x="{c-3.6}" y="9" width="7.2" height="{c-9-34}" rx="3.6" fill="#f6f7f9"/>')
    o.append('</svg>')
    return "\n".join(o) + "\n"


# ---------------------------------------------------------------- Slope AC

SWITCH_X, SWITCH_Y = 0.30 * W, 0.655 * H
RATE_X, RATE_T, RATE_B = 0.70 * W, 0.633 * H, 0.677 * H
TIME_MIN, TIME_DECADES = 1e-4, 5  # Rise and Fall: 0.0001 s to 10 s, ShapeExp


def time_norm(seconds):
    return math.log10(seconds / TIME_MIN) / TIME_DECADES


def slope_plate_svg():
    o = []
    o.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
    o.append('<!-- Slope AC plate. Copyright (c) 2026 Martial Systems LLC. All rights reserved. -->')
    o.append('<!-- Lettering is the rise-and-fall mark and the legends (slope/README.md). Layout fractions match SlopeAC/SlopeView.h: knobs at x 0.30 W and 0.70 W, y 0.38 H, diameter 0.29 W; captions 0.495 H to 0.545 H; VC attenuverter centre (0.50 W, 0.529 H) diameter 40; route switch centre (0.481 W, 0.615 H); cycle switch centre (0.30 W, 0.655 H); rate readout 0.70 W, 0.633 H to 0.677 H; jacks at y 0.785 H, x 0.12/0.27/0.42/0.58/0.73/0.88 W. Text is outlined (Liberation Sans). -->')
    slope_body(o)
    o.append('</svg>')
    return "\n".join(o) + "\n"


SLOPE6_X = [0.12 * W, 0.27 * W, 0.42 * W, 0.58 * W, 0.73 * W, 0.88 * W]
VC_X, VC_Y, VC_D = 0.50 * W, 0.529 * H, 40.0
ROUTE_X, ROUTE_Y = 0.481 * W, 0.615 * H


def slope_body(o, combined=False, mark="", normal_out=True, caps=False):
    panel_base(o)
    if mark:
        o.append(f'<path fill="{WHITE}" d="{text_path("SLOPE " + mark, W/2, 28, 13, BOLD, 2.4)}"/>')
    # The panel mark: one rise, one fall, one peak, on a baseline.
    base, peak = 86, 34
    xs = (118, 152, 196, 264, 298)
    o.append(f'<polyline points="{xs[0]},{base} {xs[1]},{base} {xs[2]},{peak} {xs[3]},{base} {xs[4]},{base}" fill="none" stroke="{WHITE}" stroke-width="3.4" stroke-linejoin="miter" stroke-miterlimit="10" stroke-linecap="square"/>')
    o.append(f'<line x1="24" y1="106" x2="{W-24}" y2="106" stroke="{WHITE}" stroke-width="1.2"/>')

    def log_scale(cx, cy, label):
        o.append(f'<path fill="{WHITE}" d="{text_path(label, cx, 134, 13, BOLD, 2.2)}"/>')
        r_in, r_minor, r_mid, r_maj = 66, 71, 73, 77
        for d in range(TIME_DECADES):
            for m, r_out, wdt in ((2, r_minor, 1.0), (5, r_mid, 1.0)):
                n = (d + math.log10(m)) / TIME_DECADES
                th = math.radians(-130 + 260 * n)
                s, c = math.sin(th), math.cos(th)
                o.append(f'<line x1="{f(cx+r_in*s)}" y1="{f(cy-r_in*c)}" x2="{f(cx+r_out*s)}" y2="{f(cy-r_out*c)}" stroke="{WHITE}" stroke-width="{wdt}" stroke-linecap="round"/>')
        # Every decade gets a major tick. The labels sit at 0.1 ms, 10 ms, 0.1 s,
        # and 10 s: the 1 ms and 1 s ticks face the other knob, where a label
        # would collide with its scale.
        for d in range(TIME_DECADES + 1):
            n = d / TIME_DECADES
            th = math.radians(-130 + 260 * n)
            s, c = math.sin(th), math.cos(th)
            o.append(f'<line x1="{f(cx+r_in*s)}" y1="{f(cy-r_in*c)}" x2="{f(cx+r_maj*s)}" y2="{f(cy-r_maj*c)}" stroke="{WHITE}" stroke-width="1.6" stroke-linecap="round"/>')
        for d, txt in ((0, "0.1 ms"), (2, "10 ms"), (3, "0.1 s"), (5, "10 s")):
            th = math.radians(-130 + 260 * d / TIME_DECADES)
            s, c = math.sin(th), math.cos(th)
            if c < 0:
                # Bottom corners: centred under the tick end.
                tx, ty = cx + 80 * s, cy - 80 * c + 13
            else:
                tx, ty = cx + 88 * s, cy - 88 * c + 3.2
            o.append(f'<path fill="#dfe3ea" d="{text_path(txt, tx, ty, 9.0, REG, 0.2)}"/>')
        # 5 ms, the default, as a small dot inside the ring.
        th = math.radians(-130 + 260 * time_norm(0.005))
        s, c = math.sin(th), math.cos(th)
        o.append(f'<circle cx="{f(cx+62*s)}" cy="{f(cy-62*c)}" r="1.6" fill="{WHITE}"/>')

    log_scale(KNOB_LX, KNOB_Y, "RISE")
    log_scale(KNOB_RX, KNOB_Y, "FALL")
    readouts(o)
    # VC attenuverter. Noon is 0, which passes none of the VC jack. The
    # pointer bitmap is drawn by the editor, same cap as Rise and Fall.
    cx, cy, r = VC_X, VC_Y, VC_D / 2
    o.append(f'<path fill="{WHITE}" d="{text_path("VC", cx, cy - r - 16, 11, BOLD, 1.6)}"/>')
    for i, (n, txt) in enumerate(((-1, "-1"), (0, "0"), (1, "+1"))):
        th = math.radians(-130 + 260 * (n + 1) / 2)
        s, c = math.sin(th), math.cos(th)
        r_out = r + (8 if i != 1 else 6)
        o.append(f'<line x1="{f(cx+(r+1)*s)}" y1="{f(cy-(r+1)*c)}" x2="{f(cx+r_out*s)}" y2="{f(cy-r_out*c)}" stroke="{WHITE}" stroke-width="{1.3 if i != 1 else 1.0}" stroke-linecap="round"/>')
        o.append(f'<path fill="#dfe3ea" d="{text_path(txt, cx+(r+16)*s, cy-(r+16)*c+3, 8, REG, 0.2)}"/>')
    o.append(f'<line x1="24" y1="362" x2="{W-24}" y2="362" stroke="{WHITE}" stroke-width="1.2"/>')

    # CYCLE switch: legend, OFF and ON, and the mounting plate behind the lever bitmap.
    o.append(f'<path fill="{WHITE}" d="{text_path("CYCLE", SWITCH_X, 386, 12, BOLD, 2.0)}"/>')
    o.append(f'<rect x="{f(SWITCH_X-27)}" y="{f(SWITCH_Y-15)}" width="54" height="30" rx="5" fill="url(#readout)" stroke="{WHITE}" stroke-opacity="0.45" stroke-width="0.8"/>')
    o.append(f'<path fill="#dfe3ea" d="{text_path("OFF", SWITCH_X-34, SWITCH_Y+3.4, 9.5, BOLD, 0.8, "end")}"/>')
    o.append(f'<path fill="#dfe3ea" d="{text_path("ON", SWITCH_X+34, SWITCH_Y+3.4, 9.5, BOLD, 0.8, "start")}"/>')

    # RISE / BOTH / FALL. The lever bitmap has three frames. BOTH is the centre.
    o.append(f'<path fill="{WHITE}" d="{text_path("BOTH", ROUTE_X, ROUTE_Y - 22, 9, BOLD, 1.0)}"/>')
    o.append(f'<rect x="{f(ROUTE_X-24)}" y="{f(ROUTE_Y-14)}" width="48" height="28" rx="5" fill="url(#readout)" stroke="{WHITE}" stroke-opacity="0.45" stroke-width="0.8"/>')
    o.append(f'<path fill="#dfe3ea" d="{text_path("RISE", ROUTE_X-30, ROUTE_Y+3.2, 8, BOLD, 0.6, "end")}"/>')
    o.append(f'<path fill="#dfe3ea" d="{text_path("FALL", ROUTE_X+30, ROUTE_Y+3.2, 8, BOLD, 0.6, "start")}"/>')

    # Rate readout well, with a small cycle train above it (no lettering).
    o.append(f'<rect x="{f(RATE_X-44)}" y="{f(RATE_T+2)}" width="88" height="{f(RATE_B-RATE_T-4)}" rx="3" fill="url(#readout)" stroke="{WHITE}" stroke-opacity="0.45" stroke-width="0.8"/>')
    tx0, ty0, th_ = RATE_X - 30, 386, 12
    pts = []
    for i in range(4):
        x = tx0 + i * 15
        pts += [f"{f(x)},{ty0}", f"{f(x+5)},{ty0-th_}"]
    pts.append(f"{f(tx0+60)},{ty0}")
    o.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{WHITE}" stroke-width="1.3" stroke-linejoin="miter"/>')

    # The DUSG section, on both plates: inputs IN, TRIG, VC, then outputs END, OUT, AC.
    # IN black, TRIG red, VC blue, END red (logic 0 V or +5 V), OUT black, AC black.
    jacks(o, ["IN", "TRIG", "VC", "END", "OUT", "AC"], ["black", "red", "blue", "red", "black", "black"], SLOPE6_X, "S" + mark if combined else "A")
    if caps:
        # Same defaults as the live slope: 5 ms rise and fall, VC at noon, Cycle off, route BOTH.
        # These caps are plate art. The editor does not turn them.
        rise = knob_angle(time_norm(0.005))
        append_knob(o, KNOB_LX, KNOB_Y, 120, rise, "sr" + mark)
        append_knob(o, KNOB_RX, KNOB_Y, 120, rise, "sf" + mark)
        append_knob(o, VC_X, VC_Y, 40, knob_angle(0.5), "sv" + mark)
        append_lever(o, SWITCH_X, SWITCH_Y, "left", "sl" + mark)
        append_lever(o, ROUTE_X, ROUTE_Y, "up", "su" + mark)
        for cx in (KNOB_LX, KNOB_RX):
            o.append(f'<path fill="#eef1f6" d="{text_path("5.00 ms", cx, (CAP_T+CAP_B)/2 + 4, 13, REG, 0.2)}"/>')
        o.append(f'<path fill="#eef1f6" d="{text_path("OFF", RATE_X, (RATE_T+RATE_B)/2 + 4, 13, REG, 0.4)}"/>')
    if not combined:
        return
    xm = (SLOPE6_X[2] + SLOPE6_X[3]) / 2
    o.append(f'<line x1="{f(xm)}" y1="440" x2="{f(xm)}" y2="532" stroke="{WHITE}" stroke-opacity="0.35" stroke-width="0.8" stroke-dasharray="2 3"/>')
    legends = [(SLOPE6_X[1], "\u2190 MIDI")]
    if normal_out:
        legends.append((SLOPE6_X[4], "\u2192 PLEAT VC"))
    normals(o, legends)


def switch_frame_svg(on: bool):
    # One frame at @2x scale: 96 by 56. The lever points right when on.
    Wf, Hf = 96, 56
    cx, cy = Wf / 2, Hf / 2
    sgn = 1 if on else -1
    tip = cx + sgn * 34
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{Wf}" height="{Hf}" viewBox="0 0 {Wf} {Hf}">',
         '<defs>'
         '<radialGradient id="nut" cx="0.4" cy="0.35" r="0.7"><stop offset="0" stop-color="#f1f3f6"/><stop offset="0.7" stop-color="#9da2aa"/><stop offset="1" stop-color="#555a63"/></radialGradient>'
         '<linearGradient id="bat" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f7f8fa"/><stop offset="0.5" stop-color="#b8bdc5"/><stop offset="1" stop-color="#6c717a"/></linearGradient>'
         '</defs>']
    # Hex nut.
    hexpts = " ".join(f"{f(cx + 15*math.cos(math.radians(60*i)))},{f(cy + 15*math.sin(math.radians(60*i)))}" for i in range(6))
    o.append(f'<polygon points="{hexpts}" fill="url(#nut)" stroke="#2a2d33" stroke-width="1.2"/>')
    o.append(f'<circle cx="{cx}" cy="{cy}" r="8" fill="#3b3f46" stroke="#14161a" stroke-width="1"/>')
    # Shadow, then the tapered bat lever.
    o.append(f'<path d="M{f(cx)},{f(cy-3)} L{f(tip)},{f(cy-1.5)} L{f(tip)},{f(cy+6.5)} L{f(cx)},{f(cy+8)} Z" fill="#000" fill-opacity="0.35"/>')
    o.append(f'<path d="M{f(cx)},{f(cy-6)} L{f(tip)},{f(cy-4.5)} L{f(tip)},{f(cy+4.5)} L{f(cx)},{f(cy+6)} Z" fill="url(#bat)" stroke="#3a3e45" stroke-width="0.8"/>')
    o.append(f'<circle cx="{f(tip)}" cy="{cy}" r="6.5" fill="url(#bat)" stroke="#3a3e45" stroke-width="0.8"/>')
    o.append(f'<circle cx="{cx}" cy="{cy}" r="5" fill="url(#nut)" stroke="#2a2d33" stroke-width="0.8"/>')
    o.append('</svg>')
    return "\n".join(o) + "\n"


def route_frame_svg(mode: str):
    # One frame at @2x: 96 by 56. RISE points left, BOTH points up, FALL points right.
    Wf, Hf = 96, 56
    cx, cy = Wf / 2, Hf / 2
    if mode == "rise":
        tipx, tipy = cx - 34, cy
    elif mode == "fall":
        tipx, tipy = cx + 34, cy
    else:
        tipx, tipy = cx, cy - 34
    dx, dy = tipx - cx, tipy - cy
    length = math.hypot(dx, dy) or 1.0
    px, py = -dy / length, dx / length
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{Wf}" height="{Hf}" viewBox="0 0 {Wf} {Hf}">',
         '<defs>'
         '<radialGradient id="nut" cx="0.4" cy="0.35" r="0.7"><stop offset="0" stop-color="#f1f3f6"/><stop offset="0.7" stop-color="#9da2aa"/><stop offset="1" stop-color="#555a63"/></radialGradient>'
         '<linearGradient id="bat" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f7f8fa"/><stop offset="0.5" stop-color="#b8bdc5"/><stop offset="1" stop-color="#6c717a"/></linearGradient>'
         '</defs>']
    hexpts = " ".join(f"{f(cx + 15*math.cos(math.radians(60*i)))},{f(cy + 15*math.sin(math.radians(60*i)))}" for i in range(6))
    o.append(f'<polygon points="{hexpts}" fill="url(#nut)" stroke="#2a2d33" stroke-width="1.2"/>')
    o.append(f'<circle cx="{cx}" cy="{cy}" r="8" fill="#3b3f46" stroke="#14161a" stroke-width="1"/>')
    # Tapered bat, perpendicular offset of 6 at the nut and 4.5 at the tip.
    nut = [(cx + px * 6, cy + py * 6), (cx - px * 6, cy - py * 6)]
    tip = [(tipx + px * 4.5, tipy + py * 4.5), (tipx - px * 4.5, tipy - py * 4.5)]
    shadow = f"M{f(nut[0][0])},{f(nut[0][1]+3)} L{f(tip[0][0])},{f(tip[0][1]+3)} L{f(tip[1][0])},{f(tip[1][1]+3)} L{f(nut[1][0])},{f(nut[1][1]+3)} Z"
    bat = f"M{f(nut[0][0])},{f(nut[0][1])} L{f(tip[0][0])},{f(tip[0][1])} L{f(tip[1][0])},{f(tip[1][1])} L{f(nut[1][0])},{f(nut[1][1])} Z"
    o.append(f'<path d="{shadow}" fill="#000" fill-opacity="0.35"/>')
    o.append(f'<path d="{bat}" fill="url(#bat)" stroke="#3a3e45" stroke-width="0.8"/>')
    o.append(f'<circle cx="{f(tipx)}" cy="{f(tipy)}" r="6.5" fill="url(#bat)" stroke="#3a3e45" stroke-width="0.8"/>')
    o.append(f'<circle cx="{cx}" cy="{cy}" r="5" fill="url(#nut)" stroke="#2a2d33" stroke-width="0.8"/>')
    o.append('</svg>')
    return "\n".join(o) + "\n"


def route_svg():
    frames = []
    for i, mode in enumerate(("rise", "both", "fall")):
        body = route_frame_svg(mode).split("\n", 1)[1].rsplit("</svg>", 1)[0]
        body = body.replace('id="nut"', f'id="rnut{i}"').replace("url(#nut)", f"url(#rnut{i})")
        body = body.replace('id="bat"', f'id="rbat{i}"').replace("url(#bat)", f"url(#rbat{i})")
        frames.append(f'<g transform="translate(0,{i * 56})">{body}</g>')
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="96" height="168" viewBox="0 0 96 168">\n'
            '<!-- Route switch strip: frame 0 RISE (lever left), frame 1 BOTH (lever up), frame 2 FALL (lever right). 48 by 28 per frame at 1x. Copyright (c) 2026 Martial Systems LLC. All rights reserved. -->\n'
            + "\n".join(frames) + '\n</svg>\n')


def switch_svg():
    # Both frames stacked vertically, OFF on top: the strip IBSwitchControl reads.
    off = switch_frame_svg(False).split("\n", 1)[1].rsplit("</svg>", 1)[0]
    on = switch_frame_svg(True).split("\n", 1)[1].rsplit("</svg>", 1)[0]
    on = on.replace('id="nut"', 'id="nut1"').replace("url(#nut)", "url(#nut1)").replace('id="bat"', 'id="bat1"').replace("url(#bat)", "url(#bat1)")
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="96" height="112" viewBox="0 0 96 112">\n'
            '<!-- CYCLE switch strip: frame 0 OFF (lever left), frame 1 ON (lever right). 48 by 28 per frame at 1x. Copyright (c) 2026 Martial Systems LLC. All rights reserved. -->\n'
            f'<g>{off}</g>\n<g transform="translate(0,56)">{on}</g>\n</svg>\n')



def knob_angle(norm):
    # Same travel as the editor knobs: -130 deg at minimum, +130 deg at maximum.
    return -130.0 + 260.0 * norm


def append_knob(o, cx, cy, diameter, angle, gid):
    """Knurled cap, pointer at 12 o'clock before `angle`. Matches pleat_knob.svg."""
    s = diameter / 240.0
    c = 120.0
    o.append(
        f'<g transform="translate({f(cx)},{f(cy)}) rotate({f(angle)}) scale({f(s)}) translate(-120,-120)">'
    )
    o.append(
        f'<defs>'
        f'<radialGradient id="{gid}skirt" cx="0.36" cy="0.3" r="0.8">'
        f'<stop offset="0" stop-color="#34373e"/><stop offset="0.55" stop-color="#141519"/><stop offset="1" stop-color="#060607"/>'
        f'</radialGradient>'
        f'<radialGradient id="{gid}cap" cx="0.38" cy="0.3" r="0.85">'
        f'<stop offset="0" stop-color="#3a3d45"/><stop offset="0.6" stop-color="#16171b"/><stop offset="1" stop-color="#0a0a0c"/>'
        f'</radialGradient>'
        f'</defs>'
    )
    teeth, r0, r1 = 60, 113, 106.5
    pts = []
    for i in range(teeth * 2):
        th = 2 * math.pi * i / (teeth * 2)
        r = r0 if i % 2 == 0 else r1
        pts.append(f"{f(c + r * math.sin(th))},{f(c - r * math.cos(th))}")
    o.append(f'<polygon points="{" ".join(pts)}" fill="#08080a" stroke="#2c2f35" stroke-width="0.9" stroke-linejoin="round"/>')
    o.append(f'<circle cx="{c}" cy="{c}" r="106" fill="url(#{gid}skirt)"/>')
    o.append(f'<circle cx="{c}" cy="{c}" r="84" fill="#000" fill-opacity="0.55"/>')
    o.append(f'<circle cx="{c}" cy="{c}" r="81" fill="url(#{gid}cap)" stroke="#4a4e57" stroke-opacity="0.7" stroke-width="1.4"/>')
    o.append(f'<rect x="{c - 3.6}" y="9" width="7.2" height="{c - 9 - 34}" rx="3.6" fill="#f6f7f9"/>')
    o.append("</g>")


def append_lever(o, cx, cy, direction, gid):
    """Bat lever at plate scale. direction is left, right, or up. Matches the switch bitmaps."""
    if direction == "left":
        tipx, tipy = cx - 17.0, cy
    elif direction == "right":
        tipx, tipy = cx + 17.0, cy
    else:
        tipx, tipy = cx, cy - 17.0
    dx, dy = tipx - cx, tipy - cy
    length = math.hypot(dx, dy) or 1.0
    px, py = -dy / length, dx / length
    o.append(
        f'<defs>'
        f'<radialGradient id="{gid}nut" cx="0.4" cy="0.35" r="0.7">'
        f'<stop offset="0" stop-color="#f1f3f6"/><stop offset="0.7" stop-color="#9da2aa"/><stop offset="1" stop-color="#555a63"/>'
        f'</radialGradient>'
        f'<linearGradient id="{gid}bat" x1="0" y1="0" x2="0" y2="1">'
        f'<stop offset="0" stop-color="#f7f8fa"/><stop offset="0.5" stop-color="#b8bdc5"/><stop offset="1" stop-color="#6c717a"/>'
        f'</linearGradient>'
        f'</defs>'
    )
    hexpts = " ".join(
        f"{f(cx + 7.5 * math.cos(math.radians(60 * i)))},{f(cy + 7.5 * math.sin(math.radians(60 * i)))}"
        for i in range(6)
    )
    o.append(f'<polygon points="{hexpts}" fill="url(#{gid}nut)" stroke="#2a2d33" stroke-width="0.8"/>')
    o.append(f'<circle cx="{f(cx)}" cy="{f(cy)}" r="4" fill="#3b3f46" stroke="#14161a" stroke-width="0.6"/>')
    nut = [(cx + px * 3.0, cy + py * 3.0), (cx - px * 3.0, cy - py * 3.0)]
    tip = [(tipx + px * 2.25, tipy + py * 2.25), (tipx - px * 2.25, tipy - py * 2.25)]
    shadow = (
        f"M{f(nut[0][0])},{f(nut[0][1] + 1.5)} L{f(tip[0][0])},{f(tip[0][1] + 1.5)} "
        f"L{f(tip[1][0])},{f(tip[1][1] + 1.5)} L{f(nut[1][0])},{f(nut[1][1] + 1.5)} Z"
    )
    bat = (
        f"M{f(nut[0][0])},{f(nut[0][1])} L{f(tip[0][0])},{f(tip[0][1])} "
        f"L{f(tip[1][0])},{f(tip[1][1])} L{f(nut[1][0])},{f(nut[1][1])} Z"
    )
    o.append(f'<path d="{shadow}" fill="#000" fill-opacity="0.35"/>')
    o.append(f'<path d="{bat}" fill="url(#{gid}bat)" stroke="#3a3e45" stroke-width="0.6"/>')
    o.append(f'<circle cx="{f(tipx)}" cy="{f(tipy)}" r="3.25" fill="url(#{gid}bat)" stroke="#3a3e45" stroke-width="0.6"/>')
    o.append(f'<circle cx="{f(cx)}" cy="{f(cy)}" r="2.5" fill="url(#{gid}nut)" stroke="#2a2d33" stroke-width="0.5"/>')


def paper_plate_svg():
    return render_paper_plate(text_path, f, append_lever, BOLD, REG)


# ---------------------------------------------------------------- output

def rasterize(svg_path: Path, png_path: Path, width: int, height: int, mode: str):
    import cairosvg
    from PIL import Image
    png = cairosvg.svg2png(url=str(svg_path), output_width=width, output_height=height)
    Image.open(io.BytesIO(png)).convert(mode).save(png_path, optimize=True)


def main() -> int:
    global BOLD, REG
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--svg-only", action="store_true")
    ap.add_argument("--font-bold")
    ap.add_argument("--font-regular")
    args = ap.parse_args()
    BOLD = load_font("bold", args.font_bold)
    REG = load_font("regular", args.font_regular)

    pleat = ROOT / "SergeMiddle" / "resources"
    slope = ROOT / "SlopeAC" / "resources"
    combo = ROOT / "SergeMiddleCombined" / "resources"
    combo.mkdir(parents=True, exist_ok=True)
    jobs = [
        (pleat / "pleat_plate.svg", pleat_plate_svg(), (W, H), "RGB"),
        (pleat / "pleat_knob.svg", knob_svg(), (120, 120), "RGBA"),
        (slope / "slope_plate.svg", slope_plate_svg(), (W, H), "RGB"),
        (slope / "slope_switch.svg", switch_svg(), (48, 56), "RGBA"),
        (slope / "slope_route.svg", route_svg(), (48, 84), "RGBA"),
        (combo / "middle_plate.svg", paper_plate_svg(), (PLATE_W, PLATE_H), "RGB"),
    ]
    for path, text, (w, h), mode in jobs:
        path.write_text(text, encoding="utf-8")
        if args.svg_only:
            continue
        rasterize(path, path.with_suffix(".png"), w, h, mode)
        rasterize(path, path.with_name(path.stem + "@2x.png"), 2 * w, 2 * h, mode)
        print("wrote", path.relative_to(ROOT), "and PNGs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
