#include "doctest.h"
#include "../pantograph.h"

using namespace pantograph;

TEST_CASE("KnobToCV normalizes 0..4096 to -2048..2047") {
  CHECK_EQ(KnobToCV(0), -2048);
  CHECK_EQ(KnobToCV(2048), 0);
  CHECK_EQ(KnobToCV(4095), 2047);
}

TEST_CASE("SumClamp sums two voltages with a clamp") {
  CHECK_EQ(SumClamp(1000, 1000), 2000);
  CHECK_EQ(SumClamp(2000, 2000), 2047);
  CHECK_EQ(SumClamp(-2000, -2000), -2048);
}

TEST_CASE("Buffer starts with length 0") {
  auto buffer = Buffer();

  CHECK_EQ(buffer.Length(), 0);

  buffer.Push({100, -100});
  buffer.Push({200, -200});

  CHECK_EQ(buffer.Length(), 2);
  CHECK_EQ(buffer.At(0).x, 100);
  CHECK_EQ(buffer.At(1).y, -200);

  buffer.Reset();

  CHECK_EQ(buffer.Length(), 0);
}

TEST_CASE("Buffer stops writing when full") {
  auto buffer = Buffer();
  auto cap = buffer.Capacity();

  for (int16_t i = 0; i < cap; ++i) {
    buffer.Push({i, i});
  }

  auto last = static_cast<int16_t>(cap - 1);
  CHECK_EQ(buffer.At(cap - 1).x, last);

  buffer.Push({9999, 9999});

  CHECK_EQ(buffer.At(cap - 1).x, last);
}

TEST_CASE("Decimator fires immediately, then every period ticks") {
  Decimator d(192);

  CHECK_EQ(d.Tick(), true); // t=0 fires at once

  for (int i = 0; i < 191; ++i) {
    CHECK_EQ(d.Tick(), false);
  }

  CHECK_EQ(d.Tick(), true); // one period later
}

TEST_CASE("Decimator restarts its cadence on Reset") {
  Decimator d(192);
  d.Tick();
  d.Tick(); // mid-period

  d.Reset();

  CHECK_EQ(d.Tick(), true); // fresh t=0 fires immediately
}

TEST_CASE("LerpI interpolates between two frames") {
  CHECK_EQ(LerpI(0, 100, 0), 0); // start
  CHECK_EQ(LerpI(0, 100, 1 << 15), 50); // halfway
  CHECK_EQ(LerpI(0, -100, 1u << 15), -50); // halfway from 0 down to -100
  CHECK_EQ(LerpI(100, 0, 1u << 15), 50); // halfway from 100 down to 0
}

TEST_CASE("TriggerOut stretches a trigger to a gate") {
  TriggerOut t;

  CHECK_EQ(t.tick(true), true);

  for (int i = 0; i < 239; ++i) {
    CHECK_EQ(t.tick(false), true);
  }

  CHECK_EQ(t.tick(false), false);
}

TEST_CASE("Schmitt gates with hysteresis") {
  Schmitt s;

  CHECK_EQ(s.update(0), false);
  CHECK_EQ(s.update(300), true);
  CHECK_EQ(s.update(100), true);
  CHECK_EQ(s.update(-100), true);
  CHECK_EQ(s.update(-300), false);
  CHECK_EQ(s.update(50), false);
}

TEST_CASE("Phasor advances one whole frame") {
  Phasor p;
  p.SetLength(4);
  p.Advance(1 << 16, true);

  CHECK_EQ(p.Index(), 1);
  CHECK_EQ(p.Frac(), 0);
}

TEST_CASE("Phasor wraps forward at the loop end") {
  Phasor p;
  p.SetLength(4);
  p.Advance(4 << 16, true);

  CHECK_EQ(p.Index(), 0);
  CHECK_EQ(p.Finished(), false);
}

TEST_CASE("Phasor wraps backward past zero") {
  Phasor p;
  p.SetLength(3);
  p.Advance(-(1 << 16), true);

  CHECK_EQ(p.Index(), 2);

  p.Advance(-(1 << 16), true);

  CHECK_EQ(p.Index(), 1);
}

