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
  // s is the DC voltage on IN 2 that this knob stands in for. Linear: knob travel matches volts.
  GetParam(kIn2)->InitDouble("IN 2 DC", serge::kSymDefault, serge::kSymMin, serge::kSymMax, 0.01, "V");

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachPanelBackground(IColor(255, 12, 17, 30));
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->AttachTextEntryControl();

    const IRECT bounds = pGraphics->GetBounds();
    const IBitmap plate = pGraphics->LoadBitmap("pleat_plate.png");
    const IBitmap knob = pGraphics->LoadBitmap("pleat_knob.png");

    // Plate, cables, both knobs, both captions. A knob is drawn after the rope, so a rope under the cap stays under it.
    pGraphics->AttachControl(new ILambdaControl(
        bounds,
        [plate](ILambdaControl*, IGraphics& g, IRECT& r) { g.DrawFittedBitmap(plate, r); },
        0, false, false, kNoParameter, true));
    pGraphics->AttachControl(new pleat::PleatCables(bounds));
    pGraphics->AttachControl(new pleat::PleatKnob(pleat::KnobBounds(bounds, pleat::kFoldX), kFold, knob));
    pGraphics->AttachControl(new pleat::PleatKnob(pleat::KnobBounds(bounds, pleat::kSymX), kIn2, knob));

    const IText valueText(14.f, IColor(235, 238, 240, 245), "Roboto-Regular", EAlign::Center);
    // GetDisplay of each knob. The same IParam::Value is the smoother target in ProcessBlock.
    pGraphics->AttachControl(new ICaptionControl(pleat::CaptionBounds(bounds, pleat::kFoldX), kFold, valueText,
                                                 COLOR_TRANSPARENT, false));
    pGraphics->AttachControl(new ICaptionControl(pleat::CaptionBounds(bounds, pleat::kSymX), kIn2, valueText,
                                                 COLOR_TRANSPARENT, false));
  };
#endif
}

#if IPLUG_DSP
void SergeMiddle::OnReset()
{
  const double fs = GetSampleRate();
  // Each smoother state starts at the current target, not at 0.
  mFold.Reset(GetParam(kFold)->Value(), fs);
  mSymmetry.Reset(GetParam(kIn2)->Value(), fs);
  for (auto& channel : mChannels)
    channel.Reset(fs);
}

void SergeMiddle::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const double target = GetParam(kFold)->Value();
  const double symmetry = GetParam(kIn2)->Value();
  const int nChans = std::min(NOutChansConnected(), kMaxChannels);

  for (int s = 0; s < nFrames; s++)
  {
    // Once per audio sample. The four oversampled phases share this g and this offset.
    const double g = mFold.Step(target);
    const double offset = mSymmetry.Step(symmetry);
    for (int c = 0; c < nChans; c++)
      outputs[c][s] = static_cast<sample>(mChannels[c].Step(inputs[c][s], g, offset));
  }
}
#endif
