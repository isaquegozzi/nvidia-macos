// test-gboot.cpp — M15 GSP boot-sequencing host-only tests. Deterministic, no HW.
// Fake-clock only: quanta are integer advances, never sleeps. Fuzz inputs are
// bounded (ms values < 6000) so no test can hang the fake clock.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "gboot_sim.hpp"

using namespace gboot;
static int gGroups = 0;
#define PASS(m) do { std::printf("PASS(%s)\n", m); gGroups++; } while (0)

static uint32_t gLcg = 0xB007ABu;
static uint32_t Next() { gLcg = gLcg * 1664525u + 1013904223u; return gLcg >> 8; }

static void DriveToExecuting(Boot &s, FakeClock &clk) {
  assert(s.flushRegistered() == Result::kOk);
  assert(s.fwsecFrts(false, true) == Result::kOk);
  assert(s.mboxBoot(0u) == Result::kOk);
  assert(s.booterLoad(0u) == Result::kOk);
  assert(s.riscvPoll(clk, 100u) == Result::kOk);
}

int main() {
  {  // 1. happy full boot to INIT_DONE with tick accounting
    Boot s;
    FakeClock clk;
    DriveToExecuting(s, clk);
    assert(clk.nowMs == 100u);
    assert(s.rpcSequence(clk, 3u, false) == Result::kOk);  // 3 ERANGE + sequence
    assert(clk.nowMs == 104u);
    assert(s.initDone(clk, 2u, false) == Result::kOk);
    assert(clk.nowMs == 107u);
    assert(s.stage() == Stage::kInitDone);
    assert(s.rpcSequence(clk, 0u, false) == Result::kAlreadyTerminal);  // no retry
    PASS("happy-init-done-ticks-107");
  }
  {  // 2. fault at each stage with exact reason + terminal pinning
    FakeClock c0;
    Boot a;
    assert(a.flushRegistered() == Result::kOk);
    assert(a.fwsecFrts(true, true) == Result::kFwsecError);
    assert(a.stage() == Stage::kFailed && a.failedAt() == FailAt::kFwsec);
    assert(a.mboxBoot(0u) == Result::kAlreadyTerminal);
    Boot b;
    assert(b.flushRegistered() == Result::kOk);
    assert(b.fwsecFrts(false, false) == Result::kWprMismatch);
    assert(b.failedAt() == FailAt::kFwsec);
    Boot c;
    FakeClock cc;
    assert(c.flushRegistered() == Result::kOk);
    assert(c.fwsecFrts(false, true) == Result::kOk);
    assert(c.mboxBoot(0xDEADu) == Result::kMboxError);
    assert(c.failedAt() == FailAt::kMbox);
    assert(c.booterLoad(0u) == Result::kAlreadyTerminal);
    Boot d;
    FakeClock dc;
    assert(d.flushRegistered() == Result::kOk);
    assert(d.fwsecFrts(false, true) == Result::kOk);
    assert(d.mboxBoot(0u) == Result::kOk);
    assert(d.booterLoad(1u) == Result::kMboxError);
    assert(d.failedAt() == FailAt::kBooter);
    (void)c0;
    (void)cc;
    (void)dc;
    PASS("fault-each-stage-exact-terminal");
  }
  {  // 3. RISC-V timeout at exactly the 5s budget
    Boot s;
    FakeClock clk;
    assert(s.flushRegistered() == Result::kOk);
    assert(s.fwsecFrts(false, true) == Result::kOk);
    assert(s.mboxBoot(0u) == Result::kOk);
    assert(s.booterLoad(0u) == Result::kOk);
    assert(s.riscvPoll(clk, 6000u) == Result::kRiscvTimeout);
    assert(s.stage() == Stage::kFailed && s.failedAt() == FailAt::kRiscv);
    assert(clk.nowMs == 5000u);
    assert(s.riscvPoll(clk, 0u) == Result::kAlreadyTerminal);
    PASS("riscv-timeout-5000-terminal");
  }
  {  // 4. sequencer + init timeouts; ERANGE-heavy-but-under-budget succeeds
    Boot s;
    FakeClock clk;
    DriveToExecuting(s, clk);
    assert(s.rpcSequence(clk, 0u, true) == Result::kSeqTimeout);
    assert(s.failedAt() == FailAt::kSequencer);
    Boot t;
    FakeClock tc;
    DriveToExecuting(t, tc);
    assert(t.rpcSequence(tc, 4998u, false) == Result::kOk);  // 4998+1 < 5000
    assert(t.initDone(tc, 0u, true) == Result::kInitTimeout);
    assert(t.failedAt() == FailAt::kInitDone);
    assert(t.initDone(tc, 0u, false) == Result::kAlreadyTerminal);
    PASS("seq-init-timeout-erange-budget-edge");
  }
  {  // 5. order enforced at every stage
    Boot s;
    FakeClock clk;
    assert(s.fwsecFrts(false, true) == Result::kOutOfOrder);
    assert(s.mboxBoot(0u) == Result::kOutOfOrder);
    assert(s.booterLoad(0u) == Result::kOutOfOrder);
    assert(s.riscvPoll(clk, 0u) == Result::kOutOfOrder);
    assert(s.rpcSequence(clk, 0u, false) == Result::kOutOfOrder);
    assert(s.initDone(clk, 0u, false) == Result::kOutOfOrder);
    assert(s.flushRegistered() == Result::kOk);
    assert(s.flushRegistered() == Result::kOutOfOrder);  // no re-entry
    assert(s.rpcSequence(clk, 0u, false) == Result::kOutOfOrder);  // skip forbidden
    PASS("order-enforced-every-stage");
  }
  {  // 6. double boot after INIT_DONE
    Boot s;
    FakeClock clk;
    DriveToExecuting(s, clk);
    assert(s.rpcSequence(clk, 0u, false) == Result::kOk);
    assert(s.initDone(clk, 0u, false) == Result::kOk);
    assert(s.flushRegistered() == Result::kAlreadyTerminal);
    assert(s.initDone(clk, 0u, false) == Result::kAlreadyTerminal);
    PASS("double-boot-terminal");
  }
  {  // 7. 5k fuzz over fault vectors with exact oracle
    for (int i = 0; i < 5000; ++i) {
      Boot s;
      FakeClock clk;
      bool fwErr = (Next() % 13u == 0u);
      bool wprBad = !fwErr && (Next() % 13u == 1u);
      uint32_t mbox = (Next() % 17u == 0u) ? (1u + Next() % 99u) : 0u;
      uint32_t sec2 = (Next() % 17u == 0u) ? (1u + Next() % 99u) : 0u;
      uint64_t riscvMs = Next() % 6000u;
      uint64_t seqEr = Next() % 6000u;
      bool seqTo = (Next() % 11u == 0u);
      uint64_t initEr = Next() % 6000u;
      bool initTo = (Next() % 11u == 0u);
      bool skipFlush = (Next() % 29u == 0u);
      if (!skipFlush) assert(s.flushRegistered() == Result::kOk);
      Result rf = s.fwsecFrts(fwErr, !wprBad);
      if (skipFlush) { assert(rf == Result::kOutOfOrder); continue; }
      if (fwErr) { assert(rf == Result::kFwsecError); continue; }
      if (wprBad) { assert(rf == Result::kWprMismatch); continue; }
      assert(rf == Result::kOk);
      Result rm = s.mboxBoot(mbox);
      if (mbox != 0u) { assert(rm == Result::kMboxError); continue; }
      assert(rm == Result::kOk);
      Result rb = s.booterLoad(sec2);
      if (sec2 != 0u) { assert(rb == Result::kMboxError); continue; }
      assert(rb == Result::kOk);
      Result rr = s.riscvPoll(clk, riscvMs);
      // 10ms sampling: observed = ceil(ms/10)*10; timeout iff observed >= 5000.
      uint64_t observed = ((riscvMs + 9u) / 10u) * 10u;
      if (observed >= kRiscvBudgetMs) { assert(rr == Result::kRiscvTimeout); continue; }
      assert(rr == Result::kOk);
      Result rs = s.rpcSequence(clk, seqEr, seqTo);
      if (seqTo || seqEr * kMsgQuantumMs >= kMsgBudgetMs) {
        assert(rs == Result::kSeqTimeout);
        continue;
      }
      assert(rs == Result::kOk);
      Result ri = s.initDone(clk, initEr, initTo);
      if (initTo || initEr * kMsgQuantumMs >= kMsgBudgetMs) {
        assert(ri == Result::kInitTimeout);
        continue;
      }
      assert(ri == Result::kOk);
      assert(s.stage() == Stage::kInitDone);
    }
    PASS("fuzz-5k-oracle");
  }
  std::printf("GBOOT_GROUPS=%d PASS\n", gGroups);
  return gGroups == 7 ? 0 : 1;
}
