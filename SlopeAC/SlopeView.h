#pragma once

// Editor for the Slope AC plate. ProcessBlock does not include this header.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// Fractions match the stamped slope_plate PNG.
// Jacks IN, TRIG, OUT, and AC are drawn on the plate at x 0.18, 0.39, 0.61,
// and 0.82, y 0.785. This round does not attach a cable control.

#include "IControls.h"

#include "dsp/Readout.h"
#include "dsp/SecondHalf.h"

#include <cmath>
#include <cstdio>
#include <functional>

namespace slopeview
{
namespace gfx = iplug::igraphics;

inline constexpr float kRiseX = 0.30f;
inline constexpr float kFallX = 0.70f;
inline constexpr float kKnobY = 0.38f;
inline constexpr float kKnobD = 0.29f;
inline constexpr float kCaptionW = 0.30f;
inline constexpr float kCaptionTop = 0.495f;
inline constexpr float kCaptionBottom = 0.545f;
inline constexpr float kSwitchX = 0.30f;
inline constexpr float kSwitchY = 0.655f;
inline constexpr float kSwitchW = 48.f;
inline constexpr float kSwitchH = 28.f;
inline constexpr float kRateX = 0.70f;
inline constexpr float kRateTop = 0.633f;
inline constexpr float kRateBottom = 0.677f;

inline gfx::IRECT KnobBounds(const gfx::IRECT& plate, float xFrac)
{
  const float diameter = kKnobD * plate.W();
  const float cx = plate.L + xFrac * plate.W();
  const float cy = plate.T + kKnobY * plate.H();
  // The contact shadow sits just outside the skirt, and drawing is clipped to the control.
  const float pad = 14.f;
  return gfx::IRECT(cx - diameter * 0.5f - pad, cy - diameter * 0.5f - pad,
                    cx + diameter * 0.5f + pad, cy + diameter * 0.5f + pad);
}

// Value caption centered on a knob. The plate bitmap carries the knob name.
inline gfx::IRECT CaptionBounds(const gfx::IRECT& plate, float xFrac)
{
  const float cx = plate.L + xFrac * plate.W();
  const float half = 0.5f * kCaptionW * plate.W();
  return gfx::IRECT(cx - half, plate.T + kCaptionTop * plate.H(),
                    cx + half, plate.T + kCaptionBottom * plate.H());
}

inline gfx::IRECT SwitchBounds(const gfx::IRECT& plate)
{
  const float cx = plate.L + kSwitchX * plate.W();
  const float cy = plate.T + kSwitchY * plate.H();
  return gfx::IRECT(cx - 0.5f * kSwitchW, cy - 0.5f * kSwitchH,
                    cx + 0.5f * kSwitchW, cy + 0.5f * kSwitchH);
}

inline gfx::IRECT RateBounds(const gfx::IRECT& plate)
{
  const float cx = plate.L + kRateX * plate.W();
  const float half = 0.5f * kCaptionW * plate.W();
  return gfx::IRECT(cx - half, plate.T + kRateTop * plate.H(),
                    cx + half, plate.T + kRateBottom * plate.H());
}

// Skirted cap. The pointer and its shadow are one bitmap, so they share one angle.
class SlopeKnob final : public gfx::IKnobControlBase
{
public:
  SlopeKnob(const gfx::IRECT& bounds, int paramIdx, const gfx::IBitmap& bitmap)
  : gfx::IKnobControlBase(bounds, paramIdx, gfx::EDirection::Vertical)
  , mBitmap(bitmap)
  {
  }

  void Draw(gfx::IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float radius = 0.5f * std::min(mRECT.W(), mRECT.H()) - 14.f;
    g.FillEllipse(gfx::IColor(90, 0, 0, 0), cx, cy + radius * 0.08f, radius * 1.02f, radius * 0.96f);
    const double angle = -130.0 + GetValue() * 260.0;
    g.DrawRotatedBitmap(mBitmap, cx, cy, angle);
  }

private:
  gfx::IBitmap mBitmap;
};

// Free-cycle rate while Cycle is on. OFF while the cord is open.
class RateReadout final : public gfx::IControl
{
public:
  RateReadout(const gfx::IRECT& bounds, int riseParam, int fallParam, int cycleParam,
              std::function<double()> sampleRate)
  : gfx::IControl(bounds)
  , mRiseParam(riseParam)
  , mFallParam(fallParam)
  , mCycleParam(cycleParam)
  , mSampleRate(std::move(sampleRate))
  , mText(14.f, gfx::IColor(235, 238, 240, 245), "Roboto-Regular", gfx::EAlign::Center, gfx::EVAlign::Middle)
  {
    SetIgnoreMouse(true);
  }

  bool IsDirty() override
  {
    if (mSampleRate && mSampleRate() != mDrawnFs)
      return true;
    return gfx::IControl::IsDirty();
  }

  void Draw(gfx::IGraphics& g) override
  {
    const double rise = GetDelegate()->GetParam(mRiseParam)->Value();
    const double fall = GetDelegate()->GetParam(mFallParam)->Value();
    const bool cycle = GetDelegate()->GetParam(mCycleParam)->Bool();
    const double fs = mSampleRate ? mSampleRate() : slope::kFs;
    char buf[32];
    if (cycle)
      slope::FormatHz(slope::CycleHz(rise, fall, fs), buf, sizeof buf);
    else
      std::snprintf(buf, sizeof buf, "OFF");
    mDrawnFs = fs;
    g.DrawText(mText, buf, mRECT);
  }

private:
  int mRiseParam;
  int mFallParam;
  int mCycleParam;
  std::function<double()> mSampleRate;
  gfx::IText mText;
  double mDrawnFs = -1.0;
};

} // namespace slopeview
