"""Acceptance for the second slope half and its AC jack.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The AC cycle is slope/tests/ac_cycle.csv. The first half stays in
slope/slope.py and is not stepped by these objects.
"""

from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

from slope.second import AC_CENTER, SecondHalf, ac_volts, scaled_ac
from slope.slope import FS, SPAN, Slope

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
RISE = 0.005
FALL = 0.005
CYCLE = 480


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


def _ac_cycle() -> list[tuple[float, float, float]]:
    half = SecondHalf(RISE, FALL)
    rows = []
    for i in range(CYCLE + 1):
        rows.append(
            half.step(trig=5.0 if i == 0 else 0.0, end_to_trig=True)
        )
    return rows


def _first_cycle() -> list[tuple[float, float]]:
    slope = Slope(RISE, FALL)
    return [
        slope.step(trig=5.0 if i == 0 else 0.0, end_to_trig=True)
        for i in range(CYCLE + 1)
    ]


class SecondHalfAcceptanceTest(unittest.TestCase):
    def test_out_and_end_match_the_first_half(self) -> None:
        first = _first_cycle()
        second = _ac_cycle()
        self.assertEqual([(out, end) for out, end, _ac in second], first)
        outs = [out for out, _end in first]
        ends = [end for _out, end in first]
        self.assertEqual(len(first) - 1, CYCLE)
        self.assertEqual(outs[0], 0.0)
        self.assertEqual(outs[240], SPAN)
        self.assertEqual(outs[CYCLE], 0.0)
        for k in range(240):
            self.assertEqual(outs[k], SPAN * k / 240)
            self.assertEqual(ends[k], 1.0)
        for k in range(240, CYCLE):
            self.assertEqual(ends[k], 0.0)
        self.assertEqual(sum(ends[:CYCLE]), 240)
        self.assertEqual(sum(ends[:CYCLE]) / CYCLE, RISE / (RISE + FALL))

    def test_trigger_train_divides_by_three(self) -> None:
        interval = CYCLE // 3
        half = SecondHalf(RISE, FALL)
        triggers = 0
        pulses = 0
        for i in range(14400):
            trig = 5.0 if i % interval == 0 else 0.0
            if trig > 1.0:
                triggers += 1
            half.step(trig=trig)
            if half.end_pulse:
                pulses += 1
        self.assertEqual(triggers, 90)
        self.assertEqual(pulses, 30)
        self.assertEqual(pulses * 3, triggers)

    def test_gate_holds_for_20_ms(self) -> None:
        self.assertEqual(960, int(round(0.020 * FS)))
        half = SecondHalf(RISE, FALL)
        outs = []
        for i in range(1200):
            trig = 5.0 if i == 400 else 0.0
            out, _end, _ac = half.step(inp=3.0 if i < 960 else 0.0, trig=trig)
            outs.append(out)
        self.assertEqual(outs[144], 3.0)
        self.assertEqual(outs[400], 3.0)
        self.assertTrue(all(value == 3.0 for value in outs[144:960]))
        self.assertEqual(outs[960], 3.0)
        self.assertLess(outs[961], 3.0)
        self.assertLess(max(outs[:960]), SPAN)

    def test_ac_cycle_landmarks_and_file(self) -> None:
        rows = _ac_cycle()
        ac = [value for _out, _end, value in rows]
        self.assertEqual(ac, _load_curve("ac_cycle.csv"))
        self.assertEqual(ac[0], AC_CENTER)
        self.assertEqual(ac[120], 0.0)
        self.assertEqual(ac[240], -AC_CENTER)
        self.assertEqual(ac[CYCLE], AC_CENTER)
        self.assertEqual(ac[120], ac_volts(SPAN / 2.0))
        self.assertEqual(scaled_ac(ac[0]), 1.0)
        self.assertEqual(scaled_ac(ac[120]), 0.0)
        self.assertEqual(scaled_ac(ac[240]), -1.0)
        self.assertEqual(scaled_ac(ac[CYCLE]), 1.0)
        for out, _end, volts in rows:
            self.assertEqual(volts, ac_volts(out))

    def test_positive_feedback_on_the_second_half_only(self) -> None:
        first = Slope(RISE, FALL)
        second = SecondHalf(RISE, FALL)
        first_out = []
        second_out = []
        second_ac = []
        for i in range(CYCLE + 1):
            trig = 5.0 if i == 0 else 0.0
            first_out.append(first.step(trig=trig, feedback=0.0, end_to_trig=True)[0])
            out, _end, ac = second.step(trig=trig, feedback=0.5, end_to_trig=True)
            second_out.append(out)
            second_ac.append(ac)
        for k in range(240):
            self.assertEqual(first_out[k], SPAN * k / 240)
        self.assertEqual(first_out[240], SPAN)
        self.assertEqual(second_out[181], SPAN)
        self.assertEqual(second_ac[181], -AC_CENTER)
        self.assertEqual(second_ac[181], ac_volts(second_out[181]))
        self.assertNotEqual(first_out, second_out)
        stored = _load_curve("feedback_p0_5.csv")
        self.assertEqual(second_out[: len(stored)], stored)

    def test_halves_stay_apart_until_a_cable(self) -> None:
        idle = Slope(RISE, FALL)
        free = Slope(RISE, FALL)
        cabled = Slope(RISE, FALL)
        second = SecondHalf(RISE, FALL)
        free_out = []
        cabled_out = []
        for i in range(CYCLE):
            second.step(trig=5.0 if i == 0 else 0.0, feedback=0.5, end_to_trig=True)
        self.assertEqual(idle.v, 0.0)
        self.assertEqual(idle.state, "idle")
        for i in range(CYCLE + 1):
            trig = 5.0 if i == 0 else 0.0
            _out, _end, ac = second.step(trig=trig, feedback=0.5, end_to_trig=True)
            free_out.append(free.step(trig=trig, end_to_trig=True)[0])
            cabled_out.append(cabled.step(trig=trig, vc=ac, end_to_trig=True)[0])
        for k in range(240):
            self.assertEqual(free_out[k], SPAN * k / 240)
        self.assertNotEqual(cabled_out, free_out)

    def test_second_run_matches_sample_for_sample(self) -> None:
        self.assertEqual(_ac_cycle(), _ac_cycle())
        one = SecondHalf(RISE, FALL)
        two = SecondHalf(RISE, FALL)
        a = [one.step(inp=3.0 if i < 960 else 0.0) for i in range(1100)]
        b = [two.step(inp=3.0 if i < 960 else 0.0) for i in range(1100)]
        self.assertEqual(a, b)

    def test_same_knob_write_keeps_the_480_sample_cycle(self) -> None:
        half = SecondHalf(RISE, FALL)
        outs = []
        for i in range(CYCLE + 1):
            half.set_times(RISE, FALL)
            out, _end, _ac = half.step(trig=5.0 if i == 0 else 0.0, end_to_trig=True)
            outs.append(out)
        self.assertEqual(outs[240], SPAN)
        self.assertEqual(outs[CYCLE], 0.0)

    def test_first_half_has_no_ac_jack(self) -> None:
        slope = Slope(RISE, FALL)
        self.assertEqual(len(slope.step(trig=5.0)), 2)
        self.assertFalse(hasattr(slope, "ac"))
        source = (HERE.parent / "slope.py").read_text(encoding="utf-8")
        self.assertNotIn("AC_CENTER", source)
        self.assertNotIn("class SecondHalf", source)

    def test_readme_and_golden_record_the_ac_jack(self) -> None:
        text = (HERE.parent / "README.md").read_text(encoding="utf-8")
        golden = (ROOT / "GOLDEN.md").read_text(encoding="utf-8")
        digest = hashlib.sha1((HERE / "ac_cycle.csv").read_bytes()).hexdigest()
        for phrase in (
            "AC = 2.5 - OUT",
            "slope/second.py",
            "ac_cycle.csv",
            "The name Serge is not part of that lettering.",
            "VC_in = VC + a * v",
            "m = 2 ** V_1v",
        ):
            self.assertIn(phrase, text)
        self.assertNotIn("\u2014", text)
        self.assertNotIn("\u2013", text)
        for phrase in (
            f"sha1 `ac_cycle.csv` | {digest}",
            "AC at sample 0 | +2.5 V",
            "AC at sample 120 | 0 V",
            "AC at sample 240 | -2.5 V",
            "AC at sample 480 | +2.5 V",
            "AC at sample 181 | -2.5 V",
            "plugin sample | AC / 2.5",
        ):
            self.assertIn(phrase, golden)
        root = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("slope/second.py", root)
        self.assertIn("SergeMiddle/", root)
        self.assertIn("SlopeAC/", root)

    def test_slope_ac_target_is_the_scaled_second_half(self) -> None:
        plugin = ROOT / "SlopeAC"
        cmake = (plugin / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("FORMATS VST3", cmake)
        self.assertNotIn("FORMATS APP", cmake)
        self.assertNotIn(".png", cmake)
        self.assertIn("Roboto-Regular.ttf", cmake)
        config = (plugin / "config.h").read_text(encoding="utf-8")
        self.assertIn('#define PLUG_NAME "Slope AC"', config)
        self.assertIn("#define PLUG_LATENCY 0", config)
        self.assertIn('VST3_SUBCATEGORY "Fx"', config)
        self.assertIn("#define PLUG_WIDTH 320", config)
        self.assertIn("#define PLUG_HEIGHT 180", config)
        cpp = (plugin / "SlopeAC.cpp").read_text(encoding="utf-8")
        self.assertIn("PluginSample", cpp)
        self.assertIn('InitDouble("Rise", 0.005', cpp)
        self.assertIn('InitDouble("Fall", 0.005', cpp)
        self.assertIn('IVKnobControl', cpp)
        self.assertIn('"Rise"', cpp)
        self.assertIn('"Fall"', cpp)
        header = (plugin / "dsp" / "SecondHalf.h").read_text(encoding="utf-8")
        self.assertIn("return ScaledAc(sample.ac);", header)
        self.assertIn("arm ? 5.0 : 0.0", header)
        for path in (
            plugin / "config.h",
            plugin / "SlopeAC.cpp",
            plugin / "SlopeAC.h",
            plugin / "dsp" / "SecondHalf.h",
            plugin / "resources" / "SlopeAC-VST3-Info.plist",
            plugin / "CMakeLists.txt",
            plugin / "README.md",
        ):
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("\u2014", text, path.name)
            self.assertNotIn("\u2013", text, path.name)
        for path in (
            plugin / "config.h",
            plugin / "SlopeAC.cpp",
            plugin / "SlopeAC.h",
            plugin / "dsp" / "SecondHalf.h",
            plugin / "resources" / "SlopeAC-VST3-Info.plist",
        ):
            self.assertNotIn("Serge", path.read_text(encoding="utf-8"), path.name)
