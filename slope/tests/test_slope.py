"""Acceptance for one universal slope at 48 kHz.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The numeric lock for the bent rises is slope/tests/feedback_p0_5.csv
and slope/tests/feedback_m0_5.csv. This file does not import the PLEAT suite.
"""

from __future__ import annotations

import math
import unittest
from pathlib import Path

from slope.slope import FS, SPAN, VC_SECONDS_PER_VOLT, Slope

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
RISE = 0.005
FALL = 0.005


def _load_curve(name: str) -> list[float]:
    lines = (HERE / name).read_text(encoding="utf-8").splitlines()
    if lines[0] != "sample,volts":
        raise AssertionError(f"{name} header")
    values = []
    for index, line in enumerate(lines[1:]):
        sample, volts = line.split(",")
        if int(sample) != index:
            raise AssertionError(f"{name} sample {sample}")
        values.append(float(volts))
    return values


def _rise_until_five(feedback: float) -> list[float]:
    slope = Slope(RISE, FALL)
    values = []
    for i in range(8000):
        out, _end = slope.step(trig=5.0 if i == 0 else 0.0, feedback=feedback)
        values.append(out)
        if out == SPAN:
            return values
    raise AssertionError(f"feedback {feedback} did not reach +5 V")


def _free_run(
    rise: float = RISE,
    fall: float = FALL,
    n: int = 2000,
    v_oct: float = 0.0,
    vc: float = 0.0,
    vc_switch: str = "both",
) -> list[tuple[float, float]]:
    slope = Slope(rise, fall, vc_switch=vc_switch)
    rows = []
    for i in range(n):
        out, end = slope.step(
            trig=5.0 if i == 0 else 0.0,
            v_oct=v_oct,
            vc=vc,
            end_to_trig=True,
        )
        rows.append((out, end))
    return rows


def _period(rows: list[tuple[float, float]]) -> tuple[int, int, int]:
    zeros = [i for i, (out, _end) in enumerate(rows) if out == 0.0]
    if len(zeros) < 2:
        raise AssertionError("cycle did not return to 0 V")
    start, stop = zeros[0], zeros[1]
    high = sum(end for _out, end in rows[start:stop])
    return stop - start, int(high), start


