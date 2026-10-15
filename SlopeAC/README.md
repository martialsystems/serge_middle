# Slope AC

Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The VST3 of the second slope half. The circuit is `slope/second.py`, ported in `dsp/SecondHalf.h`. Rise and Fall default to 5 ms. The jack equation stays `AC = 2.5 - OUT`. The sample written to the host is the coupling capacitor on that jack, `B(AC - 2.5) / 2.5`.

Cycle defaults to off. It is the switch on the later Serge GTS panel: one internal cord from the end pulse to TRIG, which is the patch a classic slope uses to cycle (Serge GTS user manual, page 5: the switch connects TRIG IN to GATE OUT). On, the plugin arms one trigger after reset and keeps that cord. Off opens the cord. With nothing patched the output sits at 0 V. The host stays on IN. An off-to-on flip arms one trigger, because END is high while the slope idles at 0 V and connecting it to TRIG is a rising edge. An on-to-off flip lets the current excursion finish, and the slope then idles. A project saved before this parameter existed loads with Cycle off, so a free-running SlopeAC in an old FL project goes silent until Cycle is switched on.

`B` is the locked 10 Hz block, one per channel, reset with the sample rate: `r = exp(-2 * pi * 10 / fs)`, `y[n] = x[n] - x[n-1] + r * y[n-1]`, state starts at 0. The block sees `AC - 2.5`. While the slope idles, OUT is 0 V and AC is 2.5 V, so the capacitor input is 0 and the first host sample is 0. The Serge GTS user manual, page 5, describes BIPOLAR OUT as an inverted version of MAIN OUT with an AC range of about -2.5 V to 2.5 V when cycling. The original DUSG had no AC output. The Random*Source and GTS versions added it. The jack equation and `slope/tests/ac_cycle.csv` stay as they are. The coupling is the plugin output, the same role as the PLEAT output block.

`ShapeExp` on Rise and Fall is only the knob mapping. The slopes themselves are straight lines. On the GTS, clockwise is faster. These knobs show time, so clockwise is slower.

The editor is the enamel plate, 416 by 624. Rise and Fall are skirted knobs. Each caption reads the time: under 10 ms as two decimals of ms (0.40 ms, 5.00 ms), under 100 ms as one decimal (12.5 ms), under 1 s as whole milliseconds (250 ms), under 10 s as two decimals of seconds (1.50 s), and from 10 s as one decimal (10.0 s). Cycle is a two-frame lever. VC is the attenuverter, from -1 to +1, default 0, so the VC jack passes nothing until the knob moves. Route is the three-position switch, RISE / BOTH / FALL, default BOTH. The rate readout shows the free-cycle rate while Cycle is on (2.50 Hz, 71.4 Hz, 714 Hz, 1.07 kHz) and OFF while it is off.

There is no patch bay on this plugin, and the audio path does not read MIDI. The host sample stays on IN in both Cycle positions. The VC jack voltage is 0 V, an unpatched jack, and the attenuverter scales that 0. The same `HostSample` call is what a patched VC voltage uses: depth +1 passes it, depth -1 inverts it, and the route switch picks the segment. TRIG hold, retrigger lockout, and the IN override live in the circuit, so a TRIG voltage passed into `Step` holds at +5 V without a DAW. Cycle's arm is one sample, so Cycle itself does not sustain.

END is the same jack law as the combined plugin, `EndJackVolts` in `dsp/SecondHalf.h`: 0 V or +5 V. +5 V while the circuit end gate is high and OUT is below 4 V, otherwise 0 V. Idle, OUT at 0 V, is +5 V. A sustain at +5 V is 0 V on the jack. The host output is still the coupled AC jack. The plate draws END next to OUT. Cycle keeps reading the circuit gate inside the slope, not this windowed jack, so a 5 ms cycle stays 480 samples.

## Layout

| Path | Role |
| --- | --- |
| `SlopeAC.cpp`, `SlopeAC.h` | iPlug2 effect: Rise, Fall, Cycle, then VC and Route, one state per channel |
| `config.h` | Name Slope AC, I/O `1-1 2-2`, latency 0, window 416 by 624 |
| `CMakeLists.txt` | iPlug2 CMake project, format VST3 |
| `SlopeView.h` | Plate, knobs, time captions, Cycle switch, rate readout |
| `dsp/Readout.h` | `FormatTime` and `FormatHz` |
| `dsp/SecondHalf.h` | Header-only port of `slope/slope.py` and `slope/second.py`. `PluginSample` stays the raw jack, AC / 2.5 |
| `dsp/AcCoupler.h` | Host coupling `B(AC - 2.5) / 2.5` |
| `dsp/tests/` | The port against `slope/tests/ac_cycle.csv`, the Cycle switch, the coupler, and the readout strings |
| `resources/` | Plate, switch, Roboto. Knob bitmaps are the PLEAT knob PNGs |

## Build

From the repository root. The DSP test does not need the VST3 SDK.

```text
bash SlopeAC/dsp/tests/run_tests.sh
cmake -S SlopeAC -B build/SlopeAC -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DIPLUG_DEPLOY_PLUGINS=OFF
cmake --build build/SlopeAC --target SlopeAC-vst3
codesign --force --sign - build/SlopeAC/out/SlopeAC.vst3
```

The bundle is `build/SlopeAC/out/SlopeAC.vst3`. The universal bundle FL Studio loads is in `BUILD.md`.
