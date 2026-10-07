# Golden

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Locked numbers for a later agent to diff. The source is `wave_middle.py`, `tests/transfer_g1.csv`, `halfband_taps.csv`, `upsample_taps.csv`, `slope/slope.py`, `slope/tests/feedback_p0_5.csv`, and `slope/tests/feedback_m0_5.csv`.

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

## Slope

One half, `slope/slope.py`. fs = 48,000. OUT is 0 V to +5 V. Reference knobs: rise = fall = 0.005 s. VC adds 0.001 s/V to the switch selection. 1V/oct uses `2 ** V_1v` on the rate. Feedback `a` enters the same exponent as `a * v`.

| figure | value |
| --- | --- |
| equal 5 ms period | 480 samples |
| equal 5 ms END high | 240 samples, duty 1/2 |
| fall 0.010 s period | 720 samples |
| fall 0.010 s END high | 240 samples, duty 1/3 |
| trigger train, 14,400 samples, 300 Hz | 90 triggers, 30 end-pulses |
| gate 3 V for 960 samples | OUT is 3 V from sample 144 through 959 |
| gate release | sample 961 is below 3 V |
| +1 V on 1V/oct | period 240 samples |
| VC +1 V, switch RISE | period 528 samples, END high 288 |
| VC +1 V, switch FALL | period 528 samples, END high 240 |
| VC +1 V, switch BOTH | period 576 samples, END high 288 |
| linear sample of emitted +5 V | 240 |
| linear OUT at sample 60 | 1.25 V |
| a = +0.5, samples through emitted +5 V | 116, +5 V on sample 115 |
| a = +0.5, OUT at sample 60 | 1.6279456556769976 V |
| a = -0.5, samples through emitted +5 V | 646, +5 V on sample 645 |
| a = -0.5, OUT at sample 60 | 1.0411407626453677 V |
| sha1 `feedback_p0_5.csv` | 0230a301a13c9a1d6ad6339ff2e9f920bda13454 |
| sha1 `feedback_m0_5.csv` | 764cfacb2e1b2dcfc72ea019275030594a3bad2b |
