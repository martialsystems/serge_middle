# Serge middle

PLEAT and the slope generator are the two parts of this instrument. Each one is a panel. The panels sit side by side later.

PLEAT is the middle section of the Serge Wave Multipliers, six identical cells in series, and the folder effect in `SergeMiddle/`. The slope generator is one universal slope in `slope/`, from 0 V to +5 V. The second half is the same circuit with its own state. Its AC jack is `AC = 2.5 - OUT`, and `SlopeAC/` is that half as a VST3. Rise and Fall default to 5 ms, and the host sample is AC / 2.5. The later slope panel mark is a rise and a fall, one peak.

The PLEAT map is six identical cells in series:

```text
y = C(C(C(C(C(C(g * x))))))
```

The acceptance test is that map at g = 1 for vin in [-6, 6]. Samples: `tests/transfer_g1.csv` (6,001 samples). Figure: `tests/transfer_g1.svg`. This repository is that test, the level law, the smoother, the decimator taps, the VST3 in `SergeMiddle/`, and the slope map in `slope/`.

## Cell

Constants: VT = 0.02585 V, Is = 2.52e-9 A, eta = 1.68, R = 33000 ohm.

Let z = (Is R) / (eta VT) * exp(|v| / (eta VT)), and let W be the principal Lambert W. For v ≠ 0:

```text
v_plus = sign(v) * (|v| - eta * VT * W(z))
v_out = 2 * v_plus - v = sign(v) * (|v| - 2 * eta * VT * W(z))
```

v_out(0) = 0.

The factor 2 is the combination 2 v_plus - v. At 1 V the node is still positive (0.386759 V) and the stage has folded (-0.226482 V).

The same closed form is equation (39) of Esqueda, Pöntynen, Parker, and Bilbao, Applied Sciences 7, 1328 (2017), https://doi.org/10.3390/app7121328, evaluated at the constants above. Table 4 of that paper is a different diode fit: eta = 1.752 and VT = 0.025864 V.

v1 keeps eta = 1.68 and VT = 0.02585. The CSV is that fit, and the plugin uses the same pair. Leave Table 4 out of the plugin.

The one-sided limits at the origin have magnitude 1.660024e-04 V, which is 2 eta VT W((Is R) / (eta VT)). The sample at vin = 0 is 0.

## Levels

g is the fold amount into the first cell. On the knob it runs from 0.5 to 8, default 1. In the plugin it is smoothed. This tree applies a constant g. The acceptance curve uses g = 1 and cell-input volts, before the drive map below.

An audio sample `a` is in [-1, 1]. The cell drive and the output bound are:

```text
d(g) = g
v = 5 * d(g) * a
y = C(C(C(C(C(C(v))))))
u = 4.379272 * (P(1) / P(g)) * y
```

With d(g) = g, the drive line is v = g * 5 * a. `v` and `y` are in volts. `u` is the sample before the DC block. P(g) is the peak of |y| on |v| <= 5 d(g). The fixed factor 4.379272 is OUTPUT_GAIN, the reciprocal of the g = 1 raw peak. At each stored knot, for |a| <= 1, the ratio P(1) / P(g) holds the peak of |u| at 1.

The acceptance curve is `y` against cell-input volts on [-6, 6] at g = 1, before OUTPUT_GAIN, before P(1) / P(g), before the smoother, and before the DC block.

On |v| <= 5 the raw map peaks at v = 4.707287 V, y = -0.228348 V. A full-scale sine at g = 1 therefore peaks at 1 after OUTPUT_GAIN and before the DC block. Its mean is below 1e-9.

Once the drive covers that peak (g = 0.941457), the raw peak stays 0.228348 through g = 1.109989. Past that drive the peak is the endpoint, and |y| is still climbing at 40 V.

Measured raw peaks at d(g) = g:

