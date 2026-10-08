#pragma once

// Time and rate strings for the Slope AC panel.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// Standard library only. The audio thread does not call these.

#include <cstddef>
#include <cstdio>

namespace slope
{

// Under 10 ms: two decimals of ms. Under 100 ms: one decimal. Under 1 s: whole
// milliseconds. Under 10 s: two decimals of seconds. From 10 s: one decimal.
inline void FormatTime(double seconds, char* buf, std::size_t n)
{
  if (buf == nullptr || n == 0)
    return;
  const double ms = seconds * 1000.0;
  if (seconds < 0.01)
    std::snprintf(buf, n, "%.2f ms", ms);
  else if (seconds < 0.1)
    std::snprintf(buf, n, "%.1f ms", ms);
  else if (seconds < 1.0)
    std::snprintf(buf, n, "%.0f ms", ms);
  else if (seconds < 10.0)
    std::snprintf(buf, n, "%.2f s", seconds);
  else
    std::snprintf(buf, n, "%.1f s", seconds);
}

// Under 10 Hz: two decimals. Under 100 Hz: one decimal. Under 1 kHz: whole
// hertz. From 1 kHz: two decimals of kHz.
inline void FormatHz(double hz, char* buf, std::size_t n)
{
  if (buf == nullptr || n == 0)
    return;
  if (hz < 10.0)
    std::snprintf(buf, n, "%.2f Hz", hz);
  else if (hz < 100.0)
    std::snprintf(buf, n, "%.1f Hz", hz);
  else if (hz < 1000.0)
    std::snprintf(buf, n, "%.0f Hz", hz);
  else
    std::snprintf(buf, n, "%.2f kHz", hz / 1000.0);
}

} // namespace slope
