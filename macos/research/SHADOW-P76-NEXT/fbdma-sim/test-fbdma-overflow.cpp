// test-fbdma-overflow.cpp — R8 finding-closure addendum (M0041). Additive only;
// exercises fbdma_sim.hpp read-only. Hostile sizes must be rejected; the allocator
// must remain usable afterwards. Deterministic, no HW.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "fbdma_sim.hpp"

using namespace fbdma;
static int gGroups = 0;
#define PASS(m) do { std::printf("PASS(%s)\n", m); gGroups++; } while (0)

int main() {
  {  // 1. wrapping sizes rejected on a fresh inventory
    Inventory inv;
    uint64_t out = 0u;
    assert(inv.reserve({BufKind::kFlushPage, 0xFFFFFFFFFFFFFFFFull, 256u}, &out) == -1);
    assert(inv.reserve({BufKind::kFrts, 0xFFFFFFFFF0000000ull, 256u}, &out) == -1 /* base+size == 2^64 -> wraps to 0 */);
    assert(inv.size() == 0u);
    PASS("wrap-sizes-rejected-empty-stays-empty");
  }
  {  // 2. exact-boundary: end == 2^64 wraps to 0 -> rejected; end == 2^64-1 accepted
    Inventory inv;
    uint64_t out = 0u;
    uint64_t toWrap = 0xFFFFFFFFFFFFFFFFull - 0x10000000ull + 1u;  // base+size == 2^64
    assert(inv.reserve({BufKind::kWpr2, toWrap, 256u}, &out) == -1);
    uint64_t maxOk = 0xFFFFFFFFFFFFFFFFull - 0x10000000ull;  // end == 2^64-1, no wrap
    assert(inv.reserve({BufKind::kWpr2, maxOk, 256u}, &out) == 0);
    assert(out == 0x10000000u);
    // Allocator now sits at the top; any further reserve must fail closed, not wrap.
    assert(inv.reserve({BufKind::kFrts, 0x1000u, 256u}, &out) == -1);
    assert(inv.size() == 1u);
    PASS("boundary-wrap-rejected-top-pinned-fail-closed");
  }
  {  // 3. allocator fully usable after rejections; results stay disjoint
    Inventory inv;
    uint64_t out = 0u;
    assert(inv.reserve({BufKind::kFlushPage, 0xFFFFFFFFFFFFFFFFull, 256u}, &out) == -1);
    assert(inv.reserve({BufKind::kFlushPage, 0u, 256u}, &out) == -1);  // zero size
    assert(inv.reserve({BufKind::kFlushPage, 0x1000u, 128u}, &out) == -1);  // align<256
    assert(inv.reserve({BufKind::kFlushPage, 0x1000u, 0x300u}, &out) == -1);  // non-pow2
    assert(inv.reserve({BufKind::kFlushPage, 0x1000u, 0x1000u}, &out) == 0);
    assert(inv.reserve({BufKind::kFrts, 0x1000u, 0x1000u}, &out) == 1);
    assert(inv.reserve({BufKind::kWpr2, 0x20000u, 256u}, &out) == 2);
    for (size_t i = 0u; i < inv.size(); ++i)
      for (size_t j = i + 1u; j < inv.size(); ++j) {
        uint64_t ae = inv.at(i).iova + inv.at(i).spec.size;
        uint64_t be = inv.at(j).iova + inv.at(j).spec.size;
        assert(inv.at(i).iova >= be || inv.at(j).iova >= ae);
      }
    PASS("rejects-then-clean-disjoint");
  }
  std::printf("FBDMA_OVERFLOW_GROUPS=%d PASS\n", gGroups);
  return gGroups == 3 ? 0 : 1;
}
