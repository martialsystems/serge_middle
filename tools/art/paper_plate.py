"""Combined Serge Middle plate: one 1970s paperface rectangle.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The instrument is a grid, not a single row. Top row is VCM top, middle, and
bottom. Bottom row is the two slopes. Jacks are the large marks. Printed lines
show the signal path. Dashed lines are normals. Knobs are drawn by the editor
on the tick marks; this plate does not paint caps except the COUPLE lever,
which is not a parameter.

Colours: black bipolar, blue unipolar CV, white DC-coupled, red trigger or gate.
"""

from __future__ import annotations

import math

PLATE_W = 1600
PLATE_H = 960
JACK_R = 23
MIN_LABEL = 12

INK = "#1a1a1a"
PAPER = "#f4efe4"
WELL = "#fffdf8"
CREAM = "#f7f4ee"

# Sections. Top row ends at 494. The gutter under it carries the slope normal.
# Slopes start at 540, so the two rows are a grid rather than one long strip.
SECTIONS = {
    "top": (18, 64, 512, 430),
    "mid": (544, 64, 512, 430),
    "bot": (1070, 64, 512, 430),
    "s1": (18, 540, 768, 400),
    "s2": (814, 540, 768, 400),
}

JACK_Y_TOP = 432
JACK_Y_SLOPE = 858


def _xs(origin, width, n, inset):
    span = width - 2 * inset
    return [int(round(origin + inset + span * i / (n - 1))) for i in range(n)]


def layout():
    """Pixel spots for the plate and the editor. Fractions are x/1600, y/960."""
    top_x = _xs(18, 512, 3, 78)
    mid_x = _xs(544, 512, 4, 58)
    bot_x = _xs(1070, 512, 4, 58)
    s1_x = _xs(18, 768, 6, 52)
    s2_x = _xs(814, 768, 6, 52)
    jacks = [
        ("TopIn", False, top_x[0], JACK_Y_TOP, "black", "IN", "TOP IN"),
        ("TopVc", False, top_x[1], JACK_Y_TOP, "blue", "VC", "TOP VC"),
        ("TopOut", True, top_x[2], JACK_Y_TOP, "black", "OUT", "TOP OUT"),
        ("PleatIn1", False, mid_x[0], JACK_Y_TOP, "black", "IN 1", "PLEAT IN 1"),
        ("PleatIn2", False, mid_x[1], JACK_Y_TOP, "white", "IN 2", "PLEAT IN 2"),
        ("PleatVc", False, mid_x[2], JACK_Y_TOP, "blue", "VC", "PLEAT VC"),
        ("PleatOut", True, mid_x[3], JACK_Y_TOP, "black", "OUT", "PLEAT OUT"),
        ("BottomIn", False, bot_x[0], JACK_Y_TOP, "black", "IN", "BOTTOM IN"),
        ("BottomVc", False, bot_x[1], JACK_Y_TOP, "blue", "VC", "BOTTOM VC"),
        ("BottomOut1", True, bot_x[2], JACK_Y_TOP, "black", "OUT 1", "BOTTOM OUT 1"),
        ("BottomOut2", True, bot_x[3], JACK_Y_TOP, "black", "OUT 2", "BOTTOM OUT 2"),
        ("SlopeIn", False, s1_x[0], JACK_Y_SLOPE, "black", "IN", "SLOPE IN"),
        ("SlopeTrig", False, s1_x[1], JACK_Y_SLOPE, "red", "TRIG", "SLOPE TRIG"),
        ("SlopeVc", False, s1_x[2], JACK_Y_SLOPE, "blue", "VC", "SLOPE VC"),
        ("SlopeEnd", True, s1_x[3], JACK_Y_SLOPE, "red", "END", "SLOPE END"),
        ("SlopeOut", True, s1_x[4], JACK_Y_SLOPE, "black", "OUT", "SLOPE OUT"),
        ("SlopeAc", True, s1_x[5], JACK_Y_SLOPE, "black", "AC", "SLOPE AC"),
        ("Slope2In", False, s2_x[0], JACK_Y_SLOPE, "black", "IN", "SLOPE 2 IN"),
        ("Slope2Trig", False, s2_x[1], JACK_Y_SLOPE, "red", "TRIG", "SLOPE 2 TRIG"),
        ("Slope2Vc", False, s2_x[2], JACK_Y_SLOPE, "blue", "VC", "SLOPE 2 VC"),
        ("Slope2End", True, s2_x[3], JACK_Y_SLOPE, "red", "END", "SLOPE 2 END"),
        ("Slope2Out", True, s2_x[4], JACK_Y_SLOPE, "black", "OUT", "SLOPE 2 OUT"),
        ("Slope2Ac", True, s2_x[5], JACK_Y_SLOPE, "black", "AC", "SLOPE 2 AC"),
    ]
    knobs = [
        ("TopGain", 274, 176, 36),
        ("Fold", 668, 170, 36),
        ("In2", 940, 170, 36),
        ("VcDepth", 866, 278, 30),
        ("BottomGain", 1326, 170, 36),
        ("BottomVc", 1260, 278, 30),
        ("Rise", 150, 648, 36),
        ("Fall", 330, 648, 36),
        ("SlopeVc", 520, 648, 30),
        ("Rise2", 946, 648, 36),
        ("Fall2", 1126, 648, 36),
        ("SlopeVc2", 1316, 648, 30),
    ]
    # Value wells. Top Gain and Bottom Level show 0 at the CCW stop, not 0.000.
    readouts = [
        ("TopGain", 274, 248, 72, 24),
        ("Fold", 668, 236, 72, 24),
        ("In2", 940, 236, 80, 24),
        ("BottomGain", 1326, 236, 72, 24),
        ("Rise", 150, 732, 84, 22),
        ("Fall", 330, 732, 84, 22),
        ("Rise2", 946, 732, 84, 22),
        ("Fall2", 1126, 732, 84, 22),
        ("Rate", 330, 792, 88, 24),
        ("Rate2", 1126, 792, 88, 24),
    ]
    switches = [
        ("TopHi", 430, 300, 48, 28),
        ("Couple", 130, 300, 48, 28),
        ("Cycle", 150, 792, 48, 28),
        ("Route", 520, 792, 48, 28),
        ("Cycle2", 946, 792, 48, 28),
        ("Route2", 1316, 792, 48, 28),
    ]
    return {
        "w": PLATE_W,
        "h": PLATE_H,
        "jacks": [
            {
                "key": key,
                "output": output,
                "x": x,
                "y": y,
                "colour": colour,
                "name": name,
                "label": label,
                "r": JACK_R,
            }
            for key, output, x, y, colour, name, label in jacks
        ],
        "knobs": [{"key": k, "x": x, "y": y, "d": d, "r": d / 2} for k, x, y, d in knobs],
        "readouts": [
            {"key": k, "x": x, "y": y, "w": w, "h": h} for k, x, y, w, h in readouts
        ],
        "switches": [
            {"key": k, "x": x, "y": y, "w": w, "h": h} for k, x, y, w, h in switches
        ],
        "min_label": MIN_LABEL,
    }


