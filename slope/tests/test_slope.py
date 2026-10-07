"""Acceptance for one universal slope at 48 kHz.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The numeric locks are slope/tests/feedback_p0_5.csv,
slope/tests/feedback_m0_5.csv, and slope/tests/sine_envelope.csv.
The PLEAT suite stays in tests/.
"""

from __future__ import annotations

import hashlib
import math
import unittest
from pathlib import Path

from slope.slope import FS, SPAN, VC_SECONDS_PER_VOLT, Slope

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
RISE = 0.005
FALL = 0.005
ENVELOPE_SAMPLES = 960
SINE_PERIOD = 480


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


def _sha1(name: str) -> str:
    return hashlib.sha1((HERE / name).read_bytes()).hexdigest()


def _vc_step(
    v: float,
    amount: float,
    vc: float = 0.0,
    v_oct: float = 0.0,
    knob: float = RISE,
    weight: float = 1.0,
) -> float:
    """Same association as patched_vc, then times, then steps.

    VC_in = VC + a * v
    T = max(1/fs, knob - weight * 0.001 * VC_in)
    step = 2**V_1v * 5 / (T * fs)
    """
    vc_in = vc + amount * v
    offset = VC_SECONDS_PER_VOLT * vc_in
    duration = max(1.0 / FS, knob - weight * offset)
    return (2.0 ** v_oct) * SPAN / (duration * FS)


def _replay_rise(
    amount: float,
    vc: float = 0.0,
    v_oct: float = 0.0,
    weight: float = 1.0,
) -> list[float]:
    v = 0.0
    samples = []
    for _ in range(20000):
        samples.append(v)
        if v >= SPAN:
            return samples
        v = v + _vc_step(v, amount, vc=vc, v_oct=v_oct, weight=weight)
        if v >= SPAN:
            v = SPAN
    raise AssertionError(f"replay a={amount} did not reach +5 V")


def _replay_oneshot(amount: float) -> list[float]:
    v = 0.0
    rising = True
    samples = []
    for i in range(30000):
        samples.append(v)
        if i > 0 and v == 0.0:
            return samples
        step = _vc_step(v, amount)
        if rising:
            v = v + step
            if v >= SPAN:
                v = SPAN
                rising = False
        else:
            v = v - step
            if v <= 0.0:
                v = 0.0
    raise AssertionError(f"replay a={amount} did not return to 0 V")


def _rise_until_five(
    feedback: float,
    vc: float = 0.0,
    v_oct: float = 0.0,
    vc_switch: str = "both",
) -> list[float]:
    slope = Slope(RISE, FALL, vc_switch=vc_switch)
    values = []
    for i in range(20000):
        out, _end = slope.step(
            trig=5.0 if i == 0 else 0.0,
            feedback=feedback,
            vc=vc,
            v_oct=v_oct,
        )
        values.append(out)
        if out == SPAN:
            return values
    raise AssertionError(f"feedback {feedback} did not reach +5 V")


def _oneshot(feedback: float) -> list[float]:
    slope = Slope(RISE, FALL)
    values = []
    for i in range(30000):
        out, _end = slope.step(trig=5.0 if i == 0 else 0.0, feedback=feedback)
        values.append(out)
        if i > 0 and out == 0.0:
            return values
    raise AssertionError(f"feedback {feedback} did not return to 0 V")