class SlopeAcceptanceTest(unittest.TestCase):
    def test_free_cycle_hits_the_rails_in_10_ms(self) -> None:
        rows = _free_run()
        period, high, start = _period(rows)
        window = [out for out, _end in rows[start : start + period]]
        self.assertEqual(period, 480)
        self.assertLessEqual(abs(period - int(round(0.010 * FS))), 1)
        self.assertIn(0.0, window)
        self.assertIn(SPAN, window)
        self.assertEqual(high, 240)
        self.assertEqual(high / period, RISE / (RISE + FALL))

    def test_end_duty_matches_rise_over_rise_plus_fall(self) -> None:
        rows = _free_run(fall=0.010, n=3000)
        period, high, _start = _period(rows)
        self.assertEqual(period, 720)
        self.assertEqual(high, 240)
        self.assertEqual(high * 3, period)
        self.assertAlmostEqual(high / period, RISE / (RISE + 0.010), delta=1e-12)

    def test_trigger_train_divides_by_three(self) -> None:
        slope = Slope(RISE, FALL)
        triggers = 0
        pulses = 0
        for i in range(14400):
            trig = 5.0 if i % 160 == 0 else 0.0
            if trig > 1.0:
                triggers += 1
            slope.step(trig=trig)
            if slope.end_pulse:
                pulses += 1
        self.assertEqual(triggers, 90)
        self.assertEqual(pulses, 30)
        self.assertEqual(pulses * 3, triggers)

    def test_gate_rises_holds_and_falls_after_release(self) -> None:
        slope = Slope(RISE, FALL)
        outs = []
        for i in range(1200):
            # A trigger during the hold must not send the output to +5 V.
            trig = 5.0 if i == 400 else 0.0
            out, _end = slope.step(inp=3.0 if i < 960 else 0.0, trig=trig)
            outs.append(out)
        self.assertEqual(outs[144], 3.0)
        self.assertTrue(all(value == 3.0 for value in outs[144:960]))
        self.assertEqual(outs[960], 3.0)
        self.assertLess(outs[961], 3.0)
        self.assertTrue(all(outs[i] <= outs[i + 1] for i in range(959)))
        self.assertLess(max(outs[:960]), SPAN)

    def test_bipolar_sine_is_full_wave_rectified(self) -> None:
        bipolar = Slope(1.0 / FS, FALL)
        ignored = Slope(1.0 / FS, FALL)
        signal = []
        full = []
        half = []
        for i in range(960):
            x = 5.0 * math.sin(2.0 * math.pi * i / 480.0)
            signal.append(x)
            full.append(bipolar.step(inp=x)[0])
            half.append(ignored.step(inp=x if x > 0.0 else 0.0)[0])
        self.assertTrue(all(value >= 0.0 for value in full))
        negative = [i for i, value in enumerate(signal) if value < 0.0]
        peak = min(negative, key=lambda i: signal[i])
        self.assertGreater(full[peak], 4.9)
        self.assertLess(half[peak], 1.0)
        self.assertTrue(any(full[i] > half[i] + 1.0 for i in negative))
        self.assertTrue(any(full[i + 1] > full[i] for i in negative if i + 1 < len(full)))

    def test_feedback_bends_both_stored_curves(self) -> None:
        linear = _rise_until_five(0.0)
        positive = _rise_until_five(0.5)
        negative = _rise_until_five(-0.5)
        self.assertEqual(positive, _load_curve("feedback_p0_5.csv"))
        self.assertEqual(negative, _load_curve("feedback_m0_5.csv"))
        self.assertEqual(len(linear), 241)
        self.assertEqual(linear[240], SPAN)
        self.assertEqual(len(positive), 116)
        self.assertEqual(len(negative), 646)
        self.assertGreater(positive[60], linear[60])
        self.assertGreater(linear[60], negative[60])
        self.assertLess(positive.index(SPAN), linear.index(SPAN))
        self.assertLess(linear.index(SPAN), negative.index(SPAN))
        pos_step = [positive[i + 1] - positive[i] for i in range(len(positive) - 2)]
        neg_step = [negative[i + 1] - negative[i] for i in range(len(negative) - 2)]
        self.assertTrue(all(pos_step[i] <= pos_step[i + 1] for i in range(len(pos_step) - 1)))
        self.assertTrue(all(neg_step[i] >= neg_step[i + 1] for i in range(len(neg_step) - 1)))

    def test_one_volt_per_octave_doubles_the_rate(self) -> None:
        base, _high, _start = _period(_free_run())
        sped, _high_v, _start_v = _period(_free_run(v_oct=1.0, n=800))
        self.assertEqual(base, 480)
        self.assertEqual(sped, 240)
        self.assertEqual(sped * 2, base)

    def test_vc_adds_to_the_selected_time(self) -> None:
        self.assertEqual(VC_SECONDS_PER_VOLT, 0.001)
        rise_only, high_r, _s = _period(_free_run(vc=1.0, vc_switch="rise", n=2000))
        fall_only, high_f, _t = _period(_free_run(vc=1.0, vc_switch="fall", n=2000))
        both, high_b, _u = _period(_free_run(vc=1.0, vc_switch="both", n=2000))
        self.assertEqual((rise_only, high_r), (528, 288))
        self.assertEqual((fall_only, high_f), (528, 240))
        self.assertEqual((both, high_b), (576, 288))
        # VC is added to the knob, then 1V/oct scales the rate.
        mixed, mixed_high, _v = _period(
            _free_run(vc=1.0, v_oct=1.0, vc_switch="both", n=1000)
        )
        self.assertEqual((mixed, mixed_high), (288, 144))

    def test_second_run_matches_sample_for_sample(self) -> None:
        self.assertEqual(_free_run(), _free_run())
        self.assertEqual(_rise_until_five(0.5), _rise_until_five(0.5))
        self.assertEqual(_rise_until_five(-0.5), _rise_until_five(-0.5))
        first = Slope(RISE, FALL)
        second = Slope(RISE, FALL)
        a = [first.step(inp=3.0 if i < 960 else 0.0) for i in range(1100)]
        b = [second.step(inp=3.0 if i < 960 else 0.0) for i in range(1100)]
        self.assertEqual(a, b)

    def test_readme_lists_the_jacks_and_the_coupling(self) -> None:
        text = (HERE.parent / "README.md").read_text(encoding="utf-8")
        for phrase in (
            "IN",
            "TRIG",
            "VC",
            "1V/oct",
            "RISE",
            "FALL",
            "BOTH",
            "OUT",
            "END",
            "m = 2 ** (V_1v + a * v)",
            "0.001",
            "a rise and a fall, one peak",
        ):
            self.assertIn(phrase, text)
        self.assertNotIn("\u2014", text)
        self.assertNotIn("\u2013", text)
        root = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("PLEAT", root)
        self.assertIn("slope generator", root)
        self.assertIn("side by side", root)
