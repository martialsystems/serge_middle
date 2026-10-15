// Second half of the slope. Port of slope/slope.py and slope/second.py.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace slope
{
inline constexpr double kFs = 48000.0;
inline constexpr double kSpan = 5.0;
inline constexpr double kAcCenter = kSpan / 2.0;
inline constexpr double kVcSecondsPerVolt = 0.001;
inline constexpr double kTrigThreshold = 1.0;
inline constexpr double kFeedbackMin = -1.0;
inline constexpr double kFeedbackMax = 1.0;
inline constexpr double kVcDepthMin = -1.0;
inline constexpr double kVcDepthMax = 1.0;
// END jack window. The circuit gate stays high for the whole rise. The jack
// drops once OUT reaches 4 V, so an END to TRIG cord cycles instead of holding.
inline constexpr double kEndWindowVolts = 4.0;
inline constexpr double kEndJackHigh = 5.0;

enum class Switch { Rise, Fall, Both };
enum class State { Idle, Rising, Falling, Holding };

inline int RoundHalfEven(double value)
{
  // Python 3 round() for a non-negative time. Ties go to the even integer.
  if (value < 0.0)
    value = 0.0;
  const double floorValue = std::floor(value);
  const double frac = value - floorValue;
  const long long whole = static_cast<long long>(floorValue);
  if (frac > 0.5)
    return static_cast<int>(whole + 1);
  if (frac < 0.5)
    return static_cast<int>(whole);
  if ((whole % 2) == 0)
    return static_cast<int>(whole);
  return static_cast<int>(whole + 1);
}

inline double AcVolts(double out) { return kAcCenter - out; }

// Host sample. ±2.5 V is ±1. Nothing is applied after this scale.
inline double ScaledAc(double ac) { return ac / kAcCenter; }

// Two-way scale in front of the VC jack. depth 0 passes nothing. +1 passes vc.
// -1 inverts it. A non-finite input passes nothing.
inline double ScaleVc(double vc, double depth)
{
  if (!std::isfinite(vc) || !std::isfinite(depth))
    return 0.0;
  if (depth < kVcDepthMin)
    depth = kVcDepthMin;
  else if (depth > kVcDepthMax)
    depth = kVcDepthMax;
  if (depth == 0.0 || vc == 0.0)
    return 0.0;
  if (depth == 1.0)
    return vc;
  if (depth == -1.0)
    return -vc;
  return depth * vc;
}

// END jack, in volts. 0 V or +5 V. The circuit gate is 0 while falling and 1
// otherwise. The jack is +5 V when that gate is high and OUT is below 4 V.
inline double EndJackVolts(double endGate, double out)
{
  if (endGate > 0.0 && out < kEndWindowVolts)
    return kEndJackHigh;
  return 0.0;
}

// 0 rise, 1 both, 2 fall. Anything else is both, the constructor default.
inline Switch SwitchFromInt(int route)
{
  if (route == 0)
    return Switch::Rise;
  if (route == 2)
    return Switch::Fall;
  return Switch::Both;
}

inline double RequireFinite(const char* name, double value)
{
  if (!std::isfinite(value))
    throw std::invalid_argument(name);
  return value;
}

// One slope. OUT is 0 V to +5 V. This type has no AC jack.
class Circuit
{
public:
  Circuit(double rise, double fall, double fs = kFs, Switch vcSwitch = Switch::Both)
  : mFs(RequireFinite("fs", fs))
  , mVcSwitch(vcSwitch)
  {
    if (mFs <= 0.0)
      throw std::invalid_argument("fs");
    SetTimes(rise, fall);
    mTMin = 1.0 / mFs;
    Reset();
  }

  void SetSampleRate(double fs)
  {
    mFs = RequireFinite("fs", fs);
    if (mFs <= 0.0)
      throw std::invalid_argument("fs");
    mTMin = 1.0 / mFs;
  }

  void SetTimes(double rise, double fall)
  {
    rise = RequireFinite("rise", rise);
    fall = RequireFinite("fall", fall);
    if (rise <= 0.0 || fall <= 0.0)
      throw std::invalid_argument("rise and fall");
    mRise = rise;
    mFall = fall;
  }

  // A segment already running keeps its count. The next segment uses the new route.
  void SetSwitch(Switch vcSwitch) { mVcSwitch = vcSwitch; }

  void Reset()
  {
    mV = 0.0;
    mState = State::Idle;
    mTrigHot = false;
    mTrigHeld = false;
    mTracking = false;
    mEndPulse = false;
    mPhase = false;
    mK = 0;
    mN = 1;
    mSegmentVc = 0.0;
    mSegmentVOct = 0.0;
  }

  double FeedbackAmount(double feedback) const
  {
    return std::min(kFeedbackMax, std::max(kFeedbackMin, feedback));
  }

  double PatchedVc(double vc, double feedback) const
  {
    return vc + FeedbackAmount(feedback) * mV;
  }

  void Times(double vc, double& riseOut, double& fallOut) const
  {
    const double offset = kVcSecondsPerVolt * vc;
    double rise = mRise;
    double fall = mFall;
    if (mVcSwitch == Switch::Rise || mVcSwitch == Switch::Both)
      rise -= offset;
    if (mVcSwitch == Switch::Fall || mVcSwitch == Switch::Both)
      fall -= offset;
    riseOut = std::max(mTMin, rise);
    fallOut = std::max(mTMin, fall);
  }

  int SegmentSamples(double timeSeconds, double vOct) const
  {
    const double scale = std::pow(2.0, vOct);
    return std::max(1, RoundHalfEven(timeSeconds * mFs / scale));
  }

  void Steps(double vc, double vOct, double feedback, double& riseStep, double& fallStep) const
  {
    double riseTime = 0.0;
    double fallTime = 0.0;
    Times(PatchedVc(vc, feedback), riseTime, fallTime);
    const double scale = std::pow(2.0, vOct);
    riseStep = scale * kSpan / (riseTime * mFs);
    fallStep = scale * kSpan / (fallTime * mFs);
  }

  // Emit OUT and END, then advance one sample. END is 0 while falling and 1 otherwise.
  void Step(double inp, double trig, double vc, double vOct, double feedback, bool endToTrig,
            double& out, double& endGate)
  {
    inp = RequireFinite("inp", inp);
    trig = RequireFinite("trig", trig);
    vc = RequireFinite("vc", vc);
    vOct = RequireFinite("vOct", vOct);
    feedback = RequireFinite("feedback", feedback);

    const double target = std::min(kSpan, std::fabs(inp));
    const bool edge = trig > kTrigThreshold && !mTrigHot;
    mTrigHot = trig > kTrigThreshold;
    // A later pulse is not the gate that started this rise.
    if (!mTrigHot)
      mTrigHeld = false;
    mEndPulse = false;

    if (target > 0.0)
    {
      // A positive IN overrides TRIG, including a hold at the top.
      mTracking = true;
      mPhase = false;
      if (mV < target)
        mState = State::Rising;
      else if (mV > target)
        mState = State::Falling;
      else
        mState = State::Holding;
    }
    else
    {
      if (mTracking)
      {
        mTracking = false;
        mPhase = false;
        if (mV > 0.0)
          mState = State::Falling;
        else
          mState = State::Idle;
      }
      else if (mState == State::Holding && !mTrigHot)
      {
        // The rise finished while TRIG was still high. It has gone low.
        BeginFall(vc, vOct, feedback);
      }
      if (mState == State::Idle && edge)
      {
        BeginRise(vc, vOct, feedback);
        mTrigHeld = true;
      }
    }

    out = mV;
    endGate = mState == State::Falling ? 0.0 : 1.0;

    if (mState == State::Rising && HoldExact(vc, vOct, feedback))
    {
      mK += 1;
      if (mK >= mN)
      {
        if (mTrigHeld)
          Sustain();
        else
          BeginFall(vc, vOct, feedback);
      }
      else
        mV = kSpan * static_cast<double>(mK) / static_cast<double>(mN);
    }
    else if (mState == State::Falling && HoldExact(vc, vOct, feedback))
    {
      mK += 1;
      if (mK >= mN)
      {
        mV = 0.0;
        mState = State::Idle;
        mEndPulse = true;
        if (endToTrig)
          BeginRise(vc, vOct, feedback);
      }
      else
      {
        mV = kSpan * static_cast<double>(mN - mK) / static_cast<double>(mN);
      }
    }
    else if (mState == State::Rising)
    {
      mPhase = false;
      double riseStep = 0.0;
      double fallStep = 0.0;
      Steps(vc, vOct, feedback, riseStep, fallStep);
      mV += riseStep;
      const double limit = target > 0.0 ? target : kSpan;
      if (mV >= limit)
      {
        mV = limit;
        if (target > 0.0)
          mState = State::Holding;
        else if (mTrigHeld)
          Sustain();
        else
          BeginFall(vc, vOct, feedback);
      }
    }
    else if (mState == State::Falling)
    {
      mPhase = false;
      double riseStep = 0.0;
      double fallStep = 0.0;
      Steps(vc, vOct, feedback, riseStep, fallStep);
      mV -= fallStep;
      const double floor = target > 0.0 ? target : 0.0;
      if (mV <= floor)
      {
        mV = floor;
        if (floor == 0.0)
        {
          mState = State::Idle;
          mEndPulse = true;
          if (endToTrig)
            BeginRise(vc, vOct, feedback);
        }
        else
        {
          mState = State::Holding;
        }
      }
    }
  }

  double Volts() const { return mV; }
  State Phase() const { return mState; }
  bool EndPulse() const { return mEndPulse; }

private:
  bool ExactSegment(double feedback) const { return FeedbackAmount(feedback) == 0.0; }

  void Sustain()
  {
    mState = State::Holding;
    mV = kSpan;
    mPhase = false;
    mTracking = false;
    mK = 0;
  }

  void BeginRise(double vc, double vOct, double feedback)
  {
    mState = State::Rising;
    mV = 0.0;
    mK = 0;
    mPhase = ExactSegment(feedback);
    if (mPhase)
    {
      mSegmentVc = vc;
      mSegmentVOct = vOct;
      double riseTime = 0.0;
      double fallTime = 0.0;
      Times(vc, riseTime, fallTime);
      mN = SegmentSamples(riseTime, vOct);
    }
  }

  void BeginFall(double vc, double vOct, double feedback)
  {
    mState = State::Falling;
    mV = kSpan;
    mK = 0;
    mPhase = ExactSegment(feedback);
    if (mPhase)
    {
      mSegmentVc = vc;
      mSegmentVOct = vOct;
      double riseTime = 0.0;
      double fallTime = 0.0;
      Times(vc, riseTime, fallTime);
      mN = SegmentSamples(fallTime, vOct);
    }
  }

  bool HoldExact(double vc, double vOct, double feedback) const
  {
    return mPhase && ExactSegment(feedback) && vc == mSegmentVc && vOct == mSegmentVOct;
  }

  double mFs;
  Switch mVcSwitch;
  double mRise = 0.005;
  double mFall = 0.005;
  double mTMin = 1.0 / kFs;
  double mV = 0.0;
  State mState = State::Idle;
  bool mTrigHot = false;
  bool mTrigHeld = false;
  bool mTracking = false;
  bool mEndPulse = false;
  bool mPhase = false;
  int mK = 0;
  int mN = 1;
  double mSegmentVc = 0.0;
  double mSegmentVOct = 0.0;
};

struct Sample
{
  double out;
  double end;
  double ac;
};

// The second half. Own circuit. AC does not enter the VC sum.
class SecondHalf
{
public:
  SecondHalf(double rise = 0.005, double fall = 0.005, double fs = kFs, Switch vcSwitch = Switch::Both)
  : mCircuit(rise, fall, fs, vcSwitch)
  {
  }

  void SetSampleRate(double fs) { mCircuit.SetSampleRate(fs); }
  void SetTimes(double rise, double fall) { mCircuit.SetTimes(rise, fall); }
  void SetSwitch(Switch vcSwitch) { mCircuit.SetSwitch(vcSwitch); }
  void Reset() { mCircuit.Reset(); }
  bool EndPulse() const { return mCircuit.EndPulse(); }
  double Volts() const { return mCircuit.Volts(); }

  Sample Step(double inp = 0.0, double trig = 0.0, double vc = 0.0, double vOct = 0.0,
              double feedback = 0.0, bool endToTrig = false)
  {
    Sample sample;
    mCircuit.Step(inp, trig, vc, vOct, feedback, endToTrig, sample.out, sample.end);
    sample.ac = AcVolts(sample.out);
    return sample;
  }

private:
  Circuit mCircuit;
};

// Samples in one free cycle. Each segment is at least one sample.
inline int CyclePeriodSamples(double rise, double fall, double fs)
{
  const int nRise = std::max(1, RoundHalfEven(rise * fs));
  const int nFall = std::max(1, RoundHalfEven(fall * fs));
  return nRise + nFall;
}

inline double CycleHz(double rise, double fall, double fs)
{
  return fs / static_cast<double>(CyclePeriodSamples(rise, fall, fs));
}

// One circuit sample. Host ±1 is ±5 V on IN. cycle is the end-pulse cord to
// TRIG. arm is one 5 V trigger on this sample. A non-finite host is read as 0.
// Every finite host keeps the values it had before that guard. vcDepth scales
// vc. The default depth is 0, so the VC voltage that reaches the circuit is 0,
// the same call this function used to make. 1V/oct and the feedback amount
// stay at 0. The route switch is whatever SetSwitch last stored, default both.
inline Sample AdvanceSample(SecondHalf& half, double rise, double fall, double host, bool arm,
                            bool cycle = true, double vc = 0.0, double vcDepth = 0.0)
{
  if (!std::isfinite(host))
    host = 0.0;
  half.SetTimes(rise, fall);
  return half.Step(5.0 * host, arm ? 5.0 : 0.0, ScaleVc(vc, vcDepth), 0.0, 0.0, cycle);
}

// The raw jack sample, AC / 2.5. cycle defaults to the cord in place, which is
// the call this function used to be.
inline double PluginSample(SecondHalf& half, double rise, double fall, double host, bool arm,
                           bool cycle = true)
{
  return ScaledAc(AdvanceSample(half, rise, fall, host, arm, cycle).ac);
}

} // namespace slope
