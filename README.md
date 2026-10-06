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

g multiplies the voltage into the first cell. In the plugin it is smoothed, from 0.5 to 8, default 1. The map in this tree applies a constant g. The acceptance curve uses g = 1.

An audio sample `a` is in [-1, 1]. Full scale is 5 V at g = 1:

```text
v = g * 5 * a
y = C(C(C(C(C(C(v))))))
u = 4.379272 * y
```

`v` and `y` are in volts. `u` is the sample before the DC block. The acceptance curve is `y` against cell-input volts on [-6, 6] at g = 1, before the factor 4.379272.

On |v| ≤ 5 the raw map peaks at v = 4.707287 V, y = -0.228348 V. The fixed gain 4.379272 is the reciprocal of that peak. A full-scale sine at g = 1 therefore peaks at 1 after the gain and before the DC block. Its mean is below 1e-9. The gain stays 4.379272 at every g, so a larger g is louder.

If full scale is 1 V, the same sine peaks at 0.161174 V and reaches only the first fold.

## Plugin

The VST3 will be built in iPlug2. JUCE is not the framework for this repository.

The module multiplies by the smoothed g, evaluates the six cells at 4× the audio rate, and downsamples with a halfband. The fixed gain and a DC block follow the halfband. The acceptance curve is the static map in cell-input volts, before that gain and before the DC block. The module adds no envelope, no oscillator, and no further filter.

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
| `wave_middle.py` | Cell, six-cell map, curve renderer |
| `tests/test_transfer.py` | Acceptance test |
| `tests/__init__.py` | Makes `tests` importable |
| `tests/transfer_g1.csv` | Curve samples, g = 1 |
| `tests/transfer_g1.svg` | Curve figure, g = 1 |
| `COPYRIGHT` | Martial Systems LLC, 2026 |

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