| g | full-scale drive (V) | raw peak (V) | level vs g = 1 (dB) |
| --- | --- | --- | --- |
| 0.5 | 2.5 | 0.177528 | -2.187 |
| 1 | 5 | 0.228348 | 0 |
| 2 | 10 | 4.104494 | 25.093 |
| 4 | 20 | 13.636726 | 35.522 |
| 8 | 40 | 33.228578 | 43.258 |

At g = 2 the raw peak is 25.093 dB above the g = 1 peak. P(1) / P(g) returns that peak to 1. The knots in `DRIVE_PEAKS` are the stored curve. Between them, linear interpolation in g keeps |P / P_hat - 1| within 0.001.

If full scale is 1 V, the same sine peaks at 0.161174 V and reaches only the first fold.

## Smoother

g is smoothed once per audio sample. The time constant is 0.020 s. The four oversampled phases of that sample use the same value.

```text
c = exp(-1 / (0.020 * fs))
g[n] = g[n-1] + (1 - c) * (g_target - g[n-1])
```

The knob target is clamped to [0.5, 8] before the update. The state starts at the current target, not at 0.

At fs = 48000, c = 0.9989588756797245. A step from 1 to 2 reaches 1 + (1 - exp(-1)) after 960 samples, which is 1.632121. The iteration at 48 kHz stays within 1e-4 of that value.

## Halfband

The downsampler is a 63-tap linear-phase lowpass, odd length, stored in `halfband_taps.csv`. The matching 4× insert-and-filter coefficients are in `upsample_taps.csv`. Each upsampler coefficient is 4 times the decimator coefficient. The decimator sum is 1 and the upsampler sum is 4. Inserting zeros scales a baseband tone by 1/4, and the factor of 4 returns that tone to unity. The plugin applies the upsampler before the cells and the decimator after them.

Frequencies in this section are cycles per sample of the 4× rate unless a sentence says otherwise. The audio Nyquist is 0.125 on that clock. The passband runs from 0 to 0.125. The stopband runs from 0.1875 to 0.5. The coefficients are a Parks-McClellan equiripple design, weights 1 and 1, scaled so the decimator sum is 1. SciPy 1.13.1 produced them. The CSV files are the lock.

On an 8,193-point grid the passband magnitude stays within 0.0007 of 1. The probe at 0.1125 cycles, 0.45 times the 4× rate divided by 4, has magnitude 1.000024. The probe at 0.1875 cycles, 0.75 times the 4× rate divided by 4, is at -69.19 dB. The stopband peak on that grid is -69.11 dB. The half-amplitude frequency is 0.156181 cycles.

A sine at the pass probe, generated at the 4× rate, filtered by the decimator, and decimated by 4, stays within 0.01 of unity on every decimation phase. A sine at the stop probe, on that same path, is rejected by 69.19 dB. The cells are not in that path.

Inserting zeros, filtering with the upsampler taps, and decimating is the linear upsampler. A sine at or below half the audio Nyquist has its first image at or above 0.1875 cycles, and that round trip stays within 0.01 of unity. The pass probe is at 0.90 times the audio Nyquist. Inserting zeros under that sine also builds an image at 0.1375 cycles, where this transition still has magnitude 0.944259. The four decimation phases of that round trip are not each within 0.01 of unity. The decimator measurement above applies the pass probe at the 4× rate, so that image is not part of the tone.

Even offsets from the center tap are not zero. The largest has absolute value 0.145542. A halfband identity would zero those offsets and cut at 0.25 cycles of the 4× rate, which is the audio sample rate, one octave above the audio Nyquist. A 63-tap Kaiser lowpass cut at 0.25 cycles, with those offsets replaced by zeros and the center tap set to 0.5, leaves the stop probe at 0.00 dB.

The acceptance curve does not go through this filter.

## DC block

The block after the bound is a one-pole highpass at 10 Hz:

```text
r = exp(-2 * pi * 10 / fs)
y[n] = x[n] - x[n-1] + r * y[n-1]
```

The state starts at x[-1] = 0 and y[-1] = 0.

