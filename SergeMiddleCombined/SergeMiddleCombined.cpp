// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "SergeMiddleCombined.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "PatchView.h"
#include "../SlopeAC/dsp/Readout.h"

#include <algorithm>
#include <cmath>
#include <vector>

static_assert(std::atomic<int8_t>::is_always_lock_free, "routing atomic");
static_assert(std::atomic<float>::is_always_lock_free, "meter atomic");

namespace
{
constexpr int kCableTag = 100;
constexpr int kRateTag = 101;

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

SergeMiddleCombined::SergeMiddleCombined(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  for (auto& slot : mRouting)
    slot.store(-1, std::memory_order_relaxed);
  for (auto& meter : mMeterMin)
    meter.store(0.f, std::memory_order_relaxed);
  for (auto& meter : mMeterMax)
    meter.store(0.f, std::memory_order_relaxed);

  GetParam(kFold)->InitDouble("Fold", serge::kGDefault, serge::kGMin, serge::kGMax, 0.001, "",
                              0, "", IParam::ShapeExp());
  GetParam(kIn2)->InitDouble("IN 2 DC", serge::kSymDefault, serge::kSymMin, serge::kSymMax, 0.01, "V");
  GetParam(kVcDepth)->InitDouble("VC Depth", 0.5, 0.0, 1.0, 0.001);
  GetParam(kRise)->InitDouble("Rise", 0.005, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());
  GetParam(kFall)->InitDouble("Fall", 0.005, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());
  GetParam(kRise)->SetDisplayFunc(ShowTime);
  GetParam(kFall)->SetDisplayFunc(ShowTime);
  GetParam(kCycle)->InitBool("Cycle", false);
  // Appended so a project saved before these controls keeps the six above.
  // Depth 0 leaves a slope VC cord at 0 V. Route 1 is BOTH, the circuit default.
  GetParam(kSlopeVc)->InitDouble("Slope VC", 0.0, -1.0, 1.0, 0.001);
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
    const float mid = bounds.L + 0.5f * bounds.W();
    const IRECT pleat(bounds.L, bounds.T, mid, bounds.B);
    const IRECT slope(mid, bounds.T, bounds.R, bounds.B);
    const IBitmap plate = pGraphics->LoadBitmap("middle_plate.png");
    const IBitmap knob = pGraphics->LoadBitmap("pleat_knob.png");
    const IBitmap cycle = pGraphics->LoadBitmap("slope_switch.png", 2);
    const IBitmap route = pGraphics->LoadBitmap("slope_route.png", 3);

    pGraphics->AttachControl(new ILambdaControl(
        bounds,
        [plate](ILambdaControl*, IGraphics& g, IRECT& r) { g.DrawFittedBitmap(plate, r); },
        0, false, false, kNoParameter, true));
    pGraphics->AttachControl(new pleat::PleatKnob(pleat::KnobBounds(pleat, pleat::kFoldX), kFold, knob));
    pGraphics->AttachControl(new pleat::PleatKnob(pleat::KnobBounds(pleat, pleat::kSymX), kIn2, knob));
    pGraphics->AttachControl(new patchview::DepthKnob(patchview::DepthBounds(bounds), kVcDepth, knob));

    const IText valueText(14.f, IColor(235, 238, 240, 245), "Roboto-Regular", EAlign::Center);
    pGraphics->AttachControl(new ICaptionControl(pleat::CaptionBounds(pleat, pleat::kFoldX), kFold, valueText,
                                                 COLOR_TRANSPARENT, false));
    pGraphics->AttachControl(new ICaptionControl(pleat::CaptionBounds(pleat, pleat::kSymX), kIn2, valueText,
                                                 COLOR_TRANSPARENT, false));
    pGraphics->AttachControl(new slopeview::SlopeKnob(slopeview::KnobBounds(slope, slopeview::kRiseX), kRise, knob));
    pGraphics->AttachControl(new slopeview::SlopeKnob(slopeview::KnobBounds(slope, slopeview::kFallX), kFall, knob));
    pGraphics->AttachControl(new slopeview::ScaledKnob(slopeview::VcKnobBounds(slope), kSlopeVc, knob, slopeview::kVcD));
    pGraphics->AttachControl(new ICaptionControl(slopeview::CaptionBounds(slope, slopeview::kRiseX), kRise, valueText,
                                                 COLOR_TRANSPARENT, false));
    pGraphics->AttachControl(new ICaptionControl(slopeview::CaptionBounds(slope, slopeview::kFallX), kFall, valueText,
                                                 COLOR_TRANSPARENT, false));
    pGraphics->AttachControl(new IBSwitchControl(slopeview::SwitchBounds(slope), cycle, kCycle));
    pGraphics->AttachControl(new slopeview::RouteSwitch(slopeview::RouteBounds(slope), kRoute, route));
    pGraphics->AttachControl(new slopeview::RateReadout(slopeview::RateBounds(slope), kRise, kFall, kCycle,
                                                        [this]() { return GetSampleRate(); }),
                             kRateTag);
    pGraphics->AttachControl(new patchview::CableControl(bounds, *this), kCableTag);
  };
#endif
}

patch::Routing SergeMiddleCombined::RoutingSnapshot() const
{
  patch::Routing routing;
  for (int i = 0; i < patch::kNumInputs; ++i)
    routing.source[i] = mRouting[static_cast<size_t>(i)].load(std::memory_order_acquire);
  return routing;
}

void SergeMiddleCombined::ApplyRouting(const patch::Routing& routing)
{
  for (int i = 0; i < patch::kNumInputs; ++i)
  {
    int8_t source = routing.source[i];
    if (source < -1 || source >= patch::kNumOutputs)
      source = -1;
    mRouting[static_cast<size_t>(i)].store(source, std::memory_order_release);
  }
}

