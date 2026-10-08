// Tests for the C++ port against the locked contract in the repository root.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// Build and run from the repository root:
//   cmake -S SergeMiddle/dsp/tests -B build/dsp-tests && cmake --build build/dsp-tests
//   ctest --test-dir build/dsp-tests --output-on-failure

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "../WaveMiddle.h"

#ifndef SERGE_REPO_ROOT
#error "SERGE_REPO_ROOT must point at the repository root"
#endif

namespace
{
int gFailures = 0;

void Check(bool ok, const char* what, double got = NAN, double want = NAN)
{
  if (ok)
  {
    std::printf("ok    %s\n", what);
    return;
  }
  gFailures++;
  std::printf("FAIL  %s  got %.17g want %.17g\n", what, got, want);
}

std::string RepoPath(const char* rel) { return std::string(SERGE_REPO_ROOT) + "/" + rel; }

// Rows of a CSV, skipping '#' comments and the first non-comment line if it is a header.
std::vector<std::vector<double>> ReadCsv(const std::string& path, bool header)
{
  std::ifstream in(path);
  if (!in)
  {
    std::printf("FAIL  cannot open %s\n", path.c_str());
    std::exit(1);
  }
  std::vector<std::vector<double>> rows;
  std::string line;
  bool skippedHeader = !header;
  while (std::getline(in, line))
  {
    if (line.empty() || line[0] == '#')
      continue;
    if (!skippedHeader)
    {
      skippedHeader = true;
      continue;
    }
    std::vector<double> row;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, ','))
      row.push_back(std::strtod(cell.c_str(), nullptr));
    rows.push_back(row);
  }
  return rows;
}

void TestTables()
{
  const auto half = ReadCsv(RepoPath("halfband_taps.csv"), false);
  const auto up = ReadCsv(RepoPath("upsample_taps.csv"), false);
  bool same = half.size() == serge::kFirTaps && up.size() == serge::kFirTaps;
  double sumHalf = 0.0, sumUp = 0.0;
  for (int i = 0; same && i < serge::kFirTaps; i++)
  {
    same = same && half[i][0] == serge::kHalfbandTaps[i] && up[i][0] == serge::kUpsampleTaps[i];
    sumHalf += serge::kHalfbandTaps[i];
    sumUp += serge::kUpsampleTaps[i];
  }
  Check(same, "embedded taps equal the CSV files bit for bit");
  Check(std::fabs(sumHalf - 1.0) < 1e-12, "decimator sum is 1", sumHalf, 1.0);
  Check(std::fabs(sumUp - 4.0) < 1e-12, "upsampler sum is 4", sumUp, 4.0);
}

void TestAcceptanceCurve()
{
  const auto rows = ReadCsv(RepoPath("tests/transfer_g1.csv"), true);
  Check(rows.size() == 6001, "acceptance curve has 6,001 samples", rows.size(), 6001);
  double worst = 0.0;
  bool odd = true;
  for (size_t i = 0; i < rows.size(); i++)
  {
    const double vin = rows[i][0];
    const double got = serge::Process(vin);
    worst = std::fmax(worst, std::fabs(got - rows[i][1]));
    odd = odd && serge::Process(-vin) + got == 0.0;
  }
  Check(worst <= 1e-12, "Process matches tests/transfer_g1.csv within 1e-12 V", worst, 0.0);
  Check(odd, "odd symmetry on the grid");
  Check(serge::Process(0.0) == 0.0, "process(0) = 0");
}

