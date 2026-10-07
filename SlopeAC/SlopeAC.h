#pragma once

// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "IPlug_include_in_plug_hdr.h"

#include "dsp/SecondHalf.h"

const int kNumPresets = 1;

enum EParams
{
  kRise = 0,
  kFall,
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

private:
  static constexpr int kMaxChannels = 2;
  slope::SecondHalf mChannels[kMaxChannels];
  bool mArm = true;
#endif
};
