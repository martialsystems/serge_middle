// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "SergeMiddle.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

#include <algorithm>

SergeMiddle::SergeMiddle(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // g, the fold amount into the first cell. Exponential shape so each
  // octave of g takes the same knob travel.
  GetParam(kFold)->InitDouble("Fold", serge::kGDefault, serge::kGMin, serge::kGMax, 0.001, "",
                              0, "", IParam::ShapeExp());

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    const IRECT bounds = pGraphics->GetBounds().GetPadded(-10.f);
    pGraphics->AttachControl(new ITextControl(bounds.GetFromTop(30.f), "Serge Middle", IText(24)));
    pGraphics->AttachControl(new IVKnobControl(bounds.GetCentredInside(140.f), kFold));
  };
#endif
}

#if IPLUG_DSP
void SergeMiddle::OnReset()
{
  const double fs = GetSampleRate();
  // The smoother state starts at the current target, not at 0.
  mFold.Reset(GetParam(kFold)->Value(), fs);
  for (auto& channel : mChannels)
    channel.Reset(fs);
}

void SergeMiddle::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const double target = GetParam(kFold)->Value();
  const int nChans = std::min(NOutChansConnected(), kMaxChannels);

  for (int s = 0; s < nFrames; s++)
  {
    // Once per audio sample; the four oversampled phases share this g.
    const double g = mFold.Step(target);
    for (int c = 0; c < nChans; c++)
      outputs[c][s] = static_cast<sample>(mChannels[c].Step(inputs[c][s], g));
  }
}
#endif
