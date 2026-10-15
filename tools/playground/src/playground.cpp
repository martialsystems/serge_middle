// Patch playground. Renders one JSON patch through the real DSP and writes
// every jack voltage as CSV. The cells, the slope, and the combined patch
// are the headers already in this repo. This file does not change them.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "SergeMiddleCombined/dsp/Patch.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

struct Value
{
  enum Kind { Null, Bool, Num, Str, Arr, Obj } kind = Null;
  bool b = false;
  double n = 0.0;
  std::string s;
  std::vector<Value> a;
  std::vector<std::pair<std::string, Value>> o;

  const Value* get(const std::string& key) const
  {
    for (const auto& kv : o)
    {
      if (kv.first == key)
        return &kv.second;
    }
    return nullptr;
  }

  bool has(const std::string& key) const { return get(key) != nullptr; }
};

class JsonParser
{
public:
  explicit JsonParser(std::string text) : mText(std::move(text)) {}

  Value parse()
  {
    skip();
    Value v = parseValue();
    skip();
    if (mPos != mText.size())
      fail("trailing data");
    return v;
  }

private:
  void fail(const char* what) const
  {
    throw std::runtime_error(std::string("JSON: ") + what + " at " + std::to_string(mPos));
  }

  void skip()
  {
    while (mPos < mText.size() && (mText[mPos] == ' ' || mText[mPos] == '\t' || mText[mPos] == '\n' ||
                                   mText[mPos] == '\r'))
      ++mPos;
  }

  char peek()
  {
    skip();
    if (mPos >= mText.size())
      fail("unexpected end");
    return mText[mPos];
  }

  char getc()
  {
    const char c = peek();
    ++mPos;
    return c;
  }

  Value parseValue()
  {
    const char c = peek();
    if (c == '{')
      return parseObject();
    if (c == '[')
      return parseArray();
    if (c == '"')
      return parseString();
    if (c == 't' || c == 'f')
      return parseBool();
    if (c == 'n')
      return parseNull();
    if (c == '-' || (c >= '0' && c <= '9'))
      return parseNumber();
    fail("value");
    return {};
  }

  Value parseObject()
  {
    Value v;
    v.kind = Value::Obj;
    getc();
    skip();
    if (peek() == '}')
    {
      ++mPos;
      return v;
    }
    while (true)
    {
      if (peek() != '"')
        fail("object key");
      const Value key = parseString();
      if (getc() != ':')
        fail("colon");
      v.o.emplace_back(key.s, parseValue());
      skip();
      const char c = getc();
      if (c == '}')
        return v;
      if (c != ',')
        fail("comma");
    }
  }

  Value parseArray()
  {
    Value v;
    v.kind = Value::Arr;
    getc();
    skip();
    if (peek() == ']')
    {
      ++mPos;
      return v;
    }
    while (true)
    {
      v.a.push_back(parseValue());
      skip();
      const char c = getc();
      if (c == ']')
        return v;
      if (c != ',')
        fail("comma");
    }
  }

  Value parseString()
  {
    if (getc() != '"')
      fail("string");
    Value v;
    v.kind = Value::Str;
    while (mPos < mText.size())
    {
      const char c = mText[mPos++];
      if (c == '"')
        return v;
      if (c == '\\')
      {
        if (mPos >= mText.size())
          fail("escape");
        const char e = mText[mPos++];
        if (e == '"' || e == '\\' || e == '/')
          v.s.push_back(e);
        else if (e == 'n')
          v.s.push_back('\n');
        else if (e == 't')
          v.s.push_back('\t');
        else
          fail("escape");
      }
      else
        v.s.push_back(c);
    }
    fail("unterminated string");
    return v;
  }

  Value parseBool()
  {
    Value v;
    v.kind = Value::Bool;
    if (mText.compare(mPos, 4, "true") == 0)
    {
      v.b = true;
      mPos += 4;
      return v;
    }
    if (mText.compare(mPos, 5, "false") == 0)
    {
      v.b = false;
      mPos += 5;
      return v;
    }
    fail("bool");
    return v;
  }

