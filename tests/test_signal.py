"""Smoother and decimator locks for the Serge middle section.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The cells are not on the decimator path. fs in the probe formulas is the
4x rate: a sine at 0.45 * fs/4 is in the passband, and a sine at
0.75 * fs/4 is the stop probe.
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

FS_LOW = 48000.0
FS_HIGH = FS_LOW * wm.OVERSAMPLE
F_PASS = 0.45 * FS_HIGH / wm.OVERSAMPLE
F_STOP = 0.75 * FS_HIGH / wm.OVERSAMPLE
# Half the audio Nyquist. Its first zero-stuff image lands on the stop probe.
F_IMAGE_ON_STOP = FS_LOW / 4.0
# The pass formula with fs read as the audio rate. Images sit in the stopband.
F_AUDIO_RATE_PASS = 0.45 * FS_LOW / wm.OVERSAMPLE


def _tone(n: int, frequency: float, sample_rate: float) -> list[float]:
    omega = 2.0 * math.pi * frequency / sample_rate
    return [math.sin(omega * i) for i in range(n)]


def _aliased_frequency(frequency: float, sample_rate: float) -> float:
    """Fold frequency into [0, sample_rate/2] after decimation to sample_rate."""
    cycles = frequency / sample_rate
    wrapped = (cycles + 0.5) % 1.0 - 0.5
    return abs(wrapped) * sample_rate


def _sine_amplitude(
    samples: list[float], frequency: float, sample_rate: float, discard: int
) -> float:
    """Least-squares amplitude of a sine at frequency. Delay is not alignment."""
    omega = 2.0 * math.pi * frequency / sample_rate
    ss = sc = cc = ys = yc = 0.0
    end = len(samples) - discard
    for i in range(discard, end):
        sine = math.sin(omega * i)
        cosine = math.cos(omega * i)
        sample = samples[i]
        ss += sine * sine
        sc += sine * cosine
        cc += cosine * cosine
        ys += sample * sine
        yc += sample * cosine
    det = ss * cc - sc * sc
    gain_sine = (ys * cc - yc * sc) / det
    gain_cosine = (ss * yc - sc * ys) / det
    return math.hypot(gain_sine, gain_cosine)


def _decimated_amplitudes(
    high_rate: list[float],
    taps: tuple[float, ...],
    frequency: float,
    discard: int = 64,
) -> list[float]:
    filtered, _state = wm.fir_run(high_rate, taps)
    aliased = _aliased_frequency(frequency, FS_LOW)
    amplitudes = []
    for phase in range(wm.OVERSAMPLE):
        low = wm.decimate(filtered, wm.OVERSAMPLE, phase)
        amplitudes.append(_sine_amplitude(low, aliased, FS_LOW, discard))
    return amplitudes


class SmootherAndDecimatorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.readme = (ROOT / "README.md").read_text(encoding="utf-8")
        cls.prompt = (ROOT / "IPLUG2.md").read_text(encoding="utf-8")

    def test_tap_files_and_symmetry(self) -> None:
        self.assertEqual(len(wm.HALFBAND_TAPS), wm.HALFBAND_TAPS_N)
        self.assertEqual(wm.HALFBAND_TAPS_N, 63)
        self.assertEqual(len(wm.UPSAMPLE_TAPS), 63)
        last = len(wm.HALFBAND_TAPS) - 1
        for index, coef in enumerate(wm.HALFBAND_TAPS):
            self.assertEqual(coef, wm.HALFBAND_TAPS[last - index])
            self.assertEqual(wm.UPSAMPLE_TAPS[index], coef * 4.0)
        self.assertAlmostEqual(sum(wm.HALFBAND_TAPS), 1.0, delta=1e-12)
        self.assertAlmostEqual(sum(wm.UPSAMPLE_TAPS), 4.0, delta=1e-12)
        center = wm.HALFBAND_TAPS_N // 2
        even_offsets = [
            wm.HALFBAND_TAPS[center + offset]
            for offset in range(-center, center + 1)
            if offset != 0 and offset % 2 == 0
        ]
        largest = max(abs(coef) for coef in even_offsets)
        self.assertEqual(f"{largest:.6f}", "0.145542")
        self.assertIn("0.145542", self.readme)
        self.assertFalse(all(coef == 0.0 for coef in even_offsets))

    def test_fir_impulse_is_the_tap_vector(self) -> None:
        impulse = [1.0] + [0.0] * (wm.HALFBAND_TAPS_N + 4)
        got, _state = wm.fir_run(impulse, wm.HALFBAND_TAPS)
        for index, coef in enumerate(wm.HALFBAND_TAPS):
            self.assertEqual(got[index], coef)
        head, state = wm.fir_run(impulse[:8], wm.HALFBAND_TAPS)
        tail, _state = wm.fir_run(impulse[8:], wm.HALFBAND_TAPS, state)
        self.assertEqual(head + tail, got)
        with self.assertRaises(ValueError):
            wm.fir_step(1.0, wm.HALFBAND_TAPS, (0.0,))
        with self.assertRaises(ValueError):
            wm.fir_step(float("nan"), wm.HALFBAND_TAPS, (0.0,) * 62)
        with self.assertRaises(ValueError):
            wm.decimate([0.0, 1.0], factor=4, phase=4)
        with self.assertRaises(ValueError):
            wm.insert_zeros([1.0], factor=0)

    def test_magnitude_probes(self) -> None:
        pass_mag = wm.fir_magnitude(wm.HALFBAND_TAPS, 0.45 / wm.OVERSAMPLE)
        stop_mag = wm.fir_magnitude(wm.HALFBAND_TAPS, wm.HALFBAND_STOP_EDGE)
        deviation = wm.passband_deviation()
        floor = wm.stopband_peak()
        self.assertEqual(f"{pass_mag:.6f}", "1.000024")
        self.assertLess(abs(pass_mag - 1.0), 0.01)
        self.assertEqual(f"{20.0 * math.log10(stop_mag):.2f}", "-69.19")
        self.assertLessEqual(stop_mag, 10.0 ** (-60.0 / 20.0))
        self.assertEqual(f"{20.0 * math.log10(floor):.2f}", "-69.11")
        self.assertLessEqual(floor, 10.0 ** (-60.0 / 20.0))
        self.assertLessEqual(deviation, 0.0007)
        self.assertGreater(deviation, 0.0005)
        self.assertEqual(
            f"{wm.fir_magnitude(wm.HALFBAND_TAPS, 0.1375):.6f}", "0.944259"
        )
        for phrase in ("1.000024", "-69.19", "-69.11", "0.0007", "0.944259"):
            self.assertIn(phrase, self.readme)
        half_amp = 0.156181
        self.assertLess(
            abs(wm.fir_magnitude(wm.HALFBAND_TAPS, half_amp) - 10.0 ** (-6.0 / 20.0)),
            2e-4,
        )
        self.assertIn("0.156181", self.readme)
        self.assertIn("leaves the stop probe at 0.00 dB", self.readme)
        with self.assertRaises(ValueError):
            wm.stopband_peak(edge=0.6)
        with self.assertRaises(ValueError):
            wm.passband_deviation(edge=-0.1)

    def test_decimator_sines(self) -> None:
        # Generated at the 4x rate. 0.75 * fs/4 is above the audio Nyquist,
        # so it is not a low-rate sequence.
        n_low = 2048
        pass_high = _tone(n_low * wm.OVERSAMPLE, F_PASS, FS_HIGH)
        pass_mag = wm.fir_magnitude(wm.HALFBAND_TAPS, F_PASS / FS_HIGH)
        for amplitude in _decimated_amplitudes(pass_high, wm.HALFBAND_TAPS, F_PASS):
            self.assertLess(abs(amplitude - 1.0), 0.01)
            self.assertLess(abs(amplitude - pass_mag), 1e-4)
        stop_high = _tone(n_low * wm.OVERSAMPLE, F_STOP, FS_HIGH)
        stop_mag = wm.fir_magnitude(wm.HALFBAND_TAPS, F_STOP / FS_HIGH)
        for amplitude in _decimated_amplitudes(stop_high, wm.HALFBAND_TAPS, F_STOP):
            self.assertLessEqual(amplitude, 10.0 ** (-60.0 / 20.0))
            self.assertLess(abs(amplitude - stop_mag), 1e-6)

    def test_insert_zeros_round_trip(self) -> None:
        n_low = 2048
        for frequency in (F_IMAGE_ON_STOP, F_AUDIO_RATE_PASS, 1000.0):
            low = _tone(n_low, frequency, FS_LOW)
            high = wm.insert_zeros(low, wm.OVERSAMPLE)
            self.assertEqual(len(high), n_low * wm.OVERSAMPLE)
            for amplitude in _decimated_amplitudes(high, wm.UPSAMPLE_TAPS, frequency):
                self.assertLess(abs(amplitude - 1.0), 0.01)
            # The decimator taps omit the factor of 4, so this path sits at 1/4.
            quarter = _decimated_amplitudes(high, wm.HALFBAND_TAPS, frequency)
            for amplitude in quarter:
                self.assertLess(abs(amplitude * 4.0 - 1.0), 0.01)
        # Upsampler then decimator, cells absent: the linear plugin path.
        low = _tone(n_low, F_IMAGE_ON_STOP, FS_LOW)
        mid, _state = wm.fir_run(wm.insert_zeros(low), wm.UPSAMPLE_TAPS)
        for amplitude in _decimated_amplitudes(mid, wm.HALFBAND_TAPS, F_IMAGE_ON_STOP):
            self.assertLess(abs(amplitude - 1.0), 0.01)
        # The pass probe's own zero-stuff image sits in the transition.
        probe = _tone(n_low, F_PASS, FS_LOW)
        probed = _decimated_amplitudes(
            wm.insert_zeros(probe), wm.UPSAMPLE_TAPS, F_PASS
        )
        self.assertTrue(any(abs(amplitude - 1.0) > 0.2 for amplitude in probed))

    def test_smoother_step_and_hold(self) -> None:
        coeff = wm.smooth_coeff(FS_LOW)
        self.assertEqual(coeff, math.exp(-1.0 / (wm.SMOOTH_SECONDS * FS_LOW)))
        self.assertEqual(
            wm.smooth_coeff(44100.0), math.exp(-1.0 / (wm.SMOOTH_SECONDS * 44100.0))
        )
        self.assertEqual(repr(coeff), "0.9989588756797245")
        self.assertIn(repr(coeff), self.readme)
        n_step = int(round(wm.SMOOTH_SECONDS * FS_LOW))
        self.assertEqual(n_step, 960)
        state = 1.0
        for _ in range(n_step):
            state = wm.smooth_g(state, 2.0, coeff)
            self.assertEqual(wm.hold_phases(state), (state, state, state, state))
        expected = 1.0 + (1.0 - math.exp(-1.0))
        self.assertEqual(f"{expected:.6f}", "1.632121")
        self.assertAlmostEqual(state, expected, delta=1e-4)
        self.assertIn("1.632121", self.readme)
        # One smooth per audio sample. Four smooths on the 4x clock are not flat.
        held = wm.hold_phases(wm.smooth_g(1.0, 2.0, coeff))
        stepped = []
        cursor = 1.0
        for _ in range(wm.OVERSAMPLE):
            cursor = wm.smooth_g(cursor, 2.0, coeff)
            stepped.append(cursor)
        self.assertEqual(len(set(held)), 1)
        self.assertNotEqual(stepped[0], stepped[-1])
        self.assertEqual(wm.smooth_g(1.5, 1.5, coeff), 1.5)
        self.assertEqual(wm.smooth_g(1.0, 2.0, 0.0), 2.0)
        self.assertEqual(wm.smooth_g(1.0, 2.0, 1.0), 1.0)
        self.assertEqual(wm.smooth_g(1.0, 100.0, 0.0), wm.G_MAX)
        self.assertEqual(wm.smooth_g(4.0, -1.0, 0.0), wm.G_MIN)
        with self.assertRaises(ValueError):
            wm.smooth_coeff(0.0)
        with self.assertRaises(ValueError):
            wm.smooth_g(1.0, 2.0, 1.1)
        with self.assertRaises(ValueError):
            wm.smooth_g(float("nan"), 1.0, coeff)
        with self.assertRaises(ValueError):
            wm.hold_phases(float("nan"))

    def test_documents_lock_the_smoother_and_the_taps(self) -> None:
        order = (
            "smooth g, v = 5 * g * a, 4× upsample, six cells, "
            "63-tap halfband, decimate, OUTPUT_GAIN, P(1) / P(g), 10 Hz block"
        )
        smoother = "g[n] = g[n-1] + (1 - c) * (g_target - g[n-1])"
        coeff = "c = exp(-1 / (0.020 * fs))"
        for document in (self.readme, self.prompt):
            self.assertIn(order, document)
            self.assertIn(smoother, document)
            self.assertIn(coeff, document)
            self.assertIn("halfband_taps.csv", document)
            self.assertIn("upsample_taps.csv", document)
            self.assertIn("The module is not in this repository", document)
        for phrase in (
            "Do not clip the output to [-1, 1]",
            "No envelope",
            "Do not search for the peak on the audio thread",
            "Do not form k * exp(|v| / (eta * VT))",
            "JUCE is not the framework",
            "Copy the smoother coefficient and the tap files. Do not redesign them.",
        ):
            self.assertIn(phrase, self.prompt)
        self.assertNotIn("not locked", self.prompt)
        self.assertNotIn("not locked", self.readme)
        for name in ("BUILD.md", "MATH.md", "GOLDEN.md"):
            text = (ROOT / name).read_text(encoding="utf-8")
            self.assertIn("Copyright (c) 2026 Martial Systems LLC", text)
            self.assertNotIn("—", text)
            self.assertNotIn("–", text)


if __name__ == "__main__":
    unittest.main()
