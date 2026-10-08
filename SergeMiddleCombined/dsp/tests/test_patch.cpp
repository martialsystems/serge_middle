// Combined patch: PLEAT bitwise path, slope normal, cables, state.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "../Patch.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace
{
int gFailures = 0;

void Check(bool ok, const char* what)
{
  if (ok)
  {
    std::printf("ok    %s\n", what);
    return;
  }
  gFailures++;
  std::printf("FAIL  %s\n", what);
}

double Noise(uint32_t& state)
{
  state = state * 1664525u + 1013904223u;
  const double unit = static_cast<double>(state >> 8) * (1.0 / static_cast<double>(1u << 24));
  return unit * 2.0 - 1.0;
}

bool RoutingClear(const patch::Routing& routing)
{
  for (int i = 0; i < patch::kNumInputs; ++i)
  {
    if (routing.source[i] != -1)
      return false;
  }
  return true;
}

void TestZeroCablesMatchPleat()
{
  constexpr double kFs = 48000.0;
  constexpr int kN = 48000 * 2;
  const double pi = std::acos(-1.0);
  const double depths[] = {0.0, 0.5, 1.0};
  bool all = true;
  int missAt = -1;
  double missDepth = 0.0;
  double missA = 0.0;
  double missB = 0.0;

  for (double depth : depths)
  {
    patch::PatchChannel left;
    patch::PatchChannel right;
    serge::WaveMiddleChannel reference;
    serge::FoldSmoother fold;
    serge::SymmetrySmoother symmetry;
    left.Reset(kFs);
    right.Reset(kFs);
    reference.Reset(kFs);
    fold.Reset(1.0, kFs);
    symmetry.Reset(0.0, kFs);
    patch::Routing routing;
    uint32_t rng = 1;
    for (int n = 0; n < kN; ++n)
    {
      const double t = static_cast<double>(n) / kFs;
      const double g = fold.Step(1.0 + 3.0 * std::sin(2.0 * pi * 0.2 * t));
      const double s = symmetry.Step(1.5 * std::sin(2.0 * pi * 0.15 * t));
      const double a = 0.25 * std::sin(2.0 * pi * 440.0 * t) + 0.05 * Noise(rng);
      const double yL = left.Step(a, 0.0, g, s, depth, 0.005, 0.005, false, false, routing);
      const double yR = right.Step(a, 0.0, g, s, depth, 0.005, 0.005, false, false, routing);
      const double y = reference.Step(a, g, s);
      if (yL != y || yR != yL)
      {
        all = false;
        missAt = n;
        missDepth = depth;
        missA = yL;
        missB = y;
        break;
      }
    }
    if (!all)
      break;
  }

  Check(all, "zero cables match WaveMiddleChannel for 2 s, both channels, three depths");
  if (!all)
    std::printf("  depth %.1f sample %d patch %.17g pleat %.17g\n", missDepth, missAt, missA, missB);
  std::printf("measure zero-cable samples %d depths 0 0.5 1 match %d\n", kN, all ? 1 : 0);
}

void TestVcNormal()
{
  constexpr double kFs = 48000.0;
  constexpr int kN = 2000;
  const double gS = 1.0;
  const double depth = 1.0;
  patch::PatchChannel channel;
  patch::PatchChannel flat;
  serge::WaveMiddleChannel reference;
  channel.Reset(kFs);
  flat.Reset(kFs);
  reference.Reset(kFs);
  patch::Routing routing;
  bool law = true;
  bool depth0 = true;
  bool moved = false;
  int lawMiss = -1;
  double shownG = 0.0;
  double shownVc = 0.0;
  double prevOut = 0.0;

  for (int n = 0; n < kN; ++n)
  {
    const bool arm = n == 0;
    const double host = 0.2;
    const double y = channel.Step(host, 0.0, gS, 0.0, depth, 0.005, 0.005, true, arm, routing);
    const double y0 = flat.Step(host, 0.0, gS, 0.0, 0.0, 0.005, 0.005, true, arm, routing);
    const double expect = reference.Step(host, gS, 0.0);
    const double vc = prevOut;
    const double g = patch::FoldWithVc(gS, depth, vc);
    if (channel.AppliedG() != g || channel.InputVolts(patch::PleatVc) != vc)
    {
      law = false;
      if (lawMiss < 0)
        lawMiss = n;
    }
    if (y0 != expect || flat.AppliedG() != gS)
      depth0 = false;
    if (vc != 0.0 && y != y0)
      moved = true;
    if (vc > 1.0 && shownVc == 0.0)
    {
      shownVc = vc;
      shownG = channel.AppliedG();
    }
    prevOut = channel.OutputVolts(patch::SlopeOut);
  }

  Check(law, "VC normal uses previous slope OUT in FoldWithVc");
  Check(depth0, "depth 0 matches the zero-cable PLEAT path bitwise");
  Check(moved, "depth 1 changes the host sample once slope OUT is nonzero");
  if (!law)
    std::printf("  law miss at %d\n", lawMiss);
  std::printf("measure vc-normal prev_out %.17g applied_g %.17g\n", shownVc, shownG);
}

void TestMidiGate()
{
  constexpr double kFs = 48000.0;
  patch::PatchChannel channel;
  channel.Reset(kFs);
  patch::Routing routing;
  int firstFive = -1;
  double at959 = -1.0;
  double midFall = -1.0;
  double after = -1.0;
  bool endLowWhileHeld = true;
  int heldSamples = 0;
  for (int n = 0; n < 960 + 480; ++n)
  {
    const double gate = n < 960 ? patch::kGateVolts : 0.0;
    channel.Step(0.0, gate, 1.0, 0.0, 0.0, 0.005, 0.005, false, false, routing);
    const double out = channel.OutputVolts(patch::SlopeOut);
    const double end = channel.OutputVolts(patch::SlopeEnd);
    if (firstFive < 0 && out == 5.0)
      firstFive = n;
    if (n == 959)
      at959 = out;
    if (n == 1100)
      midFall = out;
    if (n == 960 + 480 - 1)
      after = out;
    if (out == 5.0 && n < 960)
    {
      ++heldSamples;
      if (end != 0.0)
        endLowWhileHeld = false;
    }
  }
  Check(at959 == 5.0 && heldSamples > 0 && endLowWhileHeld,
        "960-sample gate holds slope OUT at 5 V and END stays 0 V");
  Check(midFall > 0.0 && midFall < 5.0, "release is falling through the middle");
  Check(after == 0.0, "release reaches 0 V");

  patch::PatchChannel blocked;
  blocked.Reset(kFs);
  patch::Routing cable;
  cable.Connect(patch::SlopeOut, patch::SlopeTrig);
  bool stayed = true;
  for (int n = 0; n < 960; ++n)
  {
    blocked.Step(0.0, patch::kGateVolts, 1.0, 0.0, 0.0, 0.005, 0.005, false, false, cable);
    if (blocked.OutputVolts(patch::SlopeOut) != 0.0)
      stayed = false;
  }
  Check(stayed, "TRIG cable at 0 V breaks the MIDI gate");
  std::printf("measure gate first_5V %d at_959 %.17g mid_fall %.17g after %.17g blocked %d\n",
              firstFive, at959, midFall, after, stayed ? 1 : 0);
}

void TestCables()
{
  constexpr double kFs = 48000.0;
  patch::Routing replaced;
  Check(replaced.Connect(patch::SlopeOut, patch::PleatIn2), "connect slope OUT to IN 2");
  Check(replaced.Connect(patch::SlopeAc, patch::PleatIn2), "replace the cable on IN 2");
  Check(replaced.source[patch::PleatIn2] == patch::SlopeAc, "occupied input keeps the new source");
  const int8_t before = replaced.source[patch::PleatIn2];
  Check(!patch::PatchJacks(replaced, false, patch::PleatIn1, false, patch::PleatVc), "input to input is refused");
  Check(!patch::PatchJacks(replaced, true, patch::PleatOut, true, patch::SlopeOut), "output to output is refused");
  Check(replaced.source[patch::PleatIn2] == before, "a refused drag leaves the routing");

  patch::PatchChannel knob;
  patch::PatchChannel cabled;
  knob.Reset(kFs);
  cabled.Reset(kFs);
  patch::Routing plain;
  patch::Routing in2;
  in2.Connect(patch::SlopeOut, patch::PleatIn2);
  bool in2Differs = false;
  for (int n = 0; n < 2000; ++n)
  {
    const bool arm = n == 0;
    const double a = knob.Step(0.15, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, true, arm, plain);
    const double b = cabled.Step(0.15, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, true, arm, in2);
    if (a != b)
      in2Differs = true;
  }
  Check(in2Differs, "slope OUT into IN 2 is not the knob path");

  patch::PatchChannel hostA;
  patch::PatchChannel hostB;
  patch::PatchChannel openHost;
  hostA.Reset(kFs);
  hostB.Reset(kFs);
  openHost.Reset(kFs);
  patch::Routing in1;
  in1.Connect(patch::SlopeOut, patch::PleatIn1);
  hostA.PrimeOutput(patch::SlopeOut, 1.25);
  hostB.PrimeOutput(patch::SlopeOut, 1.25);
  const double patchedLow = hostA.Step(-0.4, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, false, false, in1);
  const double patchedHigh = hostB.Step(0.2, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, false, false, in1);
  const double openLow = openHost.Step(-0.4, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, false, false, plain);
  Check(patchedLow == patchedHigh, "a cable into IN 1 ignores the host sample");
  Check(patchedLow != openLow, "the same host without the cable takes a different path");

  patch::PatchChannel delayed;
  serge::WaveMiddleChannel direct;
  delayed.Reset(kFs);
  direct.Reset(kFs);
  patch::Routing acToIn1;
  acToIn1.Connect(patch::SlopeAc, patch::PleatIn1);
  bool delayMatch = true;
  bool anyAc = false;
  int delayMiss = -1;
  double prevAc = 0.0;
  double peakAc = 0.0;
  for (int n = 0; n < 4000; ++n)
  {
    const double host = n == 0 ? 0.8 : 0.0;
    const double y = delayed.Step(host, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, true, n == 0, acToIn1);
    const double a = n == 0 ? 0.0 : prevAc / serge::kAudioFullScaleVolts;
    const double expect = direct.Step(a, 1.0, 0.0);
    if (y != expect)
    {
      delayMatch = false;
      if (delayMiss < 0)
        delayMiss = n;
    }
    const double ac = delayed.OutputVolts(patch::SlopeAc);
    if (ac != 0.0)
      anyAc = true;
    peakAc = std::fabs(ac) > peakAc ? std::fabs(ac) : peakAc;
    prevAc = ac;
  }
  Check(delayMatch && anyAc, "slope AC into IN 1 matches PLEAT on the delayed sequence");
  if (!delayMatch)
    std::printf("  delay miss at %d\n", delayMiss);

  auto firstIn2 = [](double primed) {
    patch::PatchChannel channel;
    channel.Reset(kFs);
    patch::Routing routing;
    routing.Connect(patch::SlopeOut, patch::PleatIn2);
    channel.PrimeOutput(patch::SlopeOut, primed);
    return channel.Step(0.2, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, false, false, routing);
  };
  const double at12 = firstIn2(12.0);
  const double at10 = firstIn2(10.0);
  const double at9 = firstIn2(9.0);
  Check(at12 == at10, "IN 2 cable clamps 12 V to the same sample as 10 V");
  Check(at9 != at10, "IN 2 cable of 9 V is not the 10 V sample");

  patch::PatchChannel loop;
  loop.Reset(kFs);
  patch::Routing feedback;
  feedback.Connect(patch::PleatOut, patch::SlopeIn);
  bool finite = true;
  double peak = 0.0;
  const double pi = std::acos(-1.0);
  for (int n = 0; n < 48000 * 10; ++n)
  {
    const double t = static_cast<double>(n) / kFs;
    const double host = 0.3 * std::sin(2.0 * pi * 110.0 * t);
    const double y = loop.Step(host, 0.0, 1.5, 0.2, 0.5, 0.005, 0.005, false, false, feedback);
    if (!std::isfinite(y))
      finite = false;
    for (int i = 0; i < patch::kNumOutputs; ++i)
    {
      if (!std::isfinite(loop.OutputVolts(i)))
        finite = false;
    }
    peak = std::fabs(y) > peak ? std::fabs(y) : peak;
  }
  Check(finite, "PLEAT OUT into slope IN stays finite for 10 s");
  std::printf("measure in2_differs %d delay_match %d any_ac %d peak_ac %.6g clamp12_eq_10 %d nine_differs %d feedback_peak %.6g\n",
              in2Differs ? 1 : 0, delayMatch ? 1 : 0, anyAc ? 1 : 0, peakAc, at12 == at10 ? 1 : 0, at9 != at10 ? 1 : 0, peak);
}

void TestSerialize()
{
  patch::Routing routing;
  routing.Connect(patch::PleatOut, patch::PleatIn1);
  routing.Connect(patch::SlopeOut, patch::SlopeTrig);
  routing.Connect(patch::SlopeAc, patch::SlopeVc);
  std::vector<uint8_t> bytes{1, 2, 3};
  patch::SerializeRouting(routing, bytes);
  Check(bytes.size() == static_cast<size_t>(patch::kRoutingBytes), "routing blob is 9 bytes");
  Check(bytes.size() >= 3 && bytes[0] == patch::kRoutingMagic && bytes[1] == patch::kRoutingVersion && bytes[2] == patch::kNumInputs,
        "magic, version, count");
  patch::Routing back;
  back.Connect(patch::SlopeEnd, patch::SlopeIn);
  Check(patch::ParseRouting(bytes.data(), bytes.size(), back), "round trip parses");
  bool same = true;
  for (int i = 0; i < patch::kNumInputs; ++i)
    same = same && back.source[i] == routing.source[i];
  Check(same, "round trip keeps each source");

  auto reject = [](const uint8_t* data, size_t size, const char* what) {
    patch::Routing dirty;
    dirty.Connect(patch::PleatOut, patch::PleatIn1);
    dirty.Connect(patch::SlopeOut, patch::PleatVc);
    const bool parsed = patch::ParseRouting(data, size, dirty);
    Check(!parsed && RoutingClear(dirty), what);
  };

  uint8_t badMagic[9] = {0x00, 1, 6, 0, 255, 255, 255, 255, 255};
  uint8_t badVersion[9] = {0x52, 2, 6, 0, 255, 255, 255, 255, 255};
  uint8_t badCount[9] = {0x52, 1, 5, 0, 255, 255, 255, 255, 255};
  uint8_t badIndex[9] = {0x52, 1, 6, 4, 255, 255, 255, 255, 255};
  uint8_t shortBlob[8] = {0x52, 1, 6, 0, 255, 255, 255, 255};
  reject(badMagic, 9, "bad magic leaves every input unpatched");
  reject(badVersion, 9, "bad version leaves every input unpatched");
  reject(badCount, 9, "wrong count leaves every input unpatched");
  reject(badIndex, 9, "source 4 leaves every input unpatched");
  reject(shortBlob, 8, "short blob leaves every input unpatched");
  reject(nullptr, 9, "null bytes leave every input unpatched");

  uint8_t open[9] = {0x52, 1, 6, 255, 255, 255, 255, 255, 255};
  patch::Routing cleared;
  cleared.Connect(patch::SlopeOut, patch::SlopeIn);
  Check(patch::ParseRouting(open, 9, cleared) && RoutingClear(cleared), "0xFF is unpatched");
}

void TestMeters()
{
  constexpr double kFs = 48000.0;
  patch::PatchChannel channel;
  channel.Reset(kFs);
  patch::Routing routing;
  patch::BlockMeter meter;
  meter.Begin();
  const double hosts[] = {0.0, 0.5, -0.25};
  for (double host : hosts)
  {
    channel.Step(host, 0.0, 1.0, 0.0, 0.5, 0.005, 0.005, false, false, routing);
    meter.Observe(patch::MeterOf(false, patch::PleatIn1), channel.InputVolts(patch::PleatIn1));
    meter.Observe(patch::MeterOf(true, patch::PleatOut), channel.OutputVolts(patch::PleatOut));
    meter.Observe(patch::MeterOf(true, patch::SlopeOut), channel.OutputVolts(patch::SlopeOut));
  }
  const int in1 = patch::MeterOf(false, patch::PleatIn1);
  const int out = patch::MeterOf(true, patch::PleatOut);
  const int slope = patch::MeterOf(true, patch::SlopeOut);
  Check(meter.min[in1] == serge::kAudioFullScaleVolts * -0.25, "IN 1 meter min is -0.625 V");
  Check(meter.max[in1] == serge::kAudioFullScaleVolts * 0.5, "IN 1 meter max is 1.25 V");
  Check(meter.min[slope] == 0.0 && meter.max[slope] == 0.0, "idle slope OUT meter is 0 V");
  Check(meter.seen[out], "PLEAT OUT meter saw the block");
  std::printf("measure meter in1 %.6g .. %.6g pleat_out %.6g .. %.6g\n",
              meter.min[in1], meter.max[in1], meter.min[out], meter.max[out]);
}

void TestGateNotes()
{
  patch::GateNotes notes;
  Check(notes.Volts() == 0.0, "no notes is 0 V");
  notes.NoteOn(60);
  notes.NoteOn(60);
  Check(notes.Count() == 1 && notes.Volts() == 5.0, "a repeated note-on stays one note at 5 V");
  notes.NoteOn(64);
  notes.NoteOff(60);
  Check(notes.Count() == 1 && notes.Volts() == 5.0, "one of two notes still holds");
  notes.NoteOff(64);
  notes.NoteOff(64);
  Check(notes.Count() == 0 && notes.Volts() == 0.0, "a repeated note-off stays at 0");
  notes.NoteOn(0);
  notes.NoteOn(127);
  notes.AllOff();
  Check(notes.Count() == 0 && notes.Volts() == 0.0, "all notes off releases");
  notes.NoteOn(-1);
  notes.NoteOn(128);
  Check(notes.Count() == 0, "notes outside 0..127 are ignored");
}

// Spacing of samples where slope OUT is exactly +5 V. -1 if the gaps differ.
int PeakSpacing(bool cable, int& first, int& count)
{
  constexpr double kFs = 48000.0;
  patch::PatchChannel channel;
  channel.Reset(kFs);
  patch::Routing routing;
  if (cable)
    routing.Connect(patch::SlopeEnd, patch::SlopeTrig);
  int prev = -1;
  int spacing = -1;
  bool stable = true;
  count = 0;
  first = -1;
  for (int n = 0; n < 480 * 8; ++n)
  {
    const bool cycle = !cable;
    channel.Step(0.0, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, cycle, cycle && n == 0, routing);
    if (channel.OutputVolts(patch::SlopeOut) != 5.0)
      continue;
    if (first < 0)
      first = n;
    if (prev >= 0)
    {
      const int gap = n - prev;
      if (spacing < 0)
        spacing = gap;
      else if (gap != spacing)
        stable = false;
    }
    prev = n;
    ++count;
  }
  return stable ? spacing : -1;
}

void TestEndJack()
{
  constexpr double kFs = 48000.0;
  patch::PatchChannel pulse;
  pulse.Reset(kFs);
  Check(pulse.OutputVolts(patch::SlopeEnd) == patch::kGateVolts, "END is +5 V at idle after reset");

  patch::Routing open;
  bool highBefore4 = false;
  bool dropAt4 = false;
  bool lowAfter4 = true;
  bool fallZero = true;
  bool returned = false;
  bool stayedIdle = true;
  bool onlyLogic = true;
  bool seenPeak = false;
  double prevOut = 0.0;
  double prevEnd = pulse.OutputVolts(patch::SlopeEnd);
  for (int n = 0; n < 2000; ++n)
  {
    const double gate = n == 0 ? patch::kGateVolts : 0.0;
    pulse.Step(0.0, gate, 1.0, 0.0, 0.0, 0.005, 0.005, false, false, open);
    const double out = pulse.OutputVolts(patch::SlopeOut);
    const double end = pulse.OutputVolts(patch::SlopeEnd);
    if (end != 0.0 && end != patch::kGateVolts)
      onlyLogic = false;
    if (!seenPeak && out > 0.0 && out < patch::kHoldVolts && end == patch::kGateVolts)
      highBefore4 = true;
    if (!dropAt4 && out == patch::kHoldVolts)
      dropAt4 = end == 0.0 && prevEnd == patch::kGateVolts && prevOut < patch::kHoldVolts;
    if (!seenPeak && out >= patch::kHoldVolts && end != 0.0)
      lowAfter4 = false;
    if (!seenPeak && out == 5.0)
    {
      seenPeak = true;
      if (end != 0.0)
        fallZero = false;
    }
    else if (seenPeak && !returned)
    {
      if (out == 0.0)
        returned = end == patch::kGateVolts;
      else if (end != 0.0)
        fallZero = false;
    }
    else if (returned)
    {
      if (out != 0.0 || end != patch::kGateVolts)
        stayedIdle = false;
    }
    prevOut = out;
    prevEnd = end;
  }
  Check(onlyLogic, "END jack is only 0 V or +5 V");
  Check(highBefore4, "END stays +5 V on the rise while OUT is below 4 V");
  Check(dropAt4 && lowAfter4, "END drops to 0 V once OUT reaches 4 V");
  Check(seenPeak && fallZero, "END is 0 V from the peak through the fall");
  Check(returned && stayedIdle, "END returns to +5 V at idle and stays there");

  int cableFirst = -1;
  int cableCount = 0;
  int cycleFirst = -1;
  int cycleCount = 0;
  const int cablePeriod = PeakSpacing(true, cableFirst, cableCount);
  const int cyclePeriod = PeakSpacing(false, cycleFirst, cycleCount);
  const int named = slope::CyclePeriodSamples(0.005, 0.005, kFs);
  Check(cableCount >= 3 && cycleCount >= 3, "END to TRIG and the Cycle switch both repeat");
  Check(named == 480 && cyclePeriod == named, "Cycle switch period is 480 samples");
  Check(cablePeriod == cyclePeriod + 1, "END to TRIG period is one sample longer than Cycle");
  std::printf("measure end-cable period %d cycle-switch period %d first-peaks %d %d\n",
              cablePeriod, cyclePeriod, cableFirst, cycleFirst);
}
} // namespace

int main()
{
  TestZeroCablesMatchPleat();
  TestVcNormal();
  TestMidiGate();
  TestCables();
  TestSerialize();
  TestMeters();
  TestGateNotes();
  TestEndJack();
  if (gFailures != 0)
  {
    std::printf("%d failed\n", gFailures);
    return 1;
  }
  std::printf("all passed\n");
  return 0;
}
