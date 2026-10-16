// Checks the Lift rope settles a drop in under a second.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "../SergeMiddleCombined/CableRope.h"

#include <cstdio>

namespace
{
int Fail(const char* message)
{
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

int Drop(slewfold::Vec a, slewfold::Vec b, float minSag)
{
  slewfold::Rope rope;
  slewfold::RopeScene scene;
  scene.floor = 1.0e9f;
  rope.attach(a, b, scene);
  const float mid = rope.at(slewfold::Rope::N / 2).y;
  const float sag = mid - 0.5f * (a.y + b.y);
  if (!(sag > minSag))
    return Fail("sag");
  rope.kick(3.f);
  int idle = 0;
  int frames = 0;
  for (; frames < 80; ++frames)
  {
    float mv = rope.step(scene, true);
    mv = std::fmax(mv, rope.step(scene, true));
    if (mv > 0.03f)
      idle = 0;
    else
      ++idle;
    if (idle >= 20)
      break;
  }
  if (idle < 20 || frames >= 60)
  {
    std::fprintf(stderr, "settle frames %d idle %d sag %.1f\n", frames, idle, sag);
    return 1;
  }
  return 0;
}
} // namespace

int main()
{
  if (Drop({80.f, 220.f}, {1510.f, 825.f}, 100.f))
    return 1;
  if (Drop({80.f, 220.f}, {480.f, 305.f}, 40.f))
    return 1;
  if (Drop({90.f, 625.f}, {1510.f, 825.f}, 100.f))
    return 1;
  // A rope laid at the origin must follow a later pair of jack pins.
  slewfold::Rope moved;
  slewfold::RopeScene scene;
  scene.floor = 1.0e9f;
  moved.attach({0.f, 0.f}, {0.f, 0.f}, scene);
  moved.attach({600.f, 309.f}, {1000.f, 309.f}, scene);
  const slewfold::Vec head = moved.at(0);
  const slewfold::Vec tail = moved.at(slewfold::Rope::N - 1);
  if (std::fabs(head.x - 600.f) > 0.5f || std::fabs(head.y - 309.f) > 0.5f || std::fabs(tail.x - 1000.f) > 0.5f ||
      std::fabs(tail.y - 309.f) > 0.5f)
    return Fail("stale anchor");
  std::puts("cable rope ok");
  return 0;
}
