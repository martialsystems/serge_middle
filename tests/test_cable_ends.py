"""Default routing draws no cables, and every end is an ids_v6 jack.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
"""

from __future__ import annotations

import json
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class CableEndsTest(unittest.TestCase):
    def test_table_matches_ids_v6(self) -> None:
        ids = json.loads((ROOT / "tools" / "art" / "ids_v6.json").read_text(encoding="utf-8"))
        centres = {row[0]: (row[1], row[2]) for row in ids}
        face = (ROOT / "tools" / "art" / "slewfold_face.py").read_text(encoding="utf-8")
        header = (ROOT / "SergeMiddleCombined" / "CableEnds.h").read_text(encoding="utf-8")
        self.assertIn("jack_pleat_in2", centres)
        self.assertEqual(centres["jack_pleat_in2"], (600, 305))
        self.assertEqual(centres["jack_pleat_out"], (1000, 305))
        self.assertEqual(centres["jack_slope1_trig"], (90, 725))
        self.assertNotIn((0, 0), centres.values())
        for name, xy in centres.items():
            if not name.startswith("jack_"):
                continue
            self.assertIn(f'"{name}"', header)
            self.assertIn(f"{xy[0]}.f, {xy[1]}.f", header)
            self.assertIn(f'"{name}"', face)

    def test_default_draws_no_cables(self) -> None:
        binary = ROOT / "build" / "cable_ends_test"
        binary.parent.mkdir(parents=True, exist_ok=True)
        compiled = subprocess.run(
            ["g++", "-std=c++17", "-O2", "-o", str(binary), str(ROOT / "tests" / "cable_ends_main.cpp")],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        ran = subprocess.run([str(binary)], check=False, capture_output=True, text=True)
        self.assertEqual(ran.returncode, 0, ran.stderr)
        self.assertIn("cable ends ok", ran.stdout)

    def test_rope_follows_a_moved_pin(self) -> None:
        binary = ROOT / "build" / "cable_rope_test"
        binary.parent.mkdir(parents=True, exist_ok=True)
        compiled = subprocess.run(
            ["g++", "-std=c++17", "-O2", "-o", str(binary), str(ROOT / "tests" / "cable_rope_main.cpp")],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        ran = subprocess.run([str(binary)], check=False, capture_output=True, text=True)
        self.assertEqual(ran.returncode, 0, ran.stderr)
        self.assertIn("cable rope ok", ran.stdout)


if __name__ == "__main__":
    unittest.main()