void SergeMiddleCombined::JackMeter(int index, float& lo, float& hi) const
{
  if (index < 0 || index >= patch::kMeterCount)
  {
    lo = 0.f;
    hi = 0.f;
    return;
  }
  lo = mMeterMin[static_cast<size_t>(index)].load(std::memory_order_acquire);
  hi = mMeterMax[static_cast<size_t>(index)].load(std::memory_order_acquire);
}

#if IPLUG_EDITOR
void SergeMiddleCombined::OnParamChangeUI(int paramIdx, EParamSource)
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
void SergeMiddleCombined::HandleMidi(const IMidiMsg& msg)
{
  switch (msg.StatusMsg())
  {
    case IMidiMsg::kNoteOn:
      if (msg.Velocity() == 0)
        mNotes.NoteOff(msg.NoteNumber());
      else
        mNotes.NoteOn(msg.NoteNumber());
      break;
    case IMidiMsg::kNoteOff:
      mNotes.NoteOff(msg.NoteNumber());
      break;
    case IMidiMsg::kControlChange:
      if (msg.ControlChangeIdx() == IMidiMsg::kAllNotesOff)
        mNotes.AllOff();
      break;
    default:
      break;
  }
}

void SergeMiddleCombined::OnReset()
{
  mMidiQueue.Resize(GetBlockSize());
  mMidiQueue.Clear();
  mNotes.AllOff();
  double fs = GetSampleRate();
  if (!(fs > 0.0) || !std::isfinite(fs))
    fs = slope::kFs;
  mFold.Reset(GetParam(kFold)->Value(), fs);
  mSymmetry.Reset(GetParam(kIn2)->Value(), fs);
  for (auto& channel : mChannels)
    channel.Reset(fs);
  mCycle = GetParam(kCycle)->Bool();
  mArm = mCycle;
}

void SergeMiddleCombined::ProcessMidiMsg(const IMidiMsg& msg)
{
  mMidiQueue.Add(msg);
}

void SergeMiddleCombined::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const bool cycle = GetParam(kCycle)->Bool();
  if (cycle && !mCycle)
    mArm = true;
  mCycle = cycle;

  const double foldTarget = GetParam(kFold)->Value();
  const double in2Target = GetParam(kIn2)->Value();
  const double depth = GetParam(kVcDepth)->Value();
  const patch::Routing routing = RoutingSnapshot();
  const int nChans = std::min(NOutChansConnected(), kMaxChannels);

  patch::BlockMeter meter;
  meter.Begin();

  for (int s = 0; s < nFrames; ++s)
  {
    while (!mMidiQueue.Empty())
    {
      const IMidiMsg& msg = mMidiQueue.Peek();
      if (msg.mOffset > s)
        break;
      HandleMidi(msg);
      mMidiQueue.Remove();
    }

    const double gate = mNotes.Volts();
    const double gS = mFold.Step(foldTarget);
    const double sS = mSymmetry.Step(in2Target);
    const double rise = GetParam(kRise)->Value();
    const double fall = GetParam(kFall)->Value();
    const double slopeDepth = GetParam(kSlopeVc)->Value();
    const int route = GetParam(kRoute)->Int();
    const bool arm = cycle && mArm;
    for (int c = 0; c < nChans; ++c)
    {
      const double host = static_cast<double>(inputs[c][s]);
      const double y = mChannels[c].Step(host, gate, gS, sS, depth, rise, fall, cycle, arm, routing,
                                         slopeDepth, route);
      outputs[c][s] = static_cast<sample>(y);
      if (c == 0)
      {
        for (int i = 0; i < patch::kNumInputs; ++i)
          meter.Observe(patch::MeterOf(false, i), mChannels[0].InputVolts(i));
        for (int i = 0; i < patch::kNumOutputs; ++i)
          meter.Observe(patch::MeterOf(true, i), mChannels[0].OutputVolts(i));
      }
    }
    if (arm && nChans > 0)
      mArm = false;
  }

  mMidiQueue.Flush(nFrames);
  for (int i = 0; i < patch::kMeterCount; ++i)
  {
    mMeterMin[static_cast<size_t>(i)].store(static_cast<float>(meter.min[i]), std::memory_order_release);
    mMeterMax[static_cast<size_t>(i)].store(static_cast<float>(meter.max[i]), std::memory_order_release);
  }
}

bool SergeMiddleCombined::SerializeState(IByteChunk& chunk) const
{
  if (!SerializeParams(chunk))
    return false;
  std::vector<uint8_t> bytes;
  patch::SerializeRouting(RoutingSnapshot(), bytes);
  return chunk.PutBytes(bytes.data(), static_cast<int>(bytes.size())) > 0;
}

int SergeMiddleCombined::UnserializeState(const IByteChunk& chunk, int startPos)
{
  const int pos = UnserializeParams(chunk, startPos);
  if (pos < 0)
    return pos;

  patch::Routing routing;
  int consumed = 0;
  // Hosts and a later version may append bytes. Read the routing block only.
  if (chunk.Size() - pos >= patch::kRoutingBytes)
  {
    uint8_t bytes[patch::kRoutingBytes];
    if (chunk.GetBytes(bytes, patch::kRoutingBytes, pos) >= 0)
    {
      consumed = patch::kRoutingBytes;
      if (!patch::ParseRouting(bytes, static_cast<size_t>(patch::kRoutingBytes), routing))
        routing.Clear();
    }
  }
  ApplyRouting(routing);
  if (GetUI() != nullptr)
  {
    if (IControl* cables = GetUI()->GetControlWithTag(kCableTag))
      cables->SetDirty(false);
    if (IControl* rate = GetUI()->GetControlWithTag(kRateTag))
      rate->SetDirty(false);
  }
  return pos + consumed;
}
#endif
