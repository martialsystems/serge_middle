# iPlug2 prompt

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The VST3 is `SergeMiddle/` in this repository: one audio effect in iPlug2, the middle section of the Serge Wave Multipliers. JUCE is not the framework. The map is the port of `wave_middle.py`. Do not start the Dual Universal Slope Generator in this module. It waits until this module loads and a sine through it matches the locked curve.

Copy the smoother coefficient and the tap files. Do not redesign them.

## Constants

Use the v1 pair already locked in `wave_middle.py`: VT = 0.02585, Is = 2.52e-9, eta = 1.68, R = 33000. Esqueda et al. 2017 Table 4 (eta = 1.752, VT = 0.025864) stays out of the plugin. The CSV is that v1 fit.

Port `lambert_w0_kexp`, `diode_node`, `cell`, and `process`. The Halley solve is on w + ln(w) - ln(z) = 0. Do not form k * exp(|v| / (eta * VT)).

`OUTPUT_GAIN` is 4.3792716960440945. `DRIVE_PEAKS` is the fold-amount curve next to it. Copy that table. Do not search for the peak on the audio thread.

The smoother time is 0.020 seconds:

```text
c = exp(-1 / (0.020 * fs))
g[n] = g[n-1] + (1 - c) * (g_target - g[n-1])
```

Copy `halfband_taps.csv` (63 taps, sum 1) and `upsample_taps.csv` (those taps times 4, sum 4). The design note is in the CSV headers and in `README.md`.

## Sample path

g is the fold amount, clamped to [0.5, 8], default 1. Smooth it once per audio sample. The state starts at the current target. Hold the smoothed g constant across the four oversampled phases of one audio sample.

d(g) = g on that interval. The processing order is: smooth g, v = 5 * g * a, 4× upsample, six cells, 63-tap halfband, decimate, OUTPUT_GAIN, P(1) / P(g), 10 Hz block.

For each audio sample `a`:

1. Update g with the smoother above. Clamp the knob target to [0.5, 8] before the update. The four oversampled phases use this same g.
2. v = 5 * g * a. With d(g) = g this is the drive line v = 5 * d(g) * a.
3. Upsample 4×: insert three zeros after the sample, then apply `upsample_taps.csv`. Evaluate the six cells on that high-rate sequence. The cells are `process(v)` with the drive already inside v. They do not apply `OUTPUT_GAIN`.
4. Lowpass with `halfband_taps.csv` and decimate by 4, keeping phase 0 (indices 0, 4, 8, ...). Each 63-tap filter has a group delay of 31 samples at the 4× rate.
5. Multiply by `OUTPUT_GAIN`.
6. Multiply by P(1) / P(g). P(g) is linear interpolation of `DRIVE_PEAKS` in g, the function `drive_peak`. Apply that ratio as a gain, including where |y| is below the peak. Do not clip the output to [-1, 1].
7. One-pole DC block at 10 Hz, at the audio rate, after the halfband:

```text
r = exp(-2 * pi * 10 / fs)
y[n] = x[n] - x[n-1] + r * y[n-1]
```

Initial state: x[-1] = 0, y[-1] = 0. `fs` is the host sample rate. This is `dc_block_pole` and `dc_block_step`.

The signal path ends at this block. No envelope, no oscillator, no further filter, no Dual Universal Slope Generator, no clip to [-1, 1], no search for the peak on the audio thread, and no k * exp(|v| / (eta * VT)). JUCE is not the framework.

## What has to match

`tests/transfer_g1.csv` stays the pre-gain acceptance test: `process` at g = 1 on vin in [-6, 6], 6,001 samples, before the 5 V drive, before `OUTPUT_GAIN`, before P(1) / P(g), before the smoother, and before the DC block.

On |a| <= 1 at g = 1, steps 5 and 6 together peak at 1 before the DC block. The raw peak on |v| <= 5 is at v = 4.707287, y = -0.228348.

At g = 2 the cell drive is ±10 V and the raw peak is 25.093 dB above the g = 1 peak. Step 6 returns that peak to 1. Between knots the same ratio stays within the relative error 0.001 locked by `DRIVE_PEAK_REL_ERROR`.

`audio_map` applies step 2, the six cells, and steps 5 and 6, with no upsampler and no halfband. With those two filters bypassed, the module matches `audio_map`, because steps 5 and 6 are linear. Odd symmetry: `audio_map(-a, g) = -audio_map(a, g)`. The DC block is linear and runs after the halfband.

The 4,096-point tests in `tests/test_transfer.py` lock the DC block at fs = 48000. One cycle in that buffer is 11.71875 Hz. After one lead-in period the peak ratio is 1.152747. The same length at 1,007.8125 Hz stays within 0.005.

`tests/test_signal.py` locks the smoother and the decimator. At fs = 48000 a step of g from 1 to 2 is within 1e-4 of 1 + (1 - exp(-1)) after 960 samples, and the four oversampled phases of one sample carry the same g. A sine at 0.45 * fs/4, with fs the 4× rate, stays within 0.01 of unity after the decimator and a 4× decimation. A sine at 0.75 * fs/4 is rejected by at least 60 dB. The measured stop probe is -69.19 dB. The cells are not in that test.

After the plugin exists, a full-scale sine at g = 1 peaks at 1 before the DC block, the pre-gain curve matches `tests/transfer_g1.csv`, g = 2 returns to peak 1 after the ratio, and the decimator test still holds inside the plugin with the cells bypassed.
