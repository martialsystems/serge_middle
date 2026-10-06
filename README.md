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

The same closed form is equation (39) of Esqueda, Pöntynen, Parker, and Bilbao, Applied Sciences 7, 1328 (2017), https://doi.org/10.3390/app7121328, evaluated at the constants above. Table 4 of that paper uses eta = 1.752 and VT = 0.025864 V.

The one-sided limits at the origin have magnitude 1.660024e-04 V, which is 2 eta VT W((Is R) / (eta VT)). The sample at vin = 0 is 0.

## Gain

g is a real multiplier on the input of the first cell. In the plugin it will be a smoothed parameter from 0.5 to 8, default 1. The map in this tree applies a constant g. The acceptance curve uses g = 1.

## Plugin

The VST3 will be built in iPlug2. JUCE is not the framework for this repository.

The module will evaluate the six cells at 4× the audio rate and downsample with a halfband. A fixed output gain and a DC block follow the halfband. The acceptance curve is the static map at g = 1, before that gain and before the DC block. The module adds no envelope, no oscillator, and no further filter.

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

For vin > 0.05 V the curve changes sign six times. The bracketing samples are 0.728 to 0.730, 1.556 to 1.558, 2.430 to 2.432, 3.340 to 3.342, 4.272 to 4.274, and 5.222 to 5.224.

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
