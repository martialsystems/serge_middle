#pragma once

// Which jacks a patch cable may end on. Message thread and tests. No iPlug2.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// Centres are tools/art/ids_v6.json. A span is emitted only when both ends are
// in this table. An empty Routing is all -1 and emits nothing. Source 0 is
// PLEAT OUT, a real cable, so it is not treated as unpatched. Stacking is
// Lift's 6 px on the output, in design pixels, for each earlier input that
// already takes that output.

#include "dsp/Patch.h"

namespace slewfold
{

struct JackSpot
{
  const char* id;
  bool output;
  int index;
  float x;
  float y;
  const char* label;
  int cloth;
};

inline constexpr float kStackStep = 6.f;
inline constexpr int kJackCount = 23;
inline constexpr JackSpot kJacks[kJackCount] = {
    {"jack_top_in", false, patch::TopIn, 80.f, 220.f, "TOP IN", 0},
    {"jack_top_vc", false, patch::TopVc, 80.f, 390.f, "TOP VC", 0},
    {"jack_top_out", true, patch::TopOut, 480.f, 305.f, "TOP OUT", 4},
    {"jack_pleat_in1", false, patch::PleatIn1, 600.f, 190.f, "PLEAT IN 1", 0},
    {"jack_pleat_in2", false, patch::PleatIn2, 600.f, 305.f, "PLEAT IN 2", 0},
    {"jack_pleat_vc", false, patch::PleatVc, 600.f, 420.f, "PLEAT VC", 0},
    {"jack_pleat_out", true, patch::PleatOut, 1000.f, 305.f, "PLEAT OUT", 4},
    {"jack_bottom_in", false, patch::BottomIn, 1120.f, 220.f, "BOTTOM IN", 0},
    {"jack_bottom_vc", false, patch::BottomVc, 1120.f, 390.f, "BOTTOM VC", 0},
    {"jack_bottom_out1", true, patch::BottomOut1, 1520.f, 220.f, "BOTTOM OUT 1", 4},
    {"jack_bottom_out2", true, patch::BottomOut2, 1520.f, 390.f, "BOTTOM OUT 2", 4},
    {"jack_slope1_in", false, patch::SlopeIn, 90.f, 625.f, "SLOPE IN", 0},
    {"jack_slope1_trig", false, patch::SlopeTrig, 90.f, 725.f, "SLOPE TRIG", 0},
    {"jack_slope1_vc", false, patch::SlopeVc, 90.f, 825.f, "SLOPE VC", 0},
    {"jack_slope1_out", true, patch::SlopeOut, 730.f, 625.f, "SLOPE OUT", 4},
    {"jack_slope1_ac", true, patch::SlopeAc, 730.f, 725.f, "SLOPE AC", 4},
    {"jack_slope1_end", true, patch::SlopeEnd, 730.f, 825.f, "SLOPE END", 1},
    {"jack_slope2_in", false, patch::Slope2In, 870.f, 625.f, "SLOPE 2 IN", 0},
    {"jack_slope2_trig", false, patch::Slope2Trig, 870.f, 725.f, "SLOPE 2 TRIG", 0},
    {"jack_slope2_vc", false, patch::Slope2Vc, 870.f, 825.f, "SLOPE 2 VC", 0},
    {"jack_slope2_out", true, patch::Slope2Out, 1510.f, 625.f, "SLOPE 2 OUT", 4},
    {"jack_slope2_ac", true, patch::Slope2Ac, 1510.f, 725.f, "SLOPE 2 AC", 4},
    {"jack_slope2_end", true, patch::Slope2End, 1510.f, 825.f, "SLOPE 2 END", 1},
};

struct CableEnd
{
  int input = -1;
  int output = -1;
  int level = 0;
  float x0 = 0.f;
  float y0 = 0.f;
  float x1 = 0.f;
  float y1 = 0.f;
};

inline bool LookupJack(bool output, int index, float& x, float& y)
{
  for (int i = 0; i < kJackCount; ++i)
  {
    if (kJacks[i].output == output && kJacks[i].index == index)
    {
      x = kJacks[i].x;
      y = kJacks[i].y;
      return true;
    }
  }
  return false;
}

inline int StackLevel(const patch::Routing& routing, int output, int input)
{
  int level = 0;
  for (int i = 0; i < patch::kNumInputs; ++i)
  {
    if (routing.source[i] != output)
      continue;
    if (i == input)
      break;
    ++level;
  }
  return level;
}

// Design pixels. False when the source is unpatched, out of range, or either
// end is not a jack in this table. (0, 0) is not a jack, so it is not an end.
inline bool ResolveCable(const patch::Routing& routing, int input, CableEnd& end)
{
  if (input < 0 || input >= patch::kNumInputs)
    return false;
  const int output = routing.source[input];
  if (output < 0 || output >= patch::kNumOutputs)
    return false;
  float x0 = 0.f;
  float y0 = 0.f;
  float x1 = 0.f;
  float y1 = 0.f;
  if (!LookupJack(true, output, x0, y0) || !LookupJack(false, input, x1, y1))
    return false;
  const int level = StackLevel(routing, output, input);
  end.input = input;
  end.output = output;
  end.level = level;
  end.x0 = x0;
  end.y0 = y0 - kStackStep * static_cast<float>(level);
  end.x1 = x1;
  end.y1 = y1;
  return true;
}

// Writes up to cap spans and returns how many cables the routing actually has.
inline int ResolveCables(const patch::Routing& routing, CableEnd* out, int cap)
{
  int count = 0;
  for (int input = 0; input < patch::kNumInputs; ++input)
  {
    CableEnd end;
    if (!ResolveCable(routing, input, end))
      continue;
    if (out != nullptr && count < cap)
      out[count] = end;
    ++count;
  }
  return count;
}

} // namespace slewfold