  Value parseNull()
  {
    if (mText.compare(mPos, 4, "null") != 0)
      fail("null");
    mPos += 4;
    Value v;
    v.kind = Value::Null;
    return v;
  }

  Value parseNumber()
  {
    const std::size_t start = mPos;
    if (mText[mPos] == '-')
      ++mPos;
    if (mPos >= mText.size() || mText[mPos] < '0' || mText[mPos] > '9')
      fail("number");
    while (mPos < mText.size() && mText[mPos] >= '0' && mText[mPos] <= '9')
      ++mPos;
    if (mPos < mText.size() && mText[mPos] == '.')
    {
      ++mPos;
      while (mPos < mText.size() && mText[mPos] >= '0' && mText[mPos] <= '9')
        ++mPos;
    }
    if (mPos < mText.size() && (mText[mPos] == 'e' || mText[mPos] == 'E'))
    {
      ++mPos;
      if (mPos < mText.size() && (mText[mPos] == '+' || mText[mPos] == '-'))
        ++mPos;
      while (mPos < mText.size() && mText[mPos] >= '0' && mText[mPos] <= '9')
        ++mPos;
    }
    Value v;
    v.kind = Value::Num;
    v.n = std::strtod(mText.c_str() + start, nullptr);
    return v;
  }

  std::string mText;
  std::size_t mPos = 0;
};

struct Knob
{
  bool ramp = false;
  double value = 0.0;
  std::vector<std::pair<double, double>> points;

  double at(double tMs) const
  {
    if (!ramp || points.empty())
      return value;
    if (tMs <= points.front().first)
      return points.front().second;
    if (tMs >= points.back().first)
      return points.back().second;
    for (std::size_t i = 1; i < points.size(); ++i)
    {
      if (tMs <= points[i].first)
      {
        const double t0 = points[i - 1].first;
        const double t1 = points[i].first;
        const double u = (t1 == t0) ? 0.0 : (tMs - t0) / (t1 - t0);
        return points[i - 1].second + u * (points[i].second - points[i - 1].second);
      }
    }
    return points.back().second;
  }
};

struct Span
{
  bool samples = true;
  double a = 0.0;
  double b = 0.0;
};

struct Signal
{
  enum Type { Dc, Sine, Saw, Gate, Pulses, Silence } type = Dc;
  double amp = 0.0;
  double freq = 0.0;
  double phase = 0.0;
  double dc = 0.0;
  double high = 5.0;
  double low = 0.0;
  int width = 1;
  std::vector<Span> spans;
  std::vector<Span> ats;

  double at(int n, double fs) const
  {
    if (type == Silence)
      return 0.0;
    if (type == Dc)
      return dc;
    if (type == Sine)
      return amp * std::sin(2.0 * std::acos(-1.0) * freq * static_cast<double>(n) / fs + phase);
    if (type == Saw)
    {
      const double cycles = freq * static_cast<double>(n) / fs;
      const double frac = cycles - std::floor(cycles);
      return amp * (2.0 * frac - 1.0);
    }
    if (type == Gate)
      return inside(n, fs, spans) ? high : low;
    if (type == Pulses)
    {
      for (const Span& at : ats)
      {
        const int start = toSample(at, fs);
        if (n >= start && n < start + width)
          return high;
      }
      return low;
    }
    return 0.0;
  }

private:
  static int toSample(const Span& span, double fs)
  {
    if (span.samples)
      return static_cast<int>(std::llround(span.a));
    return static_cast<int>(std::llround(span.a * fs / 1000.0));
  }

  static bool inside(int n, double fs, const std::vector<Span>& spans)
  {
    for (const Span& span : spans)
    {
      const int a = toSample(span, fs);
      const int b = span.samples ? static_cast<int>(std::llround(span.b))
                                 : static_cast<int>(std::llround(span.b * fs / 1000.0));
      if (n >= a && n < b)
        return true;
    }
    return false;
  }
};

struct MidiNote
{
  int note = 0;
  Span on;
  Span off;

