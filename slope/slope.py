"""One universal slope, the first half of the dual slope generator.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Output runs from 0 V to +5 V. Rise time and fall time are the linear
times for that full excursion. A trigger starts a rise from 0. At +5 V
the slope falls. At 0 V it emits one end-pulse and waits. Triggers
during the rise or the fall are ignored.

Each sample emits the voltage held from the previous update, then
applies this sample's inputs. END is low only while falling.

With the OUT-to-VC patch at 0, a full excursion of T seconds is
max(1, round(T * fs / 2**V_1v)) samples, and the voltage on sample k of
that segment is exact: span * k / n on the way up, span * (n - k) / n
on the way down. A nonzero patch adds the scaled output to the VC jack.
Positive VC shortens the selected time by 0.001 s/V. The step is then
span / (T(v) * fs) * 2**V_1v volts per sample. 1V/oct is that factor
alone.

The second half is the same circuit on its own output jack and is not
this file.
"""

from __future__ import annotations

import math

FS = 48000.0
SPAN = 5.0
# Seconds per volt of VC. Positive VC subtracts this from the selected knob time.
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
        self.segment_vc = 0.0
        self.segment_v_oct = 0.0

    def feedback_amount(self, feedback: float) -> float:
        """Patch scale from OUT to VC, clamped to [-1, +1]."""
        return min(FEEDBACK_MAX, max(FEEDBACK_MIN, feedback))

    def patched_vc(self, vc: float, feedback: float) -> float:
        """VC jack plus the scaled voltage held on OUT."""
        return vc + self.feedback_amount(feedback) * self.v

    def times(self, vc: float) -> tuple[float, float]:
        """Knob time after the VC offset, floored at one sample.

        Positive ``vc`` shortens the selected time. Negative ``vc``
        lengthens it. The offset is ``VC_SECONDS_PER_VOLT`` seconds per volt,
        summed with the knob.
        """
        offset = VC_SECONDS_PER_VOLT * vc
        rise = self.rise
        fall = self.fall
        if self.vc_switch in ("rise", "both"):
            rise -= offset
        if self.vc_switch in ("fall", "both"):
            fall -= offset
        return max(self.t_min, rise), max(self.t_min, fall)

    def segment_samples(self, time_seconds: float, v_oct: float) -> int:
        """Samples for one full 0 V to +5 V excursion at a constant 1V/oct."""
        scale = 2.0 ** v_oct
        return max(1, int(round(time_seconds * self.fs / scale)))

    def steps(self, vc: float, v_oct: float, feedback: float) -> tuple[float, float]:
        """Volts per sample from the held output.

        1V/oct scales the rate by ``2 ** V``. The OUT patch changes the
        selected time through the rise, fall, or both switch.
        """
        rise_time, fall_time = self.times(self.patched_vc(vc, feedback))
        scale = 2.0 ** v_oct
        rise_step = scale * SPAN / (rise_time * self.fs)
        fall_step = scale * SPAN / (fall_time * self.fs)
        return rise_step, fall_step

    def _exact_segment(self, feedback: float) -> bool:
        return self.feedback_amount(feedback) == 0.0

    def _begin_rise(self, vc: float, v_oct: float, feedback: float) -> None:
        self.state = _RISING
        self.v = 0.0
        self.k = 0
        self.phase = self._exact_segment(feedback)
        if self.phase:
            self.segment_vc = vc
            self.segment_v_oct = v_oct
            self.n = self.segment_samples(self.times(vc)[0], v_oct)

    def _begin_fall(self, vc: float, v_oct: float, feedback: float) -> None:
        self.state = _FALLING
        self.v = SPAN
        self.k = 0
        self.phase = self._exact_segment(feedback)
        if self.phase:
            self.segment_vc = vc
            self.segment_v_oct = v_oct
            self.n = self.segment_samples(self.times(vc)[1], v_oct)

    def _hold_exact(self, vc: float, v_oct: float, feedback: float) -> bool:
        """Rounded count while a is 0 and VC and 1V/oct are unchanged."""
        return (
            self.phase
            and self._exact_segment(feedback)
            and vc == self.segment_vc
            and v_oct == self.segment_v_oct
        )

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

        ``feedback`` scales the patch from OUT to the VC jack, from -1 to +1.
        ``end_to_trig`` is the cable from the end-pulse back to TRIG.
        IN is full-wave rectified. A positive IN overrides TRIG.
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

        if self.state == _RISING and self._hold_exact(vc, v_oct, feedback):
            self.k += 1
            if self.k >= self.n:
                self._begin_fall(vc, v_oct, feedback)
            else:
                self.v = SPAN * self.k / self.n
        elif self.state == _FALLING and self._hold_exact(vc, v_oct, feedback):
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
            self.phase = False
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
            self.phase = False
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
