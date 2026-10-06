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
        self.assertEqual(wm.AUDIO_FULL_SCALE_VOLTS, 5.0)
        self.assertEqual(wm.FULL_SCALE_PEAK_VIN, 4.7072868603604885)
        self.assertEqual(wm.OUTPUT_GAIN, 4.3792716960440945)
        self.assertEqual(wm.DC_BLOCK_HZ, 10.0)
        self.assertEqual(wm.DRIVE_PEAK_REL_ERROR, 1e-3)
        self.assertEqual(len(wm.DRIVE_PEAKS), 53)
        self.assertEqual(wm.DRIVE_PEAKS[0][0], 0.5)
        self.assertEqual(wm.DRIVE_PEAKS[-1][0], 8.0)
        self.assertEqual(wm.drive_peak(1.0), abs(wm.process(wm.FULL_SCALE_PEAK_VIN)))

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
        # Coarse grid straddles. The roots themselves are positive_fold_roots().
        self.assertEqual(
            got,
            [
                (0.728, 0.730),
                (1.556, 1.558),
                (2.430, 2.432),
                (3.340, 3.342),
                (4.272, 4.274),
                (5.222, 5.224),
            ],
        )

    def test_lambert_residual_and_scipy_values(self) -> None:
        for abs_v, expected in _SCIPY_W.items():
            got = wm.lambert_w0_kexp(abs_v)
            self.assertAlmostEqual(got, expected, delta=1e-12)
        for i in range(0, 601):
            self.assertLess(abs(_residual(i * 0.01)), 1e-12)
        # Top of the stated gain range at the acceptance edge: g = 8, |vin| = 6.
        self.assertLess(abs(_residual(wm.G_MAX * wm.VIN_MAX)), 1e-12)
        self.assertTrue(math.isfinite(wm.process(6.0, wm.G_MAX)))

    def test_fold_roots(self) -> None:
        roots = wm.positive_fold_roots()
        self.assertEqual(len(roots), 6)
        self.assertLess(roots[0], 1.0)
        self.assertGreater(roots[1], 1.0)
        for root in roots:
            self.assertLess(abs(wm.process(root)), 1e-8)
            self.assertLess(wm.process(root - 0.02) * wm.process(root + 0.02), 0.0)
            self.assertIn(f"{root:.4f}", self.readme)

    def test_full_scale_gain(self) -> None:
        peak_y = wm.process(wm.FULL_SCALE_PEAK_VIN)
        self.assertEqual(wm.OUTPUT_GAIN * abs(peak_y), 1.0)
        self.assertLess(peak_y, 0.0)
        for delta in (1e-4, 1e-3, 1e-2):
            self.assertLess(abs(wm.process(wm.FULL_SCALE_PEAK_VIN - delta)), abs(peak_y))
            self.assertLess(abs(wm.process(wm.FULL_SCALE_PEAK_VIN + delta)), abs(peak_y))
        step = 1e-4
        n = int(wm.AUDIO_FULL_SCALE_VOLTS / step)
        ceiling = abs(peak_y) + 1e-12
        for i in range(n + 1):
            self.assertLessEqual(abs(wm.process(i * step)), ceiling)
        # process() is the raw map. The fixed gain is a later multiply.
        self.assertNotEqual(peak_y, wm.OUTPUT_GAIN * peak_y)

    def test_full_scale_sine_mean_and_one_volt_map(self) -> None:
        n = 4096
        full = [
            wm.process(wm.AUDIO_FULL_SCALE_VOLTS * math.sin(2.0 * math.pi * i / n))
            for i in range(n)
        ]
        one_volt = [wm.process(math.sin(2.0 * math.pi * i / n)) for i in range(n)]
        full_mean = sum(full) / n
        self.assertLess(abs(full_mean), 1e-9)
        self.assertAlmostEqual(max(abs(v) for v in full), abs(wm.process(wm.FULL_SCALE_PEAK_VIN)), delta=1e-5)
        self.assertEqual(f"{max(abs(v) for v in one_volt):.6f}", "0.161174")
        self.assertIn("0.161174", self.readme)

    def test_readme_locks_the_curve_and_the_framework(self) -> None:
        self.assertIn(f"{wm.diode_node(1.0):.6f}", self.readme)
        self.assertIn(f"{wm.cell(1.0):.6f}", self.readme)
        self.assertIn(f"| 0.5 | {wm.process(0.5):.6f} |", self.readme)
        self.assertIn(f"| 1 | {wm.process(1.0):.6f} |", self.readme)
        self.assertIn(f"| 6 | {wm.process(6.0):.6f} |", self.readme)
        self.assertIn(f"| -6 | {-wm.process(6.0):.6f} |", self.readme)
        limit = 2.0 * wm.ETA * wm.VT * wm.lambert_w0_kexp(0.0)
        self.assertIn(f"{limit:.6e}", self.readme)
        self.assertIn(f"{wm.OUTPUT_GAIN:.6f}", self.readme)
        self.assertIn(f"{wm.FULL_SCALE_PEAK_VIN:.6f}", self.readme)
        self.assertIn(f"{wm.process(wm.FULL_SCALE_PEAK_VIN):.6f}", self.readme)
        self.assertIn("v = g * 5 * a", self.readme)
        self.assertIn("d(g) = g", self.readme)
        self.assertIn("P(1) / P(g)", self.readme)
        self.assertIn("y[n] = x[n] - x[n-1] + r * y[n-1]", self.readme)
        self.assertIn("within 0.001", self.readme)
        self.assertIn("within 0.005", self.readme)
        self.assertIn(f"{wm.FULL_SCALE_PEAK_VIN / 5.0:.6f}", self.readme)
        unity = wm.drive_peak(1.0)
        plateau = max(g for g, peak in wm.DRIVE_PEAKS if peak == unity)
        self.assertIn(f"{plateau:.6f}", self.readme)
        for g, db_text in (
            (0.5, "-2.187"),
            (2.0, "25.093"),
            (4.0, "35.522"),
            (8.0, "43.258"),
        ):
            db = 20.0 * math.log10(wm.drive_peak(g) / unity)
            self.assertEqual(f"{db:.3f}", db_text)
            self.assertIn(db_text, self.readme)
            self.assertIn(f"{wm.drive_peak(g):.6f}", self.readme)
        self.assertIn("v1 keeps eta = 1.68 and VT = 0.02585", self.readme)
        self.assertIn("1.752", self.readme)
        self.assertIn("0.025864", self.readme)
        self.assertIn("iPlug2", self.readme)
        self.assertIn("JUCE", self.readme)
        self.assertIn("Martial Systems LLC", self.readme)
        copying = (ROOT / "COPYRIGHT").read_text(encoding="utf-8")
        self.assertIn("Copyright (c) 2026 Martial Systems LLC", copying)
        self.assertIn("All rights reserved.", copying)
        prompt = (ROOT / "IPLUG2.md").read_text(encoding="utf-8")
        for phrase in (
            "iPlug2",
            "JUCE",
            "DRIVE_PEAKS",
            "OUTPUT_GAIN",
            "transfer_g1.csv",
            "y[n] = x[n] - x[n-1] + r * y[n-1]",
            "Do not clip the output to [-1, 1]",
            "halfband",
            "4×",
            "10 Hz",
            "Dual Universal Slope Generator",
            "No envelope",
            "1.152747",
            "within 0.005",
            "25.093",
        ):
            self.assertIn(phrase, prompt)

    def test_fold_amount_bound(self) -> None:
        unity = wm.drive_peak(1.0)
        g0, p0 = wm.DRIVE_PEAKS[0]
        self.assertEqual(wm.drive_peak(g0), p0)
        self.assertAlmostEqual(wm.max_abs_process(5.0 * g0), p0, delta=1e-12)
        self.assertAlmostEqual(wm.output_scale(g0) * p0, 1.0, delta=1e-12)
        previous_g, previous_p = g0, p0
        for g, peak in wm.DRIVE_PEAKS[1:]:
            self.assertGreater(g, previous_g)
            self.assertGreaterEqual(peak + 1e-15, previous_p)
            self.assertEqual(wm.drive_peak(g), peak)
            self.assertAlmostEqual(wm.max_abs_process(5.0 * g), peak, delta=1e-12)
            self.assertAlmostEqual(wm.output_scale(g) * peak, 1.0, delta=1e-12)
            previous_g, previous_p = g, peak
        self.assertEqual(wm.output_scale(1.0), wm.OUTPUT_GAIN)
        self.assertEqual(unity, abs(wm.process(wm.FULL_SCALE_PEAK_VIN)))
        lobes = wm.positive_lobe_peaks(40.0)
        self.assertEqual(len(lobes), 6)
        self.assertAlmostEqual(lobes[-1][0], wm.FULL_SCALE_PEAK_VIN, delta=1e-6)
        for g in (1.2, 2.0, 3.0, 4.0, 8.0):
            limit = 5.0 * g
            self.assertEqual(wm.max_abs_process(limit), abs(wm.process(limit)))
        for g in (2.0, 4.0, 8.0):
            self.assertEqual(wm.audio_map(1.0, g), 1.0)
            self.assertEqual(wm.audio_map(-1.0, g), -1.0)
        worst = 0.0
        for index in range(1, len(wm.DRIVE_PEAKS)):
            g0 = wm.DRIVE_PEAKS[index - 1][0]
            g1 = wm.DRIVE_PEAKS[index][0]
            for step in range(41):
                g = g0 + (g1 - g0) * step / 40.0
                measured = wm.max_abs_process(5.0 * g)
                rel = abs(measured / wm.drive_peak(g) - 1.0)
                if rel > worst:
                    worst = rel
        self.assertLessEqual(worst, wm.DRIVE_PEAK_REL_ERROR)
        # A sample below the g = 2 peak keeps the ratio. Clipping OUTPUT_GAIN * y
        # would pin this one at ±1.
        sample = 0.7
        volts = wm.AUDIO_FULL_SCALE_VOLTS * wm.drive_scale(2.0) * sample
        raw = wm.process(volts)
        bounded = wm.audio_map(sample, 2.0)
        self.assertEqual(bounded, wm.output_scale(2.0) * raw)
        self.assertGreater(abs(wm.OUTPUT_GAIN * raw), 1.0)
        self.assertLess(abs(bounded), 1.0)
        for g in (0.5, 1.0, 1.7, 8.0):
            for sample in (-1.0, -0.3, 0.0, 0.3, 1.0):
                self.assertEqual(wm.audio_map(-sample, g), -wm.audio_map(sample, g))
        for sample in (0.0, 0.2, -0.5, 1.0, -1.0):
            self.assertEqual(
                wm.audio_map(sample, 1.0),
                wm.OUTPUT_GAIN * wm.process(wm.AUDIO_FULL_SCALE_VOLTS * sample),
            )
        self.assertNotEqual(wm.audio_map(1.0, 2.0), wm.process(1.0, 2.0))
        self.assertEqual(wm.drive_scale(0.1), wm.G_MIN)
        self.assertEqual(wm.drive_scale(9.0), wm.G_MAX)
        self.assertEqual(wm.drive_peak(0.1), wm.drive_peak(wm.G_MIN))
        self.assertEqual(wm.audio_map(0.25, 100.0), wm.audio_map(0.25, wm.G_MAX))
        self.assertEqual(wm.audio_map(0.25, -3.0), wm.audio_map(0.25, wm.G_MIN))
        with self.assertRaises(ValueError):
            wm.drive_scale(float("nan"))
        with self.assertRaises(ValueError):
            wm.audio_map(float("nan"), 1.0)
        n = 4096
        gained = [
            wm.audio_map(math.sin(2.0 * math.pi * i / n), 1.0) for i in range(n)
        ]
        self.assertAlmostEqual(max(abs(v) for v in gained), 1.0, delta=5e-5)
        self.assertLess(abs(sum(gained) / n), 1e-8)

    def test_dc_block_equation_and_sine_tilt(self) -> None:
        fs = 48000.0
        n = 4096
        pole = wm.dc_block_pole(fs)
        self.assertEqual(pole, math.exp(-2.0 * math.pi * wm.DC_BLOCK_HZ / fs))
        constant, _state = wm.dc_block([1.0] * 1001, fs)
        self.assertEqual(constant[0], 1.0)
        self.assertAlmostEqual(constant[1000], pole ** 1000, delta=1e-15)
        self.assertEqual(wm.dc_block_magnitude(0.0, fs), 0.0)
        with self.assertRaises(ValueError):
            wm.dc_block_pole(0.0)
        with self.assertRaises(ValueError):
            wm.dc_block_magnitude(-1.0, fs)

        def settled(cycles: int) -> tuple[list[float], list[float]]:
            src = [
                wm.audio_map(math.sin(2.0 * math.pi * cycles * i / n), 1.0)
                for i in range(n)
            ]
            whole, _state = wm.dc_block(src + src, fs)
            _lead, state = wm.dc_block(src, fs)
            second, _state = wm.dc_block(src, fs, state)
            self.assertEqual(second, whole[n:])
            return src, second

        src, got = settled(1)
        peak = max(abs(v) for v in src)
        ratio = max(abs(v) for v in got) / peak
        deviation = max(abs(got[i] - src[i]) for i in range(n)) / peak
        self.assertEqual(f"{ratio:.6f}", "1.152747")
        self.assertEqual(f"{deviation:.6f}", "0.390095")
        self.assertIn("11.71875", self.readme)
        self.assertIn("1.152747", self.readme)
        self.assertIn("0.390095", self.readme)

        src, got = settled(86)
        peak = max(abs(v) for v in src)
        ratio = max(abs(v) for v in got) / peak
        deviation = max(abs(got[i] - src[i]) for i in range(n)) / peak
        self.assertLess(abs(ratio - 1.0), 0.005)
        self.assertLess(deviation, 0.005)
        self.assertGreater(deviation, 1e-3)

        frequency = 86.0 * fs / n
        w = 2.0 * math.pi * frequency / fs
        gain = wm.dc_block_magnitude(frequency, fs)
        numer = 1.0 - complex(math.cos(w), -math.sin(w))
        denom = 1.0 - pole * complex(math.cos(w), -math.sin(w))
        phase = math.atan2((numer / denom).imag, (numer / denom).real)
        tone = [math.sin(w * i) for i in range(n * 4)]
        filtered, _state = wm.dc_block(tone, fs)
        error = max(
            abs(filtered[i] - gain * math.sin(w * i + phase))
            for i in range(n * 4 - 512, n * 4)
        )
        self.assertLess(error, 1e-9)


if __name__ == "__main__":
    unittest.main()