NUT = {
    "black": "#1c1c1c",
    "blue": "#1d4ed8",
    "white": "#f7f7f4",
    "red": "#c4232a",
}


def _polar(cx, cy, radius, norm):
    th = math.radians(-130.0 + 260.0 * norm)
    return cx + radius * math.sin(th), cy - radius * math.cos(th)


def _by_key(items):
    return {item["key"]: item for item in items}


def render(text_path, fmt, append_lever, bold, reg):
    lay = layout()
    jack = _by_key(lay["jacks"])
    knob = _by_key(lay["knobs"])
    o = []
    o.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{PLATE_W}" height="{PLATE_H}" '
        f'viewBox="0 0 {PLATE_W} {PLATE_H}">'
    )
    o.append("<!-- Serge Middle combined plate, paperface grid. Copyright (c) 2026 Martial Systems LLC. All rights reserved. -->")
    o.append(
        "<!-- 1600 by 960. Top row y 64 h 430: TOP, MIDDLE, BOTTOM. "
        "Bottom row y 540 h 400: SLOPE 1, SLOPE 2. "
        "Jacks are the large circles. Dashed lines are normals. "
        "COUPLE is plate art. Other knobs and levers are editor controls. "
        "Text is outlined (Liberation Sans). Live positions match PatchView.h. -->"
    )
    o.append(f'<rect width="{PLATE_W}" height="{PLATE_H}" fill="{PAPER}"/>')
    o.append(f'<rect x="8" y="8" width="{PLATE_W - 16}" height="{PLATE_H - 16}" fill="none" stroke="{INK}" stroke-width="2.4"/>')
    o.append(f'<rect x="14" y="14" width="{PLATE_W - 28}" height="{PLATE_H - 28}" fill="none" stroke="{INK}" stroke-width="0.8"/>')

    _title(o, text_path, bold)
    for key, title, sub in (
        ("top", "TOP", "VCA"),
        ("mid", "MIDDLE", "PLEAT"),
        ("bot", "BOTTOM", "FULL WAVE"),
        ("s1", "SLOPE 1", "DUSG"),
        ("s2", "SLOPE 2", "DUSG"),
    ):
        _section(o, text_path, bold, SECTIONS[key], title, sub)

    _ticks_unit(o, text_path, reg, knob["TopGain"], "GAIN", [("0", 0), ("0.5", 0.5), ("1", 1)])
    _ticks_unit(o, text_path, reg, knob["Fold"], "FOLD", [("0.5", 0), ("1", 0.25), ("2", 0.5), ("4", 0.75), ("8", 1)])
    _ticks_unit(o, text_path, reg, knob["In2"], "IN 2 DC", [("-2.5", 0), ("0", 0.5), ("+2.5", 1)])
    _ticks_unit(o, text_path, reg, knob["VcDepth"], "VC DEPTH", [("0", 0), ("1", 1)], label_r=28)
    _ticks_unit(o, text_path, reg, knob["BottomGain"], "LEVEL", [("0", 0), ("0.5", 0.5), ("1", 1)])
    _ticks_unit(o, text_path, reg, knob["BottomVc"], "VC ATTEN", [("0", 0), ("1", 1)], label_r=28)
    for key in ("Rise", "Fall", "Rise2", "Fall2"):
        _ticks_time(o, text_path, reg, knob[key], "RISE" if "Rise" in key else "FALL")
    for key in ("SlopeVc", "SlopeVc2"):
        _ticks_unit(o, text_path, reg, knob[key], "VC", [("-1", 0), ("0", 0.5), ("+1", 1)], label_r=26)

    for item in lay["readouts"]:
        _well(o, item["x"], item["y"], item["w"], item["h"])

    _switch_legend(o, text_path, bold, reg, 130, 300, "COUPLE", "AC", "DC")
    _switch_legend(o, text_path, bold, reg, 430, 300, "MODE", "LO", "HI")
    for x in (150, 946):
        _switch_legend(o, text_path, bold, reg, x, 792, "CYCLE", "OFF", "ON")
    for x in (520, 1316):
        _switch_legend(o, text_path, bold, reg, x, 792, "BOTH", "RISE", "FALL")
    # COUPLE stays on the plate. The audio path is the black AC jack.
    append_lever(o, 130, 300, "left", "couple")

    _flows(o, text_path, bold, jack, knob)
    for item in lay["jacks"]:
        _banana(o, text_path, bold, item)
    _foot_tags(o, text_path, bold, jack)

    o.append("</svg>")
    return "\n".join(o) + "\n"


