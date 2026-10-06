# iPlug2 prompt

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Build one VST3 audio effect in iPlug2, the middle section of the Serge Wave Multipliers. JUCE is not the framework. Port the map from `wave_middle.py`. Do not start the Dual Universal Slope Generator in this module. It waits until this module loads and a sine through it matches the locked curve.

## Constants

Use the v1 pair already locked in `wave_middle.py`: VT = 0.02585, Is = 2.52e-9, eta = 1.68, R = 33000. Esqueda et al. 2017 Table 4 (eta = 1.752, VT = 0.025864) stays out of the plugin. The CSV is that v1 fit.

Port `lambert_w0_kexp`, `diode_node`, `cell`, and `process`. The Halley solve is on w + ln(w) - ln(z) = 0. Do not form k * exp(|v| / (eta * VT)).

`OUTPUT_GAIN` is 4.3792716960440945. `DRIVE_PEAKS` is the fold-amount curve next to it. Copy that table. Do not search for the peak on the audio thread.

## Sample path

g is the fold amount, clamped to [0.5, 8], default 1. Smooth it at the audio rate. The smoothing coefficient is not locked in this repository. Hold the smoothed g constant across the four oversampled phases of one audio sample.

d(g) = g on that interval.

For each audio sample `a`:

1. v = 5 * d(g) * a.
2. Upsample 4× and evaluate the six cells on that sequence. The cells are `process(v)` with the drive already inside v. They do not apply `OUTPUT_GAIN`.
3. Downsample with a halfband.
4. Multiply by `OUTPUT_GAIN`.
5. Multiply by P(1) / P(g). P(g) is linear interpolation of `DRIVE_PEAKS` in g, the function `drive_peak`. Apply that ratio as a gain, including where |y| is below the peak. Do not clip the output to [-1, 1].
6. One-pole DC block at 10 Hz, at the audio rate, after the halfband:

```text
r = exp(-2 * pi * 10 / fs)
y[n] = x[n] - x[n-1] + r * y[n-1]
```

Initial state: x[-1] = 0, y[-1] = 0. `fs` is the host sample rate. This is `dc_block_pole` and `dc_block_step`.

The signal path ends at this block. No envelope, no oscillator, and no further filter.

Halfband taps, and the coefficients of the 4× upsampler, are chosen when the module is built. This repository does not lock them.

## What has to match

`tests/transfer_g1.csv` stays the pre-gain acceptance test: `process` at g = 1 on vin in [-6, 6], 6,001 samples, before the 5 V drive, before `OUTPUT_GAIN`, before P(1) / P(g), and before the DC block.

On |a| <= 1 at g = 1, steps 4 and 5 together peak at 1 before the DC block. The raw peak on |v| <= 5 is at v = 4.707287, y = -0.228348.

At g = 2 the cell drive is ±10 V and the raw peak is 25.093 dB above the g = 1 peak. Step 5 returns that peak to 1. Between knots the same ratio stays within the relative error 0.001 locked by `DRIVE_PEAK_REL_ERROR`.

`audio_map` is steps 1, 4, and 5 with no oversampling. With the upsampler and the halfband bypassed, the module matches `audio_map`, because steps 4 and 5 are linear. Odd symmetry: `audio_map(-a, g) = -audio_map(a, g)`. The DC block is linear and runs after the halfband.

The 4,096-point tests in `tests/test_transfer.py` lock the DC block at fs = 48000. One cycle in that buffer is 11.71875 Hz. After one lead-in period the peak ratio is 1.152747. The same length at 1,007.8125 Hz stays within 0.005.
