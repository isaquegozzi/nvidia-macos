// test-channel.cpp — P77 M10 channel/runlist host-only model tests.
// Covers P77 §6 only where applicable to proven evidence. No HW, no I/O.
#include "channel_sim.hpp"
#include <cstdio>
#include <cstdint>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond) do { if (cond) { ++g_pass; } else { ++g_fail; \
  std::printf("FAIL %d: %s\n", __LINE__, #cond); } } while (0)

using namespace chsim;

static ChannelReq GoodReq(uint32_t chid) {
  ChannelReq r;
  r.chid = chid; r.runq = 0; r.priv = false; r.engine = Engine::kGr0;
  r.gpfifoOffset = 0x1000U; r.gpfifoLength = 0x2000U;  // 1024 entries
  r.vasHandle = 0xA1U; r.instSize = 0x4000U; r.userdSize = 0x1000U;
  r.instAddr = 0x10000000ULL + (uint64_t)chid * 0x10000ULL;
  r.userdAddr = 0x20000000ULL + (uint64_t)chid * 0x1000ULL;
  r.mthdbufAddr = 0x30000000ULL + (uint64_t)chid * 0x1000ULL;
  r.mthdbufSize = 0x1000U; r.runlId = 3;
  return r;
}
static FifoModel ReadyModel() {
  FifoModel m; m.NotifyInitDone(); m.NotifyStaticInfo(); return m;
}

int main() {
  // 1. happy path
  { FifoModel m = ReadyModel();
    CHECK(m.Alloc(GoodReq(7)) == Result::kOk);
    CHECK(m.StateOf(7) == ChanState::kAllocated);
    const Channel &c = m.Get(7);
    CHECK(c.userdPage == 0U && c.userdIndex == 7U);
    CHECK(c.entries == 0x2000U / 8U);
    CHECK(c.doorbell == ((uint32_t)3 << 16 | 7U));
    CHECK(c.inst.addrSpace == 2U && c.inst.cacheAttrib == 1U);
    CHECK(c.userd.addrSpace == 2U && c.userd.cacheAttrib == 1U);
    CHECK(c.ramfc.base == c.inst.base && c.ramfc.size == 512U);
    CHECK(c.ramfc.addrSpace == 2U && c.ramfc.cacheAttrib == 1U);
    CHECK(c.mthdbuf.addrSpace == 1U && c.mthdbuf.cacheAttrib == 0U);
    CHECK(m.Bind(7) == Result::kOk);
    CHECK(m.Schedule(7, true) == Result::kOk);
    CHECK(m.StateOf(7) == ChanState::kScheduled);
    CHECK(m.Schedule(7, false) == Result::kOk);
    CHECK(m.StateOf(7) == ChanState::kBound);
    CHECK(m.Free(7) == Result::kOk);
    CHECK(!m.IsLive(7) && m.LiveCount() == 0U);
    CHECK(m.Alloc(GoodReq(7)) == Result::kOk);  // reuse after free
  }
  // 2. invalid order
  { FifoModel m;
    CHECK(m.Alloc(GoodReq(1)) == Result::kInvalidOrder);
    m.NotifyInitDone();
    CHECK(m.Alloc(GoodReq(1)) == Result::kInvalidOrder);  // staticInfo missing
    m.NotifyStaticInfo();
    CHECK(m.Alloc(GoodReq(1)) == Result::kOk);
    CHECK(m.Schedule(1, true) == Result::kInvalidOrder);  // bind first
    CHECK(m.Bind(1) == Result::kOk);
    CHECK(m.Bind(1) == Result::kInvalidOrder);            // double bind
    CHECK(m.Schedule(1, false) == Result::kInvalidOrder); // not scheduled
    CHECK(m.Schedule(1, true) == Result::kOk);
    CHECK(m.Schedule(1, true) == Result::kInvalidOrder);  // double schedule
  }
  // 3. invalid chid
  { FifoModel m = ReadyModel();
    ChannelReq r = GoodReq(2048U);
    CHECK(m.Alloc(r) == Result::kInvalidChid);
    r.chid = 0xFFFFFFFFU;
    CHECK(m.Alloc(r) == Result::kInvalidChid);
    CHECK(m.Bind(2048U) == Result::kNotFound);
    CHECK(m.Free(2048U) == Result::kNotFound);
  }
  // 4. duplicate
  { FifoModel m = ReadyModel();
    CHECK(m.Alloc(GoodReq(9)) == Result::kOk);
    CHECK(m.Alloc(GoodReq(9)) == Result::kDuplicate);
  }
  // 5. field overflow
  { FifoModel m = ReadyModel();
    ChannelReq r = GoodReq(10);
    r.gpfifoLength = ((uint64_t)0xFFFFFFFFULL + 1ULL) << 3;  // entries = 2^32 -> InvalidSize
    CHECK(m.Alloc(r) == Result::kInvalidSize);
    r = GoodReq(11);
    r.instAddr = UINT64_MAX - 0xFFFULL; r.instSize = 0x4000U;  // wrap
    CHECK(m.Alloc(r) == Result::kOverflow);
    r = GoodReq(12);
    r.userdAddr = UINT64_MAX; r.userdSize = 0x1000U;
    CHECK(m.Alloc(r) == Result::kOverflow);
    uint64_t o = 0U;
    // ManagementOverhead overflow is PROVABLY UNREACHABLE for u64 fb:
    // max gb=ceil(2^64-1/2^30)=2^34, raw=2^34*96KiB=3*2^49 << 2^64, align never wraps.
    // Check retained in model as defense-in-depth (M7 precedent); no mutant claimed.
    CHECK(ManagementOverhead(UINT64_MAX, &o) == Result::kOk && o == 1688849860263936ULL);
    uint64_t w = 0U;
    CHECK(WprHeapSize(UINT64_MAX - 1U, 8U, 0U, 0U, &w) == Result::kOverflow);
  }
  // 6. alignment
  { FifoModel m = ReadyModel();
    ChannelReq r = GoodReq(13);
    r.gpfifoLength = 100U;  // %8 != 0
    CHECK(m.Alloc(r) == Result::kMisaligned);
    r = GoodReq(14); r.gpfifoOffset = 3U;
    CHECK(m.Alloc(r) == Result::kMisaligned);
    r = GoodReq(15); r.instSize = 0x1001U;  // %4K != 0
    CHECK(m.Alloc(r) == Result::kMisaligned);
    r = GoodReq(16); r.gpfifoLength = 0U;
    CHECK(m.Alloc(r) == Result::kInvalidSize);
  }
  // 7. runlist slot/range + USERD math + doorbell
  { FifoModel m = ReadyModel();
    ChannelReq r = GoodReq(17); r.runq = 2U;
    CHECK(m.Alloc(r) == Result::kInvalidRunq);
    r.runq = 1U; r.chid = 17U;
    CHECK(m.Alloc(r) == Result::kOk);
    CHECK(m.Get(17).userdPage == 2U && m.Get(17).userdIndex == 1U);  // 17/8=2,17%8=1
    CHECK(m.Alloc(GoodReq(0)) == Result::kOk);
    CHECK(m.Get(0).userdPage == 0U && m.Get(0).userdIndex == 0U);
    CHECK(m.Alloc(GoodReq(8)) == Result::kOk);
    CHECK(m.Get(8).userdPage == 1U && m.Get(8).userdIndex == 0U);
    ChannelReq e = GoodReq(2047U);
    CHECK(m.Alloc(e) == Result::kOk);
    CHECK(m.Get(2047).userdPage == 255U && m.Get(2047).userdIndex == 7U);
    CHECK(Doorbell(0x12U, 0x34U) == 0x00120034U);
    CHECK(m.RunlistBlock() == Result::kOk && m.RunlistAllow() == Result::kOk);
    CHECK(m.StateOf(17) == ChanState::kAllocated);  // no-ops changed nothing
    CHECK(m.StateOf(0) == ChanState::kAllocated);
    CHECK(m.StateOf(8) == ChanState::kAllocated);
    CHECK(m.StateOf(2047) == ChanState::kAllocated);
  }
  // 8. engine incompatible
  { FifoModel m = ReadyModel();
    ChannelReq r = GoodReq(20); r.engine = Engine::kInvalid;
    CHECK(m.Alloc(r) == Result::kInvalidEngine);
    for (int e = 0; e <= (int)Engine::kOfa; ++e) {
      FifoModel m2 = ReadyModel();
      ChannelReq r2 = GoodReq(30U + (uint32_t)e);
      if (30U + (uint32_t)e >= kMaxChannels) break;
      r2.engine = (Engine)e;
      CHECK(m2.Alloc(r2) == Result::kOk);
    }
  }
  // 9. free/unbind/reuse
  { FifoModel m = ReadyModel();
    CHECK(m.Free(31U) == Result::kNotFound);  // never allocated
    CHECK(m.Alloc(GoodReq(31)) == Result::kOk);
    CHECK(m.Free(31U) == Result::kOk);
    CHECK(m.Free(31U) == Result::kNotFound);  // double free
    CHECK(m.Bind(31U) == Result::kNotFound);  // bind after free
    CHECK(m.Alloc(GoodReq(31)) == Result::kOk);  // reuse ok
    CHECK(m.Bind(31U) == Result::kOk);
    CHECK(m.Free(31U) == Result::kOk);  // free from Bound ok
  }
  // 10. terminal/error state
  { FifoModel m = ReadyModel();
    CHECK(m.Alloc(GoodReq(40)) == Result::kOk);
    CHECK(m.Bind(40) == Result::kOk);
    CHECK(m.Schedule(40, true) == Result::kOk);
    CHECK(m.TriggerRc(40) == Result::kOk);
    CHECK(m.StateOf(40) == ChanState::kErrored);
    CHECK(m.Bind(40) == Result::kErrorPinned);
    CHECK(m.Schedule(40, true) == Result::kErrorPinned);
    CHECK(m.Alloc(GoodReq(40)) == Result::kErrorPinned);
    CHECK(m.TriggerRc(40) == Result::kErrorPinned);  // already pinned
    CHECK(m.Free(40) == Result::kOk);                // free clears error
    CHECK(m.Alloc(GoodReq(40)) == Result::kOk);      // realloc works
    CHECK(m.TriggerRc(9999U) == Result::kNotFound);
  }
  // 11. payload size/version (handles, sizes)
  { FifoModel m = ReadyModel();
    ChannelReq r = GoodReq(50); r.vasHandle = 0U;
    CHECK(m.Alloc(r) == Result::kInvalidHandle);
    r = GoodReq(51); r.instSize = 0U;
    CHECK(m.Alloc(r) == Result::kInvalidSize);
    r = GoodReq(52); r.userdSize = 0U;
    CHECK(m.Alloc(r) == Result::kInvalidSize);
    r = GoodReq(53); r.mthdbufSize = 0U;
    CHECK(m.Alloc(r) == Result::kInvalidSize);
  }
  // 12. unknown enum/function
  { CHECK(!IsKnownFunction(0U));
    CHECK(!IsKnownFunction(999U));
    CHECK(IsKnownFunction(kFuncGspRmAlloc) && IsKnownFunction(kFuncFree));
    CHECK(IsKnownFunction(kFuncAllocChannelDma));  // recognized (legacy)
    CHECK(IsChannelPathFunction(kFuncGspRmAlloc));
    CHECK(!IsChannelPathFunction(kFuncAllocChannelDma));  // NOT the GSP-RM path
    CHECK(!IsKnownEngine(Engine::kInvalid));
    CHECK(IsKnownEngine(Engine::kGr0) && IsKnownEngine(Engine::kOfa));
    Result o = Result::kOk;
    CHECK(RpcStatusToResult(0U, &o) == 0U && o == Result::kOk);
    CHECK(RpcStatusToResult(0x55U, &o) == 0x55U && o == Result::kBusy);
    CHECK(RpcStatusToResult(0x66U, &o) == 0x66U && o == Result::kBusy);
    CHECK(RpcStatusToResult(0x51U, &o) == 0x51U && o == Result::kNoMem);
    CHECK(RpcStatusToResult(0xDEADU, &o) == 0xDEADU && o == Result::kRejected);
  }
  // 13. heap constants
  { uint64_t ca = 0U;
    CHECK(ClientAllocSize(&ca) == Result::kOk && ca == 100663296ULL);
    uint64_t ov = 0U;
    CHECK(ManagementOverhead(0U, &ov) == Result::kOk && ov == 0U);
    CHECK(ManagementOverhead(kOneGb, &ov) == Result::kOk && ov == kOneMb);
    CHECK(ManagementOverhead(8ULL * kOneGb, &ov) == Result::kOk && ov == kOneMb);
    // 8 GiB FB: ceil=8 *96KiB = 768KiB -> align 1MiB
    uint64_t w = 0U;
    CHECK(WprHeapSize(kOsLibos3Bare, kBaseRmTu10x, ca, ov, &w) == Result::kOk);
    CHECK(w == kOsLibos3Bare + kBaseRmTu10x + ca + ov);
    CHECK(w >= kHeapMinLibos3Bare && w <= kHeapMaxLibos3Bare);
    uint64_t a = 0U;
    CHECK(AlignUpU64(1U, kOneMb, &a) && a == kOneMb);
    CHECK(!AlignUpU64(UINT64_MAX - 1U, kOneMb, &a));
  }
  // 14. deterministic sweep: all 2048 chids allocatable exactly once, then freed
  { FifoModel m = ReadyModel();
    for (uint32_t i = 0U; i < kMaxChannels; ++i) {
      ChannelReq r = GoodReq(i);
      r.instAddr = 0x40000000ULL + (uint64_t)i * 0x10000ULL;
      r.userdAddr = 0x50000000ULL + (uint64_t)i * 0x1000ULL;
      r.mthdbufAddr = 0x60000000ULL + (uint64_t)i * 0x1000ULL;
      if (!(m.Alloc(r) == Result::kOk)) { CHECK(false); break; }
    }
    CHECK(m.LiveCount() == kMaxChannels);
    CHECK(m.Alloc(GoodReq(0)) == Result::kDuplicate);
    for (uint32_t i = 0U; i < kMaxChannels; ++i) {
      if (!(m.Free(i) == Result::kOk)) { CHECK(false); break; }
    }
    CHECK(m.LiveCount() == 0U);
  }
  std::printf("channel tests: pass=%d fail=%d\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