  int onSample(double fs) const { return sampleOf(on, fs); }
  int offSample(double fs) const { return sampleOf(off, fs); }

private:
  static int sampleOf(const Span& span, double fs)
  {
    if (span.samples)
      return static_cast<int>(std::llround(span.a));
    return static_cast<int>(std::llround(span.a * fs / 1000.0));
  }
};

struct Segment
{
  double t0 = 0.0;
  double t1 = 0.0;
  bool reset = true;
  bool hasRoute = false;
  std::string route = "both";
  int cycle = -1; // -1 inherit, 0 off, 1 on
  std::map<std::string, Knob> knobs;
};

struct PatchFile
{
  std::string name;
  std::string engine;
  double sampleRate = 48000.0;
  double durationMs = 0.0;
  std::map<std::string, Knob> knobs;
  std::string route = "both";
  bool cycle = false;
  std::map<std::string, Signal> signals;
  std::vector<MidiNote> notes;
  std::vector<std::pair<std::string, std::string>> cables;
  std::vector<Segment> segments;
};

[[noreturn]] void die(const std::string& what)
{
  throw std::runtime_error(what);
}

const Value& need(const Value& obj, const char* key)
{
  const Value* v = obj.get(key);
  if (v == nullptr)
    die(std::string("missing ") + key);
  return *v;
}

Knob parseKnob(const Value& v, double fallback)
{
  Knob knob;
  knob.value = fallback;
  if (v.kind == Value::Num)
  {
    knob.value = v.n;
    return knob;
  }
  if (v.kind == Value::Arr)
  {
    knob.ramp = true;
    for (const Value& pt : v.a)
    {
      if (pt.kind != Value::Arr || pt.a.size() != 2 || pt.a[0].kind != Value::Num || pt.a[1].kind != Value::Num)
        die("a ramp point is [time_ms, value]");
      knob.points.emplace_back(pt.a[0].n, pt.a[1].n);
    }
    if (knob.points.empty())
      die("empty ramp");
    return knob;
  }
  die("knob must be a number or a ramp");
}

Span parseSpanPair(const Value& v, bool samples)
{
  if (v.kind != Value::Arr || v.a.size() != 2)
    die("span must be a pair");
  Span span;
  span.samples = samples;
  span.a = v.a[0].kind == Value::Num ? v.a[0].n : 0.0;
  span.b = v.a[1].kind == Value::Num ? v.a[1].n : 0.0;
  if (v.a[0].kind != Value::Num || v.a[1].kind != Value::Num)
    die("span endpoints must be numbers");
  return span;
}

Signal parseSignal(const Value& v)
{
  Signal signal;
  const std::string type = need(v, "type").s;
  if (type == "dc")
  {
    signal.type = Signal::Dc;
    signal.dc = v.has("volts") ? need(v, "volts").n : 0.0;
  }
  else if (type == "silence")
    signal.type = Signal::Silence;
  else if (type == "sine" || type == "saw")
  {
    signal.type = type == "sine" ? Signal::Sine : Signal::Saw;
    signal.amp = need(v, "amp").n;
    signal.freq = need(v, "freq_hz").n;
    if (v.has("phase_deg") && signal.type == Signal::Sine)
      signal.phase = need(v, "phase_deg").n * std::acos(-1.0) / 180.0;
  }
  else if (type == "gate")
  {
    signal.type = Signal::Gate;
    signal.high = v.has("high") ? need(v, "high").n : 5.0;
    signal.low = v.has("low") ? need(v, "low").n : 0.0;
    const bool samples = v.has("spans_samples");
    const Value& spans = samples ? need(v, "spans_samples") : need(v, "spans_ms");
    for (const Value& span : spans.a)
      signal.spans.push_back(parseSpanPair(span, samples));
  }
  else if (type == "pulses")
  {
    signal.type = Signal::Pulses;
    signal.high = v.has("high") ? need(v, "high").n : 5.0;
    signal.low = 0.0;
    signal.width = v.has("width_samples") ? static_cast<int>(need(v, "width_samples").n) : 1;
    const bool samples = v.has("at_samples");
    const Value& ats = samples ? need(v, "at_samples") : need(v, "at_ms");
    for (const Value& at : ats.a)
    {
      if (at.kind != Value::Num)
        die("pulse time must be a number");
      Span span;
      span.samples = samples;
      span.a = at.n;
      signal.ats.push_back(span);
    }
  }
  else
    die("unknown signal type " + type);
  return signal;
}

