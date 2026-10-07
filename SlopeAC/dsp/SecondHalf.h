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

  void Reset()
  {
    mV = 0.0;
    mState = State::Idle;
    mTrigHot = false;
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
    mEndPulse = false;

    if (target > 0.0)
    {
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
      if (mState == State::Idle && edge)
        BeginRise(vc, vOct, feedback);
    }

    out = mV;
    endGate = mState == State::Falling ? 0.0 : 1.0;

    if (mState == State::Rising && HoldExact(vc, vOct, feedback))
    {
      mK += 1;
      if (mK >= mN)
        BeginFall(vc, vOct, feedback);
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

// One host sample. Host ±1 is ±5 V on IN. The arm is a 5 V trigger on the
// sample after reset. VC, 1V/oct, and the feedback amount stay at 0, and the
// end pulse is patched to TRIG. The return is AC / 2.5.
inline double PluginSample(SecondHalf& half, double rise, double fall, double host, bool arm)
{
  half.SetTimes(rise, fall);
  const Sample sample = half.Step(5.0 * host, arm ? 5.0 : 0.0, 0.0, 0.0, 0.0, true);
  return ScaledAc(sample.ac);
}

} // namespace slope