def _free_run(
    rise: float = RISE,
    fall: float = FALL,
    n: int = 2000,
    v_oct: float = 0.0,
    vc: float = 0.0,
    vc_switch: str = "both",
    feedback: float = 0.0,
) -> list[tuple[float, float]]:
    slope = Slope(rise, fall, vc_switch=vc_switch)
    rows = []
    for i in range(n):
        out, end = slope.step(
            trig=5.0 if i == 0 else 0.0,
            v_oct=v_oct,
            vc=vc,
            feedback=feedback,
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


def _envelope() -> list[float]:
    slope = Slope(1.0 / FS, FALL)
    values = []
    for i in range(ENVELOPE_SAMPLES):
        x = 5.0 * math.sin(2.0 * math.pi * i / SINE_PERIOD)
        values.append(slope.step(inp=x)[0])
    return values


def _bipolar_input() -> list[float]:
    return [
        5.0 * math.sin(2.0 * math.pi * i / SINE_PERIOD)
        for i in range(ENVELOPE_SAMPLES)
    ]


class SlopeAcceptanceTest(unittest.TestCase):
    def test_free_cycle_hits_the_rails_in_10_ms(self) -> None:
        rows = _free_run()
        period, high, start = _period(rows)
        window = rows[start : start + period]
        outs = [out for out, _end in window]
        self.assertEqual(period, 480)
        self.assertLessEqual(abs(period - int(round(0.010 * FS))), 1)
        self.assertEqual(outs[0], 0.0)
        self.assertEqual(outs[240], SPAN)
        self.assertEqual(high, 240)
        self.assertEqual(high / period, RISE / (RISE + FALL))
        for k in range(240):
            self.assertEqual(outs[k], SPAN * k / 240)
            self.assertEqual(window[k][1], 1.0)
        for k in range(240, 480):
            self.assertEqual(window[k][1], 0.0)

    def test_end_duty_matches_rise_over_rise_plus_fall(self) -> None:
        rows = _free_run(fall=0.010, n=3000)
        period, high, _start = _period(rows)
        self.assertEqual(period, 720)
        self.assertEqual(high, 240)
        self.assertEqual(high * 3, period)
        self.assertAlmostEqual(high / period, RISE / (RISE + 0.010), delta=1e-12)

    def test_trigger_train_divides_by_three(self) -> None:
        cycle = 480
        interval = cycle // 3
        self.assertEqual(interval, 160)
        slope = Slope(RISE, FALL)
        triggers = 0
        pulses = 0
        for i in range(14400):
            trig = 5.0 if i % interval == 0 else 0.0
            if trig > 1.0:
                triggers += 1
            slope.step(trig=trig)
            if slope.end_pulse:
                pulses += 1
        self.assertEqual(triggers, 90)
        self.assertEqual(pulses, 30)
        self.assertEqual(pulses * 3, triggers)

    def test_triggers_during_rise_or_fall_are_ignored(self) -> None:
        quiet = _free_run(n=480)
        slope = Slope(RISE, FALL)
        noisy = []
        for i in range(480):
            trig = 5.0 if i in (0, 100, 239, 240, 300) else 0.0
            noisy.append(slope.step(trig=trig, end_to_trig=True))
        self.assertEqual(noisy, quiet)
        self.assertGreater(quiet[100][0], 0.0)
        self.assertLess(quiet[100][0], SPAN)
        self.assertEqual(quiet[100][1], 1.0)
        self.assertEqual(quiet[300][1], 0.0)
        self.assertGreater(quiet[300][0], 0.0)

    def test_gate_rises_holds_and_falls_after_release(self) -> None:
        self.assertEqual(960, int(round(0.020 * FS)))
        slope = Slope(RISE, FALL)
        outs = []
        for i in range(1200):
            # A trigger during the hold must not send the output to +5 V.
            trig = 5.0 if i == 400 else 0.0
            out, _end = slope.step(inp=3.0 if i < 960 else 0.0, trig=trig)
            outs.append(out)
        self.assertEqual(outs[144], 3.0)
        self.assertEqual(outs[400], 3.0)
        self.assertTrue(all(value == 3.0 for value in outs[144:960]))
        self.assertEqual(outs[960], 3.0)
        self.assertLess(outs[961], 3.0)
        self.assertTrue(all(outs[i] <= outs[i + 1] for i in range(959)))
        self.assertLess(max(outs[:960]), SPAN)

    def test_bipolar_sine_is_full_wave_rectified_and_stored(self) -> None:
        signal = _bipolar_input()
        full = _envelope()
        ignored = Slope(1.0 / FS, FALL)
        half = [
            ignored.step(inp=x if x > 0.0 else 0.0)[0]
            for x in signal
        ]
        self.assertEqual(full, _load_curve("sine_envelope.csv"))
        self.assertEqual(len(full), ENVELOPE_SAMPLES)
        self.assertTrue(all(value >= 0.0 for value in full))
        negative = [i for i, value in enumerate(signal) if value < 0.0]
        peak = min(negative, key=lambda i: signal[i])
        self.assertEqual(peak, 360)
        self.assertEqual(signal[peak], -5.0)
        self.assertEqual(full[peak], 4.9995716378700354)
        self.assertEqual(full[361], SPAN)
        self.assertGreater(full[peak], 4.9)
        self.assertLess(half[peak], 1.0)
        self.assertTrue(any(full[i] > half[i] + 1.0 for i in negative))
        valley_at = 121 + full[121:361].index(min(full[121:361]))
        self.assertEqual(valley_at, 273)
        self.assertEqual(full[valley_at], 2.088817314142192)
        self.assertGreater(min(full[121:361]), 0.0)

    def test_feedback_bends_both_stored_curves(self) -> None:
        linear = _rise_until_five(0.0)
        positive = _rise_until_five(0.5)
        negative = _rise_until_five(-0.5)
        self.assertEqual(positive, _replay_rise(0.5))
        self.assertEqual(negative, _replay_rise(-0.5))
        self.assertEqual(positive, _load_curve("feedback_p0_5.csv"))
        self.assertEqual(negative, _load_curve("feedback_m0_5.csv"))
        self.assertEqual(len(linear), 241)
        self.assertEqual(linear[240], SPAN)
        self.assertEqual(linear[60], 1.25)
        self.assertEqual(len(positive), 182)
        self.assertEqual(positive[181], SPAN)
        self.assertEqual(positive[60], 1.3380204722304918)
        self.assertEqual(len(negative), 301)
        self.assertEqual(negative[300], SPAN)
        self.assertEqual(negative[60], 1.181381188120199)
        self.assertEqual(positive[1], linear[1])
        self.assertEqual(negative[1], linear[1])
        self.assertGreater(positive[2], linear[2])
        self.assertGreater(linear[2], negative[2])
        self.assertGreater(positive[60], linear[60])
        self.assertGreater(linear[60], negative[60])
        self.assertLess(positive.index(SPAN), linear.index(SPAN))
        self.assertLess(linear.index(SPAN), negative.index(SPAN))
        self.assertEqual(_oneshot(0.0)[-1], 0.0)
        self.assertEqual(len(_oneshot(0.0)) - 1, 480)
        self.assertEqual(_oneshot(0.5), _replay_oneshot(0.5))
        self.assertEqual(_oneshot(-0.5), _replay_oneshot(-0.5))
        self.assertEqual(len(_oneshot(0.5)) - 1, 361)
        self.assertEqual(len(_oneshot(-0.5)) - 1, 601)
        pos_step = [positive[i + 1] - positive[i] for i in range(len(positive) - 2)]
        neg_step = [negative[i + 1] - negative[i] for i in range(len(negative) - 2)]
        self.assertTrue(all(pos_step[i] <= pos_step[i + 1] for i in range(len(pos_step) - 1)))
        self.assertTrue(all(neg_step[i] >= neg_step[i + 1] for i in range(len(neg_step) - 1)))

    def test_feedback_patch_follows_the_vc_switch(self) -> None:
        both = _rise_until_five(0.5, vc_switch="both")
        rise_only = _rise_until_five(0.5, vc_switch="rise")
        fall_only = _rise_until_five(0.5, vc_switch="fall")
        self.assertEqual(rise_only, both)
        self.assertEqual(fall_only, _replay_rise(0.5, weight=0.0))
        self.assertGreater(len(fall_only), len(both))
        self.assertNotEqual(fall_only, both)

    def test_a_change_mid_rise_leaves_the_exact_count(self) -> None:
        def run(feedback_at: int = -1, vc_at: int = -1, octave_at: int = -1):
            slope = Slope(RISE, FALL)
            outs = []
            for i in range(20):
                feedback = 0.5 if feedback_at >= 0 and i >= feedback_at else 0.0
                vc = 1.0 if vc_at >= 0 and i >= vc_at else 0.0
                v_oct = 1.0 if octave_at >= 0 and i >= octave_at else 0.0
                outs.append(
                    slope.step(
                        trig=5.0 if i == 0 else 0.0,
                        feedback=feedback,
                        vc=vc,
                        v_oct=v_oct,
                    )[0]
                )
            return outs

        held = SPAN * 10 / 240
        bent = run(feedback_at=10)
        plain = run()
        self.assertEqual(bent[10], held)
        self.assertEqual(plain[10], held)
        self.assertEqual(bent[11], held + _vc_step(held, 0.5))
        self.assertEqual(plain[11], SPAN * 11 / 240)
        self.assertGreater(bent[11], plain[11])
        moved = run(vc_at=10)
        self.assertEqual(moved[11], held + _vc_step(held, 0.0, vc=1.0))
        sped = run(octave_at=10)
        self.assertEqual(sped[11], held + _vc_step(held, 0.0, v_oct=1.0))

    def test_external_vc_sums_with_the_patch(self) -> None:
        mixed = _rise_until_five(0.5, vc=1.0)
        self.assertEqual(mixed, _replay_rise(0.5, vc=1.0))
        self.assertNotEqual(mixed[1], _rise_until_five(0.5)[1])
        self.assertEqual(Slope(RISE, FALL).feedback_amount(2.0), 1.0)
        self.assertEqual(Slope(RISE, FALL).feedback_amount(-2.0), -1.0)
        self.assertEqual(_rise_until_five(2.0), _rise_until_five(1.0))

    def test_one_volt_per_octave_doubles_the_rate(self) -> None:
        base_rows = _free_run()
        base, _high, _start = _period(base_rows)
        sped_rows = _free_run(v_oct=1.0, n=800)
        sped, _high_v, _start_v = _period(sped_rows)
        self.assertEqual(base, 480)
        self.assertEqual(sped, 240)
        self.assertEqual(sped * 2, base)
        outs = [out for out, _end in sped_rows]
        for k in range(120):
            self.assertEqual(outs[k], SPAN * k / 120)
        self.assertEqual(outs[120], SPAN)
        bent = _rise_until_five(0.5, v_oct=1.0)
        self.assertEqual(bent, _replay_rise(0.5, v_oct=1.0))
        self.assertLess(len(bent), len(_rise_until_five(0.5)))

    def test_vc_shortens_when_positive_and_lengthens_when_negative(self) -> None:
        self.assertEqual(VC_SECONDS_PER_VOLT, 0.001)
        rise_only, high_r, _s = _period(_free_run(vc=1.0, vc_switch="rise", n=2000))
        fall_only, high_f, _t = _period(_free_run(vc=1.0, vc_switch="fall", n=2000))
        both, high_b, _u = _period(_free_run(vc=1.0, vc_switch="both", n=2000))
        longer, high_l, _w = _period(_free_run(vc=-1.0, vc_switch="both", n=3000))
        self.assertEqual((rise_only, high_r), (432, 192))
        self.assertEqual((fall_only, high_f), (432, 240))
        self.assertEqual((both, high_b), (384, 192))
        self.assertEqual((longer, high_l), (576, 288))
        mixed, mixed_high, _v = _period(
            _free_run(vc=1.0, v_oct=1.0, vc_switch="both", n=1000)
        )
        self.assertEqual((mixed, mixed_high), (192, 96))
        floored, floored_high, _x = _period(_free_run(vc=1000.0, n=16))
        self.assertEqual((floored, floored_high), (2, 1))

    def test_second_run_matches_sample_for_sample(self) -> None:
        self.assertEqual(_free_run(), _free_run())
        self.assertEqual(_rise_until_five(0.5), _rise_until_five(0.5))
        self.assertEqual(_rise_until_five(-0.5), _rise_until_five(-0.5))
        self.assertEqual(_envelope(), _envelope())
        self.assertEqual(_oneshot(0.5), _oneshot(0.5))
        first = Slope(RISE, FALL)
        second = Slope(RISE, FALL)
        a = [first.step(inp=3.0 if i < 960 else 0.0) for i in range(1100)]
        b = [second.step(inp=3.0 if i < 960 else 0.0) for i in range(1100)]
        self.assertEqual(a, b)

    def test_golden_records_the_slope_locks(self) -> None:
        golden = (ROOT / "GOLDEN.md").read_text(encoding="utf-8")
        for phrase in (
            f"sha1 `feedback_p0_5.csv` | {_sha1('feedback_p0_5.csv')}",
            f"sha1 `feedback_m0_5.csv` | {_sha1('feedback_m0_5.csv')}",
            f"sha1 `sine_envelope.csv` | {_sha1('sine_envelope.csv')}",
            "a = +0.5, OUT at sample 60 | 1.3380204722304918 V",
            "a = -0.5, OUT at sample 60 | 1.181381188120199 V",
            "a = +0.5, return to 0 V | sample 361",
            "a = -0.5, return to 0 V | sample 601",
            "+5 V on sample 181",
            "+5 V on sample 300",
            "sine envelope at sample 360 | 4.9995716378700354 V",
            "sine envelope valley | 2.088817314142192 V at sample 273",
            "VC +1 V, switch BOTH | period 384 samples, END high 192",
            "VC -1 V, switch BOTH | period 576 samples, END high 288",
            "period 240 samples",
        ):
            self.assertIn(phrase, golden)
        for stale in (
            "0230a301a13c9a1d6ad6339ff2e9f920bda13454",
            "764cfacb2e1b2dcfc72ea019275030594a3bad2b",
            "1.6279456556769976",
            "1.0411407626453677",
            "period 528 samples",
            "V_1v + a * v",
        ):
            self.assertNotIn(stale, golden)

    def test_readme_lists_the_jacks_and_the_coupling(self) -> None:
        text = (HERE.parent / "README.md").read_text(encoding="utf-8")
        source = (HERE.parent / "slope.py").read_text(encoding="utf-8")
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
            "VC_in = VC + a * v",
            "m = 2 ** V_1v",
            "T_r = max(1/fs, T_rise - w_r * 0.001 * VC_in)",
            "T_f = max(1/fs, T_fall - w_f * 0.001 * VC_in)",
            "0.001",
            "a rise and a fall, one peak",
            "The name Serge is not part of that lettering.",
            "sine_envelope.csv",
            "feedback_p0_5.csv",
            "feedback_m0_5.csv",
        ):
            self.assertIn(phrase, text)
        self.assertNotIn("V_1v + a * v", text)
        self.assertNotIn("V_1v + a * v", source)
        self.assertNotIn("v_oct +", source)
        self.assertIn("2.0 ** v_oct", source)
        self.assertNotIn("\u2014", text)
        self.assertNotIn("\u2013", text)
        root = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("PLEAT", root)
        self.assertIn("slope generator", root)
        self.assertIn("side by side", root)
        self.assertIn("rectified envelope", root)
