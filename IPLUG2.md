# iPlug2 prompt

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The VST3 is `SergeMiddle/` in this repository: one audio effect in iPlug2, the middle section of the Serge Wave Multipliers. JUCE is not the framework. The map is the port of `wave_middle.py`. Do not start the Dual Universal Slope Generator in this module. The slope map is `slope/`. This VST3 stays the folder effect.

Copy the smoother coefficient and the tap files. Do not redesign them.

## Constants

Use the v1 pair already locked in `wave_middle.py`: VT = 0.02585, Is = 2.52e-9, eta = 1.68, R = 33000. Esqueda et al. 2017 Table 4 (eta = 1.752, VT = 0.025864) stays out of the plugin. The CSV is that v1 fit.

Port `lambert_w0_kexp`, `diode_node`, `cell`, and `process`. The Halley solve is on w + ln(w) - ln(z) = 0. Do not form k * exp(|v| / (eta * VT)).

Serge level: host ±1 is ±2.5 V. `OUTPUT_GAIN` is 5.632920993639031. `DRIVE_PEAKS` is the fold-amount curve next to it, 54 knots from `build_drive_peaks`. Copy that table. Do not search for the peak on the audio thread.

The smoother time is 0.020 seconds:

```text
c = exp(-1 / (0.020 * fs))
g[n] = g[n-1] + (1 - c) * (g_target - g[n-1])
```

Copy `halfband_taps.csv` (63 taps, sum 1) and `upsample_taps.csv` (those taps times 4, sum 4). The design note is in the CSV headers and in `README.md`.

## Sample path

g is the fold amount, clamped to [0.5, 8], default 1. s is the DC voltage on IN 2, in cell-input volts, clamped to [-2.5, 2.5], default 0. A non-finite s reads as 0. The host audio on IN 1 passes the 10 Hz block below before the drive, one state per channel, reset with the sample rate. Smooth g and s once per audio sample with the one-pole above. The state starts at the current target. Hold the smoothed g and the smoothed s constant across the four oversampled phases of one audio sample.

d(g) = g on that interval. The processing order is: IN 1 block, smooth g, smooth s, v = 2.5 * g * a, 4× upsample, add s, six cells, subtract C^6(s), 63-tap halfband, decimate, OUTPUT_GAIN, P(1) / P(g), 10 Hz block.

For each audio sample `a`:

1. Run the IN 1 block on `a`. The law is the 10 Hz one-pole at the bottom of this section. State starts at 0.
2. Update g with the smoother above. Clamp the knob target to [0.5, 8] before the update. Update s with the same smoother. Clamp the knob target to [-2.5, 2.5] before the update. The four oversampled phases use this same g and this same s.
3. v = 2.5 * g * a. With d(g) = g this is the drive line v = 2.5 * d(g) * a. `a` here is the blocked sample.
4. Upsample 4×: insert three zeros after the sample, then apply `upsample_taps.csv`. Add s to that high-rate sequence and evaluate the six cells on `hi + s`, where hi is the upsampler output. Subtract `process(s)`, the output of s alone, so a silent input stays silent while s moves. The cells are `process(hi + s) - process(s)`. They do not apply `OUTPUT_GAIN`. s = 0 is the locked default and is bit-identical: for every finite hi, hi + 0.0 equals hi, and process(0) is +0. With a nonzero s the pre-DC peak can exceed 1. Do not clip the output to [-1, 1]. The subtraction holds the silent input at exactly 0. On the hardware the AC-coupled output would instead show a decaying transient when s jumps.
5. Lowpass with `halfband_taps.csv` and decimate by 4, keeping phase 0 (indices 0, 4, 8, ...). Each 63-tap filter has a group delay of 31 samples at the 4× rate.
6. Multiply by `OUTPUT_GAIN`.
7. Multiply by P(1) / P(g). P(g) is linear interpolation of `DRIVE_PEAKS` in g, the function `drive_peak`. Apply that ratio as a gain, including where |y| is below the peak. Do not clip the output to [-1, 1].
8. One-pole DC block at 10 Hz, at the audio rate, after the halfband:

```text
r = exp(-2 * pi * 10 / fs)
y[n] = x[n] - x[n-1] + r * y[n-1]
```

Initial state: x[-1] = 0, y[-1] = 0. `fs` is the host sample rate. This is `dc_block_pole` and `dc_block_step`.

The signal path ends at this block. No envelope, no oscillator, no further filter, no Dual Universal Slope Generator, no clip to [-1, 1], no search for the peak on the audio thread, and no k * exp(|v| / (eta * VT)). JUCE is not the framework.

## What has to match

`tests/transfer_g1.csv` stays the pre-gain acceptance test: `process` at g = 1 on vin in [-6, 6], 6,001 samples, before the 2.5 V drive, before `OUTPUT_GAIN`, before P(1) / P(g), before the smoother, and before the DC block.

On |a| <= 1 at g = 1, `OUTPUT_GAIN` and P(1) / P(g) together peak at 1 before the output DC block. The raw peak on |v| <= 2.5 is at v = 1.954021, y = 0.177528.

At g = 2 the cell drive is ±5 V and the raw peak is 2.187 dB above the g = 1 peak. At g = 4 the cell drive is ±10 V and the raw peak is 27.280 dB above the g = 1 peak. At g = 8 the cell drive is ±20 V and the raw peak is 37.709 dB above the g = 1 peak. P(1) / P(g) returns each of those peaks to 1. Between knots the same ratio stays within the relative error 0.001 locked by `DRIVE_PEAK_REL_ERROR`.

`audio_map` applies the drive line, the IN 2 voltage, the six cells less process(s), and `OUTPUT_GAIN` times P(1) / P(g), with no IN 1 block, no upsampler, and no halfband. At s = 0, with those two filters bypassed, the module matches `audio_map`, because the gain and the ratio are linear. Odd symmetry at s = 0: `audio_map(-a, g) = -audio_map(a, g)`. s = 0 is the locked default and is bit-identical to the map with no offset. The output DC block is linear and runs after the halfband.

The 4,096-point tests in `tests/test_transfer.py` lock the DC block at fs = 48000. One cycle in that buffer is 11.71875 Hz. After one lead-in period the peak ratio is 1.109037 and the maximum deviation is 0.310004 times the pre-block peak. The same length at 1,007.8125 Hz stays within 0.005.

`tests/test_signal.py` locks the smoother and the decimator. At fs = 48000 a step of g from 1 to 2 is within 1e-4 of 1 + (1 - exp(-1)) after 960 samples, and the four oversampled phases of one sample carry the same g. A sine at 0.45 * fs/4, with fs the 4× rate, stays within 0.01 of unity after the decimator and a 4× decimation. A sine at 0.75 * fs/4 is rejected by at least 60 dB. The measured stop probe is -69.19 dB. The cells are not in that test.

After the plugin exists, a full-scale sine at g = 1 peaks at 1 before the DC block, the pre-gain curve matches `tests/transfer_g1.csv`, g = 2 returns to peak 1 after the ratio, and the decimator test still holds inside the plugin with the cells bypassed.
