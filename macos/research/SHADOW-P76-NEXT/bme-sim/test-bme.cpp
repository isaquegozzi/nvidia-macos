// test-bme.cpp — M6 BME policy tests (mocked PCI only, zero HW).
#include <cassert>
#include <cstdio>
#include "bme_sim.hpp"

#define PASS(name) do { std::printf("PASS(%s)\n", name); } while (0)

static bme::Preconditions Good() {
  bme::Preconditions p;
  p.gateAReady = true;
  p.gateAActive = false;
  p.flushRegistered = true;
  p.gfwReady = true;
  p.bar0Mapped = true;
  p.flushPhase = bme::FlushPhase::kRegistered;
  return p;
}

int main() {
  using namespace bme;
  // 1. Baseline preserved.
  {
    MockPci pci;
    assert(pci.readCommand() == kCmdBaseline && pci.writes.empty());
    PASS("baseline-0x0003-no-writes");
  }
  // 2. Enable happy path 0x0003 -> 0x0007, exactly one write.
  {
    MockPci pci;
    assert(BmePolicy::enable(pci, Good()) == kOk);
    assert(pci.readCommand() == kCmdEnabled && pci.writes.size() == 1u);
    assert(pci.writes[0].off == kCmdOff && pci.writes[0].val == kCmdEnabled);
    PASS("enable-0x0003-to-0x0007-one-write");
  }
  // 3. Disable 0x0007 -> 0x0003.
  {
    MockPci pci;
    assert(BmePolicy::enable(pci, Good()) == kOk);
    assert(BmePolicy::disable(pci) == kOk);
    assert(pci.readCommand() == kCmdBaseline && pci.writes.size() == 2u);
    PASS("disable-returns-0x0003");
  }
  // 4. Double enable AlreadyOn, no extra write.
  {
    MockPci pci;
    assert(BmePolicy::enable(pci, Good()) == kOk);
    assert(BmePolicy::enable(pci, Good()) == kAlreadyOn);
    assert(pci.writes.size() == 1u);
    PASS("double-enable-noop");
  }
  // 5. Double disable AlreadyOff.
  {
    MockPci pci;
    assert(BmePolicy::disable(pci) == kAlreadyOff && pci.writes.empty());
    PASS("double-disable-noop");
  }
  // 6. Deprecated API rejected, zero writes.
  {
    MockPci pci;
    assert(BmePolicy::setBusMasterEnable(true) == kInvalidArg);
    assert(BmePolicy::setBusMasterEnable(false) == kInvalidArg);
    assert(pci.writes.empty() && pci.readCommand() == kCmdBaseline);
    PASS("deprecated-api-rejected-zero-writes");
  }
  // 7. Each missing precondition Denied, zero writes.
  {
    for (int i = 0; i < 4; ++i) {
      MockPci pci;
      Preconditions p = Good();
      if (i == 0) p.gateAReady = false;
      if (i == 1) p.flushRegistered = false;
      if (i == 2) p.gfwReady = false;
      if (i == 3) p.bar0Mapped = false;
      assert(BmePolicy::enable(pci, p) == kDenied);
      assert(pci.writes.empty() && pci.readCommand() == kCmdBaseline);
    }
    PASS("each-missing-precondition-denied");
  }
  // 8. Gate A active -> Denied (BME stays OFF during Gate A).
  {
    MockPci pci;
    Preconditions p = Good();
    p.gateAActive = true;
    assert(BmePolicy::enable(pci, p) == kDenied);
    assert(pci.writes.empty());
    PASS("gatea-active-denied");
  }
  // 9. Unknown Command bits -> Error, no transition.
  {
    MockPci pci;
    pci.cfg[kCmdOff + 1] = 0x01u;  // 0x0103: unknown bit8
    assert(BmePolicy::enable(pci, Good()) == kError);
    assert(BmePolicy::disable(pci) == kError);
    assert(pci.writes.empty());
    PASS("unknown-bits-error-no-touch");
  }
  // 10. Flush-phase timing matrix.
  {
    MockPci a;
    Preconditions p = Good();
    p.flushPhase = FlushPhase::kUnregistered;
    assert(BmePolicy::enable(a, p) == kDenied && a.writes.empty());
    MockPci b;
    p.flushPhase = FlushPhase::kRegistered;
    assert(BmePolicy::enable(b, p) == kOk);
    MockPci c;
    p.flushPhase = FlushPhase::kVerified;
    assert(BmePolicy::enable(c, p) == kOk);
    PASS("phase-matrix-unreg-denied-reg-verified-ok");
  }
  // 11. Write confinement: only Command offset, only legal values.
  {
    MockPci pci;
    Preconditions p = Good();
    assert(BmePolicy::enable(pci, p) == kOk);
    assert(BmePolicy::disable(pci) == kOk);
    assert(BmePolicy::enable(pci, p) == kOk);
    for (auto &w : pci.writes) {
      assert(w.off == kCmdOff);
      assert(w.val == kCmdEnabled || w.val == kCmdBaseline);
    }
    PASS("writes-confined-command-legal-values");
  }
  // 12. Deterministic sweep: reserved-bit fuzz always Error; precond fuzz exact.
  {
    for (uint32_t r = 1; r < 256; ++r) {
      MockPci pci;
      pci.cfg[kCmdOff + 1] = static_cast<uint8_t>(r);
      uint16_t cmd = pci.readCommand();
      if ((cmd & ~kKnownBits) != 0u) {
        assert(BmePolicy::enable(pci, Good()) == kError && pci.writes.empty());
      }
    }
    uint64_t s = 0x12345678u;
    for (int i = 0; i < 20000; ++i) {
      s = s * 6364136223846793005ull + 1442695040888963407ull;
      Preconditions p;
      p.gateAReady = (s >> 0) & 1u;
      p.gateAActive = (s >> 1) & 1u;
      p.flushRegistered = (s >> 2) & 1u;
      p.gfwReady = (s >> 3) & 1u;
      p.bar0Mapped = (s >> 4) & 1u;
      p.flushPhase = static_cast<FlushPhase>((s >> 5) % 3u);
      MockPci pci;
      Result r = BmePolicy::enable(pci, p);
      bool shouldOk = p.gateAReady && !p.gateAActive && p.flushRegistered && p.gfwReady &&
                      p.bar0Mapped && p.flushPhase != FlushPhase::kUnregistered;
      assert((r == kOk) == shouldOk);
      if (!shouldOk) assert(pci.writes.empty());
    }
    PASS("sweep-reserved-error-precond-exact");
  }
  std::printf("BME_GROUPS=12 PASS\n");
  return 0;
}
