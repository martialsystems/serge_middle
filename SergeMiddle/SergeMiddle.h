#pragma once

// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "IPlug_include_in_plug_hdr.h"

#include "dsp/WaveMiddle.h"

const int kNumPresets = 1;

enum EParams
{
  kFold = 0,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class SergeMiddle final : public Plugin
{
public:
  SergeMiddle(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void OnReset() override;
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;

private:
  static constexpr int kMaxChannels = 2;
  serge::FoldSmoother mFold;
  serge::WaveMiddleChannel mChannels[kMaxChannels];
#endif
};