void TestLevels()
{
  const double peakY = serge::Process(serge::kFullScalePeakVin);
  Check(std::fabs(serge::kOutputGain * std::fabs(peakY) - 1.0) < 1e-12, "OUTPUT_GAIN * |y(4.707287 V)| = 1",
        serge::kOutputGain * std::fabs(peakY), 1.0);

  // Peak of |audio_map| over a in [-1, 1], at g = 1, a knot (2), and between knots.
  const double gs[] = {1.0, 2.0, 4.0, 8.0, 0.5, 3.3, 1.2};
  for (double g : gs)
  {
    double peak = 0.0;
    const int n = 200000;
    for (int i = 0; i <= n; i++)
      peak = std::fmax(peak, std::fabs(serge::AudioMap(-1.0 + 2.0 * i / n, g)));
    char what[96];
    std::snprintf(what, sizeof what, "audio_map peak is 1 at g = %.1f (rel 1e-3)", g);
    Check(std::fabs(peak - 1.0) <= 1e-3, what, peak, 1.0);
  }

  Check(std::fabs(serge::DrivePeak(2.0) - 4.104493885791202) == 0.0, "P(2) is the stored knot");
  Check(serge::DrivePeak(100.0) == serge::kDrivePeakP[serge::kDrivePeakCount - 1], "g above 8 clamps");
  Check(serge::DrivePeak(0.1) == serge::kDrivePeakP[0], "g below 0.5 clamps");
}

void TestSmoother()
{
  const double fs = 48000.0;
  Check(serge::SmoothCoeff(fs) == std::exp(-1.0 / (0.020 * fs)), "smoother coefficient formula");
  Check(std::fabs(serge::SmoothCoeff(fs) - 0.9989588756797245) < 1e-15, "c at 48 kHz", serge::SmoothCoeff(fs),
        0.9989588756797245);
  serge::FoldSmoother s;
  s.Reset(1.0, fs);
  Check(s.Value() == 1.0, "state starts at the target");
  double g = 0.0;
  for (int i = 0; i < 960; i++)
    g = s.Step(2.0);
  const double want = 1.0 + (1.0 - std::exp(-1.0));
  Check(std::fabs(g - want) < 1e-4, "step 1 to 2 after 960 samples", g, want);
  s.Reset(20.0, fs);
  Check(s.Value() == 8.0, "target is clamped to 8");
}

// Amplitude of a sinusoid at frequency f (cycles per sample) by projection.
double Amplitude(const std::vector<double>& x, size_t from, double f)
{
  double re = 0.0, im = 0.0;
  for (size_t i = from; i < x.size(); i++)
  {
    re += x[i] * std::cos(2.0 * serge::kPi * f * i);
    im += x[i] * std::sin(2.0 * serge::kPi * f * i);
  }
  const double n = static_cast<double>(x.size() - from);
  return 2.0 * std::hypot(re, im) / n;
}

void TestDecimator()
{
  // Generated at the 4x rate, decimator, keep phase 0. Cells absent.
  const int nLow = 2048;
  const double probes[] = {0.45 / 4.0, 0.75 / 4.0};
  for (int p = 0; p < 2; p++)
  {
    serge::Fir63 dec(serge::kHalfbandTaps);
    std::vector<double> low;
    for (int i = 0; i < nLow * 4; i++)
    {
      const double y = dec.Step(std::sin(2.0 * serge::kPi * probes[p] * i));
      if (i % 4 == 0)
        low.push_back(y);
    }
    // At the low rate the probe aliases to 4 * f, folded into [0, 0.5].
    double fLow = std::fmod(4.0 * probes[p], 1.0);
    if (fLow > 0.5)
      fLow = 1.0 - fLow;
    const double amp = Amplitude(low, 64, fLow);
    if (p == 0)
      Check(std::fabs(amp - 1.0) < 0.01, "decimator pass probe within 0.01 of unity", amp, 1.0);
    else
      Check(20.0 * std::log10(amp) <= -60.0, "decimator stop probe rejected by 60 dB", 20.0 * std::log10(amp), -69.19);
  }

  // Inside the plugin path with the cells bypassed: a 1 kHz sine round trip at
  // 48 kHz through upsampler and decimator. Divide out the bound to see the filters.
  const double fs = 48000.0;
  serge::WaveMiddleChannel ch;
  ch.Reset(fs);
  ch.SetCellsBypassed(true);
  const double scale = serge::kAudioFullScaleVolts * serge::OutputScale(1.0);
  std::vector<double> out;
  for (int i = 0; i < nLow; i++)
    out.push_back(ch.StepPreDc(std::sin(2.0 * serge::kPi * 1000.0 * i / fs), 1.0) / scale);
  const double amp = Amplitude(out, 64, 1000.0 / fs);
  Check(std::fabs(amp - 1.0) < 0.01, "plugin path, cells bypassed: 1 kHz round trip within 0.01", amp, 1.0);
}