TEST_CASE("When not looping, phasor clamps") {
  Phasor p;
  p.SetLength(4);
  p.Advance(4 << 16, false);

  CHECK_EQ(p.Index(), 3);
  CHECK_EQ(p.Finished(), true);

  p.Trigger();

  CHECK_EQ(p.Index(), 0);
  CHECK_EQ(p.Finished(), false);
}

TEST_CASE("One-shot phasor reports the end only once") {
  Phasor p;
  p.SetLength(4);

  CHECK_EQ(p.Advance(4 << 16, false), true); // arrives at the end
  CHECK_EQ(p.Advance(1 << 16, false), false); // pinned: no new event
  CHECK_EQ(p.Advance(0, false), false); // frozen at the end: no event
  CHECK_EQ(p.Finished(), true);
}

TEST_CASE("One-shot phasor reports arrival at zero only once when reversed") {
  Phasor p;
  p.SetLength(4);
  p.Advance(2 << 16, false); // move to frame 2

  CHECK_EQ(p.Advance(-(3 << 16), false), true); // arrives at zero
  CHECK_EQ(p.Advance(-(1 << 16), false), false); // pinned at zero
}

TEST_CASE("One-shot phasor does not report reversing away from the start") {
  Phasor p;
  p.SetLength(4);

  CHECK_EQ(p.Advance(-(1 << 16), false), false); // never moved: no event
}

TEST_CASE("SetLength restarts playback from the top") {
  Phasor p;
  p.SetLength(10);
  p.Advance(7 << 16, true); // deep into the old loop

  p.SetLength(4); // a new, shorter recording

  CHECK_EQ(p.Advance(0, true), false); // no phantom wrap event
  CHECK_EQ(p.Index(), 0); // plays from the start, not mid-loop
}

TEST_CASE("SetLength clears one-shot finished state") {
  Phasor p;
  p.SetLength(4);
  p.Advance(4 << 16, false); // finish the one-shot

  CHECK_EQ(p.Finished(), true);

  p.SetLength(8); // a new recording

  CHECK_EQ(p.Finished(), false);
}

TEST_CASE("Phasor reports when it forward wraps") {
  Phasor p;
  p.SetLength(4);

  CHECK_EQ(p.Advance(1 << 16, true), false); // start
  CHECK_EQ(p.Advance(1 << 16, true), false);
  CHECK_EQ(p.Advance(1 << 16, true), false);
  CHECK_EQ(p.Advance(1 << 16, true), true); // wrap
}

TEST_CASE("Phasor reports when it backward wraps") {
  Phasor p;
  p.SetLength(4);

  CHECK_EQ(p.Advance(-(1 << 16), true), true);
}

TEST_CASE("KnobToSpeed maps big knob to bipolar speed") {
  CHECK_EQ(KnobToSpeed(2048), 0); // center = freeze
  CHECK(KnobToSpeed(4095) > 0); // right = forward
  CHECK(KnobToSpeed(0) < 0); // left = reverse
  CHECK_EQ(KnobToSpeed(2048 + 10), 0); // inside dead-zone still freezes
}

TEST_CASE("KnobToSpeed eases in from zero as the knob leaves centre") {
  // Still inside the do-nothing band around centre: frozen...
  CHECK_EQ(KnobToSpeed(2048 + 40), 0);
  CHECK_EQ(KnobToSpeed(2048 - 40), 0);

  // ...and one step further, it barely moves — no sudden jump.
  CHECK(KnobToSpeed(2048 + 41) > 0);
  CHECK(KnobToSpeed(2048 + 41) <= 128);
  CHECK(KnobToSpeed(2048 - 41) < 0);
  CHECK(KnobToSpeed(2048 - 41) >= -128);
}

TEST_CASE("ApplyDepth attenuates") {
  CHECK_EQ(ApplyDepth(2000, 0), 0);
  CHECK_EQ(ApplyDepth(2000, 4095), 2000); // full CW passes the trace through exactly
  CHECK_EQ(ApplyDepth(2000, 2048), 1000);
  CHECK_EQ(ApplyDepth(-2000, 4095), -2000);
}
