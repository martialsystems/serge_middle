// Serge Wave Multipliers middle section, C++ port of wave_middle.py.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// Header-only and free of iPlug2, so the tests in dsp/tests build without it.
// The audio path never allocates, locks, or throws.

#pragma once

#include <cmath>

#include "WaveMiddleTables.h"

namespace serge
{

// Shockley parameters for one cell, v1 lock. Esqueda et al. 2017 Table 4
// (eta 1.752, VT 0.025864) is a different fit and is not the plugin.
inline constexpr double kVT = 0.02585;
inline constexpr double kIs = 2.52e-9;
inline constexpr double kEta = 1.68;
inline constexpr double kR = 33000.0;

inline constexpr int kCells = 6;
inline constexpr double kGMin = 0.5;
inline constexpr double kGMax = 8.0;
inline constexpr double kGDefault = 1.0;

inline constexpr double kAudioFullScaleVolts = 5.0;
inline constexpr double kFullScalePeakVin = 4.7072868603604885;
inline constexpr double kOutputGain = 4.3792716960440945;

inline constexpr double kDcBlockHz = 10.0;
inline constexpr double kSmoothSeconds = 0.020;
inline constexpr int kOversample = 4;

inline constexpr double kPi = 3.141592653589793;

namespace detail
{
inline const double kEtaVT = kEta * kVT;
inline const double kLogK = std::log((kIs * kR) / kEtaVT);

// Seed for the principal branch. logZ is ln of the W argument.
inline double W0Initial(double logZ)
{
  if (logZ < -2.0)
  {
    const double z = std::exp(logZ);
    const double z2 = z * z;
    return z - z2 + 1.5 * z2 * z - (8.0 / 3.0) * z2 * z2;
  }
  if (logZ < 1.0)
  {
    const double z = std::exp(logZ);
    return z / (1.0 + z);
  }
  const double l2 = std::log(logZ);
  return logZ - l2 + l2 / logZ;
}
} // namespace detail

// Principal Lambert W of (Is R)/(eta VT) * exp(|v|/(eta VT)).
// Halley on w + ln(w) - ln(z) = 0. The product k * exp(|v|/(eta VT)) is never
// formed. absV must be finite and non-negative; callers guarantee it.
inline double LambertW0KExp(double absV)
{
  const double logZ = absV / detail::kEtaVT + detail::kLogK;
  double w = detail::W0Initial(logZ);
  if (!std::isfinite(w) || w <= 0.0)
  {
    w = std::exp(std::fmin(logZ, 0.0));
    if (w <= 0.0)
      w = 1e-300;
  }
  const double scale = std::fmax(1.0, std::fabs(logZ));
  for (int i = 0; i < 8; i++)
  {
    const double logW = std::log(w);
    const double f = w + logW - logZ;
    if (std::fabs(f) <= 1e-14 * scale)
      return w;
    const double invW = 1.0 / w;
    const double fp = 1.0 + invW;
    // f'' = -1/w^2, so the Halley denominator is fp + f/(2 fp w^2).
    const double step = f / (fp + f * (invW * invW) / (2.0 * fp));
    double wNext = w - step;
    if (!std::isfinite(wNext) || wNext <= 0.0)
      wNext = 0.5 * w;
    if (wNext == w)
      return w;
    w = wNext;
  }
  return w;
}

// Op-amp positive-input node. v_plus = sign(v) * (|v| - eta VT W(z)), v_plus(0) = 0.
inline double DiodeNode(double vIn)
{
  if (vIn == 0.0)
    return 0.0;
  const double mag = std::fabs(vIn);
  const double signedMag = mag - detail::kEtaVT * LambertW0KExp(mag);
  return vIn < 0.0 ? -signedMag : signedMag;
}

// One stage. v_out = 2 v_plus - v_in. v_out(0) = 0.
inline double Cell(double vIn)
{
  if (vIn == 0.0)
    return 0.0;
  return 2.0 * DiodeNode(vIn) - vIn;
}

// y = C applied `cells` times to g * x. The acceptance curve is Process(x).
inline double Process(double x, double g = kGDefault, int cells = kCells)
{
  double y = g * x;
  for (int i = 0; i < cells; i++)
    y = Cell(y);
  return y;
}

// d(g). Identity on [kGMin, kGMax], clamped outside. A non-finite g reads as the default.
inline double DriveScale(double g)
{
  if (!std::isfinite(g))
    return kGDefault;
  if (g < kGMin)
    return kGMin;
  if (g > kGMax)
    return kGMax;
  return g;
}

// P(g), linear in g between the DRIVE_PEAKS knots.
inline double DrivePeak(double g)
{
  g = DriveScale(g);
  if (g <= kDrivePeakG[0])
    return kDrivePeakP[0];
  const int last = kDrivePeakCount - 1;
  if (g >= kDrivePeakG[last])
    return kDrivePeakP[last];
  int lo = 0;
  int hi = last;
  while (hi - lo > 1)
  {
    const int mid = (lo + hi) / 2;
    if (kDrivePeakG[mid] <= g)
      lo = mid;
    else
      hi = mid;
  }
  const double g0 = kDrivePeakG[lo];
  const double p0 = kDrivePeakP[lo];
  const double t = (g - g0) / (kDrivePeakG[hi] - g0);
  return p0 + t * (kDrivePeakP[hi] - p0);
}

// OUTPUT_GAIN * P(1) / P(g).
inline double OutputScale(double g)
{
  return kOutputGain * DrivePeak(kGDefault) / DrivePeak(g);
}

// One sample with no oversampling and no DC block: the reference for the
// plugin with both filters bypassed.
inline double AudioMap(double sample, double g = kGDefault)
{
  const double volts = kAudioFullScaleVolts * DriveScale(g) * sample;
  return OutputScale(g) * Process(volts);
}

// c = exp(-1 / (0.020 * fs)).
inline double SmoothCoeff(double sampleRate)
{
  return std::exp(-1.0 / (kSmoothSeconds * sampleRate));
}

// r = exp(-2 pi 10 / fs).
inline double DcBlockPole(double sampleRate)
{
  return std::exp(-2.0 * kPi * kDcBlockHz / sampleRate);
}

// One-pole smoother on the fold amount, stepped once per audio sample.
class FoldSmoother
{
public:
  void Reset(double target, double sampleRate)
  {
    mCoeff = SmoothCoeff(sampleRate);
    mState = DriveScale(target);
  }

