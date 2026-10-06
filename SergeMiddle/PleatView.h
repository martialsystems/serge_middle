#pragma once

// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "IControls.h"

#include <cmath>
#include <vector>

// Editor for the enamel plate. ProcessBlock does not include this header.
// Fractions match the stamped pleat_plate PNG.

namespace pleat
{
namespace gfx = iplug::igraphics;

inline constexpr float kKnobX = 0.50f;
inline constexpr float kKnobY = 0.40f;
inline constexpr float kKnobD = 0.42f;
inline constexpr float kJackY = 0.785f;
inline constexpr float kJackD = 0.145f;
inline constexpr int kJackCount = 4;
inline constexpr float kJackX[kJackCount] = {0.18f, 0.39f, 0.61f, 0.82f};

inline gfx::IRECT KnobBounds(const gfx::IRECT& plate)
{
  const float diameter = kKnobD * plate.W();
  const float cx = plate.L + kKnobX * plate.W();
  const float cy = plate.T + kKnobY * plate.H();
  // The contact shadow sits just outside the skirt, and drawing is clipped to the control.
  const float pad = 14.f;
  return gfx::IRECT(cx - diameter * 0.5f - pad, cy - diameter * 0.5f - pad,
                    cx + diameter * 0.5f + pad, cy + diameter * 0.5f + pad);
}

// Fold, drawn as the skirted cap. The pointer and its shadow are one bitmap, so they share one angle.
class PleatKnob final : public gfx::IKnobControlBase
{
public:
  PleatKnob(const gfx::IRECT& bounds, int paramIdx, const gfx::IBitmap& bitmap)
  : gfx::IKnobControlBase(bounds, paramIdx, gfx::EDirection::Vertical)
  , mBitmap(bitmap)
  {
  }

  void Draw(gfx::IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float radius = 0.5f * std::min(mRECT.W(), mRECT.H()) - 14.f;
    g.FillEllipse(gfx::IColor(90, 0, 0, 0), cx, cy + 10.f, radius * 1.02f, radius * 0.96f);
    const double angle = -130.0 + GetValue() * 260.0;
    g.DrawRotatedBitmap(mBitmap, cx, cy, angle);
  }

private:
  gfx::IBitmap mBitmap;
};

// Rope between banana jacks. The audio path does not read mCables.
class PleatCables final : public gfx::IControl
{
public:
  explicit PleatCables(const gfx::IRECT& bounds)
  : gfx::IControl(bounds)
  {
  }

  bool IsHit(float x, float y) const override
  {
    return JackAt(x, y) >= 0;
  }

  void OnMouseDown(float x, float y, const gfx::IMouseMod& mod) override
  {
    const int jack = JackAt(x, y);
    if (jack < 0)
      return;

    if (mod.R)
    {
      std::vector<Cable> kept;
      kept.reserve(mCables.size());
      for (const Cable& cable : mCables)
      {
        if (cable.a != jack && cable.b != jack)
          kept.push_back(cable);
      }
      mCables.swap(kept);
      mDragging = false;
      SetDirty(false);
      return;
    }

    mDragging = true;
    mFrom = jack;
    mX = x;
    mY = y;
    SetDirty(false);
  }

  void OnMouseDrag(float x, float y, float, float, const gfx::IMouseMod&) override
  {
    if (!mDragging)
      return;
    mX = x;
    mY = y;
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const gfx::IMouseMod&) override
  {
    if (!mDragging)
      return;
    mDragging = false;
    const int jack = JackAt(x, y);
    if (jack >= 0 && jack != mFrom)
      AddCable(mFrom, jack);
    SetDirty(false);
  }

  void Draw(gfx::IGraphics& g) override
  {
    for (const Cable& cable : mCables)
    {
      float x0 = 0.f;
      float y0 = 0.f;
      float x1 = 0.f;
      float y1 = 0.f;
      JackCentre(cable.a, x0, y0);
      JackCentre(cable.b, x1, y1);
      StrokeRope(g, x0, y0, x1, y1);
    }

    if (mDragging && mFrom >= 0)
    {
      float x0 = 0.f;
      float y0 = 0.f;
      JackCentre(mFrom, x0, y0);
      StrokeRope(g, x0, y0, mX, mY);
    }
  }

private:
  struct Cable
  {
    int a;
    int b;
  };

  std::vector<Cable> mCables;
  int mFrom = -1;
  float mX = 0.f;
  float mY = 0.f;
  bool mDragging = false;

  float JackDiameter() const { return kJackD * mRECT.W(); }

  void JackCentre(int index, float& x, float& y) const
  {
    x = mRECT.L + kJackX[index] * mRECT.W();
    y = mRECT.T + kJackY * mRECT.H();
  }

  int JackAt(float x, float y) const
  {
    const float reach = 0.55f * JackDiameter();
    const float reach2 = reach * reach;
    int found = -1;
    for (int i = 0; i < kJackCount; ++i)
    {
      float jx = 0.f;
      float jy = 0.f;
      JackCentre(i, jx, jy);
      const float dx = x - jx;
      const float dy = y - jy;
      if (dx * dx + dy * dy <= reach2)
        found = i;
    }
    return found;
  }

  void AddCable(int a, int b)
  {
    if (a > b)
    {
      const int swap = a;
      a = b;
      b = swap;
    }
    for (const Cable& cable : mCables)
    {
      if (cable.a == a && cable.b == b)
        return;
    }
    mCables.push_back(Cable{a, b});
  }

  void StrokeRope(gfx::IGraphics& g, float x0, float y0, float x1, float y1) const
  {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float span = std::sqrt(dx * dx + dy * dy);
    // The curve sags about half of this, because a quadratic stays inside the hull.
    const float sag = std::max(48.f, span * 0.55f);
    const float cx = 0.5f * (x0 + x1);
    float cy = std::max(y0, y1) + sag;
    const float limit = mRECT.B - 8.f;
    if (cy > limit)
      cy = limit;

    gfx::IStrokeOptions opt;
    opt.mCapOption = gfx::ELineCap::Round;
    opt.mJoinOption = gfx::ELineJoin::Round;
    const float thickness = std::max(6.f, JackDiameter() * 0.13f);

    g.PathClear();
    g.PathMoveTo(x0, y0);
    g.PathQuadraticBezierTo(cx, cy, x1, y1);
    g.PathStroke(gfx::IColor(235, 36, 28, 24), thickness, opt);

    g.PathClear();
    g.PathMoveTo(x0, y0);
    g.PathQuadraticBezierTo(cx - 1.2f, cy - 1.8f, x1, y1);
    g.PathStroke(gfx::IColor(160, 122, 96, 78), std::max(2.f, thickness * 0.28f), opt);

    const float plug = JackDiameter() * 0.20f;
    g.FillEllipse(gfx::IColor(255, 24, 22, 20), x0, y0, plug, plug);
    g.FillEllipse(gfx::IColor(255, 24, 22, 20), x1, y1, plug, plug);
    g.FillEllipse(gfx::IColor(210, 86, 76, 66), x0, y0, plug * 0.32f, plug * 0.32f);
    g.FillEllipse(gfx::IColor(210, 86, 76, 66), x1, y1, plug * 0.32f, plug * 0.32f);
  }
};

} // namespace pleat