At fs = 48000, a 4,096-point full-scale sine at g = 1 with one cycle in the buffer has its fundamental at 11.71875 Hz. After one lead-in period from zero state, the next 4,096 samples have peak ratio 1.152747 and a maximum absolute deviation of 0.390095 times the pre-block peak. The same length with 86 cycles, at 1,007.8125 Hz, stays within 0.005 on both the peak ratio and that deviation.

A pure sine at 11.71875 Hz has steady-state magnitude 0.761185, which reads 0.761 to three decimals. At 40 Hz the magnitude is 0.970778 (0.971). At 100 Hz it is 0.995689 (0.996). The folded buffer is a different measurement. The cutoff stays at 10 Hz.

The acceptance curve does not go through this block.

## Plugin

The VST3 is `SergeMiddle/`, an iPlug2 effect. JUCE is not the framework for this repository. The processing contract is `IPLUG2.md`. The macOS build uses Unix Makefiles.

The processing order is: smooth g, v = 5 * g * a, 4× upsample, six cells, 63-tap halfband, decimate, OUTPUT_GAIN, P(1) / P(g), 10 Hz block. With d(g) = g, the drive line is v = g * 5 * a. The acceptance curve is the static map in cell-input volts, before that gain, before the ratio, before the smoother, and before the DC block. The module adds no envelope, no oscillator, and no further filter. This VST3 is the PLEAT panel. The slope generator is the panel in `slope/`. The second half's VST3 is `SlopeAC/`.

## Curve at g = 1

On the 6,001-point grid, vout runs from -0.574899 V to 0.574899 V, at vin = -6 and vin = 6.

| vin (V) | vout (V) |
| --- | --- |
| -6 | -0.574899 |
| -1 | 0.160778 |
| -0.5 | -0.136312 |
| 0 | 0 |
| 0.5 | 0.136312 |
| 1 | -0.160778 |
| 6 | 0.574899 |

Odd symmetry holds on this grid: process(-x) + process(x) = 0.

For vin > 0.05 V the map crosses zero six times, at 0.7272, 1.5548, 2.4310, 3.3392, 4.2715, and 5.2231 V. Each value is the first root of a cluster about 2 mV wide.

## How to run

From the repository root:

```text
python3 -m unittest discover -s tests -t .
python3 -m unittest discover -s slope/tests -t .
```

Regenerate the curve after a change to the map:

```text
python3 -c "import wave_middle; wave_middle.write_acceptance()"
```

## Files

| File | Role |
| --- | --- |
| `wave_middle.py` | Cell, six-cell map, fold-amount bound, smoother, decimator, DC block, curve renderer |
| `halfband_taps.csv` | 63-tap decimator, sum 1 |
| `upsample_taps.csv` | 4× insert-and-filter taps, sum 4 |
| `SergeMiddle/` | iPlug2 VST3: one Fold knob, the port of `wave_middle.py` |
| `SlopeAC/` | iPlug2 VST3 of the second slope half: Rise, Fall, output AC / 2.5 |
| `IPLUG2.md` | Processing order and exclusions |
| `BUILD.md` | Copied constants, build command, project name, window size |
| `MATH.md` | Derivation of the locked map |
| `GOLDEN.md` | Index of the locked numbers |
| `tests/test_transfer.py` | Curve, level law, and DC block |
| `tests/test_signal.py` | Smoother and decimator |
| `tests/__init__.py` | Makes `tests` importable |
| `tests/transfer_g1.csv` | Curve samples, g = 1 |
| `tests/transfer_g1.svg` | Curve figure, g = 1 |
| `slope/slope.py` | First slope half, 0 V to +5 V |
| `slope/second.py` | Second slope half, own state, AC = 2.5 - OUT |
| `slope/README.md` | Slope equations, jacks, and the 48 kHz reference |
| `slope/tests/` | Slope acceptance, the feedback curves, the rectified envelope, and the AC cycle |
| `COPYRIGHT` | Martial Systems LLC, 2026 |

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
