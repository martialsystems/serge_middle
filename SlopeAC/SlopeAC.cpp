// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "SlopeAC.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "SlopeView.h"
#include "dsp/Readout.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kRateTag = 1;

void ShowTime(double value, WDL_String& display)
{
  char buf[32];
  slope::FormatTime(value, buf, sizeof buf);
  display.Set(buf);
}

void ShowRoute(double value, WDL_String& display)
{
  const int index = static_cast<int>(std::lround(value));
  const char* name = "BOTH";
  if (index == 0)
    name = "RISE";
  else if (index == 2)
    name = "FALL";
  display.Set(name);
}
} // namespace

SlopeAC::SlopeAC(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // Seconds for the full 0 V to +5 V excursion. The shape is exponential.
  // 0.0001 s is above zero, which ShapeExp requires. The caption uses FormatTime.
  GetParam(kRise)->InitDouble("Rise", 0.005, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());
  GetParam(kFall)->InitDouble("Fall", 0.005, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());
  GetParam(kRise)->SetDisplayFunc(ShowTime);
  GetParam(kFall)->SetDisplayFunc(ShowTime);
  // Off leaves the end-pulse cord open. A project saved before this
  // parameter existed loads that default.
  GetParam(kCycle)->InitBool("Cycle", false);
  // Appended after Cycle so a project saved before these knobs keeps Rise,
  // Fall, and Cycle. Depth 0 passes no VC. Route 1 is BOTH.
  GetParam(kVc)->InitDouble("VC", 0.0, -1.0, 1.0, 0.001);
  GetParam(kRoute)->InitInt("Route", 1, 0, 2);
  GetParam(kRoute)->SetDisplayFunc(ShowRoute);

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachPanelBackground(IColor(255, 12, 17, 30));
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->AttachTextEntryControl();

    const IRECT bounds = pGraphics->GetBounds();
    const IBitmap plate = pGraphics->LoadBitmap("slope_plate.png");
    const IBitmap knob = pGraphics->LoadBitmap("pleat_knob.png");
    const IBitmap cycle = pGraphics->LoadBitmap("slope_switch.png", 2);
    const IBitmap route = pGraphics->LoadBitmap("slope_route.png", 3);

    pGraphics->AttachControl(new ILambdaControl(
        bounds,
        [plate](ILambdaControl*, IGraphics& g, IRECT& r) { g.DrawFittedBitmap(plate, r); },
        0, false, false, kNoParameter, true));
    pGraphics->AttachControl(new slopeview::SlopeKnob(slopeview::KnobBounds(bounds, slopeview::kRiseX), kRise, knob));
    pGraphics->AttachControl(new slopeview::SlopeKnob(slopeview::KnobBounds(bounds, slopeview::kFallX), kFall, knob));
    pGraphics->AttachControl(new slopeview::ScaledKnob(slopeview::VcKnobBounds(bounds), kVc, knob, slopeview::kVcD));

    const IText valueText(14.f, IColor(235, 238, 240, 245), "Roboto-Regular", EAlign::Center);
    pGraphics->AttachControl(new ICaptionControl(slopeview::CaptionBounds(bounds, slopeview::kRiseX), kRise,
                                                 valueText, COLOR_TRANSPARENT, false));
    pGraphics->AttachControl(new ICaptionControl(slopeview::CaptionBounds(bounds, slopeview::kFallX), kFall,
                                                 valueText, COLOR_TRANSPARENT, false));
    pGraphics->AttachControl(new IBSwitchControl(slopeview::SwitchBounds(bounds), cycle, kCycle));
    pGraphics->AttachControl(new slopeview::RouteSwitch(slopeview::RouteBounds(bounds), kRoute, route));
    pGraphics->AttachControl(new slopeview::RateReadout(slopeview::RateBounds(bounds), kRise, kFall, kCycle,
                                                        [this]() { return GetSampleRate(); }),
                             kRateTag);
  };
#endif
}

#if IPLUG_EDITOR
void SlopeAC::OnParamChangeUI(int paramIdx, EParamSource)
{
  if (paramIdx != kRise && paramIdx != kFall && paramIdx != kCycle)
    return;
  if (GetUI() == nullptr)
    return;
  if (IControl* rate = GetUI()->GetControlWithTag(kRateTag))
    rate->SetDirty(false);
}
#endif

#if IPLUG_DSP
void SlopeAC::OnReset()
{
  const double fs = GetSampleRate();
  const double rise = GetParam(kRise)->Value();
  const double fall = GetParam(kFall)->Value();
  const bool cycle = GetParam(kCycle)->Bool();
  const int route = GetParam(kRoute)->Int();
  for (auto& channel : mChannels)
  {
    channel.SetSampleRate(fs);
    channel.SetTimes(rise, fall);
    channel.SetSwitch(slope::SwitchFromInt(route));
    channel.Reset();
  }
  // The cord is live only while Cycle is on, so the reset trigger follows it.
  mCycle = cycle;
  mArm = cycle;
  for (auto& coupler : mCoupler)
  {
    coupler.SetSampleRate(fs);
    coupler.Reset();
  }
}

void SlopeAC::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  // Once per block. Rise and Fall stay per sample, as they were.
  const bool cycle = GetParam(kCycle)->Bool();
  // Connecting END, which is high at idle, to TRIG is a rising edge.
  if (cycle && !mCycle)
    mArm = true;
  mCycle = cycle;

  const int nChans = std::min(NInChansConnected(), std::min(NOutChansConnected(), kMaxChannels));

  for (int s = 0; s < nFrames; ++s)
  {
    const double rise = GetParam(kRise)->Value();
    const double fall = GetParam(kFall)->Value();
    // The VC jack is unpatched on this plugin, so the voltage is 0. The knob
    // still scales it. A cord in the combined plugin uses the same call.
    const double vcDepth = GetParam(kVc)->Value();
    const int route = GetParam(kRoute)->Int();
    // An open cord does not deliver the reset trigger.
    const bool arm = cycle && mArm;
    for (int c = 0; c < nChans; ++c)
    {
      const double host = static_cast<double>(inputs[c][s]);
      outputs[c][s] = static_cast<sample>(
          slope::HostSample(mCoupler[c], mChannels[c], rise, fall, host, arm, cycle, 0.0, vcDepth, route));
    }
    if (arm && nChans > 0)
      mArm = false;
  }
}
#endif
