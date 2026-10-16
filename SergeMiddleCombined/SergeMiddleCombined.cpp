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
constexpr int kRateTag = 101;
constexpr int kRate2Tag = 102;
constexpr double kStarterFold = 2.0;
constexpr double kStarterRise = 1.5;
constexpr double kStarterFall = 2.5;

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

void ShowUnit(double value, WDL_String& display)
{
  char buf[16];
  vcm::FormatUnit(value, buf, sizeof buf);
  display.Set(buf);
}
} // namespace

SergeMiddleCombined::SergeMiddleCombined(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  for (auto& slot : mRouting)
    slot.source.store(-1, std::memory_order_relaxed);
  for (auto& meter : mMeterMin)
    meter.store(0.f, std::memory_order_relaxed);
  for (auto& meter : mMeterMax)
    meter.store(0.f, std::memory_order_relaxed);

  // Defaults are the starter patch (see README): a sine folded at 2, swept by
  // Slope 1 cycling 1.5 s up and 2.5 s down through the PLEAT VC normal.
  GetParam(kFold)->InitDouble("Fold", kStarterFold, serge::kGMin, serge::kGMax, 0.001, "",
                              0, "", IParam::ShapeExp());
  GetParam(kIn2)->InitDouble("IN 2 DC", serge::kSymDefault, serge::kSymMin, serge::kSymMax, 0.01, "V");
  GetParam(kVcDepth)->InitDouble("VC Depth", 0.5, 0.0, 1.0, 0.001);
  GetParam(kRise)->InitDouble("Rise", kStarterRise, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());
  GetParam(kFall)->InitDouble("Fall", kStarterFall, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());
  GetParam(kRise)->SetDisplayFunc(ShowTime);
  GetParam(kFall)->SetDisplayFunc(ShowTime);
  GetParam(kCycle)->InitBool("Cycle", true);
  // Appended so a project saved before these controls keeps the six above.
  // Depth 0 leaves a slope VC cord at 0 V. Route 1 is BOTH, the circuit default.
  GetParam(kSlopeVc)->InitDouble("Slope VC", 0.0, -1.0, 1.0, 0.001);
  GetParam(kRoute)->InitInt("Route", 1, 0, 2);
  GetParam(kRoute)->SetDisplayFunc(ShowRoute);
  // Appended after Route. An older project keeps the eight above and loads these at the defaults.
  GetParam(kTopGain)->InitDouble("Top Gain", 0.0, 0.0, 1.0, 0.001);
  GetParam(kTopGain)->SetDisplayFunc(ShowUnit);
  GetParam(kTopHi)->InitBool("Top HI", false);
  GetParam(kBottomGain)->InitDouble("Bottom Level", 0.0, 0.0, 1.0, 0.001);
  GetParam(kBottomGain)->SetDisplayFunc(ShowUnit);
  GetParam(kBottomVc)->InitDouble("Bottom VC", 1.0, 0.0, 1.0, 0.001);
  GetParam(kRise2)->InitDouble("Rise 2", 0.005, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());
  GetParam(kFall2)->InitDouble("Fall 2", 0.005, 0.0001, 10.0, 0.0001, "s", 0, "", IParam::ShapeExp());
  GetParam(kRise2)->SetDisplayFunc(ShowTime);
  GetParam(kFall2)->SetDisplayFunc(ShowTime);
  GetParam(kCycle2)->InitBool("Cycle 2", false);
  GetParam(kSlopeVc2)->InitDouble("Slope VC 2", 0.0, -1.0, 1.0, 0.001);
  GetParam(kRoute2)->InitInt("Route 2", 1, 0, 2);
  GetParam(kRoute2)->SetDisplayFunc(ShowRoute);
  // Test oscillator, appended after Route 2.
  GetParam(kOscWave)->InitEnum("Osc Wave", 0, {"SINE", "SAW", "SQUARE"});
  GetParam(kOscFreq)->InitDouble("Osc Freq", 110.0, 20.0, 2000.0, 0.01, "Hz", 0, "", IParam::ShapeExp());
  GetParam(kOscLevel)->InitDouble("Osc Level", -12.0, -60.0, 0.0, 0.1, "dB");
  GetParam(kOscToIn1)->InitBool("Osc to IN 1", true);

  ApplyRouting(StarterRouting());

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    // Scale keeps the plate aspect. The minimum is 1200 by 720. The maximum
    // is 2400 by 1440. The default 1440 by 864 fits 1920 by 1080 with the Dock.
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false, IColor(210, 26, 24, 22));
    pGraphics->SetScaleConstraints(static_cast<float>(PLUG_MIN_WIDTH) / static_cast<float>(PLUG_WIDTH),
                                   static_cast<float>(PLUG_MAX_WIDTH) / static_cast<float>(PLUG_WIDTH));
    pGraphics->EnableMouseOver(true);
    pGraphics->AttachPanelBackground(IColor(255, 244, 239, 228));
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->AttachTextEntryControl();
    // Spots are the ids_v6 centres. tools/art/slewfold_face.py is the same list.
    patchview::AttachFace(pGraphics, *this);
  };
