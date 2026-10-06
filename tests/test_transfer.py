"""Acceptance test: six-cell transfer curve at g = 1 on [-6, 6].

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The numeric lock is tests/transfer_g1.csv. The figure is tests/transfer_g1.svg.
Both must match wave_middle.py. Odd symmetry is process(-x) == -process(x).
"""

from __future__ import annotations

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import wave_middle as wm

# Principal-branch values from scipy.special.lambertw on
# z = (Is*R)/(eta*VT) * exp(|v|/(eta*VT)), checked once outside this file.
_SCIPY_W = {
    1e-4: 0.0019156348759897398,
    0.2: 0.16275287525431673,
    1.0: 14.120871602551665,
    6.0: 127.0569837407369,
}


def _residual(abs_v: float) -> float:
    w = wm.lambert_w0_kexp(abs_v)
    log_z = abs_v / (wm.ETA * wm.VT) + math.log((wm.IS * wm.R) / (wm.ETA * wm.VT))
    return w + math.log(w) - log_z


class TransferAcceptanceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.samples = wm.acceptance_samples()
        cls.csv_path = ROOT / "tests" / "transfer_g1.csv"
        cls.svg_path = ROOT / "tests" / "transfer_g1.svg"
        cls.readme = (ROOT / "README.md").read_text(encoding="utf-8")

    def test_constants(self) -> None:
        self.assertEqual(wm.VT, 0.02585)
        self.assertEqual(wm.IS, 2.52e-9)
        self.assertEqual(wm.ETA, 1.68)
        self.assertEqual(wm.R, 33000.0)
        self.assertEqual(wm.N_CELLS, 6)
        self.assertEqual(wm.G_MIN, 0.5)
        self.assertEqual(wm.G_MAX, 8.0)
        self.assertEqual(wm.G_DEFAULT, 1.0)
        self.assertEqual(wm.VIN_MIN, -6.0)
        self.assertEqual(wm.VIN_MAX, 6.0)
        self.assertEqual(wm.N_SAMPLES, 6001)

    def test_grid_is_odd_and_closed(self) -> None:
        vin = [v for v, _ in self.samples]
        self.assertEqual(len(vin), 6001)
        self.assertEqual(vin[0], -6.0)
        self.assertEqual(vin[-1], 6.0)
        self.assertEqual(vin[3000], 0.0)
        for i, v in enumerate(vin):
            self.assertEqual(v, -vin[-1 - i])

    def test_csv_matches_map(self) -> None:
        text = self.csv_path.read_text(encoding="utf-8")
        self.assertEqual(text, wm.acceptance_csv(self.samples))
        lines = text.splitlines()
        self.assertEqual(lines[0], "vin,vout")
        self.assertEqual(len(lines), 6002)
        for line, (v, y) in zip(lines[1:], self.samples):
            raw_v, raw_y = line.split(",")
            self.assertEqual(float(raw_v), v)
            self.assertEqual(float(raw_y), y)
            self.assertEqual(y, wm.process(v, 1.0))

    def test_svg_matches_renderer(self) -> None:
        text = self.svg_path.read_text(encoding="utf-8")
        self.assertEqual(text, wm.acceptance_svg(self.samples))
        self.assertIn("Serge middle section, g = 1", text)
        self.assertIn("<polyline ", text)
        self.assertNotIn("<script", text)

    def test_origin_and_odd_symmetry(self) -> None:
        self.assertEqual(wm.cell(0.0), 0.0)
        self.assertEqual(wm.diode_node(0.0), 0.0)
        self.assertEqual(wm.process(0.0), 0.0)
        self.assertEqual(wm.process(-0.0), 0.0)
        probes = [v for v, _ in self.samples[::20]]
        probes.extend([1e-16, 1e-8, 1e-4, 0.727, math.pi, 5.223, 6.0])
        for g in (wm.G_MIN, wm.G_DEFAULT, wm.G_MAX):
            for x in probes:
                self.assertEqual(wm.process(-x, g), -wm.process(x, g))
                self.assertEqual(wm.cell(-x), -wm.cell(x))
                self.assertEqual(wm.diode_node(-x), -wm.diode_node(x))

    def test_six_cells(self) -> None:
        self.assertEqual(wm.process(1.0), wm.process(1.0, cells=6))
        self.assertNotEqual(wm.process(1.0, cells=6), wm.process(1.0, cells=5))
        self.assertNotEqual(wm.process(1.0, cells=6), wm.process(1.0, cells=7))
        y = 1.0
        for _ in range(6):
            y = wm.cell(y)
        self.assertEqual(y, wm.process(1.0))

    def test_stage_folds_and_node_does_not(self) -> None:
        # At 1 V the diode node is still positive. The stage, 2*node - v, has folded.
        self.assertGreater(wm.diode_node(1.0), 0.0)
        self.assertLess(wm.cell(1.0), 0.0)
        self.assertGreater(wm.process(0.5), 0.0)
        self.assertLess(wm.process(1.0), 0.0)

    def test_six_positive_sign_changes(self) -> None:
        changes: list[tuple[float, float]] = []
        previous: tuple[float, float] | None = None
        for vin, vout in self.samples:
            if vin < 0.05:
                previous = (vin, vout)
                continue
            if previous is not None and previous[1] * vout < 0.0:
                changes.append((previous[0], vin))
            previous = (vin, vout)
        got = [(round(a, 3), round(b, 3)) for a, b in changes]
        expected = [
            (0.728, 0.730),
            (1.556, 1.558),
            (2.430, 2.432),
            (3.340, 3.342),
            (4.272, 4.274),
            (5.222, 5.224),
        ]
        self.assertEqual(got, expected)
        for left, right in expected:
            self.assertIn(f"{left:.3f} to {right:.3f}", self.readme)

    def test_lambert_residual_and_scipy_values(self) -> None:
        for abs_v, expected in _SCIPY_W.items():
            got = wm.lambert_w0_kexp(abs_v)
            self.assertAlmostEqual(got, expected, delta=1e-12)
        for i in range(0, 601):
            self.assertLess(abs(_residual(i * 0.01)), 1e-12)
        # Top of the stated gain range at the acceptance edge: g = 8, |vin| = 6.
        self.assertLess(abs(_residual(wm.G_MAX * wm.VIN_MAX)), 1e-12)
        self.assertTrue(math.isfinite(wm.process(6.0, wm.G_MAX)))

    def test_readme_locks_the_curve_and_the_framework(self) -> None:
        self.assertIn(f"{wm.diode_node(1.0):.6f}", self.readme)
        self.assertIn(f"{wm.cell(1.0):.6f}", self.readme)
        self.assertIn(f"| 0.5 | {wm.process(0.5):.6f} |", self.readme)
        self.assertIn(f"| 1 | {wm.process(1.0):.6f} |", self.readme)
        self.assertIn(f"| 6 | {wm.process(6.0):.6f} |", self.readme)
        self.assertIn(f"| -6 | {-wm.process(6.0):.6f} |", self.readme)
        limit = 2.0 * wm.ETA * wm.VT * wm.lambert_w0_kexp(0.0)
        self.assertIn(f"{limit:.6e}", self.readme)
        self.assertIn("iPlug2", self.readme)
        self.assertIn("JUCE", self.readme)
        self.assertIn("Martial Systems LLC", self.readme)
        copying = (ROOT / "COPYRIGHT").read_text(encoding="utf-8")
        self.assertIn("Copyright (c) 2026 Martial Systems LLC", copying)
        self.assertIn("All rights reserved.", copying)


if __name__ == "__main__":
    unittest.main()
