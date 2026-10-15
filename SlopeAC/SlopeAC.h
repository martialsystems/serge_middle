#pragma once

// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "IPlug_include_in_plug_hdr.h"

#include "dsp/AcCoupler.h"

const int kNumPresets = 1;

enum EParams
{
  kRise = 0,
  kFall,
  kCycle,
  kVc,
  kRoute,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class SlopeAC final : public Plugin
{
public:
  SlopeAC(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void OnReset() override;
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif

#if IPLUG_EDITOR
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
#endif

#if IPLUG_DSP
private:
  static constexpr int kMaxChannels = 2;
  slope::SecondHalf mChannels[kMaxChannels];
  slope::AcCoupler mCoupler[kMaxChannels];
  bool mCycle = false;
  bool mArm = false;
#endif
};
