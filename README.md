# Serge middle

The middle section of the Serge Wave Multipliers is six identical cells in series:

```text
y = C(C(C(C(C(C(g * x))))))
```

The acceptance test is that map at g = 1 for vin in [-6, 6]. Samples: `tests/transfer_g1.csv` (6,001 samples). Figure: `tests/transfer_g1.svg`. This repository is that test. The VST3 module comes later.

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

The acceptance curve is `y` against cell-input volts on [-6, 6] at g = 1, before OUTPUT_GAIN and before P(1) / P(g).

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

## DC block

The block after the bound is a one-pole highpass at 10 Hz:

```text
r = exp(-2 * pi * 10 / fs)
y[n] = x[n] - x[n-1] + r * y[n-1]
```

The state starts at x[-1] = 0 and y[-1] = 0.

At fs = 48000, a 4,096-point full-scale sine at g = 1 with one cycle in the buffer has its fundamental at 11.71875 Hz. After one lead-in period from zero state, the next 4,096 samples have peak ratio 1.152747 and a maximum absolute deviation of 0.390095 times the pre-block peak. The same length with 86 cycles, at 1,007.8125 Hz, stays within 0.005 on both the peak ratio and that deviation.

The acceptance curve does not go through this block.

## Plugin

The VST3 will be built in iPlug2. JUCE is not the framework for this repository. The build prompt is `IPLUG2.md`.

The module clamps g to [0.5, 8], smooths it, and holds that value across the four oversampled phases of one audio sample. Cell drive is v = 5 * d(g) * a with d(g) = g. The six cells run at 4× the audio rate, and a halfband downsamples. OUTPUT_GAIN, the ratio P(1) / P(g), and the 10 Hz DC block follow the halfband. The acceptance curve is the static map in cell-input volts, before that gain, before the ratio, and before the DC block. The module adds no envelope, no oscillator, and no further filter.

Halfband taps and the 4× upsampler coefficients are not locked in this repository. The Dual Universal Slope Generator waits until this module loads and a sine through it matches the locked curve.

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
```

Regenerate the curve after a change to the map:

```text
python3 -c "import wave_middle; wave_middle.write_acceptance()"
```

## Files

| File | Role |
| --- | --- |
| `wave_middle.py` | Cell, six-cell map, fold-amount bound, DC block, curve renderer |
| `IPLUG2.md` | Build prompt for the later module |
| `tests/test_transfer.py` | Acceptance test |
| `tests/__init__.py` | Makes `tests` importable |
| `tests/transfer_g1.csv` | Curve samples, g = 1 |
| `tests/transfer_g1.svg` | Curve figure, g = 1 |
| `COPYRIGHT` | Martial Systems LLC, 2026 |

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
