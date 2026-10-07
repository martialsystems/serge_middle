"""Second half of the dual slope generator.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The circuit is ``Slope`` with its own state, its own rise and fall, its
own VC, its own 1V/oct, and its own BOTH switch. Stepping this half does
not step another half. AC is a jack on this half only:

    AC = 2.5 - OUT

OUT at 0 V is AC at +2.5 V. OUT at +5 V is AC at -2.5 V. OUT at +2.5 V
is AC at 0 V. AC is not added to ``VC_in``. A caller who wants that cable
passes the emitted AC back in as ``vc`` on a later ``step``.
"""

from __future__ import annotations

from slope.slope import FS, SPAN, Slope

# OUT is 0 V to +5 V, so the inverted triangle is centered at 2.5 V.
AC_CENTER = SPAN / 2.0


def ac_volts(out: float) -> float:
    """AC jack. ``out`` is the voltage just emitted on OUT."""
    return AC_CENTER - out


def scaled_ac(ac: float) -> float:
    """Plugin sample. ±2.5 V is ±1, and this scale has no block after it."""
    return ac / AC_CENTER


class SecondHalf:
    """One slope beside the first half, plus the AC jack."""

    def __init__(
        self,
        rise: float,
        fall: float,
        fs: float = FS,
        vc_switch: str = "both",
    ) -> None:
        self._circuit = Slope(rise, fall, fs=fs, vc_switch=vc_switch)

    def set_times(self, rise: float, fall: float) -> None:
        self._circuit.set_times(rise, fall)

    def reset(self) -> None:
        self._circuit.reset()

    @property
    def end_pulse(self) -> bool:
        return self._circuit.end_pulse

    def step(
        self,
        inp: float = 0.0,
        trig: float = 0.0,
        vc: float = 0.0,
        v_oct: float = 0.0,
        feedback: float = 0.0,
        end_to_trig: bool = False,
    ) -> tuple[float, float, float]:
        """Emit (OUT, END, AC), then advance this half one sample."""
        out, end = self._circuit.step(
            inp=inp,
            trig=trig,
            vc=vc,
            v_oct=v_oct,
            feedback=feedback,
            end_to_trig=end_to_trig,
        )
        return out, end, ac_volts(out)
