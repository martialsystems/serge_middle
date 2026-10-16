"""Editor and config contract for the combined Serge Middle plugin.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "SergeMiddleCombined"

NORMALS = (
    "Host audio to PLEAT IN 1",
    "IN 2 DC knob to PLEAT IN 2",
    "Slope OUT to PLEAT VC",
    "Host MIDI gate to slope TRIG",
    "Cycle switch is the END to TRIG cord",
    "Slope IN and slope VC are 0 V unpatched",
    "PLEAT OUT to the host",
)

PARAMS = (
    'InitDouble("Fold"',
    'InitDouble("IN 2 DC"',
    'InitDouble("VC Depth"',
    'InitDouble("Rise"',
    'InitDouble("Fall"',
    'InitBool("Cycle"',
    'InitDouble("Slope VC"',
    'InitInt("Route"',
)


class CombinedEditorTest(unittest.TestCase):
    def test_config(self) -> None:
        config = (PLUGIN / "config.h").read_text(encoding="utf-8")
        self.assertIn('#define PLUG_NAME "Serge Middle"', config)
        self.assertIn('#define BUNDLE_NAME "SergeMiddleCombined"', config)
        self.assertIn("#define PLUG_WIDTH 2080", config)
        self.assertIn("#define PLUG_HEIGHT 624", config)
        self.assertIn("#define PLUG_DOES_MIDI_IN 1", config)
        self.assertIn("#define PLUG_DOES_STATE_CHUNKS 1", config)
        self.assertIn("#define PLUG_UNIQUE_ID 'SmCb'", config)
        self.assertIn("#define PLUG_MFR_ID 'Mrsy'", config)
        self.assertNotIn("'Smid'", config)
        self.assertNotIn("'SlAC'", config)
        self.assertIn("#define PLUG_CHANNEL_IO \"1-1 2-2\"", config)
        self.assertIn('#define SHARED_RESOURCES_SUBPATH "SergeMiddleCombined"', config)

    def test_hit_box_and_params(self) -> None:
        view = (PLUGIN / "PatchView.h").read_text(encoding="utf-8")
        match = re.search(r"kJackHit\s*=\s*([0-9]+(?:\.[0-9]+)?)f", view)
        self.assertIsNotNone(match)
        self.assertGreaterEqual(float(match.group(1)), 28.0)
        header = (PLUGIN / "SergeMiddleCombined.h").read_text(encoding="utf-8")
        names = ("kFold = 0", "kIn2", "kVcDepth", "kRise", "kFall", "kCycle", "kSlopeVc", "kRoute", "kNumParams")
        positions = [header.index(name) for name in names]
        self.assertEqual(positions, sorted(positions))
        source = (PLUGIN / "SergeMiddleCombined.cpp").read_text(encoding="utf-8")
        param_at = [source.index(name) for name in PARAMS]
        self.assertEqual(param_at, sorted(param_at))
        body = source[source.index("void SergeMiddleCombined::ProcessBlock") :]
        for token in (
            "GetParam(kFold)",
            "GetParam(kIn2)",
            "GetParam(kVcDepth)",
            "GetParam(kRise)",
            "GetParam(kFall)",
            "GetParam(kCycle)",
            "GetParam(kSlopeVc)",
            "GetParam(kRoute)",
            "BlockMeter",
            "mMidiQueue",
        ):
            self.assertIn(token, body)
        for token in ("SerializeRouting", "ParseRouting", "SerializeState", "UnserializeState"):
            self.assertIn(token, source)

    def test_readme_normals_and_resources(self) -> None:
        readme = (PLUGIN / "README.md").read_text(encoding="utf-8")
        for phrase in NORMALS:
            self.assertIn(phrase, readme)
        for phrase in (
            "modelling choice",
            "La Bestia",
            "one-sample delay",
            "VC Depth",
            "modern-panel",
            "MIDI input port",
            "0x52",
            "GTS",
            "481 samples",
            "releases every held note",
            "## Gaps",
            "attenuverter",
            "1V/OCT",
            "sidechain",
            "host-tested",
            "SLOPE END  +5.00 V",
        ):
            self.assertIn(phrase, readme)
        source = (PLUGIN / "SergeMiddleCombined.cpp").read_text(encoding="utf-8")
        self.assertIn("mNotes.AllOff()", source)
        self.assertIn("chunk.Size() - pos >= patch::kRoutingBytes", source)
        view = (PLUGIN / "PatchView.h").read_text(encoding="utf-8")
        self.assertIn("CableColor(input)", view)
        self.assertNotIn("What it is not", readme)
        cmake = (PLUGIN / "CMakeLists.txt").read_text(encoding="utf-8")
        for name in (
            "resources/middle_plate.png",
            "resources/middle_plate@2x.png",
            "../SergeMiddle/resources/pleat_knob.png",
            "../SergeMiddle/resources/pleat_knob@2x.png",
            "../SlopeAC/resources/slope_switch.png",
            "../SlopeAC/resources/slope_switch@2x.png",
            "../SlopeAC/resources/slope_route.png",
            "../SlopeAC/resources/slope_route@2x.png",
            "../SergeMiddle/resources/fonts/Roboto-Regular.ttf",
            "FORMATS VST3",
        ):
            self.assertIn(name, cmake)
        self.assertNotIn("FORMATS APP", cmake)

    def test_prose_has_no_decorative_dash(self) -> None:
        paths = [
            PLUGIN / "README.md",
            PLUGIN / "PatchView.h",
            PLUGIN / "SergeMiddleCombined.cpp",
            PLUGIN / "SergeMiddleCombined.h",
            PLUGIN / "dsp" / "Patch.h",
            PLUGIN / "config.h",
            ROOT / "BUILD.md",
            ROOT / "README.md",
            Path(__file__),
        ]
        for path in paths:
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("\u2014", text, path.name)
            self.assertNotIn("\u2013", text, path.name)


if __name__ == "__main__":
    unittest.main()
