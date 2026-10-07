// C++ port against slope/tests/ac_cycle.csv and the locked rise.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../SecondHalf.h"

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

} // namespace

int main()
{
  TestAcCycle();
  TestFeedbackStaysOnOut();
  TestIsolationAndGate();
  TestPluginSample();
  if (gFailures != 0)
  {
    std::printf("%d failed\n", gFailures);
    return 1;
  }
  std::printf("all passed\n");
  return 0;
}