def _title(o, text_path, bold):
    o.append(f'<rect x="18" y="18" width="1564" height="38" fill="{INK}"/>')
    o.append(f'<path fill="{CREAM}" d="{text_path("SERGE MIDDLE", 32, 43, 18, bold, 1.4, "start")}"/>')
    o.append(f'<path fill="{CREAM}" d="{text_path("WAVE MULTIPLIERS  +  DUAL SLOPE", 250, 43, 13, bold, 1.1, "start")}"/>')
    legend = [("black", "BIPOLAR"), ("blue", "CV"), ("white", "DC"), ("red", "GATE")]
    x = 1188
    for colour, word in legend:
        o.append(f'<circle cx="{x}" cy="37" r="7" fill="{NUT[colour]}" stroke="{CREAM}" stroke-width="1"/>')
        o.append(f'<path fill="{CREAM}" d="{text_path(word, x + 12, 42, 12, bold, 0.8, "start")}"/>')
        x += 96


def _section(o, text_path, bold, box, title, sub):
    x, y, w, h = box
    o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{PAPER}" stroke="{INK}" stroke-width="1.7"/>')
    o.append(f'<rect x="{x}" y="{y}" width="{w}" height="30" fill="{INK}"/>')
    o.append(f'<path fill="{CREAM}" d="{text_path(title, x + 12, y + 21, 15, bold, 1.2, "start")}"/>')
    o.append(f'<path fill="{CREAM}" d="{text_path(sub, x + w - 12, y + 21, 12, bold, 0.8, "end")}"/>')


