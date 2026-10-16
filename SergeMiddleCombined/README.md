# Serge Middle

`SergeMiddleCombined` is the Wave Multipliers module and two slope sections in one VST3. The plate is five panels, left to right: VCM top, PLEAT, VCM bottom, slope 1, slope 2. Cables between the live jacks are part of the audio path. `SergeMiddle/` and `SlopeAC/` stay the single-panel plugins.

Window: 2080 by 624. Format: VST3, via Unix Makefiles. The plate art holds the lettering. The code draws the live knobs, the Cycle switch, the value captions, the rate readout, the cables, and the hover labels. Top, bottom, and slope 2 are drawn on the plate, including their knobs and switches, and those controls are not in the audio path yet.

## Parameters

| Index | Name | Law |
| --- | --- | --- |
| 0 | Fold | 0.5 to 8, default 1, exponential, same knob as PLEAT |
| 1 | IN 2 DC | -2.5 V to +2.5 V, default 0, same knob as PLEAT |
| 2 | VC Depth | 0 to 1, default 0.5, linear |
| 3 | Rise | 0.0001 s to 10 s, default 5 ms, same display as Slope AC |
| 4 | Fall | 0.0001 s to 10 s, default 5 ms, same display as Slope AC |
| 5 | Cycle | off by default |
| 6 | Slope VC | -1 to +1, default 0, linear. Attenuverter in front of the slope VC jack |
| 7 | Route | 0 RISE, 1 BOTH, 2 FALL, default BOTH |

Fold and IN 2 DC are smoothed once per audio sample, shared by the left and right channels, and held across the four oversampled phases. Rise, Fall, Slope VC, and Route are read per sample. Cycle is read once per block. VC Depth is read once per block and is not smoothed. Slope VC and Route were appended after Cycle so a project saved before them restores the first six parameters and loads these two at the defaults. Default 0 and BOTH pass no slope VC, which is the unpatched jack.

## Jacks

Inputs and outputs are separate lists.

| Input | Coupling when a cable is present |
| --- | --- |
| PLEAT IN 1 | AC coupled by the 10 Hz block, then the drive |
| PLEAT IN 2 | DC coupled. Not smoothed. Own 63-tap upsampler. Clamped to ±10 V |
| PLEAT VC | Volts into the fold law below. Not smoothed |
| SLOPE IN | Volts into the slope input. No extra gain |
| SLOPE TRIG | Volts. A rising edge starts a slope. A level that has stayed high since that edge holds at +5 V until it goes low. A later pulse does not hold, and a new edge is ignored until the output is back at 0 V. A signal on SLOPE IN overrides TRIG |
| SLOPE VC | Volts into the slope time control, after the Slope VC attenuverter and the Route switch |

| Output | Voltage |
| --- | --- |
| PLEAT OUT | 2.5 times the PLEAT host sample, after the output 10 Hz block |
| SLOPE END | 0 V or +5 V. +5 V while the circuit end gate is high and slope OUT is below 4 V. Otherwise 0 V. Idle, with OUT at 0 V, is +5 V. The same `EndJackVolts` law as the standalone slope |
| SLOPE OUT | The slope sample, 0 V to +5 V |
| SLOPE AC | The 10 Hz block of (circuit AC - 2.5), in volts. Circuit AC is 2.5 - OUT |

Host ±1 is ±2.5 V, the PLEAT Serge level. An unpatched PLEAT IN 1 does not round-trip through that scale: the host sample goes straight into the same drive as the PLEAT plugin. PLEAT OUT in volts is 2.5 times the sample that also goes to the host.

Slope IN volts go straight into the slope input. There is no 5x host scaling on that jack. Slope VC volts are multiplied by the Slope VC attenuverter, then they go into the slope VC on the segment Route selects. Depth 0, the default, passes 0 V, so the jack does nothing until the knob moves. Depth +1 is the unscaled jack. Depth -1 inverts it. Route defaults to BOTH. 1V/oct stays at 0. Feedback stays at 0. The circuit is the second half.

