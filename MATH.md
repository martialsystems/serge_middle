# Math

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

This note derives the locked map. It does not introduce constants. The values live in `wave_middle.py`, `GOLDEN.md`, and the tap files.

## Op amp

One middle-section cell is an op-amp stage. The diode network sets the positive input, v_plus. The output resistors form

```text
v_out = 2 * v_plus - v_in
```

With v_plus = sign(v) * (|v| - eta * VT * W(z)) and v ≠ 0, that is

```text
v_out = sign(v) * (|v| - 2 * eta * VT * W(z))
```

v_out(0) = 0. The factor 2 is the fold. The diode node alone stays positive at 1 V (0.386759 V). The stage at 1 V has folded (-0.226482 V). The same expression with that factor removed is a compressor.

The same closed form is equation (39) of Esqueda, Pöntynen, Parker, and Bilbao, Applied Sciences 7, 1328 (2017), https://doi.org/10.3390/app7121328, at the v1 constants. Table 4 of that paper is a different diode fit, eta = 1.752 and VT = 0.025864, and stays out of the plugin.

## Lambert W

```text
z = (Is * R) / (eta * VT) * exp(|v| / (eta * VT))
```

W is the principal branch. The product k * exp(|v| / (eta * VT)), with k = (Is * R) / (eta * VT), overflows float64 at the top of the drive range. The solver never forms it. Halley runs on

```text
w + ln(w) - ln(z) = 0
```

with ln(z) = |v| / (eta * VT) + ln(k). A port of the solver has to keep that residual under 1e-12 on |v| through 6 V and at |v| = 48 V, which is what `tests/test_transfer.py` checks.

The one-sided limits at the origin have magnitude 2 * eta * VT * W(k), because the Shockley law used here has no -1 term. The acceptance sample at vin = 0 is still 0, by the explicit `v_out(0) = 0`.

## Why P(g) is a table

P(g) is the maximum of |C^6(v)| for |v| <= 5 g. On (0, 40 V] the absolute map has six interior lobes, and then the peak is the endpoint. From the last lobe through g = 1.109989 the running maximum stays on the g = 1 peak. Past that drive the endpoint is the peak, and it is still climbing at 40 V. A single `OUTPUT_GAIN` cannot hold unity at every g, because that running maximum leaves the g = 1 plateau.

The 53 knots in `DRIVE_PEAKS` are linear in g. Between them the relative error stays within 0.001. The audio thread reads the table. Searching for the peak on that thread would retune the bound per sample and would not match the knots.

## The 11.7 Hz buffer

The one-pole magnitude is

```text
|H|^2 = (2 - 2 cos w) / (1 + r^2 - 2 r cos w)
```

with w = 2 * pi * f / fs and r = exp(-2 * pi * 10 / fs). |H| at 0 Hz is 0. A pure sine at 11.71875 Hz, one cycle of a 4,096-point buffer at 48 kHz, has |H| = 0.761185. At 40 Hz it is 0.970778. At 100 Hz it is 0.995689.

The folded buffer is a different waveform. One cycle of `audio_map` at g = 1, after one lead-in period from zero state, has peak ratio 1.152747 and a maximum deviation of 0.390095 times the pre-block peak. The fundamental sits near the pole, and the folds put energy on harmonics whose gains differ. The mean of that one-cycle buffer is below 1e-9, and the peak ratio is still 1.152747. The cutoff stays at 10 Hz.

## Smoother

The fold knob is a one-pole with time constant 0.020 s, updated at the audio rate.

```text
c = exp(-1 / (0.020 * fs))
g[n] = g[n-1] + (1 - c) * (g_target - g[n-1])
```

From g0 toward a target T the closed form is g[n] = T + c^n * (g0 - T). At 48 kHz, 20 ms is 960 samples, and c^960 equals exp(-1) in real arithmetic. A step from 1 to 2 lands on 2 - exp(-1), which is 1 + (1 - exp(-1)). Float iteration of the difference equation stays inside 1e-4 of that value. The state starts at the clamped target, so a constant knob does not slew up from 0. The update runs once per audio sample. The four oversampled phases read that one value. Updating again on each phase would run the pole on the 4× clock.

## Decimator

The filter length is 63, type I linear phase, so the tap vector is symmetric and the group delay is 31 samples at the 4× rate. The passband edge is the audio Nyquist, 0.125 cycles of that rate. The stopband edge is the probe at 0.1875 cycles, which is 0.75 * fs/4 with fs the 4× rate. Parks-McClellan equiripple with equal weights meets both edges: the passband stays within 0.0007 of unity, and the stop probe is at -69.19 dB. The half-amplitude frequency is 0.156181 cycles. It sits in the transition, above the audio Nyquist, because the passband is held at unity through 0.125 and the stopband begins at 0.1875.

Zero-insertion of a low-rate tone replicates its spectrum at multiples of the audio rate. The first image of a tone at frequency f sits at (audio rate) - f, measured on the 4× clock. For f at or below half the audio Nyquist, that image is at or above 0.1875 cycles and the stopband removes it. The pass probe is higher: 0.90 times the audio Nyquist. Its image lands at 0.1375 cycles, where the transition magnitude is still 0.944259. The four decimation phases of that zero-stuffed tone therefore do not each return gain 1. The locked pass measurement generates the probe at the 4× rate, filters, and decimates, which is the decimator alone.

A structural halfband zeros every even offset from the center tap and then satisfies H(f) + H(0.5 - f) = 1. Its cutoff is 0.25 cycles of the filter rate, the audio sample rate, an octave above the audio Nyquist. The stop probe is below that cutoff. A 63-tap Kaiser lowpass built that way, center tap 0.5, leaves the stop probe at 0.00 dB. The stored taps do not zero those offsets. The largest even-offset coefficient is 0.145542.

Inserting zeros scales the baseband by 1/4. The upsampler taps are the decimator taps times 4, so one filter restores unity and the other, after the cells, does not apply the factor a second time. Using the gain-4 taps on both sides would multiply the level by 4.
