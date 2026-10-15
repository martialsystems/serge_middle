# Patch playground

Headless check of the Serge Middle DSP. A small C++ program renders one named patch through the real cells, slope, and combined patch, and writes every jack voltage as CSV. `run.py` renders the library, draws a scope plot of each jack, and fails when a trace drifts or a Serge rule breaks.

The DSP headers are included as they are. This tool does not change their behavior.

## Run

From the repository root, with CMake, a C++17 compiler, and the packages in `requirements.txt` (numpy and matplotlib):

```text
python3 -m pip install -r tools/playground/requirements.txt
python3 tools/playground/run.py
```

Plots and the HTML index land in `tools/playground/out/` (gitignored). The index links every scope. On Linux the runner configures CMake with `g++` when `CXX` is unset, because a `c++` that is Clang without libstdc++ cannot link.

One patch, or one engine by hand:

```text
python3 tools/playground/run.py --patch trig_sustain
build/playground/playground --patch tools/playground/patches/trig_sustain.json --out /tmp/trig.csv
```

`--sample-rate` overrides the rate in the file. The same sources build on Linux, macOS, and Windows. No iPlug2 and no plugin SDK.

## What a patch file is

JSON in `tools/playground/patches/`. The runner ignores keys it does not check, and the C++ program ignores keys it does not render (`checks`, `note`, `golden_abs_tol`, `known_failures`, `sample_rates`).

| Field | Role |
| --- | --- |
| `engine` | `slope` (SlopeAC `SecondHalf` and `AcCoupler`), `pleat` (`WaveMiddleChannel`), or `patch` (`PatchChannel`) |
| `sample_rate` | Hz for a single render. `sample_rates` is a list the runner expands |
| `duration_ms` | Length of the render |
| `params` | Knobs. A number is constant. `[[time_ms, value], ...]` is a linear ramp |
| `signals` | `host`, `in`, `trig`, `vc`, and `midi` |
| `cables` | `{"from": "slope_end", "to": "slope_trig"}` on the patch engine |
| `segments` | Time ranges that override params and reset the DSP at the start |

Host audio is in host units, where ±1 is ±2.5 V. Slope `in`, `trig`, and `vc` are in volts. Signal types are `silence`, `dc`, `sine`, `saw`, `gate`, `pulses`, and MIDI `notes`. Gates and pulses take `spans_samples` / `at_samples` or the same fields in milliseconds.

Knobs: `fold` (default 1), `in2` (0 V), `rise_s` and `fall_s` (0.005 s), `vc_depth` (0, the slope attenuverter), `pleat_vc_depth` (0), `route` (`rise`, `both`, `fall`), `cycle` (false).

CSV columns, after `sample` and `time_ms`:

| Engine | Jacks |
| --- | --- |
| slope | `in`, `trig`, `vc`, `out`, `end`, `ac`, `ac_coupled` |
| pleat | `in1`, `in2`, `out` |
| patch | `pleat_in1`, `pleat_in2`, `pleat_vc`, `slope_in`, `slope_trig`, `slope_vc`, `pleat_out`, `slope_end`, `slope_out`, `slope_ac` |

`ac` is the circuit jack `2.5 - OUT`. `ac_coupled` and the patch `slope_ac` jack are the 10 Hz block of `AC - 2.5`, in volts. Pleat `in1` and `out` are in volts (2.5 times the host sample).

## Checks

Every render checks that every sample is finite. Slope and patch renders check the END jack (only 0 V or +5 V, +5 V when OUT is 0, 0 V once OUT reaches 4 V) and `AC = 2.5 - OUT`. Output jacks are checked against the Serge rails: slope OUT is 0 V to +5 V, and the other outputs are within ±5 V.

A patch adds its own checks under `checks`. The types are `hold`, `oneshot`, `period`, `peak_offset`, `peak_abs`, `mean_abs`, `silent`, `not_silent`, `asymmetry`, `more_peaks`, `windows_differ`, and `matches_other`. `period` accepts a sample count or a map keyed by sample rate. Put the expected numbers from the slope law in the JSON (`round` half to even, then the Cycle switch period is rise + fall samples; an END→TRIG cable is one sample longer).

`known_failures` maps a check id to a sentence. A listed check that still fails is reported and does not fail the run. A listed check that starts passing fails the run, so the mark gets removed.

## Golden traces

`tools/playground/golden/<name>_<rate>.csv` is the locked render. Comparison is the maximum absolute voltage error, against `golden_abs_tol` in the patch (otherwise 1e-5 V). Slope stairs that are exact integer ratios use 1e-9 V. Patches that run the cells or the AC coupler use 1e-6 V or 1e-5 V.

That gap is for libm. On this machine the locked `sine_envelope.csv` already disagrees with a fresh `math.sin` by 1 ULP on 17 samples and 2 ULP on 2 samples, and `tests/transfer_g1.csv` disagrees with `wave_middle.acceptance_csv()` by 12 ULP at vin = ±0.808. A one-sample slope error is about 20 mV, so these tolerances still catch a real change.

Regenerate after an intentional DSP change:

```text
python3 tools/playground/run.py --update-golden
```

Review the scope plots, then commit the CSVs.

## Adding a patch

1. Add `tools/playground/patches/<name>.json`. Keep the render short. A few tens of milliseconds is enough for a slope cycle or a sine.
2. Give it a `note`, the stimulus, and checks that state the rule in numbers (a period, a hold at +5 V, a peak, a mean).
3. Set `golden_abs_tol`. Use 1e-9 V for an exact slope stair and 1e-5 V when the cells or `exp`/`pow` are in the path.
4. Run `python3 tools/playground/run.py --patch <name> --update-golden`.
5. Open `tools/playground/out/index.html`, look at the plot, and commit the patch and `golden/<name>_<rate>.csv`.

If a check fails because the current DSP does not meet the rule, add `known_failures` with the measured value and the reason. Leave the DSP alone unless the change is the point of the PR.

## CI

`.github/workflows/playground.yml` runs the same command on Ubuntu and macOS and uploads `tools/playground/out/` (the HTML index and the PNGs).

## Starter library

| Patch | What it locks |
| --- | --- |
| `pleat_fold_sweep` | Fold from 0.5 to 8 under a sine. Late cycles have more peaks |
| `pleat_in2_dc` | IN 2 DC shifts the fold. After the 10 Hz block settles, the mean is near 0 |
| `pleat_silence` | A silent input stays at 0 V while IN 2 is +1.5 V |
| `pleat_fullscale_sine` | 1 kHz full-scale sine at 44.1, 48, and 96 kHz. Peak near ±2.5 V, mean near 0 |
| `pleat_saw` | Full-scale saw through Fold 1.5, AC-coupled |
| `slope_cycles` | Cycle at 5 ms and at 0.4/1.0 ms, three sample rates. Coupled AC mean near 0 |
| `trig_sustain` | TRIG held through the rise holds OUT at +5 V |
| `trig_no_retrigger` | Later pulses do not restart the cycle and do not hold |
| `vc_routing` | Depth 0 ignores +1 V. +1 shortens, −1 lengthens. RISE and FALL each move one segment |
| `end_to_trig` | END cabled to TRIG. Period is one sample longer than the Cycle switch |
| `slope_into_fold` | No cables, slope OUT normalled to PLEAT VC. Depth 0 matches PLEAT. Depth 1 changes the audio |
| `midi_note_gate` | A held note is +5 V on TRIG and sustains. A later note does not retrigger |

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