def _well(o, cx, cy, w, h):
    o.append(
        f'<rect x="{fmt_num(cx - w / 2)}" y="{fmt_num(cy - h / 2)}" width="{w}" height="{h}" '
        f'fill="{WELL}" stroke="{INK}" stroke-width="1"/>'
    )


def fmt_num(v):
    return f"{v:.2f}".rstrip("0").rstrip(".")


def _ticks_unit(o, text_path, reg, knob, label, marks, label_r=None):
    cx, cy, radius = knob["x"], knob["y"], knob["r"]
    o.append(f'<path fill="{INK}" d="{text_path(label, cx, cy - radius - 16, 13, reg, 0.4)}"/>')
    inner = radius + 6
    for i in range(11):
        x0, y0 = _polar(cx, cy, inner, i / 10)
        x1, y1 = _polar(cx, cy, inner + (6 if i % 5 == 0 else 3.5), i / 10)
        o.append(
            f'<line x1="{fmt_num(x0)}" y1="{fmt_num(y0)}" x2="{fmt_num(x1)}" y2="{fmt_num(y1)}" '
            f'stroke="{INK}" stroke-width="{1.3 if i % 5 == 0 else 0.8}" stroke-linecap="round"/>'
        )
    lr = label_r if label_r is not None else radius + 22
    size = 12 if radius < 17 else 12
    for txt, norm in marks:
        x, y = _polar(cx, cy, lr, norm)
        o.append(f'<path fill="{INK}" d="{text_path(txt, x, y + 3.5, size, reg, 0.15)}"/>')


def _ticks_time(o, text_path, reg, knob, label):
    cx, cy, radius = knob["x"], knob["y"], knob["r"]
    o.append(f'<path fill="{INK}" d="{text_path(label, cx, cy - radius - 16, 13, reg, 0.6)}"/>')
    inner = radius + 6
    for decade in range(5):
        for mark in (2, 5):
            norm = (decade + math.log10(mark)) / 5
            x0, y0 = _polar(cx, cy, inner, norm)
            x1, y1 = _polar(cx, cy, inner + 3.2, norm)
            o.append(
                f'<line x1="{fmt_num(x0)}" y1="{fmt_num(y0)}" x2="{fmt_num(x1)}" y2="{fmt_num(y1)}" '
                f'stroke="{INK}" stroke-width="0.8" stroke-linecap="round"/>'
            )
    for decade in range(6):
        norm = decade / 5
        x0, y0 = _polar(cx, cy, inner, norm)
        x1, y1 = _polar(cx, cy, inner + 6, norm)
        o.append(
            f'<line x1="{fmt_num(x0)}" y1="{fmt_num(y0)}" x2="{fmt_num(x1)}" y2="{fmt_num(y1)}" '
            f'stroke="{INK}" stroke-width="1.3" stroke-linecap="round"/>'
        )
    # Outer end label only. The marks that face the other time knob stay ticks, so the VC knob stays clear.
    for seconds, txt in ((1e-4, "0.1 ms"), (1e-2, "10 ms"), (1e-1, "0.1 s"), (10.0, "10 s")):
        norm = math.log10(seconds / 1e-4) / 5
        # Skip the label that faces the neighbouring time knob.
        faces_in = (label == "RISE" and txt in ("10 ms", "0.1 s", "10 s")) or (
            label == "FALL" and txt in ("0.1 ms", "10 ms", "0.1 s")
        )
        if faces_in and txt not in ("0.1 ms", "10 s"):
            continue
        if label == "RISE" and txt == "10 s":
            continue
        if label == "FALL" and txt == "0.1 ms":
            continue
        x, y = _polar(cx, cy, radius + 26, norm)
        o.append(f'<path fill="{INK}" d="{text_path(txt, x, y + 4, 12, reg, 0.1)}"/>')
    # 5 ms, the default, as a dot inside the arc.
    norm = math.log10(0.005 / 1e-4) / 5
    x, y = _polar(cx, cy, radius + 2, norm)
    o.append(f'<circle cx="{fmt_num(x)}" cy="{fmt_num(y)}" r="1.7" fill="{INK}"/>')


