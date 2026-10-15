// C++ port against slope/tests/ac_cycle.csv and the locked rise.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "../AcCoupler.h"
#include "../Readout.h"

#ifndef SERGE_REPO_ROOT
#error "SERGE_REPO_ROOT must point at the repository root"
#endif

namespace
{
int gFailures = 0;

void Check(bool ok, const char* what)
{
  if (ok)
  {
    std::printf("ok    %s\n", what);
    return;
  }
  gFailures++;
  std::printf("FAIL  %s\n", what);
}

std::vector<double> ReadVolts(const char* rel)
{
  const std::string path = std::string(SERGE_REPO_ROOT) + "/" + rel;
  std::ifstream in(path);
  if (!in)
  {
    std::printf("FAIL  cannot open %s\n", path.c_str());
    std::exit(1);
  }
  std::vector<double> values;
  std::string line;
  bool header = true;
  while (std::getline(in, line))
  {
    if (line.empty())
      continue;
    if (header)
    {
      header = false;
      continue;
    }
    const auto comma = line.find(',');
    values.push_back(std::strtod(line.c_str() + static_cast<int>(comma) + 1, nullptr));
  }
  return values;
}

void TestAcCycle()
{
  const auto stored = ReadVolts("slope/tests/ac_cycle.csv");
  slope::SecondHalf half(0.005, 0.005, slope::kFs);
  bool match = stored.size() == 481;
  for (int i = 0; match && i < 481; ++i)
  {
    half.SetTimes(0.005, 0.005);
    const auto sample = half.Step(0.0, i == 0 ? 5.0 : 0.0, 0.0, 0.0, 0.0, true);
    match = sample.ac == stored[static_cast<size_t>(i)] && sample.ac == slope::AcVolts(sample.out);
  }
  Check(match, "ac cycle matches ac_cycle.csv");
  slope::SecondHalf marks(0.005, 0.005, slope::kFs);
  slope::Sample at[481];
  for (int i = 0; i < 481; ++i)
    at[i] = marks.Step(0.0, i == 0 ? 5.0 : 0.0, 0.0, 0.0, 0.0, true);
  Check(at[0].ac == 2.5 && slope::ScaledAc(at[0].ac) == 1.0, "sample 0 is +2.5 V, scale +1");
  Check(at[120].ac == 0.0 && slope::ScaledAc(at[120].ac) == 0.0, "sample 120 is 0 V, scale 0");
  Check(at[240].ac == -2.5 && at[240].out == 5.0 && slope::ScaledAc(at[240].ac) == -1.0,
        "sample 240 is -2.5 V, scale -1");
  Check(at[480].ac == 2.5 && at[480].out == 0.0, "sample 480 is +2.5 V");
}

void TestFeedbackStaysOnOut()
{
  const auto stored = ReadVolts("slope/tests/feedback_p0_5.csv");
  slope::Circuit first(0.005, 0.005, slope::kFs);
  slope::SecondHalf second(0.005, 0.005, slope::kFs);
  bool rise = true;
  bool apart = false;
  for (int i = 0; i < 481; ++i)
  {
    double out = 0.0;
    double end = 0.0;
    first.Step(0.0, i == 0 ? 5.0 : 0.0, 0.0, 0.0, 0.0, true, out, end);
    const auto sample = second.Step(0.0, i == 0 ? 5.0 : 0.0, 0.0, 0.0, 0.5, true);
    if (i < static_cast<int>(stored.size()))
      rise = rise && sample.out == stored[static_cast<size_t>(i)];
    if (sample.out != out)
      apart = true;
    if (i < 240)
      rise = rise && out == slope::kSpan * static_cast<double>(i) / 240.0;
  }
  Check(rise, "a = 0 first half and a = +0.5 second half");
  Check(apart, "the halves differ when only the second is bent");
  slope::SecondHalf bent(0.005, 0.005, slope::kFs);
  slope::Sample at181{};
  for (int i = 0; i <= 181; ++i)
    at181 = bent.Step(0.0, i == 0 ? 5.0 : 0.0, 0.0, 0.0, 0.5, true);
  Check(at181.out == 5.0 && at181.ac == -2.5, "sample 181 AC is -2.5 V");
}

void TestIsolationAndGate()
{
  slope::SecondHalf moved(0.005, 0.005, slope::kFs);
  slope::SecondHalf idle(0.005, 0.005, slope::kFs);
  for (int i = 0; i < 200; ++i)
    moved.Step(0.0, i == 0 ? 5.0 : 0.0, 0.0, 0.0, 0.5, true);
  const auto still = idle.Step();
  Check(still.out == 0.0 && idle.Volts() == 0.0, "an unstepped half stays at 0 V");

  slope::SecondHalf gate(0.005, 0.005, slope::kFs);
  bool hold = true;
  double previous = 0.0;
  for (int i = 0; i < 962; ++i)
  {
    const auto sample = gate.Step(i < 960 ? 3.0 : 0.0, i == 400 ? 5.0 : 0.0);
    if (i == 144 || i == 400 || i == 960)
      hold = hold && sample.out == 3.0;
    if (i == 961)
      hold = hold && sample.out < 3.0;
    if (i > 0 && i < 960)
      hold = hold && sample.out >= previous;
    previous = sample.out;
  }
  Check(hold, "20 ms gate holds at 3 V");
}

void TestPluginSample()
{
  const auto stored = ReadVolts("slope/tests/ac_cycle.csv");
  slope::SecondHalf half(0.005, 0.005, slope::kFs);
  slope::SecondHalf other(0.005, 0.005, slope::kFs);
  bool match = stored.size() == 481;
  bool idle = true;
  for (int i = 0; match && i < 481; ++i)
  {
    const double y = slope::PluginSample(half, 0.005, 0.005, 0.0, i == 0);
    match = y == slope::ScaledAc(stored[static_cast<size_t>(i)]);
    idle = idle && other.Volts() == 0.0;
  }
  Check(match, "plugin silence cycle is AC / 2.5 of ac_cycle.csv");
  Check(idle, "the other channel stays at 0 V");

  const double held = slope::ScaledAc(slope::AcVolts(3.0));
  slope::SecondHalf gate(0.005, 0.005, slope::kFs);
  bool gateOk = true;
  double at960 = 0.0;
  double at961 = 0.0;
  for (int i = 0; i < 962; ++i)
  {
    const double host = i < 960 ? (3.0 / 5.0) : 0.0;
    const double y = slope::PluginSample(gate, 0.005, 0.005, host, false);
    if (i == 144 || i == 400 || i == 960)
      gateOk = gateOk && y == held;
    if (i == 960)
      at960 = y;
    if (i == 961)
      at961 = y;
  }
  Check(gateOk && at960 == held && at961 != held, "host 0.6 holds, then the release leaves 3 V");

  slope::SecondHalf armed(0.005, 0.005, slope::kFs);
  slope::SecondHalf unarmed(0.005, 0.005, slope::kFs);
  bool ignored = true;
  for (int i = 0; i < 200; ++i)
  {
    const double host = i < 100 ? (3.0 / 5.0) : 0.0;
    const double withArm = slope::PluginSample(armed, 0.005, 0.005, host, i == 0);
    const double without = slope::PluginSample(unarmed, 0.005, 0.005, host, false);
    ignored = ignored && withArm == without;
  }
  Check(ignored, "a positive input ignores the reset trigger");
}

// The call before Cycle: END is always patched to TRIG, and a non-finite host
// is not rewritten. Finite hosts must match PluginSample with cycle on.
double LegacyPluginSample(slope::SecondHalf& half, double rise, double fall, double host, bool arm)
{
  half.SetTimes(rise, fall);
  const auto sample = half.Step(5.0 * host, arm ? 5.0 : 0.0, 0.0, 0.0, 0.0, true);
  return slope::ScaledAc(sample.ac);
}

int MeasureCycle(double rise, double fall, double fs)
{
  slope::SecondHalf half(rise, fall, fs);
  const double y0 = slope::PluginSample(half, rise, fall, 0.0, true);
  for (int i = 1; i < 100000; ++i)
  {
    const double y = slope::PluginSample(half, rise, fall, 0.0, false);
    if (y == y0)
      return i;
  }
  return -1;
}

void CheckStr(const char* got, const char* want, const char* what)
{
  if (std::strcmp(got, want) == 0)
  {
    std::printf("ok    %s\n", what);
    return;
  }
  gFailures++;
  std::printf("FAIL  %s  got \"%s\" want \"%s\"\n", what, got, want);
}

void TestCycleSwitch()
{
  const double fs = 48000.0;
  const double hosts[] = {0.0, 0.6, -0.6, 1.0, -1.0, 0.19, 0.2};
  bool same = true;
  for (double host : hosts)
  {
    for (int arm = 0; arm < 2; ++arm)
    {
      slope::SecondHalf now(0.005, 0.005, fs);
      slope::SecondHalf old(0.005, 0.005, fs);
      for (int i = 0; i < 500; ++i)
      {
        const double yNow = slope::PluginSample(now, 0.005, 0.005, host, arm != 0, true);
        const double yOld = LegacyPluginSample(old, 0.005, 0.005, host, arm != 0);
        same = same && yNow == yOld;
      }
    }
  }
  Check(same, "cycle on matches the previous PluginSample on finite hosts");

  slope::SecondHalf quiet(0.005, 0.005, fs);
  bool idle = true;
  for (int i = 0; i < 48000; ++i)
    idle = idle && slope::PluginSample(quiet, 0.005, 0.005, 0.0, false, false) == 1.0;
  Check(idle, "cycle off: silence is +1 for 48,000 samples");

  const double held = slope::ScaledAc(slope::AcVolts(3.0));
  slope::SecondHalf released(0.005, 0.005, fs);
  slope::SecondHalf cycling(0.005, 0.005, fs);
  bool hold = true;
  int idleAt = -1;
  bool stayed = true;
  bool leftIdle = false;
  for (int i = 0; i < 4000; ++i)
  {
    const double host = i < 960 ? 0.6 : 0.0;
    const double yOff = slope::PluginSample(released, 0.005, 0.005, host, false, false);
    const double yOn = slope::PluginSample(cycling, 0.005, 0.005, host, false, true);
    if (i == 144 || i == 400 || i == 960)
      hold = hold && yOff == held && yOn == held;
    if (i == 961)
      hold = hold && yOff != held && yOn != held;
    if (i > 960 && idleAt < 0 && yOff == 1.0)
      idleAt = i;
    if (idleAt >= 0 && i >= idleAt)
      stayed = stayed && yOff == 1.0;
    if (idleAt >= 0 && i > idleAt && yOn != 1.0)
      leftIdle = true;
  }
  Check(hold, "cycle off: host 0.6 holds at 3 V, then the release leaves 3 V");
  Check(idleAt > 961 && stayed && released.Volts() == 0.0,
        "cycle off: after the fall the host stays at +1");
  Check(leftIdle, "cycle on: after the release the slope cycles");

  slope::SecondHalf parked(0.005, 0.005, fs);
  bool parkedOk = true;
  for (int i = 0; i < 32; ++i)
    parkedOk = parkedOk && slope::PluginSample(parked, 0.005, 0.005, 0.0, false, false) == 1.0;
  slope::SecondHalf fresh(0.005, 0.005, fs);
  const int period = slope::CyclePeriodSamples(0.005, 0.005, fs);
  bool flipped = true;
  double first = 0.0;
  double second = 0.0;
  for (int i = 0; i < period; ++i)
  {
    const bool arm = i == 0;
    const double y = slope::PluginSample(parked, 0.005, 0.005, 0.0, arm, true);
    const double yRef = slope::PluginSample(fresh, 0.005, 0.005, 0.0, arm, true);
    flipped = flipped && y == yRef;
    if (i == 0)
      first = y;
    if (i == 1)
      second = y;
  }
  Check(parkedOk && flipped && first == 1.0 && second != 1.0 && period == 480,
        "off-to-on at idle starts a 480-sample cycle on the next sample");

  const int at48 = slope::CyclePeriodSamples(0.0004, 0.0010, 48000.0);
  const int at44 = slope::CyclePeriodSamples(0.0004, 0.0010, 44100.0);
  const double hz48 = slope::CycleHz(0.0004, 0.0010, 48000.0);
  const double hz44 = slope::CycleHz(0.0004, 0.0010, 44100.0);
  Check(slope::CyclePeriodSamples(0.005, 0.005, fs) == 480, "5 ms cycle is 480 samples");
  Check(MeasureCycle(0.005, 0.005, fs) == 480, "measured 5 ms PluginSample period is 480");
  Check(at48 == 67 && MeasureCycle(0.0004, 0.0010, 48000.0) == 67,
        "0.4 ms / 1.0 ms at 48 kHz is 67 samples");
  Check(at44 == 62 && MeasureCycle(0.0004, 0.0010, 44100.0) == 62,
        "0.4 ms / 1.0 ms at 44.1 kHz is 62 samples");
  Check(hz48 == 48000.0 / 67.0 && hz44 == 44100.0 / 62.0, "cycle rate is fs / period");
  std::printf("cycle rates: 48 kHz %.6f Hz, 44.1 kHz %.6f Hz\n", hz48, hz44);

  const double nonFinite[] = {
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(),
  };
  bool guarded = true;
  for (double host : nonFinite)
  {
    for (int cycle = 0; cycle < 2; ++cycle)
    {
      slope::SecondHalf bad(0.005, 0.005, fs);
      slope::SecondHalf zero(0.005, 0.005, fs);
      const double yBad = slope::PluginSample(bad, 0.005, 0.005, host, false, cycle != 0);
      const double yZero = slope::PluginSample(zero, 0.005, 0.005, 0.0, false, cycle != 0);
      guarded = guarded && yBad == yZero;
    }
  }
  Check(guarded, "a non-finite host reads as 0");
}

void TestCoupler()
{
  const double fs = 48000.0;
  const double pole = slope::AcCouplerPole(fs);
  const double want = std::exp(-2.0 * std::acos(-1.0) * 10.0 / fs);
  Check(pole == want, "coupler pole at 48 kHz matches exp(-2 * pi * 10 / fs)");

  slope::AcCoupler law;
  law.SetSampleRate(fs);
  law.Reset();
  const double xs[5] = {0.0, 1.0, -0.5, 0.25, 2.5};
  double xPrev = 0.0;
  double yPrev = 0.0;
  bool steps = true;
  for (double x : xs)
  {
    const double y = law.Step(x);
    const double expect = x - xPrev + pole * yPrev;
    steps = steps && y == expect;
    xPrev = x;
    yPrev = y;
  }
  Check(steps, "coupler step matches the 10 Hz law for 5 samples");

  slope::AcCoupler idleC;
  idleC.SetSampleRate(fs);
  idleC.Reset();
  slope::SecondHalf idle(0.005, 0.005, fs);
  bool zero = true;
  for (int i = 0; i < 48000; ++i)
  {
    const double y = slope::HostSample(idleC, idle, 0.005, 0.005, 0.0, false, false);
    zero = zero && y == 0.0;
  }
  Check(zero, "cycle off: coupled host is 0 for 48,000 silent samples");

  slope::AcCoupler runC;
  runC.SetSampleRate(fs);
  runC.Reset();
  slope::SecondHalf run(0.005, 0.005, fs);
  const int period = 480;
  const int settle = 48000;
  std::vector<double> y(static_cast<size_t>(period));
  for (int i = 0; i < settle + period; ++i)
  {
    const double sample = slope::HostSample(runC, run, 0.005, 0.005, 0.0, i == 0, true);
    if (i >= settle)
      y[static_cast<size_t>(i - settle)] = sample;
  }
  double sum = 0.0;
  double lo = y[0];
  double hi = y[0];
  for (double sample : y)
  {
    sum += sample;
    if (sample < lo)
      lo = sample;
    if (sample > hi)
      hi = sample;
  }
  const double mean = sum / static_cast<double>(period);
  const double pp = hi - lo;
  std::printf("coupled cycle: mean %.6e peak-to-peak %.6f\n", mean, pp);
  Check(std::fabs(mean) < 1e-3, "coupled 5 ms cycle: mean over one period within 1e-3 of 0");
  Check(std::fabs(pp - 2.0) <= 0.04, "coupled 5 ms cycle: peak-to-peak within 2% of 2");
}

void TestVcRouteAndHold()
{
  const double fs = 48000.0;
  slope::SecondHalf muted(0.005, 0.005, fs);
  slope::SecondHalf plain(0.005, 0.005, fs);
  bool depth0 = true;
  for (int i = 0; i < 600; ++i)
  {
    muted.SetSwitch(slope::Switch::Rise);
    const auto scaled = muted.Step(0.0, i == 0 ? 5.0 : 0.0, slope::ScaleVc(5.0, 0.0), 0.0, 0.0, true);
    const auto bare = plain.Step(0.0, i == 0 ? 5.0 : 0.0, 0.0, 0.0, 0.0, true);
    depth0 = depth0 && scaled.out == bare.out && scaled.end == bare.end && scaled.ac == bare.ac;
  }
  Check(depth0, "VC depth 0 matches a VC of 0 V");
  Check(slope::ScaleVc(1.25, 1.0) == 1.25 && slope::ScaleVc(1.25, -1.0) == -1.25, "attenuverter +1 and -1");
  Check(slope::ScaleVc(2.0, 0.5) == 1.0 && slope::ScaleVc(5.0, 0.0) == 0.0, "attenuverter half and zero");
  Check(slope::ScaleVc(std::numeric_limits<double>::quiet_NaN(), 1.0) == 0.0, "non-finite VC passes nothing");

  auto periodOf = [fs](double vc, slope::Switch route) {
    slope::SecondHalf half(0.005, 0.005, fs);
    half.SetSwitch(route);
    int prev = -1;
    int gap = -1;
    for (int i = 0; i < 4000; ++i)
    {
      const auto sample = half.Step(0.0, i == 0 ? 5.0 : 0.0, vc, 0.0, 0.0, true);
      if (sample.out != 0.0)
        continue;
      if (prev >= 0 && gap < 0)
        gap = i - prev;
      prev = i;
    }
    return gap;
  };
  Check(periodOf(slope::ScaleVc(1.0, 1.0), slope::Switch::Both) == 384, "depth +1, BOTH, period 384");
  Check(periodOf(slope::ScaleVc(1.0, 1.0), slope::Switch::Rise) == 432, "depth +1, RISE, period 432");
  Check(periodOf(slope::ScaleVc(1.0, 1.0), slope::Switch::Fall) == 432, "depth +1, FALL, period 432");
  Check(slope::SwitchFromInt(0) == slope::Switch::Rise && slope::SwitchFromInt(1) == slope::Switch::Both
            && slope::SwitchFromInt(2) == slope::Switch::Fall && slope::SwitchFromInt(9) == slope::Switch::Both,
        "route integers");

  slope::SecondHalf held(0.005, 0.005, fs);
  bool riseExact = true;
  bool topped = true;
  bool released = false;
  for (int i = 0; i < 500; ++i)
  {
    const auto sample = held.Step(0.0, i < 300 ? 5.0 : 0.0);
    if (i < 240)
      riseExact = riseExact && sample.out == slope::kSpan * static_cast<double>(i) / 240.0;
    if (i >= 240 && i < 300)
      topped = topped && sample.out == 5.0;
    if (i == 301)
      released = sample.out < 5.0;
  }
  Check(riseExact && topped && released, "held TRIG stays at +5 V from sample 240 through 299");

  slope::SecondHalf early(0.005, 0.005, fs);
  bool noEarlyHold = true;
  for (int i = 0; i < 242; ++i)
  {
    const auto sample = early.Step(0.0, i < 200 ? 5.0 : 0.0);
    if (i <= 240)
      noEarlyHold = noEarlyHold && sample.out == slope::kSpan * static_cast<double>(i) / 240.0;
    if (i == 241)
      noEarlyHold = noEarlyHold && sample.out < 5.0;
  }
  Check(noEarlyHold, "a gate that ends before the top does not sustain");

  slope::SecondHalf pulsed(0.005, 0.005, fs);
  slope::SecondHalf quiet(0.005, 0.005, fs);
  bool pulseIgnored = true;
  for (int i = 0; i < 480; ++i)
  {
    const bool extra = i == 0 || i == 100 || i == 239 || i == 240 || i == 300;
    const auto noisy = pulsed.Step(0.0, extra ? 5.0 : 0.0, 0.0, 0.0, 0.0, true);
    const auto bare = quiet.Step(0.0, i == 0 ? 5.0 : 0.0, 0.0, 0.0, 0.0, true);
    pulseIgnored = pulseIgnored && noisy.out == bare.out && noisy.end == bare.end;
  }
  Check(pulseIgnored, "a pulse during the rise or the fall does not hold or retrigger");

  slope::SecondHalf fast(2.0 / fs, 0.005, fs);
  bool fastHold = true;
  for (int i = 0; i < 16; ++i)
  {
    const auto sample = fast.Step(0.0, i < 10 ? 5.0 : 0.0);
    if (i >= 2 && i < 10)
      fastHold = fastHold && sample.out == 5.0;
    if (i == 11)
      fastHold = fastHold && sample.out < 5.0;
  }
  Check(fastHold, "a 2-sample rise holds a 10-sample gate at +5 V");

  slope::SecondHalf tracked(0.005, 0.005, fs);
  bool follow = true;
  for (int i = 0; i < 420; ++i)
  {
    const auto sample = tracked.Step(i < 400 ? 3.0 : 0.0, 5.0);
    if (i == 144 || i == 400)
      follow = follow && sample.out == 3.0;
    if (i == 401)
      follow = follow && sample.out < 3.0;
    if (sample.out >= 5.0)
      follow = false;
  }
  Check(follow, "IN overrides a held TRIG");

  Check(slope::EndJackVolts(1.0, 0.0) == 5.0 && slope::EndJackVolts(1.0, 5.0) == 0.0
            && slope::EndJackVolts(0.0, 1.0) == 0.0,
        "END jack is +5 V at idle and 0 V at +5 V");

  slope::AcCoupler coupler;
  coupler.SetSampleRate(fs);
  coupler.Reset();
  slope::AcCoupler reference;
  reference.SetSampleRate(fs);
  reference.Reset();
  slope::SecondHalf withKnob(0.005, 0.005, fs);
  slope::SecondHalf without(0.005, 0.005, fs);
  bool pluginSame = true;
  for (int i = 0; i < 500; ++i)
  {
    const double y = slope::HostSample(coupler, withKnob, 0.005, 0.005, 0.0, i == 0, true, 3.0, 0.0, 0);
    const double y0 = slope::HostSample(reference, without, 0.005, 0.005, 0.0, i == 0, true);
    pluginSame = pluginSame && y == y0;
  }
  Check(pluginSame, "HostSample depth 0 matches the unscaled call");
}

void TestReadout()
{
  char buf[32];
  const double times[] = {0.0004, 0.001, 0.005, 0.0125, 0.25, 1.5, 10.0};
  const char* timeText[] = {"0.40 ms", "1.00 ms", "5.00 ms", "12.5 ms", "250 ms", "1.50 s", "10.0 s"};
  for (int i = 0; i < 7; ++i)
  {
    slope::FormatTime(times[i], buf, sizeof buf);
    CheckStr(buf, timeText[i], timeText[i]);
  }
  const double rates[] = {2.5, 71.4, 714.2, 1071.0};
  const char* rateText[] = {"2.50 Hz", "71.4 Hz", "714 Hz", "1.07 kHz"};
  for (int i = 0; i < 4; ++i)
  {
    slope::FormatHz(rates[i], buf, sizeof buf);
    CheckStr(buf, rateText[i], rateText[i]);
  }
}

} // namespace

int main()
{
  TestAcCycle();
  TestFeedbackStaysOnOut();
  TestIsolationAndGate();
  TestPluginSample();
  TestCycleSwitch();
  TestCoupler();
  TestVcRouteAndHold();
  TestReadout();
  if (gFailures != 0)
  {
    std::printf("%d failed\n", gFailures);
    return 1;
  }
  std::printf("all passed\n");
  return 0;
}
