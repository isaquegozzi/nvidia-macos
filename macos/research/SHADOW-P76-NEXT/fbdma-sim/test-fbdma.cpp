// test-fbdma.cpp — M7 FBDMA host-only tests (fake clock/descriptors, zero HW).
#include <cassert>
#include <cstdio>
#include "fbdma_sim.hpp"

#define PASS(name) do { std::printf("PASS(%s)\n", name); } while (0)
using namespace fbdma;

static BufSpec Spec(BufKind k, uint64_t sz, uint64_t al) {
  BufSpec s;
  s.kind = k;
  s.size = sz;
  s.align = al;
  return s;
}

int main() {
  // 1. Full 8-kind inventory: aligned, ordered, disjoint.
  {
    Inventory inv;
    const BufSpec specs[] = {
      Spec(BufKind::kFlushPage, 4096, 4096), Spec(BufKind::kFrts, 4096, 4096),
      Spec(BufKind::kWpr2, 131072, 256), Spec(BufKind::kRadixL0, 4096, 4096),
      Spec(BufKind::kRadixL1, 4096, 4096), Spec(BufKind::kRadixL2, 4096, 4096),
      Spec(BufKind::kBootloader, 65536, 4096), Spec(BufKind::kLibOs, 8192, 4096),
      Spec(BufKind::kRpcCmdQ, 4096, 4096), Spec(BufKind::kRpcStatusQ, 4096, 4096),
    };
    uint64_t prevEnd = 0u;
    for (auto &s : specs) {
      uint64_t io = 0u;
      assert(inv.reserve(s, &io) >= 0);
      assert((io % s.align) == 0u && (io % 256u) == 0u);
      assert(io >= prevEnd);
      prevEnd = io + s.size;
    }
    assert(inv.size() == 10u);
    PASS("inventory-10-disjoint-aligned");
  }
  // 2. Bad specs rejected: zero size, align<256, non-pow2 align.
  {
    Inventory inv;
    assert(inv.reserve(Spec(BufKind::kFrts, 0, 4096), nullptr) < 0);
    assert(inv.reserve(Spec(BufKind::kFrts, 4096, 128), nullptr) < 0);
    assert(inv.reserve(Spec(BufKind::kFrts, 4096, 300), nullptr) < 0);
    assert(inv.size() == 0u);
    PASS("bad-specs-rejected");
  }
  // 3. Single-chunk happy: 256B Done, regs programmed, bytesDone full.
  {
    Transfer t;
    Transcfg c{Target::kCoherentSysmem, MemType::kPhysical};
    FakeClock clk;
    assert(t.program(c, kIovaBase, 0u, 256u) == kDone);
    assert(t.programmed() && t.regs().trfBase == kIovaBase && t.regs().trfCmd == 1u);
    assert(t.run(clk, -1, false) == kDone && t.bytesDone() == 256u);
    assert(!t.programmed());
    PASS("single-chunk-done");
  }
  // 4. Multi-chunk 4096B happy: Done, 4096 bytes.
  {
    Transfer t;
    Transcfg c{Target::kCoherentSysmem, MemType::kPhysical};
    FakeClock clk;
    assert(t.program(c, kIovaBase, 0x1000u, 4096u) == kDone);
    assert(t.run(clk, -1, false) == kDone && t.bytesDone() == 4096u);
    PASS("multi-chunk-16-done");
  }
  // 5. Timeout at chunk 3 of 16: Timeout + bytesDone 768 (256B granularity proof).
  {
    Transfer t;
    Transcfg c{Target::kCoherentSysmem, MemType::kPhysical};
    FakeClock clk;
    assert(t.program(c, kIovaBase, 0u, 4096u) == kDone);
    assert(t.run(clk, 3, false) == kTimeout && t.bytesDone() == 768u);
    PASS("timeout-chunk3-bytes768");
  }
  // 6. Wrong target / memtype rejected, never programmed.
  {
    Transfer t1, t2;
    Transcfg badT{Target::kDeviceLocal, MemType::kPhysical};
    Transcfg badM{Target::kCoherentSysmem, MemType::kVirtual};
    assert(t1.program(badT, kIovaBase, 0u, 256u) == kInvalidArg && !t1.programmed());
    assert(t2.program(badM, kIovaBase, 0u, 256u) == kInvalidArg && !t2.programmed());
    PASS("bad-target-memtype-rejected");
  }
  // 7. Bad lengths/offsets: zero, non-256-multiple, unaligned IOVA.
  {
    Transfer t;
    Transcfg c{Target::kCoherentSysmem, MemType::kPhysical};
    assert(t.program(c, kIovaBase, 0u, 0u) == kInvalidArg);
    assert(t.program(c, kIovaBase, 0u, 100u) == kInvalidArg);
    assert(t.program(c, kIovaBase + 1u, 0u, 256u) == kInvalidArg);
    assert(!t.programmed());
    PASS("bad-length-iova-rejected");
  }
  // 8. Unprogrammed run + backend fault paths.
  {
    Transfer t;
    FakeClock clk;
    assert(t.run(clk, -1, false) == kInvalidArg);
    Transcfg c{Target::kCoherentSysmem, MemType::kPhysical};
    assert(t.program(c, kIovaBase, 0u, 256u) == kDone);
    assert(t.run(clk, -1, true) == kError);
    PASS("unprogrammed-and-backend-error");
  }
  // 9. Deterministic 10k transfer fuzz with oracle.
  {
    uint64_t s = 0xDEADBEEF12345678ull;
    for (int i = 0; i < 10000; ++i) {
      s = s * 6364136223846793005ull + 1442695040888963407ull;
      uint64_t nchunks = 1u + (s % 32u);
      uint64_t len = nchunks * 256u;
      int fault = static_cast<int>((s >> 16) % (nchunks + 1u)) - 1;  // -1..n-1
      Transfer t;
      Transcfg c{Target::kCoherentSysmem, MemType::kPhysical};
      FakeClock clk;
      assert(t.program(c, kIovaBase, 0u, len) == kDone);
      Result r = t.run(clk, fault, false);
      if (fault < 0) {
        assert(r == kDone && t.bytesDone() == len);
      } else {
        assert(r == kTimeout && t.bytesDone() == static_cast<uint64_t>(fault) * 256u);
      }
    }
    PASS("fuzz-10k-oracle");
  }
    // 10. Align-up is real: odd-size alloc followed by 4K-align alloc.
  {
    Inventory inv;
    uint64_t io0 = 0u, io1 = 0u;
    assert(inv.reserve(Spec(BufKind::kFrts, 256, 256), &io0) == 0);
    assert(inv.reserve(Spec(BufKind::kRadixL0, 4096, 4096), &io1) == 1);
    assert(io1 > io0 && (io1 % 4096u) == 0u);
    PASS("align-up-enforced");
  }
  std::printf("FBDMA_GROUPS=10 PASS\n");
  return 0;
}
