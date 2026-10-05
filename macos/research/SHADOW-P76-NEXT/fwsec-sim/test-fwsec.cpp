// test-fwsec.cpp — M8 FWSEC sequencing host-only tests. Deterministic, no HW.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "fwsec_sim.hpp"

using namespace fwsec;
static int gGroups = 0;
#define PASS(m) do { std::printf("PASS(%s)\n", m); gGroups++; } while (0)

static uint32_t gLcg = 0xC0FFEEu;
static uint32_t Next() { gLcg = gLcg * 1664525u + 1013904223u; return gLcg >> 8; }

struct Imgs { Range frts, wpr2, radix, sig, boot; };
static Imgs GoodImgs() {
  return {{0x10000000u, 0x1000u}, {0x10001000u, 0x20000u}, {0x10021000u, 0x1000u},
          {0x10022000u, 0x1000u}, {0x10023000u, 0x10000u}};
}
static Wpr2Meta GoodMeta() {
  Wpr2Meta m;
  m.radix = {0x10021000u, 0x800u};
  m.sig = {0x10022000u, 0x100u};
  m.boot = {0x10023000u, 0x1000u};
  m.frtsDma = 0x10000000u;
  m.wpr2Dma = 0x10001000u;
  return m;
}
static void DriveToBoot(Sequencer &s) {
  Imgs g = GoodImgs();
  assert(s.stageImages(g.frts, g.wpr2, g.radix, g.sig, g.boot) == Result::kOk);
  assert(s.validateWpr2(GoodMeta()) == Result::kOk);
  assert(s.buildTables() == Result::kOk);
  assert(s.readyBootloader() == Result::kOk);
}

