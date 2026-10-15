#pragma once

// Plugin output coupling for the Slope AC host sample.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// The jack equation AC = 2.5 - OUT stays in the circuit. This block is the
// capacitor after that jack: host = B(AC - 2.5) / 2.5. B is the locked
// one-pole, r = exp(-2 * pi * 10 / fs), state starts at 0. AC - 2.5 is 0
// while the slope idles at 0 V, so the first host sample is 0.

#include "SecondHalf.h"

#include <cmath>

namespace slope
{

inline double AcCouplerPole(double fs)
{
  return std::exp(-2.0 * std::acos(-1.0) * 10.0 / fs);
}

class AcCoupler
{
public:
  void SetSampleRate(double fs) { mPole = AcCouplerPole(fs); }

  void Reset()
  {
    mXPrev = 0.0;
    mYPrev = 0.0;
  }

  // One sample of y[n] = x[n] - x[n-1] + r * y[n-1].
  double Step(double x)
  {
    const double y = x - mXPrev + mPole * mYPrev;
    mXPrev = x;
    mYPrev = y;
    return y;
  }

  double Pole() const { return mPole; }

private:
  double mPole = 0.0;
  double mXPrev = 0.0;
  double mYPrev = 0.0;
};

// host = B(AC - 2.5) / 2.5. acVolts is the circuit AC jack, not AC / 2.5.
inline double HostSample(AcCoupler& coupler, double acVolts)
{
  return coupler.Step(acVolts - kAcCenter) / kAcCenter;
}

// One host sample: the circuit advance, then the coupling formula above.
inline double HostSample(AcCoupler& coupler, SecondHalf& half, double rise, double fall, double host,
                         bool arm, bool cycle)
{
  return HostSample(coupler, AdvanceSample(half, rise, fall, host, arm, cycle).ac);
}

// Same path with the VC jack, the attenuverter, and the RISE / BOTH / FALL route.
// route is 0 rise, 1 both, 2 fall. vcDepth 0 passes none of vc.
inline double HostSample(AcCoupler& coupler, SecondHalf& half, double rise, double fall, double host,
                         bool arm, bool cycle, double vc, double vcDepth, int route)
{
  half.SetSwitch(SwitchFromInt(route));
  return HostSample(coupler, AdvanceSample(half, rise, fall, host, arm, cycle, vc, vcDepth).ac);
}

} // namespace slope
