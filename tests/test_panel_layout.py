"""Combined plate layout: one grid, readable type, jacks clear of the knobs.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
"""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_plate():
    path = ROOT / "tools" / "art" / "paper_plate.py"
    spec = importlib.util.spec_from_file_location("paper_plate", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PanelLayoutTest(unittest.TestCase):
    def test_window_fits_the_screens(self) -> None:
        plate = load_plate()
        lay = plate.layout()
        self.assertEqual((lay["w"], lay["h"]), (1600, 960))
        self.assertLessEqual(lay["w"], 1920)
        self.assertLessEqual(lay["h"], 1040)
        self.assertLessEqual(1200, 1280)
        self.assertLessEqual(720, 800)
        self.assertGreaterEqual(lay["min_label"], 12)

    def test_jacks_match_the_editor(self) -> None:
        plate = load_plate()
        view = (ROOT / "SergeMiddleCombined" / "PatchView.h").read_text(encoding="utf-8")
        for jack in plate.layout()["jacks"]:
            token = f"{jack['x']}.f / kDesignW, {jack['y']}.f / kDesignH, \"{jack['label']}\""
            self.assertIn(token, view, jack["key"])

    def test_knobs_match_the_editor(self) -> None:
        plate = load_plate()
        source = (ROOT / "SergeMiddleCombined" / "SergeMiddleCombined.cpp").read_text(encoding="utf-8")
        for knob in plate.layout()["knobs"]:
            token = f"{knob['x']}.f, {knob['y']}.f, {knob['d']}.f"
            self.assertIn(token, source, knob["key"])

    def test_controls_do_not_pile_up(self) -> None:
        lay = load_plate().layout()
        jacks = lay["jacks"]
        knobs = lay["knobs"]
        for i, a in enumerate(jacks):
            for b in jacks[i + 1 :]:
                dist = ((a["x"] - b["x"]) ** 2 + (a["y"] - b["y"]) ** 2) ** 0.5
                self.assertGreater(dist, a["r"] + b["r"] + 8, f"{a['key']} {b['key']}")
        for knob in knobs:
            for jack in jacks:
                dist = ((knob["x"] - jack["x"]) ** 2 + (knob["y"] - jack["y"]) ** 2) ** 0.5
                self.assertGreater(dist, knob["r"] + jack["r"] + 8, f"{knob['key']} {jack['key']}")
        by_knob = {item["key"]: item for item in knobs}
        by_switch = {item["key"]: item for item in lay["switches"]}
        rise = by_knob["Rise"]
        fall = by_knob["Fall"]
        vc = by_knob["SlopeVc"]
        self.assertGreater(abs(vc["x"] - rise["x"]), 140)
        self.assertGreater(abs(vc["x"] - fall["x"]), 140)
        cycle = by_switch["Cycle"]
        route = by_switch["Route"]
        self.assertGreater(abs(cycle["x"] - route["x"]), 250)


if __name__ == "__main__":
    unittest.main()