int main() {
  {  // 1. happy full sequence
    Sequencer s;
    FakeClock clk;
    DriveToBoot(s);
    assert(s.boot(clk, false) == Result::kOk);
    assert(s.stage() == Stage::kDone && s.bootTicks() == 100u);
    PASS("happy-full-sequence-done");
  }
  {  // 2. image errors
    Imgs g = GoodImgs();
    Sequencer a;
    Range zero = {0x20000000u, 0u};
    assert(a.stageImages(zero, g.wpr2, g.radix, g.sig, g.boot) == Result::kBadMeta);
    assert(a.stage() == Stage::kUnloaded);
    Sequencer b;
    Range unal = {0x10000001u, 0x1000u};
    assert(b.stageImages(unal, g.wpr2, g.radix, g.sig, g.boot) == Result::kBadMeta);
    Sequencer c;
    Range wrap = {0xFFFFFFFFFFFFF000ull, 0x2000u};  // wraps past 2^64
    assert(c.stageImages(wrap, g.wpr2, g.radix, g.sig, g.boot) == Result::kBadMeta);
    Sequencer d;
    assert(d.stageImages(g.frts, g.frts, g.radix, g.sig, g.boot) == Result::kOverlap);
    assert(d.stage() == Stage::kUnloaded);
    PASS("image-errors-zero-unaligned-wrap-overlap");
  }
  {  // 3. meta corruptions
    Imgs g = GoodImgs();
    Sequencer a;
    assert(a.stageImages(g.frts, g.wpr2, g.radix, g.sig, g.boot) == Result::kOk);
    Wpr2Meta m = GoodMeta();
    m.sig = {0x10023000u, 0x100u};  // outside sig image (inside boot instead)
    assert(a.validateWpr2(m) == Result::kBadMeta);
    assert(a.stage() == Stage::kFrtsStaged);
    Sequencer b;
    assert(b.stageImages(g.frts, g.wpr2, g.radix, g.sig, g.boot) == Result::kOk);
    Wpr2Meta m2 = GoodMeta();
    m2.radix = {0x10021001u, 0x800u};  // unaligned
    assert(b.validateWpr2(m2) == Result::kBadMeta);
    Sequencer c;
    assert(c.stageImages(g.frts, g.wpr2, g.radix, g.sig, g.boot) == Result::kOk);
    Wpr2Meta m3 = GoodMeta();
    m3.sig = m3.radix;  // meta/meta overlap (also outside sig image -> BadMeta first?)
    Result r3 = c.validateWpr2(m3);
    assert(r3 == Result::kBadMeta || r3 == Result::kOverlap);
    Sequencer d;
    assert(d.stageImages(g.frts, g.wpr2, g.radix, g.sig, g.boot) == Result::kOk);
    Wpr2Meta m4 = GoodMeta();
    m4.wpr2Dma = 0x10001001u;
    assert(d.validateWpr2(m4) == Result::kBadMeta);
    PASS("meta-corrupt-outside-unaligned-overlap-dma");
  }
  {  // 4. stage order enforced
    Sequencer s;
    FakeClock clk;
    assert(s.buildTables() == Result::kOutOfOrder);
    assert(s.readyBootloader() == Result::kOutOfOrder);
    assert(s.boot(clk, false) == Result::kOutOfOrder);
    Imgs g = GoodImgs();
    assert(s.stageImages(g.frts, g.wpr2, g.radix, g.sig, g.boot) == Result::kOk);
    assert(s.stageImages(g.frts, g.wpr2, g.radix, g.sig, g.boot) == Result::kOutOfOrder);
    assert(s.buildTables() == Result::kOutOfOrder);  // meta not yet valid
    assert(s.validateWpr2(GoodMeta()) == Result::kOk);
    assert(s.validateWpr2(GoodMeta()) == Result::kOutOfOrder);
    PASS("stage-order-enforced");
  }
  {  // 5. timeout at boot: terminal Failed, pinned, no retry
    Sequencer s;
    FakeClock clk;
    DriveToBoot(s);
    assert(s.boot(clk, true) == Result::kTimeout);
    assert(s.stage() == Stage::kFailed && s.failedAt() == FailAt::kBoot);
    assert(s.boot(clk, false) == Result::kAlreadyTerminal);  // no auto-retry
    assert(s.buildTables() == Result::kAlreadyTerminal);
    assert(s.validateWpr2(GoodMeta()) == Result::kOutOfOrder);  // staged, not lost
    PASS("timeout-boot-terminal-no-retry");
  }
  {  // 6. double boot after Done
    Sequencer s;
    FakeClock clk;
    DriveToBoot(s);
    assert(s.boot(clk, false) == Result::kOk);
    assert(s.boot(clk, false) == Result::kAlreadyTerminal);
    PASS("double-boot-terminal");
  }
  {  // 7. 5k deterministic fuzz with exact oracle
    for (int i = 0; i < 5000; ++i) {
      Sequencer s;
      FakeClock clk;
      Imgs g = GoodImgs();
      Wpr2Meta m = GoodMeta();
      int mode = (int)(Next() % 8u);
      if (mode == 2) g.boot.size = 0u;
      if (mode == 3) g.wpr2 = g.frts;
      if (mode == 4) m.boot = {0x90000000u, 0x1000u};
      if (mode == 5) { m.sig = m.radix; }
      if (mode == 6) m.frtsDma = 1u;
      Result rs = s.stageImages(g.frts, g.wpr2, g.radix, g.sig, g.boot);
      if (mode == 2) { assert(rs == Result::kBadMeta); continue; }
      if (mode == 3) { assert(rs == Result::kOverlap); continue; }
      assert(rs == Result::kOk);
      if (mode == 7) {  // skip validation
        assert(s.buildTables() == Result::kOutOfOrder);
        continue;
      }
      Result rv = s.validateWpr2(m);
      if (mode == 4 || mode == 6) { assert(rv == Result::kBadMeta); continue; }
      if (mode == 5) { assert(rv == Result::kBadMeta || rv == Result::kOverlap); continue; }
      assert(rv == Result::kOk);
      assert(s.buildTables() == Result::kOk);
      assert(s.readyBootloader() == Result::kOk);
      bool fault = (mode == 1);
      Result rb = s.boot(clk, fault);
      assert(rb == (fault ? Result::kTimeout : Result::kOk));
      assert(s.stage() == (fault ? Stage::kFailed : Stage::kDone));
    }
    PASS("fuzz-5k-oracle");
  }
  std::printf("FWSEC_GROUPS=%d PASS\n", gGroups);
  return gGroups == 7 ? 0 : 1;
}