The circuit end gate is still 0 while falling and 1 otherwise. The Cycle switch reads that gate inside the circuit. The END jack is a separate 5 V logic output with the GTS window. The Serge GTS manual, page 5, says END goes high at the end of the fall and stays high for about 80% of the rise. On a straight line from 0 V to 5 V, 80% is 4 V. A jack that stayed high for the whole rise, patched to TRIG, would meet the hold rule and sit at +5 V. The window drops the jack to 0 V once OUT reaches 4 V, so that cable cycles instead. At 48 kHz, with rise and fall at 5 ms, the END to TRIG cable repeats every 481 samples. The Cycle switch repeats every 480 samples. The cable is one sample longer: the jack goes high on the idle sample after the fall, and TRIG reads that high on the next sample.

## Normals

Zero cables is a complete instrument. A cable in an input breaks that input's normal.

| Normal | What the jack receives |
| --- | --- |
| Host audio to PLEAT IN 1 | Volts are 2.5 times the host sample. The path is the PLEAT plugin: 10 Hz IN 1 block, then the drive |
| IN 2 DC knob to PLEAT IN 2 | Smoothed, held across the four phases, and C^6(s) is subtracted. A cable replaces the knob: the voltage is DC coupled, not smoothed, upsampled by its own 63-tap filter with the same zero stuffing as the drive, added on each phase, and C^6(s) is not subtracted. A patched IN 2 is clamped to ±10 V. A non-finite cable reads as 0 |
| Slope OUT to PLEAT VC | The previous sample of slope OUT, through VC Depth. VC Depth applies to a cable on this jack as well |
| Host MIDI gate to slope TRIG | Any note held is 5 V. No notes is 0 V. The edge is the sample the message is stamped with. The sustain comes from that voltage still being high at +5 V, not from a DAW-specific hold. All-notes-off, CC 123, releases. A note-on with velocity 0 is a note-off. A TRIG cable breaks this normal |
| Cycle switch is the END to TRIG cord | The cord is on while the switch is on. Turning it on arms one trigger, including after reset while it is already on. It runs together with whatever voltage is on TRIG |
| Slope IN and slope VC are 0 V unpatched | Host audio is not normalled to slope IN |
| PLEAT OUT to the host | Always. A cable from PLEAT OUT to another jack does not mute the host |

## Routing

One cable per input. An output may feed any number of inputs. A drag from a jack to a jack always stores one output into one input. Input to input, and output to output, are refused. A new cable into an occupied input replaces the old one. Right-click a jack to unplug that jack, or to unplug all.

Every jack-to-jack transfer reads the source from the previous sample. That one-sample delay covers cables and the slope OUT normal, so a feedback patch is defined and does not depend on which jack is stepped first. Host audio into an unpatched PLEAT IN 1 is the same sample. PLEAT OUT back to the host is the same sample. The MIDI gate into an unpatched TRIG is the same sample.

The UI writes one atomic byte per input. The audio block copies those bytes once, then uses that copy for every sample in the block. Hover meters are the minimum and maximum of channel 0 over the last block. A spread above 0.05 V draws as a range, for example `SLOPE END  +0.00 .. +5.00 V`. A tighter block draws as one voltage, for example `SLOPE END  +5.00 V` at idle. Cable colour follows the input index, so unplugging one cable leaves the colours of the others. A drag that is hovering an input uses that input's colour. Otherwise the drag is red.

## VC law

This is a modelling choice. The manuals say FOLD sets the baseline as CV is added on top (La Bestia 3, page 20). They do not give a volts-per-fold figure. The law here adds the attenuated jack to the fold amount:

```text
g = clamp(g_s + depth * VC, 0.5, 8)
```

