// Top, bottom, and the second slope.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "../Patch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

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

double Sine(int n, double fs, double hz, double amp)
{
  return amp * std::sin(2.0 * std::acos(-1.0) * hz * static_cast<double>(n) / fs);
}

void Harmonic(const std::vector<double>& y, int i0, double hz, double fs, double& mag)
{
  const double w = 2.0 * std::acos(-1.0) * hz / fs;
  double re = 0.0;
  double im = 0.0;
  int count = 0;
  for (int i = i0; i < static_cast<int>(y.size()); ++i)
  {
    re += y[static_cast<size_t>(i)] * std::cos(w * static_cast<double>(i));
    im += y[static_cast<size_t>(i)] * std::sin(w * static_cast<double>(i));
    ++count;
  }
  mag = count > 0 ? std::hypot(re / static_cast<double>(count), im / static_cast<double>(count)) : 0.0;
}

void TestTop()
{
  constexpr double kFs = 48000.0;
  constexpr int kN = 4800;
  vcm::TopSection silentLo;
  vcm::TopSection silentHi;
  vcm::TopSection full;
  vcm::TopSection half;
  vcm::TopSection fromVc;
  vcm::TopSection fromKnob;
  vcm::TopSection pos;
  vcm::TopSection neg;
  vcm::TopSection posHi;
  vcm::TopSection negHi;
  vcm::TopSection hot;
  silentLo.Reset(kFs);
  silentHi.Reset(kFs);
  full.Reset(kFs);
  half.Reset(kFs);
  fromVc.Reset(kFs);
  fromKnob.Reset(kFs);
  pos.Reset(kFs);
  neg.Reset(kFs);
  posHi.Reset(kFs);
  negHi.Reset(kFs);
  hot.Reset(kFs);

  bool zero = true;
  bool halfMatch = true;
  bool vcMatch = true;
  bool oddLo = true;
  bool oddHi = true;
  bool sawPos = false;
  bool sawNeg = false;
  bool inside = true;
  const double gVc = vcm::ControlSum(0.0, 1.0, 5.0);
  const double gKnob = vcm::ControlSum(1.0, 1.0, 0.0);
  for (int n = 0; n < kN; ++n)
  {
    const double x = Sine(n, kFs, 440.0, 2.5);
    if (silentLo.Step(x, 0.0, false) != 0.0 || silentHi.Step(x, 0.0, true) != 0.0)
      zero = false;
    const double unity = full.Step(x, 1.0, false);
    if (half.Step(x, 0.5, false) != 0.5 * unity)
      halfMatch = false;
    if (fromVc.Step(x, gVc, false) != fromKnob.Step(x, gKnob, false))
      vcMatch = false;
    const double yp = pos.Step(x, 1.0, false);
    const double yn = neg.Step(-x, 1.0, false);
    if (yn != -yp)
      oddLo = false;
    const double hp = posHi.Step(x, 1.0, true);
    const double hn = negHi.Step(-x, 1.0, true);
    if (hn != -hp)
      oddHi = false;
    const double clipped = hot.Step(Sine(n, kFs, 100.0, 20.0), 1.0, false);
    if (!std::isfinite(clipped) || clipped > 5.0 || clipped < -5.0)
      inside = false;
    if (clipped == 5.0)
      sawPos = true;
    if (clipped == -5.0)
      sawNeg = true;
  }
  Check(zero, "top gain 0 is exact 0 in LO and HI");
  Check(halfMatch, "top LO at half gain is half the unity sample");
  Check(vcMatch && gVc == 1.0 && gKnob == 1.0, "top VC of +5 V at gain 0 matches unity");
  Check(oddLo && oddHi, "top LO and HI are odd, sample for sample");
  Check(inside && sawPos && sawNeg, "a 20 V sine on top LO clamps to exactly ±5 V");
  Check(vcm::ControlSum(0.4, 0.0, 5.0) == 0.4, "depth 0 leaves the knob with no add");
}