def _switch_legend(o, text_path, bold, reg, cx, cy, title, left, right):
    o.append(f'<path fill="{INK}" d="{text_path(title, cx, cy - 24, 12, bold, 0.7)}"/>')
    o.append(
        f'<rect x="{cx - 27}" y="{cy - 15}" width="54" height="30" fill="{WELL}" stroke="{INK}" stroke-width="1"/>'
    )
    o.append(f'<path fill="{INK}" d="{text_path(left, cx - 34, cy + 4, 12, reg, 0.3, "end")}"/>')
    o.append(f'<path fill="{INK}" d="{text_path(right, cx + 34, cy + 4, 12, reg, 0.3, "start")}"/>')


def _above_name(item):
    """Y just above a jack name that sits over the nut, clear of the letters."""
    return item["y"] - 54


def _banana(o, text_path, bold, item):
    cx, cy = item["x"], item["y"]
    nut = NUT[item["colour"]]
    ring = CREAM if item["colour"] == "white" else INK
    # Slope jacks name downward so the bus above them stays clear of the words.
    name_y = cy + 40 if cy > 700 else cy - 36
    o.append(f'<circle cx="{cx}" cy="{cy}" r="{JACK_R}" fill="{nut}" stroke="{ring}" stroke-width="1.6"/>')
    o.append(f'<circle cx="{cx}" cy="{cy}" r="12" fill="#d9dce2" stroke="{INK}" stroke-width="0.8"/>')
    o.append(f'<circle cx="{cx}" cy="{cy}" r="5.2" fill="#111111"/>')
    o.append(f'<path fill="{INK}" d="{text_path(item["name"], cx, name_y, 14, bold, 0.5)}"/>')


def _flow(o, pts, dashed=False, head=True):
    if len(pts) < 2:
        return
    draw = list(pts)
    if head:
        x0, y0 = draw[-2]
        x1, y1 = draw[-1]
        dx, dy = x1 - x0, y1 - y0
        length = math.hypot(dx, dy) or 1.0
        draw[-1] = (x1 - dx / length * 8, y1 - dy / length * 8)
        _head(o, x0, y0, x1, y1)
    d = "M " + " L ".join(f"{fmt_num(x)} {fmt_num(y)}" for x, y in draw)
    dash = ' stroke-dasharray="6 4"' if dashed else ""
    o.append(
        f'<path d="{d}" fill="none" stroke="{INK}" stroke-width="1.5" '
        f'stroke-linejoin="miter" stroke-linecap="square"{dash}/>'
    )


def _head(o, x0, y0, x1, y1, size=9):
    dx, dy = x1 - x0, y1 - y0
    length = math.hypot(dx, dy) or 1.0
    ux, uy = dx / length, dy / length
    px, py = -uy, ux
    bx, by = x1 - ux * size, y1 - uy * size
    o.append(
        f'<path d="M{fmt_num(x1)} {fmt_num(y1)} L{fmt_num(bx + px * 4.2)} {fmt_num(by + py * 4.2)} '
        f'L{fmt_num(bx - px * 4.2)} {fmt_num(by - py * 4.2)} Z" fill="{INK}"/>'
    )


def _box(o, text_path, bold, cx, cy, w, h, word):
    o.append(
        f'<rect x="{fmt_num(cx - w / 2)}" y="{fmt_num(cy - h / 2)}" width="{w}" height="{h}" '
        f'fill="{WELL}" stroke="{INK}" stroke-width="1.3"/>'
    )
    o.append(f'<path fill="{INK}" d="{text_path(word, cx, cy + 4, 12, bold, 0.6)}"/>')


