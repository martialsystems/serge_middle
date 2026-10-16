#pragma once

// VCM top and bottom. Header-only. The standalone PLEAT plugin does not include this.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// NOTES name the top as a VCA or a soft square-up with HI/LO, and the bottom as a
// level-compensated full-wave rectifier with a second, squared, output. They do
// not give a schematic. The shapes below are the model of that description.
// LO is linear. HI is an odd tanh. The bottom rectifies, then divides by a
// linear peak follower so the level, not the knob, sets the compensation.
// Both outputs are AC coupled with the same 10 Hz block as PLEAT and hard-clamped
// to ±5 V. A value already inside the rail is left as the block produced it.

#include "../../SergeMiddle/dsp/WaveMiddle.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace vcm
{

inline constexpr double kRail = 5.0;
inline constexpr double kVcFullScale = 5.0;
inline constexpr double kFloor = 0.02;
inline constexpr double kHiDrive = 3.0;
inline constexpr double kBottomDrive = 2.5;
inline constexpr double kBottomOut = 4.0;
inline constexpr double kSquareDrive = 8.0;
inline constexpr double kAttackSeconds = 0.001;
inline constexpr double kReleaseSeconds = 0.030;

// Top Gain and Bottom Level are 0 at full CCW. The caption prints "0", not "0.000".
inline void FormatUnit(double value, char* buf, std::size_t n)
{
  if (buf == nullptr || n == 0)
    return;
  if (!std::isfinite(value))
    value = 0.0;
  if (value < 0.0)
    value = 0.0;
  if (value > 1.0)
    value = 1.0;
  if (value < 0.0005)
  {
    std::snprintf(buf, n, "0");
    return;
  }
  if (value > 0.9995)
  {
    std::snprintf(buf, n, "1");
    return;
  }
  std::snprintf(buf, n, "%.2f", value);
  char* dot = std::strchr(buf, '.');
  if (dot == nullptr)
    return;
  char* end = buf + std::strlen(buf);
  while (end > dot + 1 && end[-1] == '0')
    --end;
  if (end[-1] == '.')
    --end;
  *end = '\0';
}

inline double Clamp01(double value)
{
  if (!std::isfinite(value) || value <= 0.0)
    return 0.0;
  if (value >= 1.0)
    return 1.0;
  return value;
}

// Hard rail. Non-finite becomes 0. A finite sample inside ±5 V is unchanged.
inline double ClampRail(double volts)
{
  if (!std::isfinite(volts))
    return 0.0;
  if (volts > kRail)
    return kRail;
  if (volts < -kRail)
    return -kRail;
  return volts;
}

// knob is already 0..1. depth 0, or a VC of 0 V, returns that knob with no add.
// One full-scale VC volt is 1/5, so +5 V at depth 1 adds 1.
inline double ControlSum(double knob, double depth, double vc)
{
  knob = Clamp01(knob);
  if (!std::isfinite(depth) || depth <= 0.0)
    return knob;
  if (depth > 1.0)
    depth = 1.0;
  if (!std::isfinite(vc) || vc == 0.0)
    return knob;
  const double summed = knob + depth * (vc / kVcFullScale);
  if (!std::isfinite(summed))
    return knob;
  return Clamp01(summed);
}

// 20 ms one-pole on a 0..1 knob. Reset starts at the clamped target, so a
// constant knob is exact from the first sample. Same coefficient as Fold.
class GainSmoother
{
public:
  void Reset(double target, double sampleRate)
  {
    mCoeff = serge::SmoothCoeff(sampleRate);
    mState = Clamp01(target);
  }

  double Step(double target)
  {
    const double clamped = Clamp01(target);
    mState = mState + (1.0 - mCoeff) * (clamped - mState);
    return mState;
  }

  double Value() const { return mState; }

private:
  double mCoeff = 0.0;
  double mState = 0.0;
};

// Top section. g is the summed control, already 0..1. HI false is the linear VCA.
class TopSection
{
public:
  void Reset(double sampleRate)
  {
    mIn.Reset(sampleRate);
    mOut.Reset(sampleRate);
  }

  double Step(double x, double g, bool hi)
  {
    if (!std::isfinite(x))
      x = 0.0;
    const double blocked = mIn.Step(x);
    g = Clamp01(g);
    double y = 0.0;
    if (g != 0.0)
    {
      if (!hi)
        y = g * blocked;
      else
      {
        const double z = g * kHiDrive * blocked / serge::kAudioFullScaleVolts;
        const double mag = std::tanh(std::fabs(z));
        y = serge::kAudioFullScaleVolts * std::copysign(mag, z);
      }
    }
    return ClampRail(mOut.Step(y));
  }

private:
  serge::DcBlock mIn;
  serge::DcBlock mOut;
};

// Bottom section. ctrl is the summed level, already 0..1.
// OUT 1 is the compensated rectifier. OUT 2 is the same wave, squared up.
class BottomSection
{
public:
  void Reset(double sampleRate)
  {
    if (!(sampleRate > 0.0) || !std::isfinite(sampleRate))
      sampleRate = 48000.0;
    mIn.Reset(sampleRate);
    mOut1.Reset(sampleRate);
    mOut2.Reset(sampleRate);
    mEnv = 0.0;
    mAttack = std::exp(-1.0 / (kAttackSeconds * sampleRate));
    mRelease = std::exp(-1.0 / (kReleaseSeconds * sampleRate));
  }

  void Step(double x, double ctrl, double& out1, double& out2)
  {
    if (!std::isfinite(x))
      x = 0.0;
    ctrl = Clamp01(ctrl);
    const double blocked = mIn.Step(x);
    const double rect = std::fabs(ctrl * blocked);
    const double coeff = rect > mEnv ? mAttack : mRelease;
    mEnv = mEnv + (1.0 - coeff) * (rect - mEnv);
    if (!std::isfinite(mEnv) || mEnv < 0.0)
      mEnv = 0.0;
    double norm = 0.0;
    if (mEnv > kFloor)
      norm = std::min(rect / mEnv, 1.0);
    const double shaped = ctrl == 0.0 ? 0.0 : std::tanh(ctrl * kBottomDrive * norm);
    const double squared = std::tanh(kSquareDrive * shaped);
    out1 = ClampRail(mOut1.Step(kBottomOut * shaped));
    out2 = ClampRail(mOut2.Step(kBottomOut * squared));
  }

private:
  serge::DcBlock mIn;
  serge::DcBlock mOut1;
  serge::DcBlock mOut2;
  double mEnv = 0.0;
  double mAttack = 0.0;
  double mRelease = 0.0;
};

} // namespace vcm
