# Build

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The module is not in this repository. This tree is the contract a later iPlug2 agent compiles from. It holds documents, taps, and tests.

## Files

| File | Role |
| --- | --- |
| `wave_middle.py` | Reference map: cells, Lambert W, `DRIVE_PEAKS`, smoother, FIR, DC block |
| `halfband_taps.csv` | 63 decimator coefficients, sum 1 |
| `upsample_taps.csv` | 63 upsampler coefficients, each 4 times the decimator |
| `tests/transfer_g1.csv` | Pre-gain acceptance curve, 6,001 samples, g = 1, vin in [-6, 6] |
| `tests/transfer_g1.svg` | Figure of that curve |
| `tests/test_transfer.py` | Curve, level law, DC block |
| `tests/test_signal.py` | Smoother and decimator |
| `IPLUG2.md` | Processing order and exclusions |
| `README.md` | Contract |
| `MATH.md` | Derivation |
| `GOLDEN.md` | Locked numbers |

## Tests

From the repository root:

```text
python3 -m unittest discover -s tests -t .
```

The tests use the standard library. They do not import SciPy. SciPy 1.13.1 was used once, offline, to design the taps. The CSV files are the lock.

`tests/transfer_g1.csv` and `tests/transfer_g1.svg` stay byte-identical. Do not regenerate them as part of building the plugin.

## Copy verbatim

- VT = 0.02585, Is = 2.52e-9, eta = 1.68, R = 33000
- `OUTPUT_GAIN` = 4.3792716960440945 and the 53 `DRIVE_PEAKS` knots
- `lambert_w0_kexp`: Halley on w + ln(w) - ln(z) = 0, never k * exp(|v| / (eta * VT))
- Smoother: c = exp(-1 / (0.020 * fs)), once per audio sample, state starts at the clamped target, held across the four phases
- `halfband_taps.csv` and `upsample_taps.csv`, including the factor of 4 between them
- DC block: r = exp(-2 * pi * 10 / fs), y[n] = x[n] - x[n-1] + r * y[n-1], state starts at 0
- Decimation phase 0

## The later agent may choose

The iPlug2 project name, and the plug-in window size.

## After a plugin exists

1. A full-scale sine at g = 1 peaks at 1 before the DC block.
2. The pre-gain curve matches `tests/transfer_g1.csv`.
3. g = 2 returns to peak 1 after the ratio P(1) / P(g).
4. With the cells bypassed, the decimator test in `tests/test_signal.py` still holds: the pass probe stays within 0.01 of unity, and the stop probe is rejected by at least 60 dB.