void parseKnobs(const Value& params, std::map<std::string, Knob>& knobs, std::string& route, int& cycleFlag)
{
  static const char* kNames[] = {"rise_s", "fall_s", "vc_depth", "pleat_vc_depth", "fold", "in2"};
  for (const auto& kv : params.o)
  {
    if (kv.first == "route")
    {
      if (kv.second.kind != Value::Str)
        die("route must be rise, both, or fall");
      route = kv.second.s;
      continue;
    }
    if (kv.first == "cycle")
    {
      if (kv.second.kind != Value::Bool)
        die("cycle must be a bool");
      cycleFlag = kv.second.b ? 1 : 0;
      continue;
    }
    bool known = false;
    for (const char* name : kNames)
      known = known || kv.first == name;
    if (!known)
      die("unknown param " + kv.first);
    const double fallback = (kv.first == "fold") ? 1.0 : (kv.first == "rise_s" || kv.first == "fall_s") ? 0.005 : 0.0;
    knobs[kv.first] = parseKnob(kv.second, fallback);
  }
}

MidiNote parseNote(const Value& v)
{
  MidiNote note;
  note.note = static_cast<int>(need(v, "note").n);
  if (v.has("on_sample"))
  {
    note.on.samples = true;
    note.on.a = need(v, "on_sample").n;
    note.off.samples = true;
    note.off.a = need(v, "off_sample").n;
  }
  else
  {
    note.on.samples = false;
    note.on.a = need(v, "on_ms").n;
    note.off.samples = false;
    note.off.a = need(v, "off_ms").n;
  }
  return note;
}

PatchFile parsePatch(const Value& root)
{
  PatchFile patch;
  patch.name = need(root, "name").s;
  patch.engine = need(root, "engine").s;
  if (patch.engine != "slope" && patch.engine != "pleat" && patch.engine != "patch")
    die("engine must be slope, pleat, or patch");
  if (root.has("sample_rate"))
    patch.sampleRate = need(root, "sample_rate").n;
  patch.durationMs = need(root, "duration_ms").n;
  if (!(patch.durationMs > 0.0) || !(patch.sampleRate > 0.0))
    die("duration_ms and sample_rate must be positive");
  if (const Value* params = root.get("params"))
  {
    int cycleFlag = patch.cycle ? 1 : 0;
    parseKnobs(*params, patch.knobs, patch.route, cycleFlag);
    patch.cycle = cycleFlag == 1;
  }
  if (const Value* signals = root.get("signals"))
  {
    for (const auto& kv : signals->o)
    {
      if (kv.first == "midi")
      {
        const Value& events = need(kv.second, "events");
        for (const Value& event : events.a)
          patch.notes.push_back(parseNote(event));
      }
      else if (kv.first == "host" || kv.first == "in" || kv.first == "trig" || kv.first == "vc")
        patch.signals[kv.first] = parseSignal(kv.second);
      else
        die("unknown signal jack " + kv.first);
    }
  }
  if (const Value* cables = root.get("cables"))
  {
    for (const Value& cable : cables->a)
    {
      patch.cables.emplace_back(need(cable, "from").s, need(cable, "to").s);
    }
  }
  if (const Value* segments = root.get("segments"))
  {
    for (const Value& seg : segments->a)
    {
      Segment segment;
      segment.t0 = need(seg, "t0_ms").n;
      segment.t1 = need(seg, "t1_ms").n;
      segment.reset = !seg.has("reset") || need(seg, "reset").b;
      if (const Value* params = seg.get("params"))
      {
        int cycleFlag = -1;
        std::string route;
        parseKnobs(*params, segment.knobs, route, cycleFlag);
        if (params->has("route"))
        {
          segment.hasRoute = true;
          segment.route = route;
        }
        segment.cycle = cycleFlag;
      }
      patch.segments.push_back(segment);
    }
  }
  return patch;
}