def _flows(o, text_path, bold, jack, knob):
    # Lines stop above the jack names. Knob lines start under the value wells.
    def up(item):
        return item["x"], _above_name(item)

    # Top: IN and VC into the VCA, GAIN down into it, OUT leaving.
    _box(o, text_path, bold, 274, 348, 108, 28, "VCA")
    _flow(o, [up(jack["TopIn"]), (jack["TopIn"]["x"], 348), (220, 348)])
    _flow(o, [up(jack["TopVc"]), (274, 362)])
    _flow(o, [(knob["TopGain"]["x"], 268), (274, 334)])
    _flow(o, [(328, 348), (jack["TopOut"]["x"], 348), up(jack["TopOut"])])

    # Middle: inputs into the cells, fold into the law, OUT to the host.
    _box(o, text_path, bold, 800, 348, 120, 28, "6 CELLS")
    _flow(o, [up(jack["PleatIn1"]), (jack["PleatIn1"]["x"], 348), (740, 348)])
    _flow(o, [up(jack["PleatIn2"]), (jack["PleatIn2"]["x"], 368), (748, 368), (748, 362)])
    _flow(o, [(knob["Fold"]["x"], 258), (knob["Fold"]["x"], 328), (748, 328), (748, 334)])
    _flow(o, [(knob["VcDepth"]["x"], knob["VcDepth"]["y"] + 20), up(jack["PleatVc"])])
    _flow(o, [(860, 348), (jack["PleatOut"]["x"], 348), up(jack["PleatOut"])])
    # IN 2 DC knob is the normal into IN 2. The run stays under the nuts.
    in2 = jack["PleatIn2"]
    _flow(
        o,
        [
            (knob["In2"]["x"], 258),
            (knob["In2"]["x"], 470),
            (in2["x"], 470),
            (in2["x"], in2["y"] + JACK_R),
        ],
        dashed=True,
    )
    o.append(f'<path fill="{INK}" d="{text_path("NORMAL", 860, 466, 12, bold, 0.4)}"/>')

    # Bottom: one rectifier, two outputs. LEVEL and the VC attenuator feed it.
    _box(o, text_path, bold, 1320, 348, 116, 28, "RECTIFY")
    _flow(o, [up(jack["BottomIn"]), (jack["BottomIn"]["x"], 348), (1262, 348)])
    _flow(o, [(knob["BottomGain"]["x"], 258), (1320, 334)])
    _flow(o, [(knob["BottomVc"]["x"], knob["BottomVc"]["y"] + 20), up(jack["BottomVc"])])
    _flow(o, [(1378, 340), (jack["BottomOut1"]["x"], 340), up(jack["BottomOut1"])])
    _flow(
        o,
        [
            (1378, 358),
            (jack["BottomOut2"]["x"], 358),
            up(jack["BottomOut2"]),
        ],
    )

    for prefix in ("", "2"):
        inn = jack["SlopeIn" if prefix == "" else "Slope2In"]
        trig = jack["SlopeTrig" if prefix == "" else "Slope2Trig"]
        vc = jack["SlopeVc" if prefix == "" else "Slope2Vc"]
        end = jack["SlopeEnd" if prefix == "" else "Slope2End"]
        out = jack["SlopeOut" if prefix == "" else "Slope2Out"]
        ac = jack["SlopeAc" if prefix == "" else "Slope2Ac"]
        bus = 816
        # Inputs point into the bus. Outputs point out to the jacks. Names sit under the nuts.
        for item in (inn, trig, vc):
            _flow(o, [(item["x"], item["y"] - JACK_R), (item["x"], bus)])
        for item in (end, out, ac):
            _flow(o, [(item["x"], bus), (item["x"], item["y"] - JACK_R)])
        _flow(o, [(inn["x"], bus), (ac["x"], bus)], head=False)
        # Cycle is the END to TRIG cord, drawn above the nuts.
        cord = 832
        _flow(
            o,
            [
                (end["x"], end["y"] - JACK_R),
                (end["x"], cord),
                (trig["x"], cord),
                (trig["x"], trig["y"] - JACK_R),
            ],
            dashed=True,
        )
        o.append(
            f'<path fill="{INK}" d="{text_path("CYCLE", (vc["x"] + end["x"]) / 2, 806, 12, bold, 0.5)}"/>'
        )

    # Slope 1 OUT is normalled to PLEAT VC. The line clears the levers, then the gutter.
    out = jack["SlopeOut"]
    vc = jack["PleatVc"]
    _flow(
        o,
        [
            (out["x"], out["y"] - JACK_R),
            (out["x"], 800),
            (800, 800),
            (800, 516),
            (vc["x"], 516),
            (vc["x"], vc["y"] + JACK_R),
        ],
        dashed=True,
    )
    o.append(
        f'<path fill="{INK}" d="{text_path("NORMAL: SLOPE 1 OUT TO PLEAT VC", 400, 534, 13, bold, 0.4)}"/>'
    )


def _foot_tags(o, text_path, bold, jack):
    def tag(key, word, dy):
        item = jack[key]
        o.append(f'<path fill="{INK}" d="{text_path(word, item["x"], item["y"] + dy, 12, bold, 0.3)}"/>')

    tag("PleatIn1", "HOST", 42)
    tag("PleatOut", "HOST", 42)
    tag("TopIn", "0 V", 42)
    tag("BottomIn", "0 V", 42)
    # Slope names already sit under the nut. The normal tag is the next line.
    tag("SlopeTrig", "MIDI", 58)
    tag("Slope2Trig", "0 V", 58)
