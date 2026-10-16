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
  kOscWave,
  kOscFreq,
  kOscLevel,
  kOscToIn1,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

namespace patchview
{
struct FaceState;
}

class SergeMiddleCombined final : public Plugin
{
public:
  SergeMiddleCombined(const InstanceInfo& info);
  ~SergeMiddleCombined();

  // The starter patch: test osc into PLEAT IN 1, Slope 1 OUT into PLEAT VC.
  static patch::Routing StarterRouting();

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
  // Hover text shared by the knobs and the readout rects. Owned here so a
  // second instance does not see the first instance's hover.
  patchview::FaceState* EditorFace();
#endif

private:
  // Born at -1. A zero would cable PLEAT OUT into every input.
  std::array<patch::RoutingWord, patch::kNumInputs> mRouting{};
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
  patchview::FaceState* mFace = nullptr;
#endif
};
