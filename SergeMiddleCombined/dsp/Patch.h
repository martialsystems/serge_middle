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
  kNumInputs = 6
};

enum Output : int8_t
{
  PleatOut = 0,
  SlopeEnd = 1,
  SlopeOut = 2,
  SlopeAc = 3,
  kNumOutputs = 4
};

inline constexpr int kMeterCount = kNumInputs + kNumOutputs;
inline constexpr uint8_t kRoutingMagic = 0x52;
inline constexpr uint8_t kRoutingVersion = 1;
inline constexpr int kRoutingBytes = 3 + kNumInputs;
inline constexpr double kVcOctavesPerVolt = 0.8;
inline constexpr double kIn2CableLimit = 10.0;
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
inline bool ParseRouting(const uint8_t* bytes, size_t size, Routing& routing)
{
  Routing cleared;
  if (bytes == nullptr || size != static_cast<size_t>(kRoutingBytes))
  {
    routing = cleared;
    return false;
  }
  if (bytes[0] != kRoutingMagic || bytes[1] != kRoutingVersion || bytes[2] != kNumInputs)
  {
    routing = cleared;
    return false;
  }
  Routing parsed;
  for (int i = 0; i < kNumInputs; ++i)
  {
    const uint8_t byte = bytes[3 + i];
    if (byte == 0xFF)
      parsed.source[i] = -1;
    else if (byte >= kNumOutputs)
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

// g = clamp(gS * 2^(0.8 * depth * VC), 0.5, 8). VC of 0, or depth of 0, returns gS
// with no multiply, so the smoother value reaches the cells unchanged.
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
  const double factor = std::pow(2.0, kVcOctavesPerVolt * depth * vc);
  if (!std::isfinite(factor))
    return serge::DriveScale(gS);
  return serge::DriveScale(gS * factor);
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
    for (double& volts : mPrev)
      volts = 0.0;
    mPrev[SlopeEnd] = kGateVolts;
    for (double& volts : mIn)
      volts = 0.0;
    for (double& volts : mOut)
      volts = 0.0;
    mOut[SlopeEnd] = kGateVolts;
    mAppliedG = serge::kGDefault;
    mHost = 0.0;
  }

  // `arm` is the one-sample Cycle arm. `cycle` keeps the END to TRIG cord.
  // `slopeVcDepth` is the attenuverter, default 0, so an unscaled jack is not
  // what a saved project gets. `slopeRoute` is 0 rise, 1 both, 2 fall.
  double Step(double hostIn, double midiGateVolts, double gS, double sS, double depth, double rise,
              double fall, bool cycle, bool arm, const Routing& routing, double slopeVcDepth = 0.0,
              int slopeRoute = 1)
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
    for (double& volts : mIn)
    {
      if (!std::isfinite(volts))
        volts = 0.0;
    }

    double inp = mIn[SlopeIn];
    // The circuit sees the TRIG level, not a one-sample edge. A level that has
    // stayed high since the starting edge holds at +5 V. The Cycle arm is one high sample.
    double circuitTrig = mIn[SlopeTrig];
    if (cycle && arm && !(circuitTrig > slope::kTrigThreshold))
      circuitTrig = kGateVolts;

    const double safeRise = SafeTime(rise);
    const double safeFall = SafeTime(fall);
    mSlope.SetSwitch(slope::SwitchFromInt(slopeRoute));
    mSlope.SetTimes(safeRise, safeFall);
    const double scaledVc = slope::ScaleVc(mIn[SlopeVc], slopeVcDepth);
    const slope::Sample sample = mSlope.Step(inp, circuitTrig, scaledVc, 0.0, 0.0, cycle);
    const double coupled = mCoupler.Step(sample.ac - slope::kAcCenter);

    mAppliedG = FoldWithVc(gS, depth, mIn[PleatVc]);
    const double a = in1Patched ? mIn[PleatIn1] / serge::kAudioFullScaleVolts : hostIn;
    mHost = mPleat.Step(a, mAppliedG, sS, in2Patched, mIn[PleatIn2]);

    mOut[PleatOut] = serge::kAudioFullScaleVolts * mHost;
    mOut[SlopeEnd] = EndJackVolts(sample.end, sample.out);
    mOut[SlopeOut] = sample.out;
    mOut[SlopeAc] = coupled;
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

  static bool Patched(const Routing& routing, int input)
  {
    const int src = routing.source[input];
    return src >= 0 && src < kNumOutputs;
  }

  double Read(const Routing& routing, int input) const
  {
    return mPrev[routing.source[input]];
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
  double mPrev[kNumOutputs] = {0.0, kGateVolts, 0.0, 0.0};
  double mIn[kNumInputs] = {};
  double mOut[kNumOutputs] = {0.0, kGateVolts, 0.0, 0.0};
  double mAppliedG = serge::kGDefault;
  double mHost = 0.0;
};

} // namespace patch
