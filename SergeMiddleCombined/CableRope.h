#pragma once

// Hanging patch cable. Message thread only. The audio thread does not include this.
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// Port of Lift (martialsystems/lift, branch tape-engine-juce):
//   panel/CableRope.h          Verlet rope: 36 nodes, gravity, damping, constraints
//   panel/LiftPanelPaint.cpp   where the rope is pinned (4 px under the plug)
// Lift's step, push, and repel are the same numbers: gravity 0.45 px/step,
// damping 0.985 while it is moving and 0.9 once a step is under 0.4 px,
// 10 constraint passes, slack d + min(0.3 d + 40, 140). A new rope is laid
// on a sine and settled before it is shown.
//
// A drop calls kick() and then steps with the 0.9 damping (Lift's resting
// branch) twice per UI frame, so the flick is done in under a second. The
// caller stops the redraw once step() stays under 0.03 for 20 frames, which
// is Lift's ropeIdle_ count.

#include <algorithm>
#include <cmath>

namespace slewfold
{

struct Vec
{
  float x = 0.f;
  float y = 0.f;
};

struct RopeScene
{
  bool ptrIn = false;
  Vec ptr;
  bool hasJack = false;
  Vec jack;
  bool hasLabel = false;
  float labelL = 0.f;
  float labelT = 0.f;
  float labelR = 0.f;
  float labelB = 0.f;
  float floor = 1.0e9f;
};

class Rope
{
public:
  static constexpr int N = 36;

  bool valid() const { return mInit; }

  void reset()
  {
    mInit = false;
    mLastMv = 1.f;
  }

  // Pin the ends. The first call lays the rope and settles it.
  void attach(Vec a, Vec b, const RopeScene& scene)
  {
    mA = a;
    mB = b;
    const float d = std::hypot(b.x - a.x, b.y - a.y);
    mL = d + std::fmin(d * 0.3f + 40.f, 140.f);
    if (mInit)
      return;
    for (int i = 0; i < N; ++i)
    {
      const float t = static_cast<float>(i) / static_cast<float>(N - 1);
      mP[i].x = a.x + (b.x - a.x) * t;
      mP[i].y = a.y + (b.y - a.y) * t + std::sin(3.14159265f * t) * 40.f;
      mQ[i] = mP[i];
    }
    mInit = true;
    mLastMv = 1.f;
    RopeScene calm = scene;
    calm.ptrIn = false;
    calm.hasJack = false;
    calm.hasLabel = false;
    // Lift settles 160 steps before the first frame.
    for (int k = 0; k < 160; ++k)
      step(calm, false);
    // Finish that settle off the timer so the first painted frame is the sag.
    int idle = 0;
    for (int k = 0; k < 600 && idle < 20; ++k)
    {
      const float mv = step(calm, true);
      idle = mv > 0.03f ? 0 : idle + 1;
    }
  }

  // One Verlet step. Returns how far the free nodes moved.
  // restDamp keeps Lift's 0.9 branch so a drop settles inside a second.
  float step(const RopeScene& scene, bool restDamp)
  {
    const float seg = mL / static_cast<float>(N - 1);
    const float damp = (restDamp || mLastMv < 0.4f) ? 0.9f : 0.985f;
    for (int i = 1; i < N - 1; ++i)
    {
      const Vec v = mP[i];
      const float nx = v.x + (v.x - mQ[i].x) * damp;
      const float ny = v.y + (v.y - mQ[i].y) * damp + 0.45f;
      mQ[i] = v;
      mP[i].x = nx;
      mP[i].y = ny;
    }
    for (int it = 0; it < 10; ++it)
    {
      mP[0] = mA;
      mP[N - 1] = mB;
      for (int i = 0; i < N - 1; ++i)
      {
        Vec& a = mP[i];
        Vec& b = mP[i + 1];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float dist = std::fmax(1.0e-3f, std::hypot(dx, dy));
        const float k = (dist - seg) / dist;
        const float wa = i ? 1.f : 0.f;
        const float wb = i < N - 2 ? 1.f : 0.f;
        const float w = wa + wb;
        a.x += dx * k * wa / w;
        a.y += dy * k * wa / w;
        b.x -= dx * k * wb / w;
        b.y -= dy * k * wb / w;
      }
      for (int i = 1; i < N - 1; ++i)
        push(mP[i], scene);
    }
    float mv = 0.f;
    for (int i = 1; i < N - 1; ++i)
      mv = std::fmax(mv, std::fabs(mP[i].x - mQ[i].x) + std::fabs(mP[i].y - mQ[i].y));
    mLastMv = mv;
    return mv;
  }

  // A small downward flick after a drop or a move. The shape stays Lift's.
  void kick(float amount)
  {
    if (!mInit)
      return;
    mLastMv = 0.2f;
    for (int i = 1; i < N - 1; ++i)
    {
      const float s = std::sin(3.14159265f * static_cast<float>(i) / static_cast<float>(N - 1));
      mQ[i].y = mP[i].y - amount * s;
    }
  }

  Vec at(int i) const { return mP[i]; }

private:
  static void repel(Vec& n, Vec c, float r)
  {
    const float dx = n.x - c.x;
    const float dy = n.y - c.y;
    const float d = std::hypot(dx, dy);
    if (d < r)
    {
      if (d > 0.f)
      {
        const float f = (r - d) / d * 0.6f;
        n.x += dx * f;
        n.y += dy * f;
      }
      else
        n.y += r * 0.6f;
    }
  }

  static void push(Vec& n, const RopeScene& scene)
  {
    if (scene.hasJack)
      repel(n, scene.jack, 40.f);
    if (scene.ptrIn)
      repel(n, scene.ptr, 28.f);
    if (scene.hasLabel && n.x > scene.labelL - 10.f && n.x < scene.labelR + 10.f && n.y > scene.labelT - 10.f &&
        n.y < scene.labelB + 10.f)
    {
      const float left = n.x - (scene.labelL - 10.f);
      const float right = scene.labelR + 10.f - n.x;
      const float top = n.y - (scene.labelT - 10.f);
      const float bottom = scene.labelB + 10.f - n.y;
      const float m = std::fmin(std::fmin(left, right), std::fmin(top, bottom));
      if (m == left)
        n.x -= left * 0.6f;
      else if (m == right)
        n.x += right * 0.6f;
      else if (m == top)
        n.y -= top * 0.6f;
      else
        n.y += bottom * 0.6f;
    }
    if (n.y > scene.floor)
      n.y = scene.floor;
  }

  Vec mP[N];
  Vec mQ[N];
  Vec mA;
  Vec mB;
  float mL = 0.f;
  float mLastMv = 1.f;
  bool mInit = false;
};

} // namespace slewfold
