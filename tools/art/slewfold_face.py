"""SLEWFOLD face: ids_v6 centres on the 1600 by 960 drawing.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Centres for jacks, knobs, and switches are ids_v6.json. Knob data-angle,
readout rects, and LED centres are the same ids in slewfold_v6.svg. The
editor's PatchView.h repeats this table. tests/test_panel_layout.py checks
the two copies.
"""

from __future__ import annotations

PLATE_W = 1600
PLATE_H = 960
MIN_LABEL = 12
JACK_R = 16.5
KNOB_D = 72
SWITCH_HIT = 44

# id, output, patch enum, x, y, hover label, jack colour, Lift cloth for outputs
JACKS = [
    ("jack_top_in", False, "TopIn", 80, 220, "TOP IN", "black", 0),
    ("jack_top_vc", False, "TopVc", 80, 390, "TOP VC", "grey", 0),
    ("jack_top_out", True, "TopOut", 480, 305, "TOP OUT", "black", 4),
    ("jack_pleat_in1", False, "PleatIn1", 600, 190, "PLEAT IN 1", "black", 0),
    ("jack_pleat_in2", False, "PleatIn2", 600, 305, "PLEAT IN 2", "white", 0),
    ("jack_pleat_vc", False, "PleatVc", 600, 420, "PLEAT VC", "grey", 0),
    ("jack_pleat_out", True, "PleatOut", 1000, 305, "PLEAT OUT", "black", 4),
    ("jack_bottom_in", False, "BottomIn", 1120, 220, "BOTTOM IN", "black", 0),
    ("jack_bottom_vc", False, "BottomVc", 1120, 390, "BOTTOM VC", "grey", 0),
    ("jack_bottom_out1", True, "BottomOut1", 1520, 220, "BOTTOM OUT 1", "black", 4),
    ("jack_bottom_out2", True, "BottomOut2", 1520, 390, "BOTTOM OUT 2", "black", 4),
    ("jack_slope1_in", False, "SlopeIn", 90, 625, "SLOPE IN", "black", 0),
    ("jack_slope1_trig", False, "SlopeTrig", 90, 725, "SLOPE TRIG", "red", 0),
    ("jack_slope1_vc", False, "SlopeVc", 90, 825, "SLOPE VC", "grey", 0),
    ("jack_slope1_out", True, "SlopeOut", 730, 625, "SLOPE OUT", "black", 4),
    ("jack_slope1_ac", True, "SlopeAc", 730, 725, "SLOPE AC", "black", 4),
    ("jack_slope1_end", True, "SlopeEnd", 730, 825, "SLOPE END", "red", 1),
    ("jack_slope2_in", False, "Slope2In", 870, 625, "SLOPE 2 IN", "black", 0),
    ("jack_slope2_trig", False, "Slope2Trig", 870, 725, "SLOPE 2 TRIG", "red", 0),
    ("jack_slope2_vc", False, "Slope2Vc", 870, 825, "SLOPE 2 VC", "grey", 0),
    ("jack_slope2_out", True, "Slope2Out", 1510, 625, "SLOPE 2 OUT", "black", 4),
    ("jack_slope2_ac", True, "Slope2Ac", 1510, 725, "SLOPE 2 AC", "black", 4),
    ("jack_slope2_end", True, "Slope2End", 1510, 825, "SLOPE 2 END", "red", 1),
]

# id, param, cx, cy, data-angle, readout x, y, w, h
KNOBS = [
    ("knob_top_gain", "kTopGain", 270, 220, 42, 243, 249, 54, 19),
    ("knob_fold", "kFold", 770, 190, -50, 743, 219, 54, 19),
    ("knob_in2_dc", "kIn2", 770, 305, 17, 743, 334, 54, 19),
    ("knob_pleat_vc_depth", "kVcDepth", 770, 420, 105, 743, 449, 54, 19),
    ("knob_bottom_level", "kBottomGain", 1290, 220, 12, 1263, 249, 54, 19),
    ("knob_bottom_vc_atten", "kBottomVc", 1290, 390, -73, 1263, 419, 54, 19),
    ("knob_slope1_rise", "kRise", 320, 625, 60, 293, 654, 54, 19),
    ("knob_slope1_fall", "kFall", 460, 625, -5, 433, 654, 54, 19),
    ("knob_slope1_vc_depth", "kSlopeVc", 320, 825, -72, 293, 854, 54, 19),
    ("knob_slope2_rise", "kRise2", 1100, 625, -29, 1073, 654, 54, 19),
    ("knob_slope2_fall", "kFall2", 1240, 625, -46, 1213, 654, 54, 19),
    ("knob_slope2_vc_depth", "kSlopeVc2", 1100, 825, 49, 1073, 854, 54, 19),
]

# id, param or "" when the lever is not a parameter, x, y
SWITCHES = [
    ("switch_top_couple", "", 230, 390),
    ("switch_top_mode", "kTopHi", 340, 390),
    ("switch_slope1_cycle", "kCycle", 460, 735),
    ("switch_slope1_vc_target", "kRoute", 550, 825),
    ("switch_slope2_cycle", "kCycle2", 1240, 735),
    ("switch_slope2_vc_target", "kRoute2", 1330, 825),
]

# id, "end" or "cycle", meter output enum, cycle param or "", x, y
LEDS = [
    ("led_slope1_end", "end", "SlopeEnd", "", 770, 798),
    ("led_slope1_cycle", "cycle", "SlopeOut", "kCycle", 460, 772),
    ("led_slope2_end", "end", "Slope2End", "", 1550, 798),
    ("led_slope2_cycle", "cycle", "Slope2Out", "kCycle2", 1240, 772),
]


def layout():
    jacks = [
        {
            "key": name,
            "output": output,
            "enum": enum,
            "x": x,
            "y": y,
            "label": label,
            "colour": colour,
            "cloth": cloth,
            "r": JACK_R,
        }
        for name, output, enum, x, y, label, colour, cloth in JACKS
    ]
    knobs = [
        {
            "key": name,
            "param": param,
            "x": x,
            "y": y,
            "angle": angle,
            "d": KNOB_D,
            "r": KNOB_D / 2,
            "readout": (rx, ry, rw, rh),
        }
        for name, param, x, y, angle, rx, ry, rw, rh in KNOBS
    ]
    switches = [
        {"key": name, "param": param, "x": x, "y": y, "hit": SWITCH_HIT}
        for name, param, x, y in SWITCHES
    ]
    leds = [
        {"key": name, "kind": kind, "meter": meter, "param": param, "x": x, "y": y}
        for name, kind, meter, param, x, y in LEDS
    ]
    return {
        "w": PLATE_W,
        "h": PLATE_H,
        "min_label": MIN_LABEL,
        "jacks": jacks,
        "knobs": knobs,
        "switches": switches,
        "leds": leds,
    }
