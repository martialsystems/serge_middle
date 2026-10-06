// Tests for the C++ port against the locked contract in the repository root.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// Build and run from the repository root:
//   cmake -S SergeMiddle/dsp/tests -B build/dsp-tests && cmake --build build/dsp-tests
//   ctest --test-dir build/dsp-tests --output-on-failure

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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
} // namespace

int main()
{
  TestTables();
  TestAcceptanceCurve();
  TestLevels();
  TestSmoother();
  TestDecimator();
  TestDcBlock();
  TestChain();
  std::printf("%s: %d failure(s)\n", gFailures ? "FAILED" : "PASSED", gFailures);
  return gFailures ? 1 : 0;
}
