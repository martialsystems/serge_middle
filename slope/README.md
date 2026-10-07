# Slope

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

One universal slope, the first half of the dual slope generator. Output runs from 0 V to +5 V. Rise and fall are independent linear times for that full excursion. The map is `slope/slope.py`. Acceptance is at 48 kHz, in `slope/tests/`.

The second half is the same circuit with its own output jack. The AC output and a plugin for this slope come after this map. The later panel mark is a rise and a fall, one peak. Panel lettering is that mark and the jack legends. The name Serge is not part of that lettering.

## Controls

| Control | Role |
| --- | --- |
| IN | Signal. Full-wave rectified, then slewed. A positive voltage overrides TRIG. |
| TRIG | Rising edge above 1 V. Starts a rise from idle at 0 V. Ignored until the output has returned to 0 V. |
| VC | Added to the selected knob time, 0.001 s/V. |
| 1V/oct | +1 V halves both rise and fall. |
| RISE | Knob. Base rise time, seconds for 0 V to +5 V. |
| FALL | Knob. Base fall time, seconds for +5 V to 0 V. |
| BOTH | Switch. VC lands on RISE, on FALL, or on both. |
| OUT | 0 V to +5 V. |
| END | High during the rise, the hold, and while the output is at 0 V. Low during the fall. |

## Law

`fs` is the sample rate. The acceptance rate is 48,000 Hz. `T_rise` and `T_fall` are the knobs, in seconds. `VC` and `V_1v` are jack volts. `a` is the scale of the patch from OUT back to the 1V/oct sum, clamped to [-1, +1]. `v` is the voltage held on OUT.

```text
u = min(5, abs(IN))
T_r = max(1/fs, T_rise + w_r * 0.001 * VC)
T_f = max(1/fs, T_fall + w_f * 0.001 * VC)
m = 2 ** (V_1v + a * v)
```

`w_r` is 1 when the switch is RISE or BOTH, and 0 when it is FALL. `w_f` is 1 when the switch is FALL or BOTH, and 0 when it is RISE.

Each sample emits the held voltage, then applies the inputs.

When `a` is 0, a full excursion of `T` seconds at a constant `V_1v` is

```text
n = max(1, round(T * fs / 2**V_1v))
```

samples. Sample `k` of the rise emits `5 * k / n`, for `k` from 0 through `n - 1`. Sample `k` of the fall emits `5 * (n - k) / n`, for `k` from 0 through `n - 1`. The next sample emits 0 V.

When `a` is not 0, the step is Euler and the rail is a snap:

```text
rise step = m * 5 / (T_r * fs)
fall step = m * 5 / (T_f * fs)
```

Positive `a` speeds the slope as `v` rises. Negative `a` slows it. The same `m` multiplies both steps. The bend is the patch of OUT into the 1V/oct sum.

A trigger edge while idle at 0 V starts a rise. At +5 V the state becomes the fall. The update that reaches 0 V records one end-pulse and waits. The following sample emits 0 V with END high. Oscillator mode is that pulse patched back to TRIG (`end_to_trig` on `step`). One end-pulse per accepted trigger, so a trigger train faster than rise + fall divides by 2, 3, and so on.

`u > 0` overrides TRIG. The output slews toward `u` at the rise rate, holds at `u` while the input stays there, and falls when `u` returns to 0. Peak-follower use is rise at one sample and audio on IN. The negative half of that audio is `abs`, so it is flipped onto the positive envelope.

Reference cycle, equal knobs: rise = fall = 5 ms, `a` = 0, `V_1v` = 0. The output is 0 V and +5 V at the corners. The period is 480 samples, 10 ms. END is high for the 240 rise samples and low for the 240 fall samples, so the duty is rise / (rise + fall).

The stored bends are `slope/tests/feedback_p0_5.csv` (`a` = +0.5) and `slope/tests/feedback_m0_5.csv` (`a` = -0.5). Locked figures are in `GOLDEN.md`.

## How to run

From the repository root:

```text
python3 -m unittest discover -s slope/tests -t .
```

The suite is the standard library. PLEAT stays on `python3 -m unittest discover -s tests -t .`.
