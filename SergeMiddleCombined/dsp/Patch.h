#pragma once

// Combined PLEAT and slope, with jacks inside one plugin.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// Header-only and free of iPlug2. SergeMiddle and SlopeAC are not modified.
// Every jack-to-jack read uses the source's previous sample, including the
// slope OUT normal into PLEAT VC. Host audio into IN 1, the MIDI gate into
// TRIG, and PLEAT OUT back to the host are same-sample.

#include "../../SergeMiddle/dsp/WaveMiddle.h"
#include "../../SlopeAC/dsp/AcCoupler.h"
#include "WaveEnds.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace patch
{

enum Input : int8_t
{
  PleatIn1 = 0,
  PleatIn2 = 1,
  PleatVc = 2,
  SlopeIn = 3,
  SlopeTrig = 4,
  SlopeVc = 5,
  TopIn = 6,
  TopVc = 7,
  BottomIn = 8,
  BottomVc = 9,
  Slope2In = 10,
  Slope2Trig = 11,
  Slope2Vc = 12,
  kNumInputs = 13
};

enum Output : int8_t
{
  PleatOut = 0,
  SlopeEnd = 1,
  SlopeOut = 2,
  SlopeAc = 3,
  TopOut = 4,
  BottomOut1 = 5,
  BottomOut2 = 6,
  Slope2End = 7,
  Slope2Out = 8,
  Slope2Ac = 9,
  kNumOutputs = 10
};

inline constexpr int kMeterCount = kNumInputs + kNumOutputs;
inline constexpr uint8_t kRoutingMagic = 0x52;
// Version 2 is the current blob: every input, sources across every output.
// Version 1 is the six-input blob from before the top, bottom, and second slope.
inline constexpr uint8_t kRoutingVersion = 2;
inline constexpr uint8_t kLegacyRoutingVersion = 1;
inline constexpr int kLegacyInputCount = 6;
inline constexpr int kLegacyOutputCount = 4;
inline constexpr int kLegacyRoutingBytes = 3 + kLegacyInputCount;
inline constexpr int kRoutingBytes = 3 + kNumInputs;
inline constexpr double kIn2CableLimit = 10.0;
// Combined PLEAT OUT jack. The cells are not clipped. This rail is applied
// after the level law, and a sample already inside it is left untouched.
inline constexpr double kPleatOutLimit = 5.0;
inline constexpr double kGateVolts = 5.0;
// 5 * (216 / 240) is not a binary 4.5, so a 4.5 V compare can miss the exact stair.
// 4 V is exact at 5 * (192 / 240), which is 80% of a straight 5 V rise.
// The END jack uses that mark. A TRIG sustain does not: the fall waits at +5 V.
inline constexpr double kHoldVolts = slope::kEndWindowVolts;
static_assert(kHoldVolts == 4.0, "END window stays at 4 V");

// SLOPE END jack, in volts. Shared with the slope header. 0 V or +5 V.
// The circuit end gate is still 0 while falling and 1 otherwise, and the
// Cycle switch still reads that gate inside the circuit. The jack is +5 V
// when the gate is high and slope OUT is below 4 V, otherwise 0 V. Idle OUT
// of 0 V is high, so an END to TRIG cable at idle is a rising edge.
inline double EndJackVolts(double endGate, double out)
{
  return slope::EndJackVolts(endGate, out);
}

inline int MeterOf(bool output, int index)
{
  return output ? kNumInputs + index : index;
}

// Output 0 is PLEAT OUT. A default std::atomic<int8_t> is 0, and a fresh page
// is 0, so an uninitialized routing word draws that cable. The word is born
// unpatched.
struct RoutingWord
{
  std::atomic<int8_t> source{-1};
};

// One output per input. -1 is unpatched. A new cable replaces the old one.
struct Routing
{
  int8_t source[kNumInputs];

  Routing() { Clear(); }

  void Clear()
  {
    for (int8_t& slot : source)
      slot = -1;
  }

  bool Connect(int output, int input)
  {
    if (output < 0 || output >= kNumOutputs || input < 0 || input >= kNumInputs)
      return false;
    source[input] = static_cast<int8_t>(output);
    return true;
  }

  void Disconnect(int input)
  {
    if (input < 0 || input >= kNumInputs)
      return;
    source[input] = -1;
  }

  // An output jack drops every cable that reads it. An input jack drops its one cable.
  void DisconnectJack(bool output, int index)
  {
    if (output)
    {
      if (index < 0 || index >= kNumOutputs)
        return;
      for (int i = 0; i < kNumInputs; ++i)
      {
        if (source[i] == index)
          source[i] = -1;
      }
      return;
    }
    Disconnect(index);
  }
};

// Drag either way. Two inputs, or two outputs, are refused and routing is left as it was.
inline bool PatchJacks(Routing& routing, bool aIsOutput, int aIndex, bool bIsOutput, int bIndex)
{
  if (aIsOutput == bIsOutput)
    return false;
  if (aIsOutput)
    return routing.Connect(aIndex, bIndex);
  return routing.Connect(bIndex, aIndex);
}

inline void SerializeRouting(const Routing& routing, std::vector<uint8_t>& out)
{
  out.clear();
  out.reserve(static_cast<size_t>(kRoutingBytes));
  out.push_back(kRoutingMagic);
  out.push_back(kRoutingVersion);
  out.push_back(static_cast<uint8_t>(kNumInputs));
  for (int i = 0; i < kNumInputs; ++i)
  {
    const int src = routing.source[i];
    if (src < 0 || src >= kNumOutputs)
      out.push_back(0xFF);
    else
      out.push_back(static_cast<uint8_t>(src));
  }
}

// On failure `routing` is all unpatched.
// A version 1 blob is nine bytes, count 6, and sources 0..3. The inputs added
// after slope VC stay unpatched. A version 2 blob is the current size.
inline bool ParseRouting(const uint8_t* bytes, size_t size, Routing& routing)
{
  Routing cleared;
  if (bytes == nullptr || size < 3 || bytes[0] != kRoutingMagic)
  {
    routing = cleared;
    return false;
  }
  const bool legacy = bytes[1] == kLegacyRoutingVersion && bytes[2] == kLegacyInputCount &&
                      size == static_cast<size_t>(kLegacyRoutingBytes);
  const bool current = bytes[1] == kRoutingVersion && bytes[2] == static_cast<uint8_t>(kNumInputs) &&
                       size == static_cast<size_t>(kRoutingBytes);
  if (!legacy && !current)
  {
    routing = cleared;
    return false;
  }
  const int count = legacy ? kLegacyInputCount : kNumInputs;
  const int outLimit = legacy ? kLegacyOutputCount : kNumOutputs;
  Routing parsed;
  for (int i = 0; i < count; ++i)
  {
    const uint8_t byte = bytes[3 + i];
    if (byte == 0xFF)
      parsed.source[i] = -1;
    else if (byte >= outLimit)
    {
      routing = cleared;
      return false;
    }
    else
      parsed.source[i] = static_cast<int8_t>(byte);
  }
  routing = parsed;
  return true;
}

// Voltages for inputs that have no normal. Used only while that input is unpatched.
// A null inject reads as 0 V. Host audio is not in here.
struct Inject
{
  double topIn = 0.0;
  double topVc = 0.0;
  double bottomIn = 0.0;
  double bottomVc = 0.0;
  double slope2In = 0.0;
  double slope2Trig = 0.0;
  double slope2Vc = 0.0;
};

// Knobs for the sections appended after slope 1. Defaults leave them idle:
// top and bottom silent, slope 2 at 5 ms with Cycle off and no VC.
struct SectionControls
{
  double topGain = 0.0;
  bool topHi = false;
  double bottomGain = 0.0;
  double bottomDepth = 1.0;
  double rise2 = 0.005;
  double fall2 = 0.005;
  bool cycle2 = false;
  bool arm2 = false;
  double slope2Depth = 0.0;
  int slope2Route = 1;
  const Inject* inject = nullptr;
};

// Patched IN 2 is a jack voltage, clamped to ±10 V. A non-finite value is 0.
inline double ClampPatchedIn2(double volts)
{
  if (!std::isfinite(volts))
    return 0.0;
  if (volts > kIn2CableLimit)
    return kIn2CableLimit;
  if (volts < -kIn2CableLimit)
    return -kIn2CableLimit;
  return volts;
}

// g = clamp(gS + depth * VC, 0.5, 8). The fold knob is the baseline and the
// attenuated VC is added to it, in fold-amount units: one volt at depth 1
// adds 1. Depth 0, or a VC of 0 V, returns gS with no addition, so the
// smoother value reaches the cells unchanged.
inline double FoldWithVc(double gS, double depth, double vc)
{
  if (!std::isfinite(gS))
    gS = serge::kGDefault;
  if (!std::isfinite(depth) || depth <= 0.0)
    return gS;
  if (depth > 1.0)
    depth = 1.0;
  if (!std::isfinite(vc) || vc == 0.0)
    return gS;
  const double summed = gS + depth * vc;
  if (!std::isfinite(summed))
    return serge::DriveScale(gS);
  return serge::DriveScale(summed);
}

// Held MIDI notes. A second note-on does not add, and a second note-off does not subtract.
class GateNotes
{
public:
  void NoteOn(int note)
  {
    if (note < 0 || note > 127)
      return;
    uint64_t& word = note < 64 ? mLo : mHi;
    const uint64_t bit = static_cast<uint64_t>(1) << (note & 63);
    if ((word & bit) == 0)
    {
      word |= bit;
      ++mCount;
    }
  }

  void NoteOff(int note)
  {
    if (note < 0 || note > 127)
      return;
    uint64_t& word = note < 64 ? mLo : mHi;
    const uint64_t bit = static_cast<uint64_t>(1) << (note & 63);
    if ((word & bit) != 0)
    {
      word &= ~bit;
      --mCount;
    }
  }

  void AllOff()
  {
    mLo = 0;
    mHi = 0;
    mCount = 0;
  }

  int Count() const { return mCount; }
  double Volts() const { return mCount > 0 ? kGateVolts : 0.0; }

private:
  uint64_t mLo = 0;
  uint64_t mHi = 0;
  int mCount = 0;
};

// Min and max of one block, one value per jack. Index 0 is PleatIn1.
struct BlockMeter
{
  double min[kMeterCount];
  double max[kMeterCount];
  bool seen[kMeterCount];

  void Begin()
  {
    for (int i = 0; i < kMeterCount; ++i)
    {
      min[i] = 0.0;
      max[i] = 0.0;
      seen[i] = false;
    }
  }

  void Observe(int index, double volts)
  {
    if (index < 0 || index >= kMeterCount)
      return;
    if (!std::isfinite(volts))
      volts = 0.0;
    if (!seen[index] || volts < min[index])
      min[index] = volts;
    if (!seen[index] || volts > max[index])
      max[index] = volts;
    seen[index] = true;
  }
};

// One channel. Fold and IN 2 smoothers stay outside and are shared.
// Reset before Step. gS and sS are the smoother outputs for this sample.
class PatchChannel
{
public:
  void Reset(double sampleRate)
  {
    if (!(sampleRate > 0.0) || !std::isfinite(sampleRate))
      sampleRate = slope::kFs;
    mPleat.Reset(sampleRate);
    mSlope.SetSampleRate(sampleRate);
    mSlope.Reset();
    mCoupler.SetSampleRate(sampleRate);
    mCoupler.Reset();
    mSlope2.SetSampleRate(sampleRate);
    mSlope2.Reset();
    mCoupler2.SetSampleRate(sampleRate);
    mCoupler2.Reset();
    mTop.Reset(sampleRate);
    mBottom.Reset(sampleRate);
    for (double& volts : mPrev)
      volts = 0.0;
    mPrev[SlopeEnd] = kGateVolts;
    mPrev[Slope2End] = kGateVolts;
    for (double& volts : mIn)
      volts = 0.0;
    for (double& volts : mOut)
      volts = 0.0;
    mOut[SlopeEnd] = kGateVolts;
    mOut[Slope2End] = kGateVolts;
    mAppliedG = serge::kGDefault;
    mHost = 0.0;
  }

  // `arm` is the one-sample Cycle arm. `cycle` keeps the END to TRIG cord.
  // `slopeVcDepth` is the attenuverter, default 0, so an unscaled jack is not
  // what a saved project gets. `slopeRoute` is 0 rise, 1 both, 2 fall.
  double Step(double hostIn, double midiGateVolts, double gS, double sS, double depth, double rise,
              double fall, bool cycle, bool arm, const Routing& routing, double slopeVcDepth = 0.0,
              int slopeRoute = 1, SectionControls extra = {})
  {
    if (!std::isfinite(hostIn))
      hostIn = 0.0;
    if (!std::isfinite(midiGateVolts))
      midiGateVolts = 0.0;
    if (!std::isfinite(sS))
      sS = 0.0;

    const bool in1Patched = Patched(routing, PleatIn1);
    const bool in2Patched = Patched(routing, PleatIn2);
    mIn[PleatIn1] = in1Patched ? Read(routing, PleatIn1) : serge::kAudioFullScaleVolts * hostIn;
    mIn[PleatIn2] = in2Patched ? ClampPatchedIn2(Read(routing, PleatIn2)) : sS;
    mIn[PleatVc] = Patched(routing, PleatVc) ? Read(routing, PleatVc) : mPrev[SlopeOut];
    mIn[SlopeIn] = Patched(routing, SlopeIn) ? Read(routing, SlopeIn) : 0.0;
    mIn[SlopeTrig] = Patched(routing, SlopeTrig) ? Read(routing, SlopeTrig) : midiGateVolts;
    mIn[SlopeVc] = Patched(routing, SlopeVc) ? Read(routing, SlopeVc) : 0.0;
    mIn[TopIn] = Patched(routing, TopIn) ? Read(routing, TopIn) : Loose(extra.inject, &Inject::topIn);
    mIn[TopVc] = Patched(routing, TopVc) ? Read(routing, TopVc) : Loose(extra.inject, &Inject::topVc);
    mIn[BottomIn] = Patched(routing, BottomIn) ? Read(routing, BottomIn) : Loose(extra.inject, &Inject::bottomIn);
    mIn[BottomVc] = Patched(routing, BottomVc) ? Read(routing, BottomVc) : Loose(extra.inject, &Inject::bottomVc);
    mIn[Slope2In] = Patched(routing, Slope2In) ? Read(routing, Slope2In) : Loose(extra.inject, &Inject::slope2In);
    mIn[Slope2Trig] = Patched(routing, Slope2Trig) ? Read(routing, Slope2Trig) : Loose(extra.inject, &Inject::slope2Trig);
    mIn[Slope2Vc] = Patched(routing, Slope2Vc) ? Read(routing, Slope2Vc) : Loose(extra.inject, &Inject::slope2Vc);
    for (double& volts : mIn)
    {
      if (!std::isfinite(volts))
        volts = 0.0;
    }

    // The circuit sees the TRIG level, not a one-sample edge. A level that has
    // stayed high since the starting edge holds at +5 V. The Cycle arm is one high sample.
    const SlopeRun slope1 = RunSlope(mSlope, mCoupler, mIn[SlopeIn], mIn[SlopeTrig], mIn[SlopeVc], rise, fall,
                                     cycle, arm, slopeVcDepth, slopeRoute);

    mAppliedG = FoldWithVc(gS, depth, mIn[PleatVc]);
    const double a = in1Patched ? mIn[PleatIn1] / serge::kAudioFullScaleVolts : hostIn;
    mHost = mPleat.Step(a, mAppliedG, sS, in2Patched, mIn[PleatIn2]);

    double pleatOut = serge::kAudioFullScaleVolts * mHost;
    if (pleatOut > kPleatOutLimit || pleatOut < -kPleatOutLimit)
    {
      pleatOut = pleatOut > kPleatOutLimit ? kPleatOutLimit : -kPleatOutLimit;
      mHost = pleatOut / serge::kAudioFullScaleVolts;
    }
    mOut[PleatOut] = pleatOut;
    mOut[SlopeEnd] = slope1.end;
    mOut[SlopeOut] = slope1.out;
    mOut[SlopeAc] = slope1.ac;

    // Top, bottom, and slope 2 read the previous sample, same as every other cable.
    // They do not write the PLEAT or slope 1 outputs.
    const double topG = vcm::ControlSum(extra.topGain, 1.0, mIn[TopVc]);
    mOut[TopOut] = mTop.Step(mIn[TopIn], topG, extra.topHi);
    const double bottomCtrl = vcm::ControlSum(extra.bottomGain, extra.bottomDepth, mIn[BottomVc]);
    mBottom.Step(mIn[BottomIn], bottomCtrl, mOut[BottomOut1], mOut[BottomOut2]);
    const SlopeRun slope2 = RunSlope(mSlope2, mCoupler2, mIn[Slope2In], mIn[Slope2Trig], mIn[Slope2Vc], extra.rise2,
                                     extra.fall2, extra.cycle2, extra.arm2, extra.slope2Depth, extra.slope2Route);
    mOut[Slope2End] = slope2.end;
    mOut[Slope2Out] = slope2.out;
    mOut[Slope2Ac] = slope2.ac;

    for (int i = 0; i < kNumOutputs; ++i)
      mPrev[i] = mOut[i];
    return mHost;
  }

  double Host() const { return mHost; }
  double AppliedG() const { return mAppliedG; }

  // Test hook. The next Step reads this as that output's previous sample.
  void PrimeOutput(int output, double volts)
  {
    if (output < 0 || output >= kNumOutputs)
      return;
    if (!std::isfinite(volts))
      volts = 0.0;
    mPrev[output] = volts;
  }
  double InputVolts(int input) const
  {
    if (input < 0 || input >= kNumInputs)
      return 0.0;
    return mIn[input];
  }
  double OutputVolts(int output) const
  {
    if (output < 0 || output >= kNumOutputs)
      return 0.0;
    return mOut[output];
  }

private:
  class PleatVoice
  {
  public:
    PleatVoice()
    : mUp(serge::kUpsampleTaps)
    , mUpIn2(serge::kUpsampleTaps)
    , mDown(serge::kHalfbandTaps)
    {
    }

    void Reset(double sampleRate)
    {
      mUp.Reset();
      mUpIn2.Reset();
      mDown.Reset();
      mIn.Reset(sampleRate);
      mDc.Reset(sampleRate);
      mBiasOffset = serge::kSymDefault;
      mBias = 0.0;
    }

    // Unpatched IN 2 matches WaveMiddleChannel::Step: one s for all four phases,
    // C^6(s) subtracted. Patched IN 2 is upsampled on its own and not subtracted.
    double Step(double a, double g, double sKnob, bool in2Patched, double in2Volts)
    {
      if (!std::isfinite(a))
        a = 0.0;
      a = mIn.Step(a);
      const double v = serge::kAudioFullScaleVolts * serge::DriveScale(g) * a;
      double decimated = 0.0;
      if (!in2Patched)
      {
        const double offset = serge::SymmetryClamp(sKnob);
        if (offset != mBiasOffset)
        {
          mBiasOffset = offset;
          mBias = serge::SymmetryBias(offset);
        }
        for (int phase = 0; phase < serge::kOversample; ++phase)
        {
          const double hi = mUp.Step(phase == 0 ? v : 0.0);
          const double cells = serge::Process(hi + offset) - mBias;
          if (phase == 0)
            decimated = mDown.Step(cells);
          else
            mDown.Push(cells);
        }
      }
      else
      {
        const double s = ClampPatchedIn2(in2Volts);
        for (int phase = 0; phase < serge::kOversample; ++phase)
        {
          const double hi = mUp.Step(phase == 0 ? v : 0.0);
          const double sHi = mUpIn2.Step(phase == 0 ? s : 0.0);
          const double cells = serge::Process(hi + sHi);
          if (phase == 0)
            decimated = mDown.Step(cells);
          else
            mDown.Push(cells);
        }
      }
      return mDc.Step(serge::OutputScale(g) * decimated);
    }

  private:
    serge::Fir63 mUp;
    serge::Fir63 mUpIn2;
    serge::Fir63 mDown;
    serge::DcBlock mIn;
    serge::DcBlock mDc;
    double mBiasOffset = serge::kSymDefault;
    double mBias = 0.0;
  };

  struct SlopeRun
  {
    double end = 0.0;
    double out = 0.0;
    double ac = 0.0;
  };

  static SlopeRun RunSlope(slope::SecondHalf& half, slope::AcCoupler& coupler, double inp, double trig, double vc,
                           double rise, double fall, bool cycle, bool arm, double depth, int route)
  {
    double circuitTrig = trig;
    if (cycle && arm && !(circuitTrig > slope::kTrigThreshold))
      circuitTrig = kGateVolts;
    half.SetSwitch(slope::SwitchFromInt(route));
    half.SetTimes(SafeTime(rise), SafeTime(fall));
    const slope::Sample sample = half.Step(inp, circuitTrig, slope::ScaleVc(vc, depth), 0.0, 0.0, cycle);
    SlopeRun run;
    run.end = EndJackVolts(sample.end, sample.out);
    run.out = sample.out;
    run.ac = coupler.Step(sample.ac - slope::kAcCenter);
    return run;
  }

  static bool Patched(const Routing& routing, int input)
  {
    const int src = routing.source[input];
    return src >= 0 && src < kNumOutputs;
  }

  double Read(const Routing& routing, int input) const
  {
    return mPrev[routing.source[input]];
  }

  static double Loose(const Inject* inject, double Inject::* member)
  {
    if (inject == nullptr)
      return 0.0;
    const double volts = inject->*member;
    return std::isfinite(volts) ? volts : 0.0;
  }

  static double SafeTime(double seconds)
  {
    if (!std::isfinite(seconds) || seconds <= 0.0)
      return 0.005;
    return seconds;
  }

  PleatVoice mPleat;
  slope::SecondHalf mSlope;
  slope::AcCoupler mCoupler;
  slope::SecondHalf mSlope2;
  slope::AcCoupler mCoupler2;
  vcm::TopSection mTop;
  vcm::BottomSection mBottom;
  double mPrev[kNumOutputs] = {};
  double mIn[kNumInputs] = {};
  double mOut[kNumOutputs] = {};
  double mAppliedG = serge::kGDefault;
  double mHost = 0.0;
};

} // namespace patch