void TestBottom()
{
  constexpr double kFs = 48000.0;
  constexpr int kN = static_cast<int>(kFs);
  vcm::BottomSection quiet;
  vcm::BottomSection openIn;
  vcm::BottomSection positive;
  vcm::BottomSection flipped;
  vcm::BottomSection loud;
  vcm::BottomSection soft;
  vcm::BottomSection depth0;
  vcm::BottomSection depth1;
  vcm::BottomSection knob;
  quiet.Reset(kFs);
  openIn.Reset(kFs);
  positive.Reset(kFs);
  flipped.Reset(kFs);
  loud.Reset(kFs);
  soft.Reset(kFs);
  depth0.Reset(kFs);
  depth1.Reset(kFs);
  knob.Reset(kFs);

  std::vector<double> out1(static_cast<size_t>(kN));
  bool zero = true;
  bool fullWave = true;
  bool differ = false;
  bool depthSilent = true;
  bool depthMatches = true;
  double worstLevel = 0.0;
  const int settle = static_cast<int>(0.6 * kFs);
  const double gDepth = vcm::ControlSum(0.0, 1.0, 5.0);
  const double gKnob = vcm::ControlSum(1.0, 1.0, 0.0);
  for (int n = 0; n < kN; ++n)
  {
    const double x = Sine(n, kFs, 100.0, 1.5);
    double a = 1.0;
    double b = 1.0;
    quiet.Step(x, 0.0, a, b);
    if (a != 0.0 || b != 0.0)
      zero = false;
    openIn.Step(0.0, 1.0, a, b);
    if (a != 0.0 || b != 0.0)
      zero = false;

    double p1 = 0.0;
    double p2 = 0.0;
    double n1 = 0.0;
    double n2 = 0.0;
    positive.Step(x, 1.0, p1, p2);
    flipped.Step(-x, 1.0, n1, n2);
    if (p1 != n1 || p2 != n2)
      fullWave = false;
    if (p1 != p2)
      differ = true;
    out1[static_cast<size_t>(n)] = p1;

    double hi1 = 0.0;
    double hi2 = 0.0;
    double lo1 = 0.0;
    double lo2 = 0.0;
    loud.Step(Sine(n, kFs, 100.0, 2.0), 1.0, hi1, hi2);
    soft.Step(Sine(n, kFs, 100.0, 0.4), 1.0, lo1, lo2);
    if (n >= settle)
    {
      worstLevel = std::max(worstLevel, std::fabs(hi1 - lo1));
      worstLevel = std::max(worstLevel, std::fabs(hi2 - lo2));
    }

    double z1 = 0.0;
    double z2 = 0.0;
    double d1 = 0.0;
    double d2 = 0.0;
    double k1 = 0.0;
    double k2 = 0.0;
    depth0.Step(x, vcm::ControlSum(0.0, 0.0, 5.0), z1, z2);
    depth1.Step(x, gDepth, d1, d2);
    knob.Step(x, gKnob, k1, k2);
    if (z1 != 0.0 || z2 != 0.0)
      depthSilent = false;
    if (d1 != k1 || d2 != k2)
      depthMatches = false;
  }
  Check(zero, "bottom gain 0 or input 0 is exact 0");
  Check(fullWave, "bottom +sine and -sine match");
  Check(differ, "bottom OUT 2 is not OUT 1");
  double h1 = 0.0;
  double h2 = 0.0;
  Harmonic(out1, settle, 100.0, kFs, h1);
  Harmonic(out1, settle, 200.0, kFs, h2);
  Check(h2 > 0.05 && h2 > 5.0 * h1, "bottom OUT 1 second harmonic beats the fundamental");
  Check(worstLevel < 1e-3, "bottom level matches across two amplitudes after the envelope settles");
  Check(depthSilent, "bottom VC depth 0 at level 0 stays silent");
  Check(depthMatches && gDepth == 1.0 && gKnob == 1.0, "bottom VC +5 V at depth 1 matches level 1");
  std::printf("measure bottom h1 %.6g h2 %.6g level_worst %.3g\n", h1, h2, worstLevel);
}

