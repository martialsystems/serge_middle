#pragma once

// Editor for the Slope AC plate. ProcessBlock does not include this header.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// Fractions match the stamped slope_plate PNG.
// Jacks IN, TRIG, VC, END, OUT, and AC are drawn on the plate at x 0.12, 0.27,
// 0.42, 0.58, 0.73, and 0.88, y 0.785. This plate does not attach a cable control.
// The host sample stays on IN. The VC jack is unpatched, so its voltage is 0 V.

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
// Attenuverter, above the divider. Route switch, below it. Fractions match the plate.
inline constexpr float kVcX = 0.50f;
inline constexpr float kVcY = 0.529f;
inline constexpr float kVcD = 40.f;
inline constexpr float kRouteX = 0.481f;
inline constexpr float kRouteY = 0.615f;
inline constexpr float kRouteW = 48.f;
inline constexpr float kRouteH = 28.f;

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

inline gfx::IRECT VcKnobBounds(const gfx::IRECT& plate)
{
  const float cx = plate.L + kVcX * plate.W();
  const float cy = plate.T + kVcY * plate.H();
  const float pad = 8.f;
  const float half = 0.5f * kVcD + pad;
  return gfx::IRECT(cx - half, cy - half, cx + half, cy + half);
}

inline gfx::IRECT RouteBounds(const gfx::IRECT& plate)
{
  const float cx = plate.L + kRouteX * plate.W();
  const float cy = plate.T + kRouteY * plate.H();
  return gfx::IRECT(cx - 0.5f * kRouteW, cy - 0.5f * kRouteH,
                    cx + 0.5f * kRouteW, cy + 0.5f * kRouteH);
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

// The pleat bitmap scaled down to the VC attenuverter.
class ScaledKnob final : public gfx::IKnobControlBase
{
public:
  ScaledKnob(const gfx::IRECT& bounds, int paramIdx, const gfx::IBitmap& bitmap, float diameter)
  : gfx::IKnobControlBase(bounds, paramIdx, gfx::EDirection::Vertical)
  , mBitmap(bitmap)
  , mDiameter(diameter)
  {
  }

  void Draw(gfx::IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float radius = 0.5f * mDiameter;
    g.FillEllipse(gfx::IColor(90, 0, 0, 0), cx, cy + radius * 0.08f, radius * 1.02f, radius * 0.96f);
    const float angle = static_cast<float>(-130.0 + GetValue() * 260.0);
    const float bw = static_cast<float>(mBitmap.W()) / mBitmap.GetDrawScale();
    const float bh = static_cast<float>(mBitmap.H()) / mBitmap.GetDrawScale();
    const float scale = mDiameter / bw;
    g.PathTransformSave();
    g.PathTransformTranslate(cx, cy);
    g.PathTransformRotate(angle);
    g.PathTransformScale(scale);
    g.DrawBitmap(mBitmap, gfx::IRECT(-0.5f * bw, -0.5f * bh, 0.5f * bw, 0.5f * bh));
    g.PathTransformRestore();
  }

private:
  gfx::IBitmap mBitmap;
  float mDiameter;
};

// Three-frame lever. Frame 0 is RISE, frame 1 is BOTH, frame 2 is FALL.
// A click advances RISE, BOTH, FALL, then back to RISE.
class RouteSwitch final : public gfx::IControl
{
public:
  RouteSwitch(const gfx::IRECT& bounds, int paramIdx, const gfx::IBitmap& bitmap)
  : gfx::IControl(bounds, paramIdx)
  , mBitmap(bitmap)
  {
  }

  void Draw(gfx::IGraphics& g) override
  {
    int index = 1;
    if (GetDelegate() != nullptr && GetParamIdx() >= 0)
      index = GetDelegate()->GetParam(GetParamIdx())->Int();
    if (index < 0)
      index = 0;
    if (index > 2)
      index = 2;
    g.DrawBitmap(mBitmap, mRECT, index + 1, nullptr);
  }

  void OnMouseDown(float, float, const gfx::IMouseMod& mod) override
  {
    if (mod.R || GetDelegate() == nullptr)
      return;
    const int index = GetDelegate()->GetParam(GetParamIdx())->Int();
    const int next = (index + 1) % 3;
    const double norm = GetDelegate()->GetParam(GetParamIdx())->ToNormalized(static_cast<double>(next));
    SetValue(norm);
    SetDirty(true);
  }

private:
  gfx::IBitmap mBitmap;
};

// Free-cycle rate while Cycle is on. OFF while the cord is open.
class RateReadout final : public gfx::IControl
{
public:
  RateReadout(const gfx::IRECT& bounds, int riseParam, int fallParam, int cycleParam,
              std::function<double()> sampleRate,
              gfx::IColor color = gfx::IColor(235, 238, 240, 245))
  : gfx::IControl(bounds)
  , mRiseParam(riseParam)
  , mFallParam(fallParam)
  , mCycleParam(cycleParam)
  , mSampleRate(std::move(sampleRate))
  , mText(14.f, color, "Roboto-Regular", gfx::EAlign::Center, gfx::EVAlign::Middle)
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
