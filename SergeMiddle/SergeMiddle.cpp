// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "SergeMiddle.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "PleatView.h"

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
    pGraphics->AttachPanelBackground(IColor(255, 46, 44, 42));
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->AttachTextEntryControl();

    const IRECT bounds = pGraphics->GetBounds();
    const IBitmap plate = pGraphics->LoadBitmap("pleat_plate.png");
    const IBitmap knob = pGraphics->LoadBitmap("pleat_knob.png");

    // Plate, then the rope, then the cap. The cap is proud of the enamel, so it covers a rope that would hit it.
    pGraphics->AttachControl(new ILambdaControl(
        bounds,
        [plate](ILambdaControl*, IGraphics& g, IRECT& r) { g.DrawFittedBitmap(plate, r); },
        0, false, false, kNoParameter, true));
    pGraphics->AttachControl(new pleat::PleatCables(bounds));
    pGraphics->AttachControl(new pleat::PleatKnob(pleat::KnobBounds(bounds), kFold, knob));

    const IRECT caption(bounds.L + 0.30f * bounds.W(), bounds.T + 0.575f * bounds.H(),
                        bounds.L + 0.70f * bounds.W(), bounds.T + 0.650f * bounds.H());
    const IText valueText(16.f, IColor(230, 236, 224, 206), "Roboto-Regular", EAlign::Center);
    // GetDisplay of Fold. The same IParam::Value is the smoother target in ProcessBlock.
    pGraphics->AttachControl(new ICaptionControl(caption, kFold, valueText, COLOR_TRANSPARENT, false));
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
