# Golden

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Locked numbers for a later agent to diff. The source is `wave_middle.py`, `tests/transfer_g1.csv`, `halfband_taps.csv`, and `upsample_taps.csv`.

## Curve at g = 1

Six decimals, from the acceptance grid.

| vin (V) | vout (V) |
| --- | --- |
| -6 | -0.574899 |
| -1 | 0.160778 |
| -0.5 | -0.136312 |
| 0 | 0 |
| 0.5 | 0.136312 |
| 1 | -0.160778 |
| 6 | 0.574899 |

Positive fold roots, first root of each cluster, four decimals: 0.7272, 1.5548, 2.4310, 3.3392, 4.2715, 5.2231.

## Level

`OUTPUT_GAIN` = 4.3792716960440945, which prints 4.379272 to six decimals.

Raw peaks of |C^6| at d(g) = g. The six-decimal column is the README figure.

| g | raw peak (V) | six decimals | dB vs g = 1 |
| --- | --- | --- | --- |
| 0.5 | 0.1775277872935283 | 0.177528 | -2.187 |
| 1 | 0.22834847193959784 | 0.228348 | 0 |
| 2 | 4.104493885791202 | 4.104494 | 25.093 |
| 4 | 13.636725832047457 | 13.636726 | 35.522 |
| 8 | 33.228577867627834 | 33.228578 | 43.258 |

The g = 1 peak is the lobe at 4.7072868603604885 V. The plateau of that peak runs from g = 0.941457 through g = 1.109989.

## DC block

fs = 48000. Pole r = exp(-2 * pi * 10 / 48000).

Folded `audio_map` buffer, 4,096 samples, one lead-in period:

| cycles | frequency (Hz) | peak ratio | deviation / peak |
| --- | --- | --- | --- |
| 1 | 11.71875 | 1.152747 | 0.390095 |
| 86 | 1,007.8125 | within 0.005 of 1 | within 0.005 |

Pure-sine steady-state magnitude of the same one-pole:

| frequency (Hz) | magnitude | three decimals |
| --- | --- | --- |
| 11.71875 | 0.761185 | 0.761 |
| 40 | 0.970778 | 0.971 |
| 100 | 0.995689 | 0.996 |

## Smoother

c at 48 kHz = 0.9989588756797245.

A step from 1 to 2, after 960 samples: 1 + (1 - exp(-1)) = 1.6321205588285577, which prints 1.632121. Tolerance 1e-4.

## Halfband

63 taps. Decimator sum 1. Upsampler sum 4. Probe frequencies use fs as the 4× rate.

| figure | value |
| --- | --- |
| pass probe, 0.45 * fs/4, magnitude | 1.000024 |
| passband deviation from 1, 0 to 0.125 cycles | within 0.0007 |
| stop probe, 0.75 * fs/4 | -69.19 dB |
| stopband peak, 0.1875 to 0.5 cycles, 8,193 points | -69.11 dB |
| half-amplitude frequency | 0.156181 cycles |
| transition magnitude at 0.1375 cycles | 0.944259 |
| largest even offset from the center tap | 0.145542 |
| Kaiser halfband comparison at the stop probe | 0.00 dB |

The pass probe through the decimator, then 4× decimation, stays within 0.01 of unity. The stop probe on that path is the -69.19 dB row. Cells are absent.
