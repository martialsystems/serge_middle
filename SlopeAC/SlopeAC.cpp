// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "SlopeAC.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

#include <algorithm>

SlopeAC::SlopeAC(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // Seconds for the full 0 V to +5 V excursion. The shape is exponential.
  // 0.0001 s is above zero, which ShapeExp requires.
  GetParam(kRise)->InitDouble("Rise", 0.005, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());
  GetParam(kFall)->InitDouble("Fall", 0.005, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachPanelBackground(IColor(255, 236, 232, 224));
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    const IRECT bounds = pGraphics->GetBounds();
    const IRECT rise = bounds.GetFromLeft(bounds.W() * 0.5f).GetCentredInside(148.f, 164.f);
    const IRECT fall = bounds.GetFromRight(bounds.W() * 0.5f).GetCentredInside(148.f, 164.f);
    pGraphics->AttachControl(new IVKnobControl(rise, kRise, "Rise"));
    pGraphics->AttachControl(new IVKnobControl(fall, kFall, "Fall"));
  };
#endif
}

#if IPLUG_DSP
void SlopeAC::OnReset()
{
  const double fs = GetSampleRate();
  const double rise = GetParam(kRise)->Value();
  const double fall = GetParam(kFall)->Value();
  for (auto& channel : mChannels)
  {
    channel.SetSampleRate(fs);
    channel.SetTimes(rise, fall);
    channel.Reset();
  }
  mArm = true;
}

void SlopeAC::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = std::min(NInChansConnected(), std::min(NOutChansConnected(), kMaxChannels));

  for (int s = 0; s < nFrames; ++s)
  {
    const double rise = GetParam(kRise)->Value();
    const double fall = GetParam(kFall)->Value();
    const bool arm = mArm;
    for (int c = 0; c < nChans; ++c)
    {
      const double host = static_cast<double>(inputs[c][s]);
      outputs[c][s] = static_cast<sample>(slope::PluginSample(mChannels[c], rise, fall, host, arm));
    }
    if (arm && nChans > 0)
      mArm = false;
  }
}
#endif
