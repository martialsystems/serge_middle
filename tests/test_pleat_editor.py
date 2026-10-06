"""Editor contract for the PLEAT plate.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The gray 300 by 300 window is gone. Fold is still the only parameter the
audio path reads. The plate PNGs are prototype art for the host test.
"""

from __future__ import annotations

import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "SergeMiddle"


def png_ihdr(path: Path) -> tuple[int, int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError(f"{path.name} is not a PNG")
    if data[12:16] != b"IHDR":
        raise AssertionError(f"{path.name} has no IHDR")
    width, height = struct.unpack(">II", data[16:24])
    color_type = data[25]
    return width, height, color_type


class PleatEditorTest(unittest.TestCase):
    def test_window_is_the_plate(self) -> None:
        config = (PLUGIN / "config.h").read_text(encoding="utf-8")
        self.assertIn("#define PLUG_WIDTH 416", config)
        self.assertIn("#define PLUG_HEIGHT 624", config)
        self.assertNotIn("#define PLUG_WIDTH 300", config)
        self.assertNotIn("#define PLUG_HEIGHT 300", config)

    def test_process_block_still_reads_fold_only(self) -> None:
        source = (PLUGIN / "SergeMiddle.cpp").read_text(encoding="utf-8")
        start = source.index("void SergeMiddle::ProcessBlock")
        body = source[start:]
        self.assertIn("GetParam(kFold)->Value()", body)
        self.assertIn("mFold.Step(target)", body)
        self.assertIn("mChannels[c].Step(inputs[c][s], g)", body)
        for token in ("Pleat", "cable", "Jack", "IN 2", "COLOR_GRAY", "IVKnobControl", "AttachCornerResizer", "Serge Middle"):
            self.assertNotIn(token, body)
        self.assertNotIn("COLOR_GRAY", source)
        self.assertNotIn("IVKnobControl", source)
        self.assertNotIn("AttachCornerResizer", source)
        self.assertNotIn("Serge Middle", source)
        self.assertIn('LoadBitmap("pleat_plate.png")', source)
        self.assertIn('LoadBitmap("pleat_knob.png")', source)
        readme = (PLUGIN / "README.md").read_text(encoding="utf-8")
        self.assertIn("prototype art for the host test", readme)
        self.assertIn("ICaptionControl", source)

    def test_cables_stay_out_of_the_audio_path(self) -> None:
        view = (PLUGIN / "PleatView.h").read_text(encoding="utf-8")
        self.assertNotIn("WaveMiddle", view)
        self.assertNotIn('#include "WaveMiddle.h"', view)
        self.assertIn("The audio path does not read mCables", view)
        dsp = (PLUGIN / "dsp" / "WaveMiddle.h").read_text(encoding="utf-8")
        self.assertNotIn("Pleat", dsp)
        self.assertNotIn("pleat_plate", dsp)

    def test_formats_stay_vst3(self) -> None:
        cmake = (PLUGIN / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("FORMATS VST3", cmake)
        self.assertNotIn("FORMATS APP", cmake)
        self.assertNotIn("FORMATS VST3 APP", cmake)
        for name in (
            "resources/pleat_plate.png",
            "resources/pleat_plate@2x.png",
            "resources/pleat_knob.png",
            "resources/pleat_knob@2x.png",
        ):
            self.assertIn(name, cmake)

    def test_plate_and_knob_pngs(self) -> None:
        plate_2x = png_ihdr(PLUGIN / "resources" / "pleat_plate@2x.png")
        plate = png_ihdr(PLUGIN / "resources" / "pleat_plate.png")
        knob_2x = png_ihdr(PLUGIN / "resources" / "pleat_knob@2x.png")
        knob = png_ihdr(PLUGIN / "resources" / "pleat_knob.png")
        self.assertEqual(plate_2x, (832, 1248, 2))
        self.assertEqual(plate, (416, 624, 2))
        self.assertEqual(knob_2x[0], knob_2x[1])
        self.assertEqual(knob_2x[2], 6)
        self.assertEqual(knob[0], knob[1])
        self.assertEqual(knob[2], 6)
        # Integer resize of an odd @2x side can leave the 1x file one pixel short.
        self.assertLessEqual(abs(knob_2x[0] - knob[0] * 2), 1)

    def test_new_prose_has_no_decorative_dash(self) -> None:
        paths = [
            PLUGIN / "PleatView.h",
            PLUGIN / "SergeMiddle.cpp",
            PLUGIN / "README.md",
            ROOT / "BUILD.md",
            Path(__file__),
        ]
        for path in paths:
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("\u2014", text, path.name)
            self.assertNotIn("\u2013", text, path.name)


if __name__ == "__main__":
    unittest.main()
