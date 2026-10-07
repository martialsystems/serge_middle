"""One universal slope, the first half of the dual slope generator.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Output runs from 0 V to +5 V. Rise time and fall time are the linear
times for that full excursion. A trigger starts a rise from 0. At +5 V
the slope falls. At 0 V it emits one end-pulse and waits. Triggers
during the rise or the fall are ignored.

Each sample emits the voltage held from the previous update, then
applies this sample's inputs. END is low only while falling.

With feedback at 0, a full excursion of T seconds is
max(1, round(T * fs / 2**V_1v)) samples, and the voltage on sample k of
that segment is exact: span * k / n on the way up, span * (n - k) / n
on the way down. Feedback other than 0 integrates
span / (T * fs) * 2**(V_1v + a * v) volts per sample.

The second half is the same circuit on its own output jack and is not
this file.
"""

from __future__ import annotations

import math

FS = 48000.0
SPAN = 5.0
# 1 V on VC adds this many seconds to the selected knob time.
VC_SECONDS_PER_VOLT = 0.001
TRIG_THRESHOLD = 1.0
FEEDBACK_MIN = -1.0
FEEDBACK_MAX = 1.0

_IDLE = "idle"
_RISING = "rising"
_FALLING = "falling"
_HOLDING = "holding"


def _require_finite(name: str, value: float) -> float:
    number = float(value)
    if not math.isfinite(number):
        raise ValueError(f"{name} must be finite")
    return number


class Slope:
    """One slope. Inputs are IN, TRIG, VC, and 1V/oct. OUT and END leave."""

    def __init__(
        self,
        rise: float,
        fall: float,
        fs: float = FS,
        vc_switch: str = "both",
    ) -> None:
        self.fs = _require_finite("fs", fs)
        if self.fs <= 0.0:
            raise ValueError("fs must be positive")
        self.rise = _require_finite("rise", rise)
        self.fall = _require_finite("fall", fall)
        if self.rise <= 0.0 or self.fall <= 0.0:
            raise ValueError("rise and fall must be positive")
        if vc_switch not in ("rise", "fall", "both"):
            raise ValueError("vc_switch must be rise, fall, or both")
        self.vc_switch = vc_switch
        self.t_min = 1.0 / self.fs
        self.reset()

    def reset(self) -> None:
        self.v = 0.0
        self.state = _IDLE
        self.trig_hot = False
        self.tracking = False
        self.end_pulse = False
        self.phase = False
        self.k = 0
        self.n = 1

    def times(self, vc: float) -> tuple[float, float]:
        """Knob time plus the additive VC, floored at one sample."""
        added = VC_SECONDS_PER_VOLT * vc
        rise = self.rise
        fall = self.fall
        if self.vc_switch in ("rise", "both"):
            rise += added
        if self.vc_switch in ("fall", "both"):
            fall += added
        return max(self.t_min, rise), max(self.t_min, fall)

    def segment_samples(self, time_seconds: float, v_oct: float) -> int:
        """Samples for one full 0 V to +5 V excursion at a constant 1V/oct."""
        scale = 2.0 ** v_oct
        return max(1, int(round(time_seconds * self.fs / scale)))

    def steps(self, vc: float, v_oct: float, feedback: float) -> tuple[float, float]:
        """Volts per sample at the voltage currently held on the output."""
        rise_time, fall_time = self.times(vc)
        amount = min(FEEDBACK_MAX, max(FEEDBACK_MIN, feedback))
        rate_v = v_oct + amount * self.v
        scale = 2.0 ** rate_v
        rise_step = scale * SPAN / (rise_time * self.fs)
        fall_step = scale * SPAN / (fall_time * self.fs)
        return rise_step, fall_step

    def _begin_rise(self, vc: float, v_oct: float, feedback: float) -> None:
        self.state = _RISING
        self.v = 0.0
        self.k = 0
        amount = min(FEEDBACK_MAX, max(FEEDBACK_MIN, feedback))
        self.phase = amount == 0.0
        if self.phase:
            self.n = self.segment_samples(self.times(vc)[0], v_oct)

    def _begin_fall(self, vc: float, v_oct: float, feedback: float) -> None:
        self.state = _FALLING
        self.v = SPAN
        self.k = 0
        amount = min(FEEDBACK_MAX, max(FEEDBACK_MIN, feedback))
        self.phase = amount == 0.0
        if self.phase:
            self.n = self.segment_samples(self.times(vc)[1], v_oct)

    def step(
        self,
        inp: float = 0.0,
        trig: float = 0.0,
        vc: float = 0.0,
        v_oct: float = 0.0,
        feedback: float = 0.0,
        end_to_trig: bool = False,
    ) -> tuple[float, float]:
        """Emit (OUT, END), then advance one sample.

        ``feedback`` scales OUT into the 1V/oct sum, from -1 to +1.
        ``end_to_trig`` is the cable from the end-pulse back to TRIG.
        A positive IN is full-wave rectified and overrides TRIG.
        """
        inp = _require_finite("inp", inp)
        trig = _require_finite("trig", trig)
        vc = _require_finite("vc", vc)
        v_oct = _require_finite("v_oct", v_oct)
        feedback = _require_finite("feedback", feedback)

        target = min(SPAN, abs(inp))
        edge = trig > TRIG_THRESHOLD and not self.trig_hot
        self.trig_hot = trig > TRIG_THRESHOLD
        self.end_pulse = False

        if target > 0.0:
            self.tracking = True
            self.phase = False
            if self.v < target:
                self.state = _RISING
            elif self.v > target:
                self.state = _FALLING
            else:
                self.state = _HOLDING
        else:
            if self.tracking:
                self.tracking = False
                self.phase = False
                if self.v > 0.0:
                    self.state = _FALLING
                else:
                    self.state = _IDLE
            if self.state == _IDLE and edge:
                self._begin_rise(vc, v_oct, feedback)

        out = self.v
        end_gate = 0.0 if self.state == _FALLING else 1.0

        if self.state == _RISING and self.phase:
            self.k += 1
            if self.k >= self.n:
                self._begin_fall(vc, v_oct, feedback)
            else:
                self.v = SPAN * self.k / self.n
        elif self.state == _FALLING and self.phase:
            self.k += 1
            if self.k >= self.n:
                self.v = 0.0
                self.state = _IDLE
                self.end_pulse = True
                if end_to_trig:
                    self._begin_rise(vc, v_oct, feedback)
            else:
                self.v = SPAN * (self.n - self.k) / self.n
        elif self.state == _RISING:
            rise_step, _fall_step = self.steps(vc, v_oct, feedback)
            self.v += rise_step
            limit = target if target > 0.0 else SPAN
            if self.v >= limit:
                self.v = limit
                if target > 0.0:
                    self.state = _HOLDING
                else:
                    self._begin_fall(vc, v_oct, feedback)
        elif self.state == _FALLING:
            _rise_step, fall_step = self.steps(vc, v_oct, feedback)
            self.v -= fall_step
            floor = target if target > 0.0 else 0.0
            if self.v <= floor:
                self.v = floor
                if floor == 0.0:
                    self.state = _IDLE
                    self.end_pulse = True
                    if end_to_trig:
                        self._begin_rise(vc, v_oct, feedback)
                else:
                    self.state = _HOLDING

        return out, end_gate
