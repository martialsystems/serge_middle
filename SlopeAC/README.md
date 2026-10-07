# Slope AC

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The VST3 of the second slope half. The circuit is `slope/second.py`, ported in `dsp/SecondHalf.h`. Rise and Fall default to 5 ms. The sample written to the host is AC / 2.5, so ±2.5 V is ±1. That scale is the output.

The editor is two knobs on a flat fill. `SergeMiddle/` is the PLEAT panel.

## Layout

| Path | Role |
| --- | --- |
| `SlopeAC.cpp`, `SlopeAC.h` | iPlug2 effect: Rise, Fall, one state per channel |
| `config.h` | Name Slope AC, I/O `1-1 2-2`, latency 0, window 320 by 180 |
| `CMakeLists.txt` | iPlug2 CMake project, format VST3 |
| `dsp/SecondHalf.h` | Header-only port of `slope/slope.py` and `slope/second.py` |
| `dsp/tests/` | The port against `slope/tests/ac_cycle.csv` |
| `resources/fonts/Roboto-Regular.ttf` | Knob labels |

## Build

From the repository root. The DSP test does not need the VST3 SDK.

```text
bash SlopeAC/dsp/tests/run_tests.sh
cmake -S SlopeAC -B build/SlopeAC -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DIPLUG_DEPLOY_PLUGINS=OFF
cmake --build build/SlopeAC --target SlopeAC-vst3
codesign --force --sign - build/SlopeAC/out/SlopeAC.vst3
```

The bundle is `build/SlopeAC/out/SlopeAC.vst3`. The universal bundle FL Studio loads is in `BUILD.md`.