void TestSlope2Matches()
{
  constexpr double kFs = 48000.0;
  constexpr int kN = 4000;
  patch::PatchChannel first;
  patch::PatchChannel second;
  first.Reset(kFs);
  second.Reset(kFs);
  patch::Routing routing;
  bool match = true;
  int miss = -1;
  for (int n = 0; n < kN; ++n)
  {
    const double gate = (n % 900) < 200 ? patch::kGateVolts : 0.0;
    const bool cycle = n >= 2000;
    const bool arm = n == 2000;
    const int route = n < 3000 ? 1 : (n < 3500 ? 0 : 2);
    patch::Inject inject;
    inject.slope2Trig = gate;
    patch::SectionControls extra;
    extra.rise2 = 0.004;
    extra.fall2 = 0.006;
    extra.cycle2 = cycle;
    extra.arm2 = arm;
    extra.slope2Route = route;
    extra.inject = &inject;
    first.Step(0.0, gate, 1.0, 0.0, 0.0, 0.004, 0.006, cycle, arm, routing, 0.0, route);
    second.Step(0.2, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, false, false, routing, 0.0, 1, extra);
    if (first.OutputVolts(patch::SlopeOut) != second.OutputVolts(patch::Slope2Out) ||
        first.OutputVolts(patch::SlopeEnd) != second.OutputVolts(patch::Slope2End) ||
        first.OutputVolts(patch::SlopeAc) != second.OutputVolts(patch::Slope2Ac))
    {
      match = false;
      if (miss < 0)
        miss = n;
    }
  }
  Check(match, "slope 2 matches slope 1 for the same gate, times, cycle, and route");
  if (!match)
    std::printf("  slope2 miss at %d\n", miss);

  patch::PatchChannel fed;
  patch::PatchChannel copy;
  fed.Reset(kFs);
  copy.Reset(kFs);
  patch::Routing ra;
  patch::Routing rb;
  ra.Connect(patch::SlopeEnd, patch::SlopeVc);
  rb.Connect(patch::Slope2End, patch::Slope2Vc);
  bool vcMatch = true;
  for (int n = 0; n < 3000; ++n)
  {
    patch::SectionControls extra;
    extra.cycle2 = true;
    extra.arm2 = n == 0;
    extra.slope2Depth = 1.0;
    fed.Step(0.0, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, true, n == 0, ra, 1.0, 1);
    copy.Step(0.0, 0.0, 1.0, 0.0, 0.0, 0.005, 0.005, false, false, rb, 0.0, 1, extra);
    if (fed.OutputVolts(patch::SlopeOut) != copy.OutputVolts(patch::Slope2Out) ||
        fed.OutputVolts(patch::SlopeEnd) != copy.OutputVolts(patch::Slope2End) ||
        fed.OutputVolts(patch::SlopeAc) != copy.OutputVolts(patch::Slope2Ac))
      vcMatch = false;
  }
  Check(vcMatch, "slope 2 VC feedback matches slope 1");
}

void TestHostUntouched()
{
  constexpr double kFs = 48000.0;
  constexpr int kN = 2000;
  patch::PatchChannel plain;
  patch::PatchChannel busy;
  plain.Reset(kFs);
  busy.Reset(kFs);
  patch::Routing routing;
  bool same = true;
  bool topSilent = true;
  bool bottomSilent = true;
  for (int n = 0; n < kN; ++n)
  {
    const double host = Sine(n, kFs, 220.0, 0.8);
    const double a = plain.Step(host, 0.0, 1.2, 0.0, 0.5, 0.005, 0.005, false, false, routing);
    patch::Inject inject;
    inject.topIn = 3.0;
    inject.bottomIn = -2.0;
    inject.slope2Trig = patch::kGateVolts;
    patch::SectionControls extra;
    extra.topGain = 1.0;
    extra.topHi = true;
    extra.bottomGain = 1.0;
    extra.cycle2 = true;
    extra.arm2 = n == 0;
    extra.inject = &inject;
    const double b = busy.Step(host, 0.0, 1.2, 0.0, 0.5, 0.005, 0.005, false, false, routing, 0.0, 1, extra);
    if (a != b || plain.OutputVolts(patch::PleatOut) != busy.OutputVolts(patch::PleatOut) ||
        plain.OutputVolts(patch::SlopeOut) != busy.OutputVolts(patch::SlopeOut))
      same = false;
  }
  patch::PatchChannel idle;
  idle.Reset(kFs);
  for (int n = 0; n < 500; ++n)
  {
    idle.Step(Sine(n, kFs, 100.0, 1.0), patch::kGateVolts, 1.0, 0.0, 0.0, 0.005, 0.005, true, n == 0, routing);
    if (idle.OutputVolts(patch::TopOut) != 0.0 || idle.InputVolts(patch::TopIn) != 0.0)
      topSilent = false;
    if (idle.OutputVolts(patch::BottomOut1) != 0.0 || idle.OutputVolts(patch::BottomOut2) != 0.0 ||
        idle.InputVolts(patch::BottomIn) != 0.0)
      bottomSilent = false;
  }
  Check(same, "top, bottom, and slope 2 do not change the PLEAT or slope 1 sample");
  Check(topSilent && bottomSilent, "unpatched top and bottom stay at 0 V while the host runs");
}
} // namespace

int main()
{
  TestTop();
  TestBottom();
  TestSlope2Matches();
  TestHostUntouched();
  if (gFailures != 0)
  {
    std::printf("%d failed\n", gFailures);
    return 1;
  }
  std::printf("all passed\n");
  return 0;
}
