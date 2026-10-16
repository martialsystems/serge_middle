"""SLEWFOLD plate: ids_v6 centres, live knobs, and the Lift cable layer.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
"""

from __future__ import annotations

import importlib.util
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_face():
    return load("slewfold_face", ROOT / "tools" / "art" / "slewfold_face.py")


class PanelLayoutTest(unittest.TestCase):
    def test_window_fits_the_screens(self) -> None:
        lay = load_face().layout()
        self.assertEqual((lay["w"], lay["h"]), (1600, 960))
        self.assertLessEqual(lay["w"], 1920)
        self.assertLessEqual(lay["h"], 1040)
        self.assertLessEqual(1200, 1280)
        self.assertLessEqual(720, 800)
        self.assertGreaterEqual(lay["min_label"], 12)
        self.assertEqual(len(lay["jacks"]), 23)
        self.assertEqual(len(lay["knobs"]), 12)
        self.assertEqual(len(lay["switches"]), 6)
        self.assertEqual(len(lay["leds"]), 4)

    def test_spots_match_the_editor(self) -> None:
        face = load_face()
        view = (ROOT / "SergeMiddleCombined" / "PatchView.h").read_text(encoding="utf-8")
        for jack in face.layout()["jacks"]:
            role = "true" if jack["output"] else "false"
            token = (
                f'"{jack["key"]}", {role}, patch::{jack["enum"]}, '
                f'{jack["x"]}.f, {jack["y"]}.f, "{jack["label"]}", {jack["cloth"]}'
            )
            self.assertIn(token, view, jack["key"])
        for knob in face.layout()["knobs"]:
            rx, ry, rw, rh = knob["readout"]
            token = (
                f'"{knob["key"]}", {knob["param"]}, {knob["x"]}.f, {knob["y"]}.f, '
                f'{knob["angle"]}.f, {rx}.f, {ry}.f, {rw}.f, {rh}.f'
            )
            self.assertIn(token, view, knob["key"])
        for switch in face.layout()["switches"]:
            param = switch["param"] if switch["param"] else "-1"
            token = f'"{switch["key"]}", {param}, {switch["x"]}.f, {switch["y"]}.f'
            self.assertIn(token, view, switch["key"])
        for led in face.layout()["leds"]:
            end = "true" if led["kind"] == "end" else "false"
            param = led["param"] if led["param"] else "-1"
            token = (
                f'"{led["key"]}", {end}, patch::{led["meter"]}, {param}, '
                f'{led["x"]}.f, {led["y"]}.f'
            )
            self.assertIn(token, view, led["key"])

    def test_controls_do_not_pile_up(self) -> None:
        lay = load_face().layout()
        spots = []
        for jack in lay["jacks"]:
            spots.append((jack["key"], jack["x"], jack["y"], jack["r"]))
        for knob in lay["knobs"]:
            spots.append((knob["key"], knob["x"], knob["y"], knob["r"]))
        for switch in lay["switches"]:
            spots.append((switch["key"], switch["x"], switch["y"], switch["hit"] / 2))
        for i, a in enumerate(spots):
            for b in spots[i + 1 :]:
                dist = ((a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5
                self.assertGreater(dist, a[3] + b[3] + 4, f"{a[0]} {b[0]}")

    def test_plate_strips_live_art(self) -> None:
        render = load("render_slewfold", ROOT / "tools" / "art" / "render_slewfold.py")
        text = (ROOT / "SergeMiddleCombined" / "resources" / "slewfold_v6.svg").read_text(encoding="utf-8")
        clean = render.strip_svg(text)
        knobs = re.findall(r'id="(knob_[^"]+)"', clean)
        self.assertTrue(knobs)
        for name in knobs:
            self.assertTrue(name.endswith("_readout"), name)
        self.assertNotIn('id="led_', clean)
        self.assertIn('id="switch_top_couple"', clean)
        self.assertNotIn('id="switch_top_mode"', clean)
        self.assertNotIn('id="switch_slope1_cycle"', clean)
        self.assertIn("readout", clean)
        self.assertIn("jack_top_in", clean)

    def test_bitmaps(self) -> None:
        from PIL import Image

        resources = ROOT / "SergeMiddleCombined" / "resources"
        expect = {
            "slewfold_plate.png": (1600, 960),
            "slewfold_plate@2x.png": (3200, 1920),
            "knob_filmstrip.png": (72, 64 * 72),
            "knob_filmstrip@2x.png": (144, 64 * 144),
        }
        for name, size in expect.items():
            with Image.open(resources / name) as image:
                self.assertEqual(image.size, size, name)


if __name__ == "__main__":
    unittest.main()
