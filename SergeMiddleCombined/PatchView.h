#pragma once

// SLEWFOLD editor. ProcessBlock does not include this header.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// The plate is slewfold_v6.svg with the knob groups, the LED groups, and the
// moving switches removed (tools/art/render_slewfold.py). Knobs are frames of
// knob_filmstrip.png, 64 frames from -135 to +135 degrees, drawn at 72 px.
// data-angle is the angle the removed art was painted at. The live travel is
// the filmstrip. Hover draws the parameter text in the knob's readout rect.
// Toggles and LEDs are drawn here. COUPLE is a hit target and not a parameter.
//
// Cables are Lift's (martialsystems/lift, branch tape-engine-juce), drawn with
// IGraphics. See CableRope.h. An output can feed several inputs. An input
// holds one cable: the patch model does not sum. A drag from an occupied
// output starts another cable. Drag an input plug to move it. The routing
// bytes are unchanged, so an older project still loads.

#include "SergeMiddleCombined.h"
#include "CableRope.h"
#include "CableEnds.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace patchview
{

// Tag for the cable layer, so the plug can redraw it after loading a patch.
constexpr int kCableTag = 103;
namespace gfx = iplug::igraphics;

struct FaceState
{
  int knob = -1;
  char text[64] = {};
};

inline constexpr float kJackHit = 44.f;
inline constexpr float kKnobD = 72.f;
inline constexpr float kSwitchHit = 44.f;
inline constexpr float kDesignW = 1600.f;
inline constexpr float kDesignH = 960.f;
inline constexpr float kStackStep = 6.f;
static_assert(kStackStep == slewfold::kStackStep, "stack step");
inline constexpr int kFilmFrames = 64;
static_assert(kJackHit >= 28.f, "jack hit target");

using slewfold::JackSpot;
using slewfold::kJackCount;
using slewfold::kJacks;

struct KnobSpot
{
  const char* id;
  int param;
  float x;
  float y;
  float angle;
  float rx;
  float ry;
  float rw;
  float rh;
  // Diameter in design px. The test oscillator knobs are smaller.
  float d = kKnobD;
};

struct SwitchSpot
{
  const char* id;
  int param;
  float x;
  float y;
};

struct LedSpot
{
  const char* id;
  bool end;
  int meterOutput;
  int cycleParam;
  float x;
  float y;
};

// Jack centres live in CableEnds.h, the same list as tools/art/ids_v6.json.

inline constexpr int kKnobCount = 14;
inline constexpr KnobSpot kKnobs[kKnobCount] = {
    {"knob_top_gain", kTopGain, 270.f, 220.f, 42.f, 243.f, 249.f, 54.f, 19.f},
    {"knob_fold", kFold, 770.f, 190.f, -50.f, 743.f, 219.f, 54.f, 19.f},
    {"knob_in2_dc", kIn2, 770.f, 305.f, 17.f, 743.f, 334.f, 54.f, 19.f},
    {"knob_pleat_vc_depth", kVcDepth, 770.f, 420.f, 105.f, 743.f, 449.f, 54.f, 19.f},
    {"knob_bottom_level", kBottomGain, 1290.f, 220.f, 12.f, 1263.f, 249.f, 54.f, 19.f},
    {"knob_bottom_vc_atten", kBottomVc, 1290.f, 390.f, -73.f, 1263.f, 419.f, 54.f, 19.f},
    {"knob_slope1_rise", kRise, 320.f, 625.f, 60.f, 293.f, 654.f, 54.f, 19.f},
    {"knob_slope1_fall", kFall, 460.f, 625.f, -5.f, 433.f, 654.f, 54.f, 19.f},
    {"knob_slope1_vc_depth", kSlopeVc, 320.f, 825.f, -72.f, 293.f, 854.f, 54.f, 19.f},
    {"knob_slope2_rise", kRise2, 1100.f, 625.f, -29.f, 1073.f, 654.f, 54.f, 19.f},
    {"knob_slope2_fall", kFall2, 1240.f, 625.f, -46.f, 1213.f, 654.f, 54.f, 19.f},
    {"knob_slope2_vc_depth", kSlopeVc2, 1100.f, 825.f, 49.f, 1073.f, 854.f, 54.f, 19.f},
    {"knob_osc_freq", kOscFreq, 190.f, 66.f, 0.f, 222.f, 57.f, 54.f, 19.f, 56.f},
    {"knob_osc_level", kOscLevel, 320.f, 66.f, 0.f, 352.f, 57.f, 54.f, 19.f, 56.f},
};

inline constexpr int kSwitchCount = 8;
inline constexpr SwitchSpot kSwitches[kSwitchCount] = {
    {"switch_top_couple", -1, 230.f, 390.f},
    {"switch_top_mode", kTopHi, 340.f, 390.f},
    {"switch_slope1_cycle", kCycle, 460.f, 735.f},
    {"switch_slope1_vc_target", kRoute, 550.f, 825.f},
    {"switch_slope2_cycle", kCycle2, 1240.f, 735.f},
    {"switch_slope2_vc_target", kRoute2, 1330.f, 825.f},
    {"switch_osc_wave", kOscWave, 70.f, 66.f},
    {"switch_osc_to_in1", kOscToIn1, 530.f, 66.f},
};

inline constexpr int kLedCount = 4;
inline constexpr LedSpot kLeds[kLedCount] = {
    {"led_slope1_end", true, patch::SlopeEnd, -1, 770.f, 798.f},
    {"led_slope1_cycle", false, patch::SlopeOut, kCycle, 460.f, 772.f},
    {"led_slope2_end", true, patch::Slope2End, -1, 1550.f, 798.f},
    {"led_slope2_cycle", false, patch::Slope2Out, kCycle2, 1240.f, 772.f},
};

inline gfx::IRECT Spot(const gfx::IRECT& plate, float x, float y, float w, float h)
{
  const float sx = plate.W() / kDesignW;
  const float sy = plate.H() / kDesignH;
  const float cx = plate.L + x * sx;
  const float cy = plate.T + y * sy;
  return gfx::IRECT(cx - 0.5f * w * sx, cy - 0.5f * h * sy, cx + 0.5f * w * sx, cy + 0.5f * h * sy);
}

inline gfx::IRECT Placed(const gfx::IRECT& plate, float x, float y, float w, float h)
{
  const float sx = plate.W() / kDesignW;
  const float sy = plate.H() / kDesignH;
  const float l = plate.L + x * sx;
  const float t = plate.T + y * sy;
  return gfx::IRECT(l, t, l + w * sx, t + h * sy);
}

// Lift CLOTH, panel/PanelData.h. c, dk, lt, boot.
struct Cloth
{
  unsigned c;
  unsigned dk;
  unsigned lt;
  unsigned boot;
};

inline constexpr Cloth kCloth[6] = {
    {0x2a2823u, 0x0e0d0bu, 0x6b665au, 0x1c1b19u},
    {0xb5332au, 0x741c16u, 0xe2766cu, 0x8e2620u},
    {0xd4a224u, 0x8e6a0cu, 0xf5d77eu, 0xa87d12u},
    {0x2c5898u, 0x15366au, 0x7aa2dcu, 0x1d3f73u},
    {0xe3dac4u, 0xa59a80u, 0xffffffu, 0xcfc4aau},
    {0x3e7d4fu, 0x1f4a2bu, 0x86c493u, 0x2c5e39u},
};

inline gfx::IColor Rgb(unsigned hex, int alpha = 255)
{
  return gfx::IColor(alpha, static_cast<int>((hex >> 16) & 255u), static_cast<int>((hex >> 8) & 255u),
                     static_cast<int>(hex & 255u));
}

inline gfx::IColor RgbA(unsigned hex, float alpha)
{
  int a = static_cast<int>(std::lround(alpha * 255.f));
  if (a < 0)
    a = 0;
  if (a > 255)
    a = 255;
  return Rgb(hex, a);
}

// Lift PaintUtil.h grayed(): grayscale, then brightness 0.8.
inline gfx::IColor Grayed(gfx::IColor color)
{
  const float r = static_cast<float>(color.R) / 255.f;
  const float g = static_cast<float>(color.G) / 255.f;
  const float b = static_cast<float>(color.B) / 255.f;
  float v = (0.2126f * r + 0.7152f * g + 0.0722f * b) * 0.8f;
  if (v > 1.f)
    v = 1.f;
  const int iv = static_cast<int>(std::lround(v * 255.f));
  return gfx::IColor(color.A, iv, iv, iv);
}

inline gfx::IColor Ink(gfx::IColor color, bool aside)
{
  gfx::IColor out = aside ? Grayed(color) : color;
  if (aside)
  {
    int a = out.A / 2;
    if (a < 1)
      a = 1;
    out.A = a;
  }
  return out;
}

inline gfx::IStrokeOptions RoundStroke()
{
  gfx::IStrokeOptions opt;
  opt.mCapOption = gfx::ELineCap::Round;
  opt.mJoinOption = gfx::ELineJoin::Round;
  return opt;
}

inline void PathRope(gfx::IGraphics& g, const slewfold::Vec* p, int n, float ox, float oy)
{
  if (n < 2)
    return;
  g.PathClear();
  g.PathMoveTo(p[0].x + ox, p[0].y + oy);
  for (int i = 1; i < n - 1; ++i)
  {
    const float mx = 0.5f * (p[i].x + p[i + 1].x) + ox;
    const float my = 0.5f * (p[i].y + p[i + 1].y) + oy;
    g.PathQuadraticBezierTo(p[i].x + ox, p[i].y + oy, mx, my);
  }
  g.PathLineTo(p[n - 1].x + ox, p[n - 1].y + oy);
}

inline void StrokeRope(gfx::IGraphics& g, const slewfold::Vec* p, int n, gfx::IColor color, float width, float ox,
                       float oy)
{
  PathRope(g, p, n, ox, oy);
  g.PathStroke(color, width, RoundStroke());
}

inline void DashRope(gfx::IGraphics& g, const slewfold::Vec* p, int n, gfx::IColor color, float width, float on,
                     float off, float ox, float oy)
{
  float carry = 0.f;
  bool drawing = true;
  const gfx::IStrokeOptions opt = RoundStroke();
  for (int i = 0; i < n - 1; ++i)
  {
    float x0 = p[i].x + ox;
    float y0 = p[i].y + oy;
    const float x1 = p[i + 1].x + ox;
    const float y1 = p[i + 1].y + oy;
    float dx = x1 - x0;
    float dy = y1 - y0;
    float left = std::hypot(dx, dy);
    if (left < 1.0e-3f)
      continue;
    const float ux = dx / left;
    const float uy = dy / left;
    while (left > 0.f)
    {
      const float span = drawing ? on : off;
      const float step = std::fmin(left, std::fmax(0.05f, span - carry));
      const float nx = x0 + ux * step;
      const float ny = y0 + uy * step;
      if (drawing)
      {
        g.PathClear();
        g.PathMoveTo(x0, y0);
        g.PathLineTo(nx, ny);
        g.PathStroke(color, width, opt);
      }
      carry += step;
      if (carry >= span - 1.0e-3f)
      {
        carry = 0.f;
        drawing = !drawing;
      }
      x0 = nx;
      y0 = ny;
      left -= step;
    }
  }
}

inline void DrawPlug(gfx::IGraphics& g, float x, float y, const Cloth& cloth, bool open, bool aside)
{
  // Lift drawPlug. The boot sheen is a disc in place of Lift's radial. The
  // open plug is a ring and a hole. The closed plug is a cap.
  auto C = [&](gfx::IColor color) { return Ink(color, aside); };
  for (int n = 0; n < 3; ++n)
  {
    const float radius = 15.f + 4.f - 2.f * static_cast<float>(n);
    g.FillCircle(C(RgbA(0x1e1406u, 0.09f + 0.04f * static_cast<float>(n))), x + 3.f, y + 6.f, radius);
  }
  g.FillCircle(C(Rgb(cloth.boot)), x, y, 14.f);
  g.FillCircle(C(RgbA(0xffffffu, 0.42f)), x - 4.f, y - 5.f, 6.f);
  if (open)
  {
    g.DrawCircle(C(Rgb(cloth.c)), x, y, 10.f, nullptr, 2.2f);
    const unsigned sock = cloth.c == kCloth[0].c ? 0xe8ddc3u : 0x151515u;
    g.FillCircle(C(Rgb(sock)), x, y, 7.6f);
    g.DrawCircle(Rgb(0x000000u), x, y, 7.6f, nullptr, 0.8f);
    g.FillCircle(C(RgbA(0xffffffu, 0.28f)), x - 2.f, y - 2.5f, 3.f);
    g.FillCircle(Rgb(0x000000u), x, y, 4.2f);
  }
  else
  {
    g.DrawCircle(RgbA(0xffffffu, 0.12f), x, y, 9.5f, nullptr, 1.5f);
    g.FillCircle(C(Rgb(cloth.c)), x, y, 5.f);
  }
}

// Lift drawCable: offset shadow, cloth body, dark and light dashes, edge, highlight, plugs.
inline void DrawLiftCable(gfx::IGraphics& g, const slewfold::Vec* p, int n, float x1, float y1, float x2, float y2,
                          int clothIndex, bool open, bool aside)
{
  const Cloth& cloth = kCloth[clothIndex < 0 || clothIndex > 5 ? 0 : clothIndex];
  const float shadowA[3] = {0.07f, 0.10f, 0.14f};
  const float shadowW[3] = {22.f, 16.f, 11.f};
  for (int i = 0; i < 3; ++i)
    StrokeRope(g, p, n, Ink(RgbA(0x1e1406u, shadowA[i]), aside), shadowW[i], 5.f, 15.f);
  StrokeRope(g, p, n, Ink(Rgb(cloth.c), aside), 8.5f, 0.f, 0.f);
  DashRope(g, p, n, Ink(RgbA(cloth.dk, 0.75f), aside), 8.5f, 1.7f, 1.5f, 0.f, 0.f);
  DashRope(g, p, n, Ink(RgbA(cloth.lt, 0.6f), aside), 4.5f, 1.f, 2.2f, 0.f, 0.f);
  StrokeRope(g, p, n, Ink(RgbA(0x000000u, 0.35f), aside), 1.5f, 1.6f, 2.4f);
  StrokeRope(g, p, n, Ink(RgbA(0xffffffu, 0.14f), aside), 2.f, -1.4f, -2.f);
  // Lift paints the same cap on both ends of one cable.
  DrawPlug(g, x1, y1, cloth, open, aside);
  DrawPlug(g, x2, y2, cloth, open, aside);
}

inline void CubicSamples(float x1, float y1, float x2, float y2, slewfold::Vec* out, int n, float& sag)
{
  const float dist = std::hypot(x2 - x1, y2 - y1);
  sag = 36.f + std::fmin(dist * 0.06f, 28.f);
  for (int i = 0; i < n; ++i)
  {
    const float t = static_cast<float>(i) / static_cast<float>(n - 1);
    const float u = 1.f - t;
    out[i].x = u * u * u * x1 + 3.f * u * u * t * x1 + 3.f * u * t * t * x2 + t * t * t * x2;
    out[i].y = u * u * u * y1 + 3.f * u * u * t * (y1 + sag) + 3.f * u * t * t * (y2 + sag) + t * t * t * y2;
  }
}

// 72 px filmstrip tile. Frame 0 is -135 degrees, frame 63 is +135.
class FilmKnob final : public gfx::IKnobControlBase
{
public:
  FilmKnob(const gfx::IRECT& bounds, int paramIdx, const gfx::IBitmap& bitmap, int index, FaceState* face)
  : gfx::IKnobControlBase(bounds, paramIdx, gfx::EDirection::Vertical)
  , mBitmap(bitmap)
  , mIndex(index)
  , mFace(face)
  {
  }

  bool IsHit(float x, float y) const override
  {
    const float dx = x - mRECT.MW();
    const float dy = y - mRECT.MH();
    const float r = 0.5f * mRECT.W();
    return dx * dx + dy * dy <= r * r;
  }

  bool IsDirty() override
  {
    if (mHover || mMouseDown)
      return true;
    return gfx::IKnobControlBase::IsDirty();
  }

  void OnMouseOver(float x, float y, const gfx::IMouseMod& mod) override
  {
    gfx::IKnobControlBase::OnMouseOver(x, y, mod);
    mHover = true;
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    gfx::IKnobControlBase::OnMouseOut();
    mHover = false;
    SetDirty(false);
  }

  // Right-click puts the knob back at its default. Double-click does the same (IControl).
  void OnMouseDown(float x, float y, const gfx::IMouseMod& mod) override
  {
    if (mod.R)
    {
      SetValueToDefault();
      return;
    }
    gfx::IKnobControlBase::OnMouseDown(x, y, mod);
    SetDirty(false);
  }

  void OnMouseDblClick(float, float, const gfx::IMouseMod&) override { SetValueToDefault(); }

  void Draw(gfx::IGraphics& g) override
  {
    Publish();
    if (!mBitmap.IsValid())
      return;
    int frame = static_cast<int>(std::lround(GetValue() * static_cast<double>(kFilmFrames - 1)));
    if (frame < 0)
      frame = 0;
    if (frame >= kFilmFrames)
      frame = kFilmFrames - 1;
    g.DrawBitmap(mBitmap, mRECT, frame + 1, nullptr);
  }

private:
  gfx::IBitmap mBitmap;
  int mIndex;
  FaceState* mFace;
  bool mHover = false;

  void Publish()
  {
    if (mFace == nullptr)
      return;
    if (mHover || mMouseDown)
    {
      WDL_String display;
      if (GetParam() != nullptr)
        GetParam()->GetDisplay(display, false);
      mFace->knob = mIndex;
      std::snprintf(mFace->text, sizeof mFace->text, "%s", display.Get());
    }
    else if (mFace->knob == mIndex)
    {
      mFace->knob = -1;
      mFace->text[0] = '\0';
    }
  }
};

class KnobReadout final : public gfx::IControl
{
public:
  KnobReadout(const gfx::IRECT& bounds, int index, FaceState* face)
  : gfx::IControl(bounds)
  , mIndex(index)
  , mFace(face)
  , mText(11.f, gfx::IColor(255, 17, 17, 17), "Roboto-Regular", gfx::EAlign::Center, gfx::EVAlign::Middle)
  {
    SetIgnoreMouse(true);
  }

  bool IsDirty() override
  {
    const bool show = mFace != nullptr && mFace->knob == mIndex;
    if (show || mWas)
      return true;
    return gfx::IControl::IsDirty();
  }

  void Draw(gfx::IGraphics& g) override
  {
    mWas = mFace != nullptr && mFace->knob == mIndex && mFace->text[0] != '\0';
    if (!mWas)
      return;
    g.FillRect(gfx::IColor(220, 246, 242, 232), mRECT);
    g.DrawText(mText, mFace->text, mRECT);
  }

private:
  int mIndex;
  FaceState* mFace;
  gfx::IText mText;
  bool mWas = false;
};

// Bool lever: up is off (LO, CYCLE off). Down is on.
// Route lever: up RISE, right BOTH, down FALL. Default BOTH points at the label.
// The osc wave lever uses the same three positions: up SINE, right SAW, down SQR.
class Lever final : public gfx::IControl
{
public:
  Lever(const gfx::IRECT& bounds, int paramIdx, bool route)
  : gfx::IControl(bounds, paramIdx)
  , mRoute(route)
  {
  }

  void Draw(gfx::IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    g.FillCircle(gfx::IColor(90, 0, 0, 0), cx + 2.f, cy + 4.f, 8.f);
    g.FillCircle(gfx::IColor(255, 176, 176, 170), cx, cy, 8.f);
    g.FillCircle(gfx::IColor(255, 228, 228, 224), cx, cy, 5.f);
    float dx = 0.f;
    float dy = -15.f;
    if (mRoute)
    {
      int index = 1;
      if (GetParam() != nullptr)
        index = GetParam()->Int();
      if (index <= 0)
        dy = -15.f;
      else if (index == 1)
      {
        dx = 15.f;
        dy = 0.f;
      }
      else
        dy = 15.f;
    }
    else if (GetValue() >= 0.5)
      dy = 15.f;
    const gfx::IStrokeOptions opt = RoundStroke();
    g.PathClear();
    g.PathMoveTo(cx, cy);
    g.PathLineTo(cx + dx, cy + dy);
    g.PathStroke(gfx::IColor(255, 70, 70, 68), 3.2f, opt);
    g.FillEllipse(gfx::IColor(255, 242, 242, 239), cx + dx, cy + dy, 3.4f, 2.f);
  }

  void OnMouseDown(float, float, const gfx::IMouseMod& mod) override
  {
    if (mod.R || GetParam() == nullptr)
      return;
    if (mRoute)
    {
      const int next = (GetParam()->Int() + 1) % 3;
      SetValue(GetParam()->ToNormalized(static_cast<double>(next)));
    }
    else
      SetValue(GetValue() >= 0.5 ? 0.0 : 1.0);
    SetDirty(true);
  }

private:
  bool mRoute;
};

// COUPLE stays at AC. The bitmap draws it. This is only the hit target.
class CoupleHit final : public gfx::IControl
{
public:
  explicit CoupleHit(const gfx::IRECT& bounds)
  : gfx::IControl(bounds)
  {
  }

  void Draw(gfx::IGraphics&) override {}

  void OnMouseDown(float, float, const gfx::IMouseMod&) override {}
};

class SlopeLamp final : public gfx::IControl
{
public:
  SlopeLamp(const gfx::IRECT& bounds, SergeMiddleCombined& plug, const LedSpot& spot)
  : gfx::IControl(bounds)
  , mPlug(&plug)
  , mSpot(spot)
  {
    SetIgnoreMouse(true);
  }

  bool IsDirty() override
  {
    if (Lit() != mOn)
      return true;
    return gfx::IControl::IsDirty();
  }

  void Draw(gfx::IGraphics& g) override
  {
    mOn = Lit();
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    g.FillCircle(gfx::IColor(255, 58, 8, 6), cx, cy, 6.f);
    if (mOn)
    {
      g.FillCircle(gfx::IColor(255, 226, 58, 48), cx, cy, 4.5f);
      g.FillCircle(gfx::IColor(255, 255, 154, 144), cx - 1.5f, cy - 1.5f, 1.6f);
    }
    else
      g.FillCircle(gfx::IColor(255, 90, 18, 16), cx, cy, 4.5f);
  }

private:
  SergeMiddleCombined* mPlug;
  LedSpot mSpot;
  bool mOn = false;

  bool Lit() const
  {
    float lo = 0.f;
    float hi = 0.f;
    mPlug->JackMeter(patch::MeterOf(true, mSpot.meterOutput), lo, hi);
    if (mSpot.end)
      return hi >= 2.5f;
    iplug::IEditorDelegate* delegate = const_cast<SlopeLamp*>(this)->GetDelegate();
    if (delegate == nullptr || mSpot.cycleParam < 0)
      return false;
    if (!delegate->GetParam(mSpot.cycleParam)->Bool())
      return false;
    return hi >= 2.5f;
  }
};

// Cables and jack hits. IsHit is true only on a jack or a stacked plug, so
// knobs and switches underneath still receive clicks.
class CableControl final : public gfx::IControl
{
public:
  CableControl(const gfx::IRECT& bounds, SergeMiddleCombined& plug, FaceState* face)
  : gfx::IControl(bounds)
  , mPlug(&plug)
  , mFace(face)
  , mText(13.f, gfx::IColor(240, 244, 240, 232), "Roboto-Regular", gfx::EAlign::Center, gfx::EVAlign::Middle)
  {
  }

  // The cable layer covers the plate so a rope can part around the pointer,
  // the way Lift's cable layer does. Knobs, switches, and the corner resizer
  // stay in front of that hit.
  bool IsHit(float x, float y) const override
  {
    if (!mRECT.Contains(x, y))
      return false;
    if (x >= mRECT.R - 20.f && y >= mRECT.B - 20.f)
      return false;
    for (int i = 0; i < kKnobCount; ++i)
    {
      const gfx::IRECT box = Spot(mRECT, kKnobs[i].x, kKnobs[i].y, kKnobs[i].d, kKnobs[i].d);
      if (box.Contains(x, y))
        return false;
    }
    for (int i = 0; i < kSwitchCount; ++i)
    {
      const gfx::IRECT box = Spot(mRECT, kSwitches[i].x, kSwitches[i].y, kSwitchHit, kSwitchHit);
      if (box.Contains(x, y))
        return false;
    }
    return true;
  }

  bool IsDirty() override
  {
    if (mAwake || mHover >= 0 || mDragging)
      return true;
    return gfx::IControl::IsDirty();
  }

  void OnMouseOver(float x, float y, const gfx::IMouseMod& mod) override
  {
    gfx::IControl::OnMouseOver(x, y, mod);
    mInside = true;
    mX = x;
    mY = y;
    const int jack = JackAt(x, y);
    const bool near = NearRope(x, y) || jack >= 0;
    if (jack != mHover || near || mNear)
    {
      mHover = jack;
      mAwake = true;
      mIdle = 0;
      SetDirty(false);
    }
    mNear = near;
  }

  void OnMouseOut() override
  {
    gfx::IControl::OnMouseOut();
    mInside = false;
    mHover = -1;
    if (mNear)
    {
      mAwake = true;
      mIdle = 0;
    }
    mNear = false;
    SetDirty(false);
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
    const patch::Routing routing = mPlug->RoutingSnapshot();
    const JackSpot& spot = kJacks[jack];
    const int occupied = spot.output ? CountOutput(routing, spot.index) : (SourceOf(routing, spot.index) >= 0 ? 1 : 0);
    mDragging = true;
    mFrom = jack;
    mX = x;
    mY = y;
    mSpeed = 0.f;
    // A drag from an output always starts a new cable, so cables stack like
    // Serge banana plugs. To move or unplug a cable, drag its input end.
    // An input cannot take a second cable.
    const bool stack = spot.output && occupied > 0;
    if (occupied > 0 && !stack)
    {
      mMove = true;
      if (spot.output)
        mMoveInput = TopInput(routing, spot.index);
      else
        mMoveInput = spot.index;
    }
    else
    {
      mMove = false;
      mMoveInput = -1;
    }
    SetDirty(false);
  }

  void OnMouseDrag(float x, float y, float, float, const gfx::IMouseMod&) override
  {
    if (!mDragging)
      return;
    const float dx = x - mX;
    const float dy = y - mY;
    mSpeed = std::sqrt(dx * dx + dy * dy);
    mX = x;
    mY = y;
    mAwake = true;
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const gfx::IMouseMod&) override
  {
    if (!mDragging)
      return;
    mDragging = false;
    const int jack = JackAt(x, y);
    patch::Routing routing = mPlug->RoutingSnapshot();
    int kick = -1;
    const JackSpot& from = kJacks[mFrom];
    if (mMove)
    {
      if (jack < 0)
        routing.Disconnect(mMoveInput);
      else if (jack != mFrom)
      {
        const JackSpot& to = kJacks[jack];
        if (from.output && to.output && mMoveInput >= 0)
        {
          routing.Connect(to.index, mMoveInput);
          kick = mMoveInput;
        }
        else if (!from.output && !to.output && mMoveInput >= 0)
        {
          const int output = SourceOf(routing, mMoveInput);
          if (to.index != mMoveInput && output >= 0)
          {
            routing.Disconnect(mMoveInput);
            routing.Connect(output, to.index);
            kick = to.index;
          }
        }
      }
    }
    else if (jack >= 0 && jack != mFrom)
    {
      const JackSpot& to = kJacks[jack];
      if (from.output != to.output)
      {
        const int output = from.output ? from.index : to.index;
        const int input = from.output ? to.index : from.index;
        if (routing.Connect(output, input))
          kick = input;
      }
    }
    mPlug->ApplyRouting(routing);
    if (kick >= 0)
    {
      float amount = 1.5f + 0.05f * mSpeed;
      if (amount > 3.f)
        amount = 3.f;
      mKickInput = kick;
      mKickAmount = amount;
      mAwake = true;
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
      const JackSpot& jack = kJacks[mMenuJack];
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
    slewfold::RopeScene scene;
    scene.floor = ToY(kDesignH - 2.f);
    if (mInside)
    {
      scene.ptrIn = true;
      scene.ptr.x = mX;
      scene.ptr.y = mY;
    }
    if (mHover >= 0)
    {
      scene.hasJack = true;
      JackCentre(mHover, scene.jack.x, scene.jack.y);
    }
    if (mFace != nullptr && mFace->knob >= 0 && mFace->knob < kKnobCount)
    {
      const KnobSpot& knob = kKnobs[mFace->knob];
      const gfx::IRECT box = Placed(mRECT, knob.rx, knob.ry, knob.rw, knob.rh);
      scene.hasLabel = true;
      scene.labelL = box.L;
      scene.labelT = box.T;
      scene.labelR = box.R;
      scene.labelB = box.B;
    }

    const bool aside = mDragging;
    for (int input = 0; input < patch::kNumInputs; ++input)
    {
      if (mKickInput == input)
        mRope[input].reset();
      const int source = routing.source[input];
      if (source < 0 || source >= patch::kNumOutputs)
      {
        mRope[input].reset();
        continue;
      }
      if (mMove && mDragging && input == mMoveInput)
        continue;
      float x0 = 0.f;
      float y0 = 0.f;
      float x1 = 0.f;
      float y1 = 0.f;
      // No span unless both ends are jack centres. A miss used to leave 0,0
      // and still stroke the rope.
      if (!PlugEnds(routing, source, input, x0, y0, x1, y1))
      {
        mRope[input].reset();
        continue;
      }
      // The rope starts 4 px under the plug, as in Lift cordEnds / ropeFor.
      const bool was = mRope[input].valid();
      slewfold::Vec oldHead;
      slewfold::Vec oldTail;
      if (was)
      {
        oldHead = mRope[input].at(0);
        oldTail = mRope[input].at(slewfold::Rope::N - 1);
      }
      mRope[input].attach({x0, y0 + 4.f}, {x1, y1 + 4.f}, scene);
      if (was)
      {
        const float dy0 = oldHead.y - (y0 + 4.f);
        const float dy1 = oldTail.y - (y1 + 4.f);
        if (std::fabs(oldHead.x - x0) > 0.5f || std::fabs(dy0) > 0.5f || std::fabs(oldTail.x - x1) > 0.5f ||
            std::fabs(dy1) > 0.5f)
        {
          mAwake = true;
          mIdle = 0;
        }
      }
    }

    if (mKickInput >= 0 && mKickInput < patch::kNumInputs && mRope[mKickInput].valid())
    {
      mRope[mKickInput].kick(mKickAmount);
      mDrop = true;
      mAwake = true;
      mIdle = 0;
    }
    mKickInput = -1;

    if (mAwake)
    {
      const int sub = mDrop ? 2 : 1;
      const bool rest = mDrop || (!scene.ptrIn && !scene.hasJack);
      float mv = 0.f;
      for (int s = 0; s < sub; ++s)
      {
        for (int input = 0; input < patch::kNumInputs; ++input)
        {
          if (!mRope[input].valid())
            continue;
          if (mMove && mDragging && input == mMoveInput)
            continue;
          mv = std::fmax(mv, mRope[input].step(scene, rest));
        }
      }
      if (mv > 0.03f)
        mIdle = 0;
      else if (++mIdle >= 20)
      {
        mAwake = false;
        mDrop = false;
      }
    }

    int seen[patch::kNumOutputs] = {};
    int count[patch::kNumOutputs] = {};
    for (int input = 0; input < patch::kNumInputs; ++input)
    {
      const int source = routing.source[input];
      if (source >= 0 && source < patch::kNumOutputs && !(mMove && mDragging && input == mMoveInput))
        ++count[source];
    }
    for (int input = 0; input < patch::kNumInputs; ++input)
    {
      const int source = routing.source[input];
      if (source < 0 || source >= patch::kNumOutputs || !mRope[input].valid())
        continue;
      if (mMove && mDragging && input == mMoveInput)
        continue;
      float x0 = 0.f;
      float y0 = 0.f;
      float x1 = 0.f;
      float y1 = 0.f;
      if (!PlugEnds(routing, source, input, x0, y0, x1, y1))
        continue;
      const slewfold::Vec head = mRope[input].at(0);
      const slewfold::Vec tail = mRope[input].at(slewfold::Rope::N - 1);
      // Do not stroke a rope whose nodes are still on a stale anchor.
      if (std::fabs(head.x - x0) > 0.5f || std::fabs(head.y - (y0 + 4.f)) > 0.5f || std::fabs(tail.x - x1) > 0.5f ||
          std::fabs(tail.y - (y1 + 4.f)) > 0.5f)
        continue;
      const int level = seen[source]++;
      slewfold::Vec pts[slewfold::Rope::N];
      for (int i = 0; i < slewfold::Rope::N; ++i)
        pts[i] = mRope[input].at(i);
      const bool open = level + 1 < count[source];
      const int cloth = ClothOf(source);
      DrawLiftCable(g, pts, slewfold::Rope::N, x0, y0, x1, y1, cloth, open, aside);
    }

    if (mDragging && mFrom >= 0)
      DrawLive(g, routing);
    DrawLabel(g, routing);
  }

private:
  SergeMiddleCombined* mPlug;
  FaceState* mFace;
  gfx::IText mText;
  gfx::IPopupMenu mMenu;
  slewfold::Rope mRope[patch::kNumInputs];
  int mFrom = -1;
  int mHover = -1;
  int mMenuJack = -1;
  int mMoveInput = -1;
  int mKickInput = -1;
  int mIdle = 0;
  float mX = 0.f;
  float mY = 0.f;
  float mSpeed = 0.f;
  float mKickAmount = 3.f;
  bool mDragging = false;
  bool mMove = false;
  bool mAwake = false;
  bool mDrop = false;
  bool mInside = false;
  bool mNear = false;

  float ToX(float design) const { return mRECT.L + design * mRECT.W() / kDesignW; }
  float ToY(float design) const { return mRECT.T + design * mRECT.H() / kDesignH; }
  float Scale() const { return std::fmin(mRECT.W() / kDesignW, mRECT.H() / kDesignH); }

  void JackCentre(int index, float& x, float& y) const
  {
    x = ToX(kJacks[index].x);
    y = ToY(kJacks[index].y);
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

  static int SourceOf(const patch::Routing& routing, int input)
  {
    if (input < 0 || input >= patch::kNumInputs)
      return -1;
    return routing.source[input];
  }

  static int CountOutput(const patch::Routing& routing, int output)
  {
    int n = 0;
    for (int i = 0; i < patch::kNumInputs; ++i)
    {
      if (routing.source[i] == output)
        ++n;
    }
    return n;
  }

  static int TopInput(const patch::Routing& routing, int output)
  {
    int found = -1;
    for (int i = 0; i < patch::kNumInputs; ++i)
    {
      if (routing.source[i] == output)
        found = i;
    }
    return found;
  }

  static int ClothOf(int output)
  {
    for (int i = 0; i < kJackCount; ++i)
    {
      if (kJacks[i].output && kJacks[i].index == output)
        return kJacks[i].cloth;
    }
    return 4;
  }

  // Output plug steps up by Lift's 6 design px per extra cable. The input has one plug.
  // False leaves the caller's coordinates alone and means "do not draw".
  bool PlugEnds(const patch::Routing& routing, int output, int input, float& x0, float& y0, float& x1,
                float& y1) const
  {
    slewfold::CableEnd end;
    if (!slewfold::ResolveCable(routing, input, end) || end.output != output)
      return false;
    x0 = ToX(end.x0);
    y0 = ToY(end.y0);
    x1 = ToX(end.x1);
    y1 = ToY(end.y1);
    return true;
  }

  bool NearRope(float x, float y) const
  {
    const float reach = 45.f * Scale();
    const float reach2 = reach * reach;
    for (int input = 0; input < patch::kNumInputs; ++input)
    {
      if (!mRope[input].valid())
        continue;
      for (int i = 0; i < slewfold::Rope::N; ++i)
      {
        const slewfold::Vec p = mRope[input].at(i);
        const float dx = x - p.x;
        const float dy = y - p.y;
        if (dx * dx + dy * dy <= reach2)
          return true;
      }
    }
    return false;
  }

  int JackAt(float x, float y) const
  {
    const patch::Routing routing = mPlug->RoutingSnapshot();
    const float reach = 0.5f * kJackHit * Scale();
    const float reach2 = reach * reach;
    for (int i = kJackCount - 1; i >= 0; --i)
    {
      float jx = 0.f;
      float jy = 0.f;
      JackCentre(i, jx, jy);
      int levels = 1;
      if (kJacks[i].output)
        levels = std::max(1, CountOutput(routing, kJacks[i].index));
      for (int level = levels - 1; level >= 0; --level)
      {
        const float py = jy - kStackStep * Scale() * static_cast<float>(level);
        const float dx = x - jx;
        const float dy = y - py;
        if (dx * dx + dy * dy <= reach2)
          return i;
      }
    }
    return -1;
  }

  void DrawLive(gfx::IGraphics& g, const patch::Routing& routing) const
  {
    const JackSpot& from = kJacks[mFrom];
    float x0 = 0.f;
    float y0 = 0.f;
    float x1 = mX;
    float y1 = mY;
    int cloth = 4;
    if (mMove && mMoveInput >= 0)
    {
      const int source = SourceOf(routing, mMoveInput);
      if (from.output)
      {
        if (!FindCentre(false, mMoveInput, x0, y0))
          return;
        cloth = source >= 0 ? ClothOf(source) : 4;
      }
      else if (source >= 0)
      {
        float unusedX = 0.f;
        float unusedY = 0.f;
        // Keep this cable in the stack count so the plug stays on its own step.
        if (!PlugEnds(routing, source, mMoveInput, x0, y0, unusedX, unusedY))
          return;
        cloth = ClothOf(source);
      }
      else
        return;
    }
    else
    {
      JackCentre(mFrom, x0, y0);
      if (from.output)
      {
        const int n = CountOutput(routing, from.index);
        y0 -= kStackStep * Scale() * static_cast<float>(n);
        cloth = from.cloth;
      }
    }
    slewfold::Vec pts[32];
    float sag = 0.f;
    CubicSamples(x0, y0, x1, y1, pts, 32, sag);
    DrawLiftCable(g, pts, 32, x0, y0, x1, y1, cloth, false, false);
  }

  void DrawLabel(gfx::IGraphics& g, const patch::Routing& routing) const
  {
    if (mHover < 0 || mHover >= kJackCount)
      return;
    const JackSpot& jack = kJacks[mHover];
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
    char buf[160];
    const char* hint = jack.output ? "  (drag to patch, more cables stack)" : "";
    if (!jack.output && routing.source[jack.index] >= 0)
      hint = "  (drag plug to move or unplug)";
    if (hi - lo > 0.05f)
      std::snprintf(buf, sizeof buf, "%s%s  %+.2f .. %+.2f V%s", jack.label, normal ? " (normal)" : "", lo, hi, hint);
    else
      std::snprintf(buf, sizeof buf, "%s%s  %+.2f V%s", jack.label, normal ? " (normal)" : "", 0.5f * (lo + hi),
                    hint);

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

inline void AttachFace(gfx::IGraphics* g, SergeMiddleCombined& plug)
{
  FaceState* face = plug.EditorFace();
  const gfx::IRECT bounds = g->GetBounds();
  const gfx::IBitmap plate = g->LoadBitmap("slewfold_plate.png");
  const gfx::IBitmap film = g->LoadBitmap("knob_filmstrip.png", kFilmFrames, false);
  g->AttachControl(new gfx::ILambdaControl(
      bounds, [plate](gfx::ILambdaControl*, gfx::IGraphics& graphics, gfx::IRECT& r) { graphics.DrawFittedBitmap(plate, r); },
      0, false, false, kNoParameter, true));

  for (int i = 0; i < kLedCount; ++i)
  {
    const LedSpot& led = kLeds[i];
    g->AttachControl(new SlopeLamp(Spot(bounds, led.x, led.y, 16.f, 16.f), plug, led));
  }
  for (int i = 0; i < kSwitchCount; ++i)
  {
    const SwitchSpot& sw = kSwitches[i];
    const gfx::IRECT box = Spot(bounds, sw.x, sw.y, kSwitchHit, kSwitchHit);
    if (sw.param < 0)
      g->AttachControl(new CoupleHit(box));
    else
      g->AttachControl(new Lever(box, sw.param, sw.param == kRoute || sw.param == kRoute2 || sw.param == kOscWave));
  }
  for (int i = 0; i < kKnobCount; ++i)
  {
    const KnobSpot& knob = kKnobs[i];
    g->AttachControl(new FilmKnob(Spot(bounds, knob.x, knob.y, knob.d, knob.d), knob.param, film, i, face));
  }
  g->AttachControl(new CableControl(bounds, plug, face), kCableTag);
  for (int i = 0; i < kKnobCount; ++i)
  {
    const KnobSpot& knob = kKnobs[i];
    g->AttachControl(new KnobReadout(Placed(bounds, knob.rx, knob.ry, knob.rw, knob.rh), i, face));
  }
}

} // namespace patchview
