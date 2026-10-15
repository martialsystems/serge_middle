# Slope

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

One universal slope, the first half of the dual slope generator. Output runs from 0 V to +5 V. Rise and fall are independent linear times for that full excursion. The map is `slope/slope.py`. Acceptance is at 48 kHz, in `slope/tests/`.

The second half is `slope/second.py`, the same circuit with its own state. Its extra jack is AC. The panel mark is a rise and a fall, one peak. Panel lettering is that mark and the jack legends. The name Serge is not part of that lettering.

## Controls

| Control | Role |
| --- | --- |
| IN | Signal. Full-wave rectified, then slewed. A positive voltage overrides TRIG. |
| TRIG | Rising edge above 1 V. Starts a rise from idle at 0 V. Ignored until the output has returned to 0 V. A level that has stayed high since that edge holds at +5 V until it goes low. A later pulse does not hold. |
| VC | Positive voltage shortens the selected time by 0.001 s/V. Negative voltage lengthens it. The plugins put an attenuverter in front of this jack. |
| 1V/oct | +1 V halves both rise and fall. |
| RISE | Knob. Base rise time, seconds for 0 V to +5 V. |
| FALL | Knob. Base fall time, seconds for +5 V to 0 V. |
| BOTH | Switch. The VC jack, including the scaled OUT patch, lands on RISE, on FALL, or on both. |
| OUT | 0 V to +5 V. |
| END | Circuit gate: high during the rise, the hold, and while the output is at 0 V. Low during the fall. The jack is 0 V or +5 V. |

## Law

`fs` is the sample rate. The acceptance rate is 48,000 Hz. `T_rise` and `T_fall` are the knobs, in seconds. `VC` is the voltage on the VC jack. `a` is the feedback amount, clamped to [-1, +1]. `v` is the voltage held on OUT. `V_1v` is the 1V/oct jack.

The bend is the OUT jack patched to the VC jack. `a` scales that patch. The attenuverter is a separate scale on the jack voltage, `scale_vc(VC, depth)`, clamped to [-1, +1]. Depth 0 passes nothing, which is the plugin default, so an unpatched or untouched VC jack stays at 0 V. Depth +1 passes the jack. Depth -1 inverts it. The sample law uses that scaled voltage as `VC`:

```text
u = min(5, abs(IN))
VC_in = VC + a * v
T_r = max(1/fs, T_rise - w_r * 0.001 * VC_in)
T_f = max(1/fs, T_fall - w_f * 0.001 * VC_in)
m = 2 ** V_1v
```

`w_r` is 1 when the switch is RISE or BOTH, and 0 when it is FALL. `w_f` is 1 when the switch is FALL or BOTH, and 0 when it is RISE.

The summed offset is -0.001 s per volt of `VC_in`. A positive voltage shortens the selected time. A negative voltage lengthens it. `m` is the 1V/oct rate scale. +1 V halves both rise and fall.

Each sample emits the held voltage, then applies the inputs.

When `a` is 0, `VC_in` is the jack voltage. A full excursion of `T` seconds at a constant `VC` and a constant `V_1v` is

```text
n = max(1, round(T * fs / 2**V_1v))
```

samples. Sample `k` of the rise emits `5 * k / n`, for `k` from 0 through `n - 1`. Sample `k` of the fall emits `5 * (n - k) / n`, for `k` from 0 through `n - 1`. The next sample emits 0 V. That count stays in force while `a` remains 0 and `VC` and `V_1v` stay at the values from the start of the excursion. On a sample where one of those changes, the update leaves the count and takes the Euler step from the voltage just emitted.

When `a` is not 0, `VC_in` moves with `v`, so `T_r` and `T_f` are computed again on each sample. The step is Euler and the rail is a snap:

```text
rise step = m * 5 / (T_r * fs)
fall step = m * 5 / (T_f * fs)
```

Positive `a` raises `VC_in` as `v` rises, shortens the rise, and climbs faster than the linear case `a` = 0. Negative `a` lowers `VC_in`, lengthens the rise, and climbs slower. The total time of the excursion changes with the bend.