int routeInt(const std::string& route)
{
  if (route == "rise")
    return 0;
  if (route == "fall")
    return 2;
  if (route == "both")
    return 1;
  die("route must be rise, both, or fall");
}

int msToSample(double ms, double fs) { return static_cast<int>(std::llround(ms * fs / 1000.0)); }

const Segment* segmentAt(const PatchFile& patch, double tMs)
{
  for (const Segment& segment : patch.segments)
  {
    if (tMs >= segment.t0 && tMs < segment.t1)
      return &segment;
  }
  return nullptr;
}

double knobAt(const PatchFile& patch, const Segment* segment, const char* name, double fallback, double tMs)
{
  if (segment != nullptr)
  {
    const auto it = segment->knobs.find(name);
    if (it != segment->knobs.end())
      return it->second.at(tMs);
  }
  const auto it = patch.knobs.find(name);
  if (it != patch.knobs.end())
    return it->second.at(tMs);
  return fallback;
}

bool cycleAt(const PatchFile& patch, const Segment* segment)
{
  if (segment != nullptr && segment->cycle >= 0)
    return segment->cycle == 1;
  return patch.cycle;
}

std::string routeAt(const PatchFile& patch, const Segment* segment)
{
  if (segment != nullptr && segment->hasRoute)
    return segment->route;
  return patch.route;
}

double signalAt(const PatchFile& patch, const char* jack, int n, double fs)
{
  const auto it = patch.signals.find(jack);
  if (it == patch.signals.end())
    return 0.0;
  return it->second.at(n, fs);
}

int outputIndex(const std::string& name)
{
  if (name == "pleat_out")
    return patch::PleatOut;
  if (name == "slope_end")
    return patch::SlopeEnd;
  if (name == "slope_out")
    return patch::SlopeOut;
  if (name == "slope_ac")
    return patch::SlopeAc;
  die("unknown output jack " + name);
}

int inputIndex(const std::string& name)
{
  if (name == "pleat_in1")
    return patch::PleatIn1;
  if (name == "pleat_in2")
    return patch::PleatIn2;
  if (name == "pleat_vc")
    return patch::PleatVc;
  if (name == "slope_in")
    return patch::SlopeIn;
  if (name == "slope_trig")
    return patch::SlopeTrig;
  if (name == "slope_vc")
    return patch::SlopeVc;
  die("unknown input jack " + name);
}

void writeHeader(std::FILE* out, std::initializer_list<const char*> names)
{
  std::fputs("sample,time_ms", out);
  for (const char* name : names)
    std::fprintf(out, ",%s", name);
  std::fputc('\n', out);
}

void writeRow(std::FILE* out, int n, double tMs, const std::vector<double>& cols)
{
  std::fprintf(out, "%d,%.17g", n, tMs);
  for (double v : cols)
    std::fprintf(out, ",%.17g", v);
  std::fputc('\n', out);
}

