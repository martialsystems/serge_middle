"""Symmetry on audio_map, the memoryless reference.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The default offset keeps the previous float. A silent sample stays 0 at any
offset. The anchor is audio_map(0.1, 1.0, 1.0).
"""

from __future__ import annotations

import math
import struct
import unittest

import wave_middle as wm

# Fifteen significant digits. The C++ port checks this within 1e-12 relative.
ANCHOR = "0.0304476745327936"


def _bits(value: float) -> bytes:
    return struct.pack("<d", value)


def _previous_audio_map(sample: float, g: float) -> float:
    """The formula before the symmetry argument, inlined."""
    if not math.isfinite(sample):
        raise ValueError(f"sample must be finite, got {sample}")
    volts = wm.AUDIO_FULL_SCALE_VOLTS * wm.drive_scale(g) * sample
    return wm.output_scale(g) * wm.process(volts)


def _bin_magnitude(samples: list[float], bin_index: int) -> float:
    count = len(samples)
    real = 0.0
    imag = 0.0
    for index, sample in enumerate(samples):
        angle = 2.0 * math.pi * ((bin_index * index) % count) / count
        real += sample * math.cos(angle)
        imag -= sample * math.sin(angle)
    return math.hypot(real, imag)


def _probe(symmetry: float) -> float:
    """86 cycles in 4,096 samples, amplitude 0.5, g = 1.

    The half period is mirrored and the two zero crossings are 0, so a libm
    residual at sin(pi) is not scored as an even harmonic of the map.
    """
    count = 4096
    cycles = 86
    raw = [0.0] * count
    for index in range(count):
        turns = ((cycles * index) % count) / count
        raw[index] = 0.5 * math.sin(2.0 * math.pi * turns)
    raw[0] = 0.0
    raw[count // 2] = 0.0
    for index in range(1, count // 2):
        raw[count - index] = -raw[index]
    mapped = [wm.audio_map(sample, 1.0, symmetry) for sample in raw]
    first = _bin_magnitude(mapped, cycles)
    second = _bin_magnitude(mapped, 2 * cycles)
    return second / first


class SymmetryTest(unittest.TestCase):
    def test_default_matches_the_previous_formula(self) -> None:
        samples = [0.0, -0.0, 0.1, -0.1, 0.25, -0.25, 0.5, -0.5, 1.0, -1.0]
        gains = [wm.G_MIN, 1.0, 2.0, wm.G_MAX]
        for gain in gains:
            for sample in samples:
                previous = _previous_audio_map(sample, gain)
                self.assertEqual(_bits(wm.audio_map(sample, gain)), _bits(previous))
                self.assertEqual(_bits(wm.audio_map(sample, gain, 0.0)), _bits(previous))
                self.assertEqual(_bits(wm.audio_map(sample, gain, -0.0)), _bits(previous))

    def test_silence_is_zero_at_any_offset(self) -> None:
        for symmetry in (0.0, -0.0, 1.0, -1.0, 2.5, -2.5, 10.0, float("nan")):
            for gain in (wm.G_MIN, 1.0, 2.0, wm.G_MAX):
                self.assertEqual(wm.audio_map(0.0, gain, symmetry), 0.0)

    def test_anchor(self) -> None:
        got = wm.audio_map(0.1, 1.0, 1.0)
        self.assertEqual(wm.AUDIO_FULL_SCALE_VOLTS * 0.1, 0.25)
        formula = wm.output_scale(1.0) * (wm.process(1.25) - wm.process(1.0))
        self.assertEqual(got, formula)
        self.assertEqual(format(got, ".15g"), ANCHOR)
        print(f"symmetry anchor {format(got, '.15g')}")

    def test_even_harmonics_of_the_map(self) -> None:
        at_zero = _probe(0.0)
        at_one = _probe(1.0)
        print(f"audio_map |H2|/|H1| s=0 {at_zero:.6e} s=+1 {at_one:.6e}")
        self.assertLess(at_zero, 1e-9)
        self.assertGreater(at_one, 0.1)


if __name__ == "__main__":
    unittest.main()