A trigger edge while idle at 0 V starts a rise. At +5 V the state becomes the fall, unless TRIG is still above 1 V. Then the output holds at +5 V and the fall does not begin until TRIG goes low (Serge Modular Music Systems DUSG kit sheet; SFU manual, page 11: the fall cycle will not begin until the trigger level goes low). A trigger that has already gone low by the time the rise finishes does not hold: the fall starts at the top. That includes a short pulse, and a gate that ends during the rise. The hold does not wait for 4 V. A second rising edge during the rise, the hold, or the fall does not start another cycle. The update that reaches 0 V records one end-pulse and waits. The following sample emits 0 V with the circuit END gate high. Oscillator mode is that pulse patched back to TRIG (`end_to_trig` on `step`). One end-pulse per accepted trigger, so a trigger train faster than rise + fall divides by 2, 3, and so on.

`u > 0` overrides TRIG. The output slews toward `u` at the rise rate, holds at `u` while the input stays there, and falls when `u` returns to 0, even if TRIG is still high. Peak-follower use is rise at one sample and audio on IN. The negative half of that audio is `abs`, so it is flipped onto the positive envelope.

The circuit END gate is 0 during the fall and 1 otherwise, including the hold at +5 V and idle at 0 V. The END jack is not that gate. It is `end_jack_volts`: +5 V while the gate is high and OUT is below 4 V, otherwise 0 V. Idle is +5 V. A hold at +5 V is 0 V on the jack. The same function is what both plugins use. The 4 V window is why an END to TRIG cord cycles: a jack that stayed high for the whole rise would meet the hold rule and sit at +5 V. The original DUSG TRIG OUT stayed high through the rise (SMMS DUSG kit sheet, page 3). This jack is the 0 V / +5 V window.

Reference cycle, equal knobs: rise = fall = 5 ms, `a` = 0, `VC` = 0, `V_1v` = 0. The output is 0 V and +5 V at the corners. The period is 480 samples, 10 ms. END is high for the 240 rise samples and low for the 240 fall samples, so the duty is rise / (rise + fall).

The stored rises are `slope/tests/feedback_p0_5.csv` (`a` = +0.5) and `slope/tests/feedback_m0_5.csv` (`a` = -0.5). Both use the BOTH switch, with the VC jack at 0 V and 1V/oct at 0 V. Each file runs from the trigger sample through the sample that emits +5 V.

The stored envelope is `slope/tests/sine_envelope.csv`. The input is a bipolar 5 V sine, period 480 samples, 960 samples long. Rise is one sample and fall is 5 ms. Locked figures are in `GOLDEN.md`.

## Second half

`slope/second.py` is the second half. It has its own rise, fall, VC, 1V/oct, and BOTH switch. Stepping it does not step the first half. A cable is a value the caller passes into `step`.

AC is a jack on this half only:

```text
AC = 2.5 - OUT
```

OUT at 0 V is AC at +2.5 V. OUT at +5 V is AC at -2.5 V. OUT at +2.5 V is AC at 0 V. AC is not part of `VC_in = VC + a * v`. The first half returns OUT and END.

Free cycle of this half, rise = fall = 5 ms, `a` = 0, end pulse patched to TRIG. The stored samples are `slope/tests/ac_cycle.csv`.

| sample | AC (V) |
| --- | --- |
| 0 | +2.5 |
| 120 | 0 |
| 240 | -2.5 |
| 480 | +2.5 |

With `a` = +0.5 on the second half only, AC is -2.5 on sample 181, the sample where OUT emits +5 V. The first half beside that run, at `a` = 0, stays on the linear rise: sample 240 is still +5 V.

The VST3 of this half is `SlopeAC/`. Rise and Fall default to 5 ms. The raw plugin sample is AC / 2.5, so ±2.5 V is ±1. The host gets that jack through a coupling capacitor, `B(AC - 2.5) / 2.5`, with B the 10 Hz block (see `SlopeAC/README.md`). `SergeMiddle/` is the PLEAT panel.

The plugin's Cycle switch defaults to off. On is this free cycle: END patched to TRIG, and one trigger armed after reset. Off opens the cord, as on a classic slope with nothing patched, and the output sits at 0 V. The host stays on IN in both positions.

## How to run

From the repository root:

```text
python3 -m unittest discover -s slope/tests -t .
```

The suite is the standard library. PLEAT stays on `python3 -m unittest discover -s tests -t .`.