void renderSlope(const PatchFile& patch, double fs, int nSamples, std::FILE* out)
{
  const double rise0 = knobAt(patch, nullptr, "rise_s", 0.005, 0.0);
  const double fall0 = knobAt(patch, nullptr, "fall_s", 0.005, 0.0);
  slope::SecondHalf half(rise0, fall0, fs);
  slope::AcCoupler coupler;
  coupler.SetSampleRate(fs);
  half.Reset();
  coupler.Reset();
  bool armNext = true;
  writeHeader(out, {"in", "trig", "vc", "out", "end", "ac", "ac_coupled"});
  for (int n = 0; n < nSamples; ++n)
  {
    const double tMs = 1000.0 * static_cast<double>(n) / fs;
    const Segment* segment = segmentAt(patch, tMs);
    if (segment != nullptr && segment->reset && n == msToSample(segment->t0, fs))
    {
      half.Reset();
      coupler.Reset();
      armNext = true;
    }
    const double rise = knobAt(patch, segment, "rise_s", 0.005, tMs);
    const double fall = knobAt(patch, segment, "fall_s", 0.005, tMs);
    const double depth = knobAt(patch, segment, "vc_depth", 0.0, tMs);
    const bool cycle = cycleAt(patch, segment);
    half.SetSwitch(slope::SwitchFromInt(routeInt(routeAt(patch, segment))));
    half.SetTimes(rise, fall);
    const double inV = signalAt(patch, "in", n, fs);
    double trig = signalAt(patch, "trig", n, fs);
    const double vc = signalAt(patch, "vc", n, fs);
    const bool arm = cycle && armNext;
    if (arm)
      armNext = false;
    if (arm && !(trig > slope::kTrigThreshold))
      trig = patch::kGateVolts;
    const slope::Sample sample = half.Step(inV, trig, slope::ScaleVc(vc, depth), 0.0, 0.0, cycle);
    const double end = slope::EndJackVolts(sample.end, sample.out);
    const double coupled = coupler.Step(sample.ac - slope::kAcCenter);
    writeRow(out, n, tMs, {inV, trig, vc, sample.out, end, sample.ac, coupled});
  }
}

void renderPleat(const PatchFile& patch, double fs, int nSamples, std::FILE* out)
{
  serge::WaveMiddleChannel channel;
  serge::FoldSmoother fold;
  serge::SymmetrySmoother symmetry;
  const double fold0 = knobAt(patch, nullptr, "fold", 1.0, 0.0);
  const double in20 = knobAt(patch, nullptr, "in2", 0.0, 0.0);
  channel.Reset(fs);
  fold.Reset(fold0, fs);
  symmetry.Reset(in20, fs);
  writeHeader(out, {"in1", "in2", "out"});
  for (int n = 0; n < nSamples; ++n)
  {
    const double tMs = 1000.0 * static_cast<double>(n) / fs;
    const Segment* segment = segmentAt(patch, tMs);
    const double foldTarget = knobAt(patch, segment, "fold", 1.0, tMs);
    const double in2Target = knobAt(patch, segment, "in2", 0.0, tMs);
    if (segment != nullptr && segment->reset && n == msToSample(segment->t0, fs))
    {
      channel.Reset(fs);
      fold.Reset(foldTarget, fs);
      symmetry.Reset(in2Target, fs);
    }
    const double g = fold.Step(foldTarget);
    const double s = symmetry.Step(in2Target);
    const double host = signalAt(patch, "host", n, fs);
    const double y = channel.Step(host, g, s);
    writeRow(out, n, tMs, {serge::kAudioFullScaleVolts * host, s, serge::kAudioFullScaleVolts * y});
  }
}

