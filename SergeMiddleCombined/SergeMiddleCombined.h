#pragma once

// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "IPlug_include_in_plug_hdr.h"
#include "IPlugMidi.h"

#include "dsp/Patch.h"

#include <array>
#include <atomic>

const int kNumPresets = 1;

enum EParams
{
  kFold = 0,
  kIn2,
  kVcDepth,
  kRise,
  kFall,
  kCycle,
  kSlopeVc,
  kRoute,
  kTopGain,
  kTopHi,
  kBottomGain,
  kBottomVc,
  kRise2,
  kFall2,
  kCycle2,
  kSlopeVc2,
  kRoute2,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class SergeMiddleCombined final : public Plugin
{
public:
  SergeMiddleCombined(const InstanceInfo& info);

  patch::Routing RoutingSnapshot() const;
  void ApplyRouting(const patch::Routing& routing);
  void JackMeter(int index, float& lo, float& hi) const;

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void OnReset() override;
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
#endif

#if IPLUG_EDITOR
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
  void OnParentWindowResize(int width, int height) override;
#endif

private:
  // A zero would cable PLEAT OUT into every input. The constructor stores -1.
  std::array<std::atomic<int8_t>, patch::kNumInputs> mRouting;
  std::array<std::atomic<float>, patch::kMeterCount> mMeterMin;
  std::array<std::atomic<float>, patch::kMeterCount> mMeterMax;

#if IPLUG_DSP
  void HandleMidi(const IMidiMsg& msg);

  static constexpr int kMaxChannels = 2;
  serge::FoldSmoother mFold;
  serge::SymmetrySmoother mSymmetry;
  vcm::GainSmoother mTopGain;
  vcm::GainSmoother mBottomGain;
  vcm::GainSmoother mBottomVc;
  patch::PatchChannel mChannels[kMaxChannels];
  patch::GateNotes mNotes;
  IMidiQueue mMidiQueue;
  bool mCycle = false;
  bool mArm = false;
  bool mCycle2 = false;
  bool mArm2 = false;
#endif

#if IPLUG_EDITOR
  bool mFittingResize = false;
#endif
};