`g_s` is the smoothed Fold knob. `VC` is volts on PLEAT VC. `depth` is VC Depth, clamped to the range 0 to 1. One volt at depth 1 adds 1 to the fold amount, so a 0 V to +5 V slope moves Fold 1 up to 6. VC is not smoothed: the hardware VC takes audio. `g` is computed once per audio sample and held across the four phases. A VC of 0, or a depth of 0, returns `g_s` with no addition, so the smoother value reaches the cells unchanged. A non-finite VC or depth is treated as 0.

PLEAT OUT is then hard-clamped to ±5 V. A sample already inside that rail is left as the cell path produced it, so an unmodulated Fold matches the standalone PLEAT plugin bit for bit. The clamp is what keeps a moving fold amount on the jack: the level correction assumes a steady `g`, and the decimator still holds a few samples of the previous amount.

VC Depth is the modern-panel addition. The original 1970s VCM middle section had no VC attenuator. Current official Serge panels do: La Bestia 3, page 20, "MID AU", and the Random*Source VCM.

## MIDI

The gate is a bitmask of note numbers. A second note-on does not count twice. A second note-off does not step below zero. Messages are queued from ProcessMidiMsg and applied inside the block at their sample offset.

FL Studio does not send notes to an effect until the wrapper has a MIDI input port. Open the plugin wrapper on the mixer slot, or on the channel, and set MIDI input port.

Reset clears the MIDI queue and releases every held note. A transport stop that resets the plugin cannot leave TRIG stuck high. The slope trigger memory is cleared with the channels, so the next note is a new edge.

## State

The chunk is the parameter block, then nine routing bytes: magic 0x52, version 1, input count 6, then one source byte per input in the input order above. 255 means unpatched. A source of 0, 1, 2, or 3 is PLEAT OUT, END, slope OUT, or slope AC. A tail shorter than nine bytes, a bad magic, a bad version, a bad count, or a source outside those outputs leaves every input unpatched. A longer tail is accepted: those nine bytes are read, and anything after them stays in the chunk. If the UI is open, it reads the routing again after the chunk loads.

## Build

From the repository root. This configure does not copy the bundle into the user VST3 folder:

```text
cmake -S SergeMiddleCombined -B build/SergeMiddleCombined -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DIPLUG_DEPLOY_PLUGINS=OFF
cmake --build build/SergeMiddleCombined --target SergeMiddleCombined-vst3
bash SergeMiddleCombined/dsp/tests/run_tests.sh
```

The universal FL Studio bundle is in `BUILD.md`.

## Limits

A TRIG voltage that has stayed high since the edge that started the rise holds the output at +5 V until the voltage goes low. A pulse that arrives later, including one that lands on the sample the rise finishes, does not hold. The END jack is 0 V for that hold, because OUT is not below 4 V. A voltage that has already gone low, including one that drops before OUT passes 4 V, does not hold: the rise finishes and the fall starts. The 4 V mark is only the END jack window. It is exact on the 5 ms stair at 48 kHz. A 4.5 V compare is not, because 5 * (216 / 240) is not a binary 4.5. A positive SLOPE IN overrides the TRIG hold and the slope slews to that input.

The host is told 16 samples of latency, the same figure as the PLEAT plugin. The two 63-tap filters delay 31 + 31 samples at the four-times rate, which is 15.5 audio samples.

Slope IN other than 0 V takes the input path and does not also inject the TRIG edge. Cycle can still arm, and the circuit ignores that edge while the input is away from 0 V.

## Gaps

There is no 1V/OCT jack. The VCM top and bottom sections, and the second slope, are drawn on the plate and are not in the audio path yet. One input takes one cable: several cables are not stacked on one input. Host audio reaches slope IN only by a cable, with no host sidechain. The editor was not host-tested by the agent. The standalone slope has the same attenuverter and route switch, and its VC jack voltage is 0 V because that plugin has no patch bay.

Copyright (c) 2026 Martial Systems LLC. All rights reserved.