void renderPatch(const PatchFile& patch, double fs, int nSamples, std::FILE* out)
{
  patch::PatchChannel channel;
  serge::FoldSmoother fold;
  serge::SymmetrySmoother symmetry;
  patch::GateNotes notes;
  patch::Routing routing;
  for (const auto& cable : patch.cables)
    routing.Connect(outputIndex(cable.first), inputIndex(cable.second));
  const double fold0 = knobAt(patch, nullptr, "fold", 1.0, 0.0);
  const double in20 = knobAt(patch, nullptr, "in2", 0.0, 0.0);
  channel.Reset(fs);
  fold.Reset(fold0, fs);
  symmetry.Reset(in20, fs);
  bool armNext = true;
  writeHeader(out, {"pleat_in1", "pleat_in2", "pleat_vc", "slope_in", "slope_trig", "slope_vc",
                    "pleat_out", "slope_end", "slope_out", "slope_ac"});
  for (int n = 0; n < nSamples; ++n)
  {
    const double tMs = 1000.0 * static_cast<double>(n) / fs;
    const Segment* segment = segmentAt(patch, tMs);
    const double foldTarget = knobAt(patch, segment, "fold", 1.0, tMs);
    const double in2Target = knobAt(patch, segment, "in2", 0.0, tMs);
    if (segment != nullptr && segment->reset && n == msToSample(segment->t0, fs))
    {
      channel.Reset(fs);
      fold.Reset(foldTarget, fs);
      symmetry.Reset(in2Target, fs);
      notes.AllOff();
      for (const MidiNote& note : patch.notes)
      {
        if (note.onSample(fs) <= n && n < note.offSample(fs))
          notes.NoteOn(note.note);
      }
      armNext = true;
    }
    for (const MidiNote& note : patch.notes)
    {
      if (note.offSample(fs) == n)
        notes.NoteOff(note.note);
      if (note.onSample(fs) == n)
        notes.NoteOn(note.note);
    }
    const double g = fold.Step(foldTarget);
    const double s = symmetry.Step(in2Target);
    const double rise = knobAt(patch, segment, "rise_s", 0.005, tMs);
    const double fall = knobAt(patch, segment, "fall_s", 0.005, tMs);
    const double depth = knobAt(patch, segment, "pleat_vc_depth", 0.0, tMs);
    const double slopeDepth = knobAt(patch, segment, "vc_depth", 0.0, tMs);
    const bool cycle = cycleAt(patch, segment);
    const bool arm = cycle && armNext;
    if (arm)
      armNext = false;
    const double host = signalAt(patch, "host", n, fs);
    channel.Step(host, notes.Volts(), g, s, depth, rise, fall, cycle, arm, routing, slopeDepth,
                 routeInt(routeAt(patch, segment)));
    writeRow(out, n, tMs,
             {channel.InputVolts(patch::PleatIn1), channel.InputVolts(patch::PleatIn2),
              channel.InputVolts(patch::PleatVc), channel.InputVolts(patch::SlopeIn),
              channel.InputVolts(patch::SlopeTrig), channel.InputVolts(patch::SlopeVc),
              channel.OutputVolts(patch::PleatOut), channel.OutputVolts(patch::SlopeEnd),
              channel.OutputVolts(patch::SlopeOut), channel.OutputVolts(patch::SlopeAc)});
  }
}

void usage()
{
  std::fputs("usage: playground --patch FILE --out CSV [--sample-rate HZ]\n", stderr);
}

} // namespace

int main(int argc, char** argv)
{
  std::string patchPath;
  std::string outPath;
  double rateOverride = 0.0;
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--patch" && i + 1 < argc)
      patchPath = argv[++i];
    else if (arg == "--out" && i + 1 < argc)
      outPath = argv[++i];
    else if (arg == "--sample-rate" && i + 1 < argc)
      rateOverride = std::strtod(argv[++i], nullptr);
    else if (arg == "--help")
    {
      usage();
      return 0;
    }
    else
    {
      usage();
      return 2;
    }
  }
  if (patchPath.empty() || outPath.empty())
  {
    usage();
    return 2;
  }
  try
  {
    std::ifstream in(patchPath);
    if (!in)
      die("cannot open " + patchPath);
    std::stringstream buffer;
    buffer << in.rdbuf();
    const Value root = JsonParser(buffer.str()).parse();
    PatchFile patch = parsePatch(root);
    if (rateOverride > 0.0)
      patch.sampleRate = rateOverride;
    const int nSamples = msToSample(patch.durationMs, patch.sampleRate);
    if (nSamples <= 0)
      die("duration is empty");
    std::FILE* out = std::fopen(outPath.c_str(), "wb");
    if (out == nullptr)
      die("cannot write " + outPath);
    if (patch.engine == "slope")
      renderSlope(patch, patch.sampleRate, nSamples, out);
    else if (patch.engine == "pleat")
      renderPleat(patch, patch.sampleRate, nSamples, out);
    else
      renderPatch(patch, patch.sampleRate, nSamples, out);
    std::fclose(out);
    std::printf("samples %d rate %.0f engine %s\n", nSamples, patch.sampleRate, patch.engine.c_str());
  }
  catch (const std::exception& ex)
  {
    std::fprintf(stderr, "playground: %s\n", ex.what());
    return 1;
  }
  return 0;
}
