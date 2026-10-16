#pragma once

// Editor for the combined plate. ProcessBlock does not include this header.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// The plate art carries the lettering. This file draws the live knobs, the
// Cycle switch is attached by the plugin, and this control draws cables and
// hover labels. Five panels: top, PLEAT, bottom, slope 1, slope 2.
// Jack fractions match middle_plate.svg. The COUPLE lever is plate art only.

#include "SergeMiddleCombined.h"

#include "../SergeMiddle/PleatView.h"
#include "../SlopeAC/SlopeView.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace patchview
{
namespace gfx = iplug::igraphics;

inline constexpr float kJackHit = 40.0f;
inline constexpr float kJackY = 0.785f;
static_assert(kJackHit >= 28.f, "jack hit target");

struct JackRef
{
  bool output;
  int index;
  float xFrac;
  int panel;
  const char* label;
};

// middle_plate.png is five panels of equal width, left to right.
inline constexpr int kPanelCount = 5;
inline constexpr int kTopPanel = 0;
inline constexpr int kPleatPanel = 1;
inline constexpr int kBottomPanel = 2;
inline constexpr int kSlopePanel = 3;
inline constexpr int kSlope2Panel = 4;

inline constexpr int kJackCount = 23;
inline constexpr JackRef kJacks[kJackCount] = {
    {false, patch::PleatIn1, 0.18f, kPleatPanel, "PLEAT IN 1"},
    {false, patch::PleatIn2, 0.39f, kPleatPanel, "PLEAT IN 2"},
    {false, patch::PleatVc, 0.61f, kPleatPanel, "PLEAT VC"},
    {true, patch::PleatOut, 0.82f, kPleatPanel, "PLEAT OUT"},
    {false, patch::SlopeIn, 0.12f, kSlopePanel, "SLOPE IN"},
    {false, patch::SlopeTrig, 0.27f, kSlopePanel, "SLOPE TRIG"},
    {false, patch::SlopeVc, 0.42f, kSlopePanel, "SLOPE VC"},
    {true, patch::SlopeEnd, 0.58f, kSlopePanel, "SLOPE END"},
    {true, patch::SlopeOut, 0.73f, kSlopePanel, "SLOPE OUT"},
    {true, patch::SlopeAc, 0.88f, kSlopePanel, "SLOPE AC"},
    {false, patch::TopIn, 0.22f, kTopPanel, "TOP IN"},
    {false, patch::TopVc, 0.50f, kTopPanel, "TOP VC"},
    {true, patch::TopOut, 0.78f, kTopPanel, "TOP OUT"},
    {false, patch::BottomIn, 0.18f, kBottomPanel, "BOTTOM IN"},
    {false, patch::BottomVc, 0.39f, kBottomPanel, "BOTTOM VC"},
    {true, patch::BottomOut1, 0.61f, kBottomPanel, "BOTTOM OUT 1"},
    {true, patch::BottomOut2, 0.82f, kBottomPanel, "BOTTOM OUT 2"},
    {false, patch::Slope2In, 0.12f, kSlope2Panel, "SLOPE 2 IN"},
    {false, patch::Slope2Trig, 0.27f, kSlope2Panel, "SLOPE 2 TRIG"},
    {false, patch::Slope2Vc, 0.42f, kSlope2Panel, "SLOPE 2 VC"},
    {true, patch::Slope2End, 0.58f, kSlope2Panel, "SLOPE 2 END"},
    {true, patch::Slope2Out, 0.73f, kSlope2Panel, "SLOPE 2 OUT"},
    {true, patch::Slope2Ac, 0.88f, kSlope2Panel, "SLOPE 2 AC"},
};

// VC Depth sits on the PLEAT panel. 253.76 is 0.61 of a 416-wide panel.
inline gfx::IRECT DepthBounds(const gfx::IRECT& pleat)
{
  const float cx = pleat.L + 253.76f;
  const float cy = pleat.T + 396.f;
  const float half = 22.f + 8.f;
  return gfx::IRECT(cx - half, cy - half, cx + half, cy + half);
}

// The pleat bitmap is the large cap. Scale it to the 44 px skirt.
class DepthKnob final : public gfx::IKnobControlBase
{
public:
  DepthKnob(const gfx::IRECT& bounds, int paramIdx, const gfx::IBitmap& bitmap)
  : gfx::IKnobControlBase(bounds, paramIdx, gfx::EDirection::Vertical)
  , mBitmap(bitmap)
  {
  }

  void Draw(gfx::IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float radius = 22.f;
    g.FillEllipse(gfx::IColor(90, 0, 0, 0), cx, cy + radius * 0.08f, radius * 1.02f, radius * 0.96f);
    const float angle = static_cast<float>(-130.0 + GetValue() * 260.0);
    const float bw = static_cast<float>(mBitmap.W()) / mBitmap.GetDrawScale();
    const float bh = static_cast<float>(mBitmap.H()) / mBitmap.GetDrawScale();
    const float scale = 44.f / bw;
    g.PathTransformSave();
    g.PathTransformTranslate(cx, cy);
    g.PathTransformRotate(angle);
    g.PathTransformScale(scale);
    g.DrawBitmap(mBitmap, gfx::IRECT(-0.5f * bw, -0.5f * bh, 0.5f * bw, 0.5f * bh));
    g.PathTransformRestore();
  }

private:
  gfx::IBitmap mBitmap;
};

inline gfx::IColor CableColor(int index)
{
  static const gfx::IColor colors[4] = {
      {255, 190, 48, 42},
      {255, 236, 236, 230},
      {255, 224, 186, 48},
      {255, 46, 150, 78},
  };
  const int wrapped = index % 4;
  return colors[wrapped < 0 ? 0 : wrapped];
}

// Cables and jack hit targets. IsHit is true only on a jack, so knobs underneath still receive clicks.
class CableControl final : public gfx::IControl
{
public:
  CableControl(const gfx::IRECT& bounds, SergeMiddleCombined& plug)
  : gfx::IControl(bounds)
  , mPlug(&plug)
  , mText(13.f, gfx::IColor(240, 244, 240, 232), "Roboto-Regular", gfx::EAlign::Center, gfx::EVAlign::Middle)
  {
  }

  bool IsHit(float x, float y) const override { return JackAt(x, y) >= 0; }

  bool IsDirty() override
  {
    if (mHover >= 0 || mDragging)
      return true;
    return gfx::IControl::IsDirty();
  }

  void OnMouseOver(float x, float y, const gfx::IMouseMod& mod) override
  {
    gfx::IControl::OnMouseOver(x, y, mod);
    const int jack = JackAt(x, y);
    if (jack != mHover)
    {
      mHover = jack;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    gfx::IControl::OnMouseOut();
    if (mHover != -1)
    {
      mHover = -1;
      SetDirty(false);
    }
  }

  void OnMouseDown(float x, float y, const gfx::IMouseMod& mod) override
  {
    const int jack = JackAt(x, y);
    if (jack < 0)
      return;
    if (mod.R)
    {
      mMenu.Clear();
      mMenu.AddItem("Unplug");
      mMenu.AddItem("Unplug all");
      mMenuJack = jack;
      GetUI()->CreatePopupMenu(*this, mMenu, x, y);
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
    {
      patch::Routing routing = mPlug->RoutingSnapshot();
      const JackRef& from = kJacks[mFrom];
      const JackRef& to = kJacks[jack];
      if (patch::PatchJacks(routing, from.output, from.index, to.output, to.index))
        mPlug->ApplyRouting(routing);
    }
    SetDirty(false);
  }

  void OnPopupMenuSelection(gfx::IPopupMenu* pMenu, int) override
  {
    if (pMenu == nullptr || mMenuJack < 0 || mMenuJack >= kJackCount)
      return;
    const int chosen = pMenu->GetChosenItemIdx();
    if (chosen < 0)
      return;
    patch::Routing routing = mPlug->RoutingSnapshot();
    if (chosen == 0)
    {
      const JackRef& jack = kJacks[mMenuJack];
      routing.DisconnectJack(jack.output, jack.index);
    }
    else if (chosen == 1)
      routing.Clear();
    else
      return;
    mPlug->ApplyRouting(routing);
    SetDirty(false);
  }

  void Draw(gfx::IGraphics& g) override
  {
    const patch::Routing routing = mPlug->RoutingSnapshot();
    for (int input = 0; input < patch::kNumInputs; ++input)
    {
      const int source = routing.source[input];
      if (source < 0 || source >= patch::kNumOutputs)
        continue;
      float x0 = 0.f;
      float y0 = 0.f;
      float x1 = 0.f;
      float y1 = 0.f;
      if (!FindCentre(true, source, x0, y0) || !FindCentre(false, input, x1, y1))
        continue;
      // Keyed by the input, so unplugging one cable does not recolour the rest.
      StrokeCable(g, x0, y0, x1, y1, CableColor(input));
    }
    if (mDragging && mFrom >= 0)
    {
      float x0 = 0.f;
      float y0 = 0.f;
      JackCentre(mFrom, x0, y0);
      int liveInput = -1;
      const int hover = JackAt(mX, mY);
      if (hover >= 0 && !kJacks[hover].output)
        liveInput = kJacks[hover].index;
      const gfx::IColor live = liveInput >= 0 ? CableColor(liveInput) : CableColor(0);
      StrokeCable(g, x0, y0, mX, mY, live);
    }
    DrawLabel(g, routing);
  }

private:
  SergeMiddleCombined* mPlug;
  gfx::IText mText;
  gfx::IPopupMenu mMenu;
  int mFrom = -1;
  int mHover = -1;
  int mMenuJack = -1;
  float mX = 0.f;
  float mY = 0.f;
  bool mDragging = false;

  void JackCentre(int index, float& x, float& y) const
  {
    const JackRef& jack = kJacks[index];
    const float panelW = mRECT.W() / static_cast<float>(kPanelCount);
    const float origin = mRECT.L + static_cast<float>(jack.panel) * panelW;
    x = origin + jack.xFrac * panelW;
    y = mRECT.T + kJackY * mRECT.H();
  }

  bool FindCentre(bool output, int index, float& x, float& y) const
  {
    for (int i = 0; i < kJackCount; ++i)
    {
      if (kJacks[i].output == output && kJacks[i].index == index)
      {
        JackCentre(i, x, y);
        return true;
      }
    }
    return false;
  }

  int JackAt(float x, float y) const
  {
    const float half = kJackHit * 0.5f;
    for (int i = 0; i < kJackCount; ++i)
    {
      float jx = 0.f;
      float jy = 0.f;
      JackCentre(i, jx, jy);
      if (x >= jx - half && x < jx + half && y >= jy - half && y < jy + half)
        return i;
    }
    return -1;
  }

  void StrokeCable(gfx::IGraphics& g, float x0, float y0, float x1, float y1, const gfx::IColor& color) const
  {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float span = std::sqrt(dx * dx + dy * dy);
    const float sag = std::max(24.f, span * 0.35f);
    const float c1x = x0 + 0.35f * dx;
    const float c1y = y0 + 0.35f * dy + sag;
    const float c2x = x0 + 0.65f * dx;
    const float c2y = y0 + 0.65f * dy + sag;
    gfx::IStrokeOptions opt;
    opt.mCapOption = gfx::ELineCap::Round;
    opt.mJoinOption = gfx::ELineJoin::Round;
    const gfx::IColor outline(255, 24, 22, 20);
    g.PathClear();
    g.PathMoveTo(x0, y0);
    g.PathCubicBezierTo(c1x, c1y, c2x, c2y, x1, y1);
    g.PathStroke(outline, 9.f, opt);
    g.PathClear();
    g.PathMoveTo(x0, y0);
    g.PathCubicBezierTo(c1x, c1y, c2x, c2y, x1, y1);
    g.PathStroke(color, 6.f, opt);
    g.FillCircle(outline, x0, y0, 7.f);
    g.FillCircle(color, x0, y0, 5.f);
    g.FillCircle(outline, x1, y1, 7.f);
    g.FillCircle(color, x1, y1, 5.f);
  }

  void DrawLabel(gfx::IGraphics& g, const patch::Routing& routing) const
  {
    if (mHover < 0 || mHover >= kJackCount)
      return;
    const JackRef& jack = kJacks[mHover];
    float lo = 0.f;
    float hi = 0.f;
    mPlug->JackMeter(patch::MeterOf(jack.output, jack.index), lo, hi);
    if (hi < lo)
    {
      const float swap = lo;
      lo = hi;
      hi = swap;
    }
    const bool normal = !jack.output && routing.source[jack.index] < 0;
    char buf[96];
    if (hi - lo > 0.05f)
      std::snprintf(buf, sizeof buf, "%s%s  %+.2f .. %+.2f V", jack.label, normal ? " (normal)" : "", lo, hi);
    else
      std::snprintf(buf, sizeof buf, "%s%s  %+.2f V", jack.label, normal ? " (normal)" : "", 0.5f * (lo + hi));

    gfx::IRECT measured;
    g.MeasureText(mText, buf, measured);
    const float boxW = std::max(measured.W() + 12.f, 72.f);
    const float boxH = std::max(measured.H() + 8.f, 22.f);
    float jx = 0.f;
    float jy = 0.f;
    JackCentre(mHover, jx, jy);
    float left = jx - 0.5f * boxW;
    float top = jy - 0.5f * kJackHit - 6.f - boxH;
    if (top < mRECT.T + 4.f)
      top = jy + 0.5f * kJackHit + 6.f;
    if (left < mRECT.L + 4.f)
      left = mRECT.L + 4.f;
    if (left + boxW > mRECT.R - 4.f)
      left = mRECT.R - 4.f - boxW;
    const gfx::IRECT box(left, top, left + boxW, top + boxH);
    g.FillRoundRect(gfx::IColor(230, 20, 18, 16), box, 4.f);
    g.DrawText(mText, buf, box);
  }
};

} // namespace patchview