#endif
}

patch::Routing SergeMiddleCombined::StarterRouting()
{
  patch::Routing routing;
  routing.Connect(patch::OscOut, patch::PleatIn1);
  routing.Connect(patch::SlopeOut, patch::PleatVc);
  return routing;
}

SergeMiddleCombined::~SergeMiddleCombined()
{
#if IPLUG_EDITOR
  delete mFace;
  mFace = nullptr;
#endif
}

#if IPLUG_EDITOR
patchview::FaceState* SergeMiddleCombined::EditorFace()
{
  if (mFace == nullptr)
    mFace = new patchview::FaceState();
  return mFace;
}
#endif

#if IPLUG_EDITOR
void SergeMiddleCombined::OnParentWindowResize(int width, int height)
{
  if (mFittingResize || GetUI() == nullptr)
    return;
  const float platform = GetUI()->GetPlatformWindowScale();
  if (!(platform > 0.f))
    return;
  const float w = static_cast<float>(width) / platform;
  const float h = static_cast<float>(height) / platform;
  float scale = std::min(w / static_cast<float>(PLUG_WIDTH), h / static_cast<float>(PLUG_HEIGHT));
  const float lo = static_cast<float>(PLUG_MIN_WIDTH) / static_cast<float>(PLUG_WIDTH);
  const float hi = static_cast<float>(PLUG_MAX_WIDTH) / static_cast<float>(PLUG_WIDTH);
  if (scale < lo)
    scale = lo;
  if (scale > hi)
    scale = hi;
  const float have = GetUI()->GetDrawScale();
  const float wantW = static_cast<float>(PLUG_WIDTH) * scale;
  const float wantH = static_cast<float>(PLUG_HEIGHT) * scale;
  if (std::fabs(w - wantW) < 2.f && std::fabs(h - wantH) < 2.f && std::fabs(have - scale) < 0.01f)
    return;
  mFittingResize = true;
  GetUI()->Resize(PLUG_WIDTH, PLUG_HEIGHT, scale, true);
  mFittingResize = false;
}
#endif

patch::Routing SergeMiddleCombined::RoutingSnapshot() const
{
  patch::Routing routing;
  for (int i = 0; i < patch::kNumInputs; ++i)
    routing.source[i] = mRouting[static_cast<size_t>(i)].source.load(std::memory_order_acquire);
  return routing;
}