void TestDcBlock()
{
  const double fs = 48000.0;
  Check(serge::DcBlockPole(fs) == std::exp(-2.0 * serge::kPi * 10.0 / fs), "DC block pole formula");
  const int n = 4096;
  std::vector<double> src(n);
  double peak = 0.0;
  for (int i = 0; i < n; i++)
  {
    src[i] = serge::AudioMap(std::sin(2.0 * serge::kPi * i / n), 1.0);
    peak = std::fmax(peak, std::fabs(src[i]));
  }
  serge::DcBlock dc;
  dc.Reset(fs);
  for (int i = 0; i < n; i++)
    dc.Step(src[i]);
  double outPeak = 0.0, dev = 0.0;
  for (int i = 0; i < n; i++)
  {
    const double y = dc.Step(src[i]);
    outPeak = std::fmax(outPeak, std::fabs(y));
    dev = std::fmax(dev, std::fabs(y - src[i]));
  }
  Check(std::fabs(outPeak / peak - 1.152747) < 5e-7, "11.71875 Hz folded buffer peak ratio 1.152747", outPeak / peak,
        1.152747);
  Check(std::fabs(dev / peak - 0.390095) < 5e-7, "11.71875 Hz deviation 0.390095", dev / peak, 0.390095);
}

void TestSinePeaks()
{
  // Full-scale sine, before the DC block. 20 Hz keeps the fold harmonics
  // inside the decimator passband, so the peak is the level law.
  const double fs = 48000.0;
  const int n = 48000;
  const int discard = 8000;
  const double freq = 20.0;
  const double gs[] = {1.0, 2.0};
  for (double g : gs)
  {
    serge::WaveMiddleChannel ch;
    ch.Reset(fs);
    double peak = 0.0;
    for (int i = 0; i < n; i++)
    {
      const double a = std::sin(2.0 * serge::kPi * freq * static_cast<double>(i) / fs);
      const double y = ch.StepPreDc(a, g);
      if (i >= discard)
        peak = std::fmax(peak, std::fabs(y));
    }
    char what[96];
    std::snprintf(what, sizeof what, "20 Hz full-scale sine peaks at 1 before the DC block, g = %.0f", g);
    Check(std::fabs(peak - 1.0) < 1e-3, what, peak, 1.0);
  }
}

void TestChain()
{
  const auto rows = ReadCsv(RepoPath("SergeMiddle/dsp/tests/chain_reference.csv"), true);
  const double fs = 48000.0;
  serge::FoldSmoother fold;
  fold.Reset(rows[0][2], fs);
  serge::WaveMiddleChannel ch;
  ch.Reset(fs);
  double worstG = 0.0, worstOut = 0.0;
  for (const auto& r : rows)
  {
    const double g = fold.Step(r[2]);
    const double y = ch.Step(r[1], g);
    worstG = std::fmax(worstG, std::fabs(g - r[3]));
    worstOut = std::fmax(worstOut, std::fabs(y - r[5]));
  }
  Check(rows.size() == 1024, "chain fixture has 1,024 samples", rows.size(), 1024);
  Check(worstG <= 1e-15, "chain: smoothed g matches Python", worstG, 0.0);
  Check(worstOut <= 1e-9, "chain: full path matches Python within 1e-9", worstOut, 0.0);
}

bool SameBits(double a, double b)
{
  return std::memcmp(&a, &b, sizeof(double)) == 0;
}

// Pre-change channel: upsampler, six cells, decimator, OUTPUT_GAIN, DC block.
// No symmetry offset. The bit-identity check compares against this copy.
class LegacyChannel
{
public:
  LegacyChannel() : mUp(serge::kUpsampleTaps), mDown(serge::kHalfbandTaps) {}

