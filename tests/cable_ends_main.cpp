// Default routing draws no cables. Every drawn end is a jack centre.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "../SergeMiddleCombined/CableEnds.h"

#include <cmath>
#include <cstdio>

namespace
{
int Fail(const char* message)
{
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

bool Near(float a, float b)
{
  return std::fabs(a - b) < 0.01f;
}

bool IsJack(float x, float y, bool output)
{
  for (int i = 0; i < slewfold::kJackCount; ++i)
  {
    const slewfold::JackSpot& jack = slewfold::kJacks[i];
    if (jack.output != output)
      continue;
    const float jy = output ? jack.y - slewfold::kStackStep * 12.f : jack.y;
    const float y0 = output ? jack.y : jack.y;
    if (Near(jack.x, x) && y <= y0 + 0.01f && y >= jy - 0.01f)
      return true;
  }
  return false;
}
} // namespace

int main()
{
  patch::RoutingWord word;
  if (word.source.load() != -1)
    return Fail("routing word");
  patch::RoutingWord words[patch::kNumInputs];
  for (const patch::RoutingWord& slot : words)
  {
    if (slot.source.load() != -1)
      return Fail("routing words");
  }

  patch::Routing empty;
  slewfold::CableEnd spans[16];
  if (slewfold::ResolveCables(empty, spans, 16) != 0)
    return Fail("default cables");

  for (int output = 0; output < patch::kNumOutputs; ++output)
  {
    for (int input = 0; input < patch::kNumInputs; ++input)
    {
      patch::Routing routing;
      if (!routing.Connect(output, input))
        return Fail("connect");
      const int n = slewfold::ResolveCables(routing, spans, 16);
      if (n != 1)
        return Fail("one cable");
      float x0 = 0.f;
      float y0 = 0.f;
      float x1 = 0.f;
      float y1 = 0.f;
      if (!slewfold::LookupJack(true, output, x0, y0) || !slewfold::LookupJack(false, input, x1, y1))
        return Fail("lookup");
      if (!Near(spans[0].x0, x0) || !Near(spans[0].y0, y0) || !Near(spans[0].x1, x1) || !Near(spans[0].y1, y1))
        return Fail("centres");
      if ((Near(spans[0].x0, 0.f) && Near(spans[0].y0, 0.f)) || (Near(spans[0].x1, 0.f) && Near(spans[0].y1, 0.f)))
        return Fail("origin");
    }
  }

  patch::Routing stacked;
  stacked.Connect(patch::PleatOut, patch::PleatIn1);
  stacked.Connect(patch::PleatOut, patch::PleatIn2);
  if (slewfold::ResolveCables(stacked, spans, 16) != 2)
    return Fail("stack count");
  if (!Near(spans[1].x0, 1000.f) || !Near(spans[1].y0, 305.f - slewfold::kStackStep) || !Near(spans[1].x1, 600.f) ||
      !Near(spans[1].y1, 305.f))
    return Fail("in2 stack");

  patch::Routing bogus;
  bogus.source[patch::PleatIn2] = 99;
  bogus.source[patch::SlopeTrig] = -1;
  bogus.source[patch::TopIn] = static_cast<int8_t>(0x7f);
  if (slewfold::ResolveCables(bogus, spans, 16) != 0)
    return Fail("bogus");

  // Source 0 is a real PLEAT OUT cable, which is what a zero-filled routing
  // used to draw. IN 2 stays horizontal at the PLEAT OUT jack. TRIG stays on
  // the TRIG jack, not on the RISE knob.
  patch::Routing zeros;
  for (int i = 0; i < patch::kNumInputs; ++i)
    zeros.source[i] = 0;
  const int all = slewfold::ResolveCables(zeros, spans, 16);
  if (all != patch::kNumInputs)
    return Fail("zero fill count");
  bool sawIn2 = false;
  bool sawTrig = false;
  for (int i = 0; i < all; ++i)
  {
    if (!IsJack(spans[i].x0, spans[i].y0, true) || !IsJack(spans[i].x1, spans[i].y1, false))
      return Fail("zero fill jack");
    if (spans[i].input == patch::PleatIn2)
    {
      sawIn2 = true;
      if (!Near(spans[i].x0, 1000.f) || !Near(spans[i].x1, 600.f) || !Near(spans[i].y1, 305.f))
        return Fail("in2 pleat");
    }
    if (spans[i].input == patch::SlopeTrig)
    {
      sawTrig = true;
      if (!Near(spans[i].x1, 90.f) || !Near(spans[i].y1, 725.f) || Near(spans[i].x1, 320.f))
        return Fail("trig jack");
    }
  }
  if (!sawIn2 || !sawTrig)
    return Fail("named spans");

  if (slewfold::LookupJack(false, 99, spans[0].x0, spans[0].y0))
    return Fail("missing jack");

  std::puts("cable ends ok");
  return 0;
}