void SergeMiddleCombined::ApplyRouting(const patch::Routing& routing)
{
  for (int i = 0; i < patch::kNumInputs; ++i)
  {
    int8_t source = routing.source[i];
    if (source < -1 || source >= patch::kNumOutputs)
      source = -1;
    mRouting[static_cast<size_t>(i)].source.store(source, std::memory_order_release);
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
  const bool slope1 = paramIdx == kRise || paramIdx == kFall || paramIdx == kCycle;
  const bool slope2 = paramIdx == kRise2 || paramIdx == kFall2 || paramIdx == kCycle2;
  if (!slope1 && !slope2)
    return;
  if (GetUI() == nullptr)
    return;
  const int tag = slope1 ? kRateTag : kRate2Tag;
  if (IControl* rate = GetUI()->GetControlWithTag(tag))
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
  mTopGain.Reset(GetParam(kTopGain)->Value(), fs);
  mBottomGain.Reset(GetParam(kBottomGain)->Value(), fs);
  mBottomVc.Reset(GetParam(kBottomVc)->Value(), fs);
  for (auto& channel : mChannels)
    channel.Reset(fs);
  mCycle = GetParam(kCycle)->Bool();
  mArm = mCycle;
  mCycle2 = GetParam(kCycle2)->Bool();
  mArm2 = mCycle2;
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
  const bool cycle2 = GetParam(kCycle2)->Bool();
  if (cycle2 && !mCycle2)
    mArm2 = true;
  mCycle2 = cycle2;

  const double foldTarget = GetParam(kFold)->Value();
  const double in2Target = GetParam(kIn2)->Value();
  const double depth = GetParam(kVcDepth)->Value();
  const patch::Routing routing = RoutingSnapshot();
  const int oscWave = GetParam(kOscWave)->Int();
  const double oscFreq = GetParam(kOscFreq)->Value();
  const double oscVolts = patch::OscVoltsFromDb(GetParam(kOscLevel)->Value());
  const bool oscToIn1 = GetParam(kOscToIn1)->Bool();
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
    patch::SectionControls extra;
    extra.topGain = mTopGain.Step(GetParam(kTopGain)->Value());
    extra.topHi = GetParam(kTopHi)->Bool();
    extra.bottomGain = mBottomGain.Step(GetParam(kBottomGain)->Value());
    extra.bottomDepth = mBottomVc.Step(GetParam(kBottomVc)->Value());
    extra.rise2 = GetParam(kRise2)->Value();
    extra.fall2 = GetParam(kFall2)->Value();
    extra.cycle2 = cycle2;
    extra.arm2 = cycle2 && mArm2;
    extra.slope2Depth = GetParam(kSlopeVc2)->Value();
    extra.slope2Route = GetParam(kRoute2)->Int();
    extra.oscWave = oscWave;
    extra.oscFreq = oscFreq;
    extra.oscVolts = oscVolts;
    extra.oscToIn1 = oscToIn1;
    for (int c = 0; c < nChans; ++c)
    {
      const double host = static_cast<double>(inputs[c][s]);
      const double y = mChannels[c].Step(host, gate, gS, sS, depth, rise, fall, cycle, arm, routing,
                                         slopeDepth, route, extra);
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
    if (extra.arm2 && nChans > 0)
      mArm2 = false;
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
      // A version 1 header is the old nine-byte block, even when a tail follows it.
      if (bytes[0] == patch::kRoutingMagic && bytes[1] == patch::kLegacyRoutingVersion)
      {
        consumed = patch::kLegacyRoutingBytes;
        if (!patch::ParseRouting(bytes, static_cast<size_t>(patch::kLegacyRoutingBytes), routing))
          routing.Clear();
      }
      else
      {
        consumed = patch::kRoutingBytes;
        if (!patch::ParseRouting(bytes, static_cast<size_t>(patch::kRoutingBytes), routing))
          routing.Clear();
      }
    }
  }
  else if (chunk.Size() - pos >= patch::kLegacyRoutingBytes)
  {
    uint8_t bytes[patch::kLegacyRoutingBytes];
    if (chunk.GetBytes(bytes, static_cast<int>(patch::kLegacyRoutingBytes), pos) >= 0)
    {
      consumed = patch::kLegacyRoutingBytes;
      if (!patch::ParseRouting(bytes, static_cast<size_t>(patch::kLegacyRoutingBytes), routing))
        routing.Clear();
    }
  }
  ApplyRouting(routing);
  if (GetUI() != nullptr)
  {
    if (IControl* cables = GetUI()->GetControlWithTag(patchview::kCableTag))
      cables->SetDirty(false);
    if (IControl* rate = GetUI()->GetControlWithTag(kRateTag))
      rate->SetDirty(false);
    if (IControl* rate2 = GetUI()->GetControlWithTag(kRate2Tag))
      rate2->SetDirty(false);
  }
  return pos + consumed;
}
#endif