  // g[n] = g[n-1] + (1 - c) * (g_target - g[n-1]), target clamped first.
  double Step(double target)
  {
    mState = mState + (1.0 - mCoeff) * (DriveScale(target) - mState);
    return mState;
  }

  double Value() const { return mState; }

private:
  double mCoeff = 0.0;
  double mState = kGDefault;
};

// 63-tap direct-form FIR. Summation order matches fir_step: newest sample first.
class Fir63
{
public:
  explicit Fir63(const double* pTaps) : mTaps(pTaps) { Reset(); }

  void Reset()
  {
    for (double& v : mBuf)
      v = 0.0;
    mPos = 0;
  }

  // Push x and return the filter output for it.
  double Step(double x)
  {
    mPos = (mPos == 0) ? kFirTaps - 1 : mPos - 1;
    mBuf[mPos] = x;
    mBuf[mPos + kFirTaps] = x;
    const double* pHist = mBuf + mPos; // pHist[k] is the input k samples ago
    double acc = mTaps[0] * pHist[0];
    for (int k = 1; k < kFirTaps; k++)
      acc += mTaps[k] * pHist[k];
    return acc;
  }

  // Push x without computing an output.
  void Push(double x)
  {
    mPos = (mPos == 0) ? kFirTaps - 1 : mPos - 1;
    mBuf[mPos] = x;
    mBuf[mPos + kFirTaps] = x;
  }

private:
  const double* mTaps;
  double mBuf[2 * kFirTaps];
  int mPos = 0;
};

// y[n] = x[n] - x[n-1] + r y[n-1], state starts at zero.
class DcBlock
{
public:
  void Reset(double sampleRate)
  {
    mPole = DcBlockPole(sampleRate);
    mXPrev = 0.0;
    mYPrev = 0.0;
  }

  double Step(double x)
  {
    const double y = x - mXPrev + mPole * mYPrev;
    mXPrev = x;
    mYPrev = y;
    return y;
  }

private:
  double mPole = 0.0;
  double mXPrev = 0.0;
  double mYPrev = 0.0;
};

// One audio channel of the full path, from v = 5 g a to the DC block.
// The smoothed g comes from a FoldSmoother shared by all channels.
class WaveMiddleChannel
{
public:
  WaveMiddleChannel() : mUp(kUpsampleTaps), mDown(kHalfbandTaps) {}

  void Reset(double sampleRate)
  {
    mUp.Reset();
    mDown.Reset();
    mDc.Reset(sampleRate);
  }

  // Test hook: replace the six cells with the identity.
  void SetCellsBypassed(bool bypass) { mCellsBypassed = bypass; }

  // Up to and including the fold-amount bound, before the DC block.
  double StepPreDc(double a, double g)
  {
    if (!std::isfinite(a))
      a = 0.0;
    const double v = kAudioFullScaleVolts * DriveScale(g) * a;
    double decimated = 0.0;
    for (int phase = 0; phase < kOversample; phase++)
    {
      // Insert three zeros after the sample, then the upsampler taps.
      const double hi = mUp.Step(phase == 0 ? v : 0.0);
      const double cells = mCellsBypassed ? hi : Process(hi);
      // Decimate by 4, keeping phase 0.
      if (phase == 0)
        decimated = mDown.Step(cells);
      else
        mDown.Push(cells);
    }
    return OutputScale(g) * decimated;
  }

  double Step(double a, double g) { return mDc.Step(StepPreDc(a, g)); }

private:
  Fir63 mUp;
  Fir63 mDown;
  DcBlock mDc;
  bool mCellsBypassed = false;
};

} // namespace serge