  void Reset(double sampleRate)
  {
    mUp.Reset();
    mDown.Reset();
    mDc.Reset(sampleRate);
  }

  double StepPreDc(double a, double g)
  {
    if (!std::isfinite(a))
      a = 0.0;
    const double v = serge::kAudioFullScaleVolts * serge::DriveScale(g) * a;
    double decimated = 0.0;
    for (int phase = 0; phase < serge::kOversample; phase++)
    {
      const double hi = mUp.Step(phase == 0 ? v : 0.0);
      const double cells = mCellsBypassed ? hi : serge::Process(hi);
      if (phase == 0)
        decimated = mDown.Step(cells);
      else
        mDown.Push(cells);
    }
    return serge::OutputScale(g) * decimated;
  }

  double Step(double a, double g) { return mDc.Step(StepPreDc(a, g)); }

private:
  serge::Fir63 mUp;
  serge::Fir63 mDown;
  serge::DcBlock mDc;
  bool mCellsBypassed = false;
};

// Single-bin DFT magnitude. bin is a cycle count over the window.
double BinMagnitude(const std::vector<double>& x, int bin)
{
  const int n = static_cast<int>(x.size());
  double re = 0.0;
  double im = 0.0;
  for (int i = 0; i < n; i++)
  {
    const double angle = 2.0 * serge::kPi * static_cast<double>(bin) * static_cast<double>(i) / static_cast<double>(n);
    re += x[static_cast<size_t>(i)] * std::cos(angle);
    im -= x[static_cast<size_t>(i)] * std::sin(angle);
  }
  return std::hypot(re, im);
}

struct EvenBins
{
  double h1;
  double h2;
  double h4;
};

// 4,096 samples at 48 kHz, 86 cycles, amplitude 0.5, g = 1. One lead-in window, then the measurement.
// The turn count is reduced before the 2 pi scale. Sample i and sample i + n then share one angle,
// and even bins of the odd map sit at roundoff. An unreduced angle is not an exact bin at 1e-9.
EvenBins MeasureEvenBins(double offset)
{
  constexpr int n = 4096;
  constexpr double fs = 48000.0;
  serge::WaveMiddleChannel channel;
  channel.Reset(fs);
  std::vector<double> y(static_cast<size_t>(n));
  for (int i = 0; i < 2 * n; i++)
  {
    const double turns = static_cast<double>((86 * i) % n) / static_cast<double>(n);
    const double a = 0.5 * std::sin(2.0 * serge::kPi * turns);
    const double sample = channel.StepPreDc(a, 1.0, offset);
    if (i >= n)
      y[static_cast<size_t>(i - n)] = sample;
  }
  EvenBins out;
  out.h1 = BinMagnitude(y, 86);
  out.h2 = BinMagnitude(y, 172);
  out.h4 = BinMagnitude(y, 344);
  return out;
}

