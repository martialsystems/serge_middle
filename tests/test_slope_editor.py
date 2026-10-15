"""Editor contract for the Slope AC plate.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Rise, Fall, and Cycle are the parameters the audio path reads. The plate and
switch PNGs are the panel art.
"""

from __future__ import annotations

import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "SlopeAC"


def png_ihdr(path: Path) -> tuple[int, int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError(f"{path.name} is not a PNG")
    if data[12:16] != b"IHDR":
        raise AssertionError(f"{path.name} has no IHDR")
    width, height = struct.unpack(">II", data[16:24])
    color_type = data[25]
    return width, height, color_type


class SlopeEditorTest(unittest.TestCase):
    def test_window_is_the_plate(self) -> None:
        config = (PLUGIN / "config.h").read_text(encoding="utf-8")
        self.assertIn("#define PLUG_WIDTH 416", config)
        self.assertIn("#define PLUG_HEIGHT 624", config)
        self.assertNotIn("#define PLUG_WIDTH 320", config)
        self.assertNotIn("#define PLUG_HEIGHT 180", config)

    def test_process_block_reads_rise_fall_and_cycle(self) -> None:
        source = (PLUGIN / "SlopeAC.cpp").read_text(encoding="utf-8")
        start = source.index("void SlopeAC::ProcessBlock")
        body = source[start:]
        self.assertIn("GetParam(kRise)->Value()", body)
        self.assertIn("GetParam(kFall)->Value()", body)
        self.assertIn("GetParam(kCycle)->Bool()", body)
        self.assertIn("GetParam(kVc)->Value()", body)
        self.assertIn("GetParam(kRoute)->Int()", body)
        self.assertIn("slope::HostSample(", body)
        self.assertLess(body.index("GetParam(kCycle)"), body.index("GetParam(kVc)"))
        self.assertLess(body.index("GetParam(kVc)"), body.index("GetParam(kRoute)"))
        self.assertNotIn("slope::PluginSample(", body)
        self.assertNotIn("PluginSampleOneShot", body)
        self.assertEqual(body.count("GetParam(kCycle)"), 1)

    def test_plate_and_switch_pngs(self) -> None:
        plate = png_ihdr(PLUGIN / "resources" / "slope_plate.png")
        plate_2x = png_ihdr(PLUGIN / "resources" / "slope_plate@2x.png")
        switch = png_ihdr(PLUGIN / "resources" / "slope_switch.png")
        switch_2x = png_ihdr(PLUGIN / "resources" / "slope_switch@2x.png")
        route = png_ihdr(PLUGIN / "resources" / "slope_route.png")
        route_2x = png_ihdr(PLUGIN / "resources" / "slope_route@2x.png")
        self.assertEqual(plate, (416, 624, 2))
        self.assertEqual(plate_2x, (832, 1248, 2))
        self.assertEqual(switch, (48, 56, 6))
        self.assertEqual(switch_2x, (96, 112, 6))
        self.assertEqual(route, (48, 84, 6))
        self.assertEqual(route_2x, (96, 168, 6))
        self.assertTrue((PLUGIN / "resources" / "slope_plate.svg").is_file())

    def test_resources_and_fractions(self) -> None:
        cmake = (PLUGIN / "CMakeLists.txt").read_text(encoding="utf-8")
        for name in (
            "resources/fonts/Roboto-Regular.ttf",
            "resources/slope_plate.png",
            "resources/slope_plate@2x.png",
            "resources/slope_switch.png",
            "resources/slope_switch@2x.png",
            "resources/slope_route.png",
            "resources/slope_route@2x.png",
            "pleat_knob.png",
            "pleat_knob@2x.png",
        ):
            self.assertIn(name, cmake)
        view = (PLUGIN / "SlopeView.h").read_text(encoding="utf-8")
        source = (PLUGIN / "SlopeAC.cpp").read_text(encoding="utf-8")
        for token in (
            "0.30f",
            "0.70f",
            "0.38f",
            "0.29f",
            "0.495f",
            "0.545f",
            "0.655f",
            "0.633f",
            "0.677f",
            "LoadBitmap(\"slope_plate.png\")",
            "LoadBitmap(\"pleat_knob.png\")",
            "LoadBitmap(\"slope_switch.png\", 2)",
            "LoadBitmap(\"slope_route.png\", 3)",
            "\"OFF\"",
        ):
            self.assertIn(token, view if token.startswith("0.") or token == "\"OFF\"" else source)
        self.assertIn("\"OFF\"", view)
        self.assertNotIn("ONE SHOT", view)
        self.assertNotIn("Serge", source)
        self.assertNotIn("Serge", view)

    def test_new_prose_has_no_decorative_dash(self) -> None:
        paths = [
            PLUGIN / "README.md",
            ROOT / "slope" / "README.md",
            Path(__file__),
            ROOT / "tests" / "test_symmetry.py",
        ]
        for path in paths:
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("\u2014", text, path.name)
            self.assertNotIn("\u2013", text, path.name)


if __name__ == "__main__":
    unittest.main()