void TestSymmetry()
{
  const double fs = 48000.0;
  constexpr int n = 8192;

  Check(serge::Cell(-0.0) == 0.0 && serge::Cell(0.0) == 0.0, "Cell of both zeros is 0");
  Check(serge::AudioMap(0.25, 2.0) == serge::AudioMap(0.25, 2.0, 0.0), "AudioMap default offset matches 0");
  Check(serge::AudioMap(0.0, 1.0) == serge::AudioMap(0.0, 1.0, 0.0), "AudioMap at 0 matches offset 0");
  Check(serge::AudioMap(-0.0, 1.0) == serge::AudioMap(-0.0, 1.0, 0.0), "AudioMap at -0 matches offset 0");
  Check(serge::AudioMap(0.0, 1.0, 1.0) == 0.0, "AudioMap at a = 0 is 0 at any offset");
  Check(serge::AudioMap(0.1, 1.0, 1.0) == serge::OutputScale(1.0) * (serge::Process(1.5) - serge::Process(1.0)),
        "AudioMap adds the offset in cell-input volts and subtracts C^6(s)");
  Check(serge::SymmetryBias(0.0) == 0.0 && !std::signbit(serge::SymmetryBias(0.0)), "SymmetryBias(0) is +0");

  LegacyChannel legacy;
  serge::WaveMiddleChannel withSmoother;
  serge::WaveMiddleChannel twoArg;
  serge::WaveMiddleChannel threeArg;
  legacy.Reset(fs);
  withSmoother.Reset(fs);
  twoArg.Reset(fs);
  threeArg.Reset(fs);

  serge::FoldSmoother fold;
  fold.Reset(serge::kGDefault, fs);
  serge::SymmetrySmoother symmetry;
  symmetry.Reset(0.0, fs);

  bool matchLegacy = true;
  bool matchArgs = true;
  bool smootherStaysZero = true;
  int legacyBad = -1;
  int argsBad = -1;
  for (int i = 0; i < n; i++)
  {
    double a = std::sin(2.0 * serge::kPi * 440.0 * static_cast<double>(i) / fs);
    if (i == 17)
      a = 0.0;
    else if (i == 18)
      a = -0.0;
    else if (i == 4000)
      a = 0.0;
    else if (i == 4001)
      a = -0.0;

    const double gTarget = serge::kGMin + (serge::kGMax - serge::kGMin) * (static_cast<double>(i) / static_cast<double>(n - 1));
    const double g = fold.Step(gTarget);
    const double offset = symmetry.Step(0.0);
    const double zero = 0.0;
    if (!SameBits(offset, zero))
      smootherStaysZero = false;

    const double yLegacy = legacy.Step(a, g);
    const double ySmoothed = withSmoother.Step(a, g, offset);
    if (!SameBits(yLegacy, ySmoothed))
    {
      if (legacyBad < 0)
      {
        legacyBad = i;
        std::printf("FAIL  legacy mismatch at %d  new %.17g legacy %.17g equal %d\n", i, ySmoothed, yLegacy,
                    ySmoothed == yLegacy);
      }
      matchLegacy = false;
    }

    const double y2 = twoArg.Step(a, g);
    const double y3 = threeArg.Step(a, g, 0.0);
    if (!SameBits(y2, y3))
    {
      if (argsBad < 0)
      {
        argsBad = i;
        std::printf("FAIL  Step arity mismatch at %d  two %.17g three %.17g\n", i, y2, y3);
      }
      matchArgs = false;
    }
  }
  Check(smootherStaysZero, "SymmetrySmoother at target 0 stays +0");
  Check(matchLegacy, "offset 0 matches the pre-change path on every sample", legacyBad, -1);
  Check(matchArgs, "Step(a, g) equals Step(a, g, 0.0) on every sample", argsBad, -1);

  const EvenBins at0 = MeasureEvenBins(0.0);
  const EvenBins atPos = MeasureEvenBins(1.0);
  const EvenBins atNeg = MeasureEvenBins(-1.0);
  const double h2OverH1At0 = at0.h2 / at0.h1;
  const double h4OverH1At0 = at0.h4 / at0.h1;
  const double h2OverH1Pos = atPos.h2 / atPos.h1;
  const double h2OverH1Neg = atNeg.h2 / atNeg.h1;
  std::printf("symmetry ratios: s=0 |H2|/|H1|=%.16g |H4|/|H1|=%.16g; s=+1 |H2|/|H1|=%.16g; s=-1 |H2|/|H1|=%.16g\n",
              h2OverH1At0, h4OverH1At0, h2OverH1Pos, h2OverH1Neg);

  Check(at0.h1 > 0.0 && atPos.h1 > 0.0 && atNeg.h1 > 0.0, "even-harmonic probe has a fundamental");
  Check(h2OverH1At0 < 1e-9, "offset 0 |H2|/|H1| below 1e-9", h2OverH1At0, 0.0);
  Check(h4OverH1At0 < 1e-9, "offset 0 |H4|/|H1| below 1e-9", h4OverH1At0, 0.0);
  // Measured |H2|/|H1| at s = +1 V is 0.714834 (GOLDEN.md). The floor is below that, with margin.
  constexpr double kH2OverH1Floor = 0.5;
  Check(h2OverH1Pos > kH2OverH1Floor, "offset +1 V |H2|/|H1| above the floor", h2OverH1Pos, kH2OverH1Floor);
  const double mirror = std::fabs(h2OverH1Pos - h2OverH1Neg) / std::fabs(h2OverH1Pos);
  std::printf("symmetry |H2| mirror relative %.3e\n", mirror);
  Check(mirror <= 1e-9, "+1 V and -1 V |H2|/|H1| agree within 1e-9 relative", mirror, 0.0);

  serge::SymmetrySmoother smoother;
  smoother.Reset(1.25, fs);
  Check(smoother.Value() == 1.25, "symmetry smoother starts at the clamped target", smoother.Value(), 1.25);
  smoother.Reset(0.0, fs);
  double stepped = 0.0;
  for (int i = 0; i < 960; i++)
    stepped = smoother.Step(1.0);
  const double want = 1.0 - std::exp(-1.0);
  Check(std::fabs(stepped - want) < 1e-4, "symmetry step 0 to +1 V after 960 samples", stepped, want);

  smoother.Reset(10.0, fs);
  Check(smoother.Value() == 2.5, "symmetry target +10 V clamps to +2.5", smoother.Value(), 2.5);
  smoother.Reset(-10.0, fs);
  Check(smoother.Value() == -2.5, "symmetry target -10 V clamps to -2.5", smoother.Value(), -2.5);
  smoother.Reset(0.0, fs);
  const double towardClamp = smoother.Step(10.0);
  serge::SymmetrySmoother reference;
  reference.Reset(0.0, fs);
  Check(towardClamp == reference.Step(2.5), "Step(+10 V) uses the clamped target +2.5", towardClamp, 2.5 * (1.0 - serge::SmoothCoeff(fs)));

  // A silent input stays silent while s moves, and after a reset at a nonzero s.
  double silentTurn = 0.0;
  double silentReset = 0.0;
  for (double target : {0.5, 1.0, 2.5, -2.5})
  {
    serge::WaveMiddleChannel turn;
    turn.Reset(fs);
    serge::SymmetrySmoother turnS;
    turnS.Reset(0.0, fs);
    serge::WaveMiddleChannel held;
    held.Reset(fs);
    serge::SymmetrySmoother heldS;
    heldS.Reset(target, fs);
    for (int i = 0; i < 9600; i++)
    {
      silentTurn = std::fmax(silentTurn, std::fabs(turn.Step(0.0, 1.0, turnS.Step(target))));
      silentReset = std::fmax(silentReset, std::fabs(held.Step(0.0, 1.0, heldS.Step(target))));
    }
  }
  Check(silentTurn == 0.0, "silent input: turning Symmetry gives exactly 0", silentTurn, 0.0);
  Check(silentReset == 0.0, "silent input: reset at a nonzero Symmetry gives exactly 0", silentReset, 0.0);

  smoother.Reset(std::numeric_limits<double>::quiet_NaN(), fs);
  Check(smoother.Value() == 0.0, "symmetry NaN target reads as 0", smoother.Value(), 0.0);
  smoother.Reset(1.0, fs);
  const double nanStep = smoother.Step(std::numeric_limits<double>::quiet_NaN());
  reference.Reset(1.0, fs);
  Check(nanStep == reference.Step(0.0), "Step(NaN) uses target 0", nanStep, 0.0);
}
} // namespace

int main()
{
  TestTables();
  TestAcceptanceCurve();
  TestLevels();
  TestSmoother();
  TestDecimator();
  TestDcBlock();
  TestSinePeaks();
  TestChain();
  TestSymmetry();
  std::printf("%s: %d failure(s)\n", gFailures ? "FAILED" : "PASSED", gFailures);
  return gFailures ? 1 : 0;
}
