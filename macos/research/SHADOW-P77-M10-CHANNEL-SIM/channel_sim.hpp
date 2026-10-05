// channel_sim.hpp — P77 M10 channel/runlist host-only model (scoped, grounded).
// Zero production wiring. Zero MMIO/PCI/BME/DMA/GSP/firmware/VRAM/interrupts/IOKit.
// All memory/addresses synthetic (u64 tokens, never dereferenced).
// Semantics from P77-M10-CHANNEL-EVIDENCE (r535 GSP-RM channel path, OpenRM heap,
// Nouveau FIFO lifetime, Nova sequencing). No code copied; behavior reimplemented.
// NOT-MODELED by design: runlist ENTRY bytes (firmware-private on GSP path),
// NV2080_ENGINE_TYPE numeric encodings (opaque validated tokens), inst/userd byte
// counts (opaque positive params), GPFIFO/pushbuffer emission, fault/GR ctx contents.
#pragma once
#include <cstdint>

namespace chsim {

inline constexpr uint64_t kGspHeapAlign = 1048576ULL;            // 1<<20 (Nova fw.rs:66)
inline constexpr uint64_t kBaseRmTu10x = 8388608ULL;            // 8<<20 (gsp_fw_heap.h)
inline constexpr uint64_t kClientAllocRaw = 100663296ULL;       // (48<<10)*2048
inline constexpr uint64_t kSizePerGb = 98304ULL;                // 96<<10
inline constexpr uint64_t kOsLibos3Bare = 23068672ULL;          // 22<<20 (GA106 path)
inline constexpr uint64_t kHeapMinLibos3Bare = 92274688ULL;     // 88<<20
inline constexpr uint64_t kHeapMaxLibos3Bare = 293601280ULL;    // 280<<20
inline constexpr uint64_t kOneGb = 1073741824ULL;
inline constexpr uint64_t kOneMb = 1048576ULL;

inline constexpr uint32_t kMaxChannels = 2048U;                 // Ampere+ (heap comment)
inline constexpr uint32_t kChidPerUserd = 8U;                   // r535 CHID_PER_USERD
inline constexpr uint64_t kRamfcSize = 512ULL;                  // 0x200 (r535 alloc)
inline constexpr uint64_t kGpfifoEntrySize = 8ULL;              // len/8 (r535 alloc)
inline constexpr uint32_t kRunqs = 2U;                          // tu102 runqs (direct corroboration)
inline constexpr uint32_t kInstAlign = 4096U;                   // chan.c gpuobj 0x1000

inline constexpr uint32_t kClassAmpereGpfifoA = 0x0000c56fU;    // nvif/class.h:89
inline constexpr uint32_t kFuncAllocChannelDma = 6U;            // legacy, recognized (rpcfn)
inline constexpr uint32_t kFuncFree = 10U;                      // rpcfn
inline constexpr uint32_t kFuncContinuation = 71U;              // rpcfn
inline constexpr uint32_t kFuncGspSetSystemInfo = 72U;          // rpcfn
inline constexpr uint32_t kFuncGspRmControl = 76U;              // rpcfn
inline constexpr uint32_t kFuncGetGspStaticInfo = 65U;          // rpcfn
inline constexpr uint32_t kFuncGspRmAlloc = 103U;               // channel path (alloc.c)
inline constexpr uint32_t kEventGspInitDone = 0x1001U;          // msgfn
inline constexpr uint32_t kCtrlBind = 0xa06f0104U;              // nvrm/fifo.h
inline constexpr uint32_t kCtrlSchedule = 0xa06f0103U;          // nvrm/fifo.h

enum class Result : int32_t {
  kOk = 0,
  kInvalidOrder = -1,   // sequencing violation (e.g. alloc before RpcReady, bind before alloc)
  kInvalidChid = -2,    // chid out of [0,2048)
  kDuplicate = -3,      // chid already live
  kInvalidRunq = -4,    // runq not in {0,1}
  kInvalidEngine = -5,  // unknown engine token
  kMisaligned = -6,     // gpfifo len/offset, inst alignment
  kOverflow = -7,       // entries/size/heap arithmetic overflow
  kInvalidSize = -8,    // zero sizes, entries==0
  kInvalidHandle = -9,  // zero VASpace handle
  kUnknownFunction = -10,
  kErrorPinned = -11,   // errored channel refuses ops until free
  kNotFound = -12,      // no live channel at chid
  kBusy = -13,          // rpc status EBUSY class (0x55/0x66)
  kNoMem = -14,         // rpc status ENOMEM class (0x51)
  kRejected = -15,      // generic RM rejection (EINVAL class)
};

enum class Engine : int32_t {
  kGr0 = 0,
  kCopy0, kCopy1, kCopy2, kCopy3, kCopy4, kCopy5, kCopy6, kCopy7,
  kNvdec0, kNvdec1, kNvdec2, kNvdec3, kNvdec4, kNvdec5, kNvdec6, kNvdec7,
  kNvenc0, kNvenc1, kNvenc2, kNvenc3,
  kNvjpg0, kNvjpg1, kNvjpg2, kNvjpg3, kNvjpg4, kNvjpg5, kNvjpg6, kNvjpg7,
  kSw, kSec2, kOfa,
  kInvalid = -1,
};

inline bool IsKnownEngine(Engine e) {
#ifdef MUT_UNKNOWN_ENGINE_OK
  (void)e;
  return true;  // MUTATION TEST-ONLY: every engine token accepted
#else
  switch (e) {
    case Engine::kGr0:
    case Engine::kCopy0: case Engine::kCopy1: case Engine::kCopy2: case Engine::kCopy3:
    case Engine::kCopy4: case Engine::kCopy5: case Engine::kCopy6: case Engine::kCopy7:
    case Engine::kNvdec0: case Engine::kNvdec1: case Engine::kNvdec2: case Engine::kNvdec3:
    case Engine::kNvdec4: case Engine::kNvdec5: case Engine::kNvdec6: case Engine::kNvdec7:
    case Engine::kNvenc0: case Engine::kNvenc1: case Engine::kNvenc2: case Engine::kNvenc3:
    case Engine::kNvjpg0: case Engine::kNvjpg1: case Engine::kNvjpg2: case Engine::kNvjpg3:
    case Engine::kNvjpg4: case Engine::kNvjpg5: case Engine::kNvjpg6: case Engine::kNvjpg7:
    case Engine::kSw: case Engine::kSec2: case Engine::kOfa:
      return true;
    default:
      return false;
  }
#endif
}

inline bool IsKnownFunction(uint32_t fn) {
#ifdef MUT_UNKNOWN_FUNCTION_OK
  (void)fn;
  return true;  // MUTATION TEST-ONLY: every function code recognized
#else
  switch (fn) {
    case kFuncAllocChannelDma: case kFuncFree: case kFuncContinuation:
    case kFuncGspSetSystemInfo: case kFuncGspRmControl: case kFuncGetGspStaticInfo:
    case kFuncGspRmAlloc: case kEventGspInitDone:
      return true;
    default:
      return false;
  }
#endif
}

// The GSP-RM channel path uses GSP_RM_ALLOC (103), NOT legacy ALLOC_CHANNEL_DMA (6).
inline bool IsChannelPathFunction(uint32_t fn) { return fn == kFuncGspRmAlloc; }

inline uint32_t RpcStatusToResult(uint32_t rpcStatus, Result *out) {
  // Mirrors r535_rpc_status_to_errno classes without inventing new codes.
  if (rpcStatus == 0U) { *out = Result::kOk; return 0U; }
  if (rpcStatus == 0x55U || rpcStatus == 0x66U) { *out = Result::kBusy; return rpcStatus; }
  if (rpcStatus == 0x51U) { *out = Result::kNoMem; return rpcStatus; }
  *out = Result::kRejected;
  return rpcStatus;
}

enum class ChanState : uint8_t { kFree = 0, kAllocated, kBound, kScheduled, kErrored };

struct MemDesc {
  uint64_t base = 0U;
  uint64_t size = 0U;
  uint32_t addrSpace = 0U;   // proven: 2 = inst/userd/ramfc, 1 = mthdbuf
  uint32_t cacheAttrib = 0U; // proven: 1 = inst/userd/ramfc, 0 = mthdbuf
};

struct ChannelReq {
  uint32_t chid = 0U;
  uint32_t runq = 0U;
  bool priv = false;
  Engine engine = Engine::kInvalid;
  uint64_t gpfifoOffset = 0U;
  uint64_t gpfifoLength = 0U;  // bytes; entries = length/8
  uint32_t vasHandle = 0U;     // opaque nonzero handle
  uint64_t instSize = 0U;      // opaque positive, 4K-aligned
  uint64_t userdSize = 0U;     // opaque positive
  uint64_t instAddr = 0U;      // synthetic token
  uint64_t userdAddr = 0U;     // synthetic token
  uint64_t mthdbufAddr = 0U;   // synthetic token
  uint64_t mthdbufSize = 0U;   // from CE_GET_FAULT_METHOD_BUFFER_SIZE ctrl
  uint16_t runlId = 0U;        // for doorbell composition
};

struct Channel {
  bool live = false;
  ChanState state = ChanState::kFree;
  ChannelReq req;
  uint32_t userdPage = 0U;    // chid/8
  uint32_t userdIndex = 0U;   // chid%8
  uint32_t entries = 0U;      // gpfifoLength/8
  uint32_t doorbell = 0U;     // (runlId<<16)|chid
  MemDesc inst, userd, ramfc, mthdbuf;
};

inline uint32_t Doorbell(uint16_t runlId, uint32_t chid) {
  return (static_cast<uint32_t>(runlId) << 16) | (chid & 0xFFFFU);
}

inline bool AddOverflowsU64(uint64_t a, uint64_t b) { return a > UINT64_MAX - b; }

inline bool AlignUpU64(uint64_t v, uint64_t align, uint64_t *out) {
  if (align == 0U || (align & (align - 1U)) != 0U) return false;
  uint64_t mask = align - 1U;
  if (v > UINT64_MAX - mask) return false;
  *out = (v + mask) & ~mask;
  return true;
}

inline Result ClientAllocSize(uint64_t *out) {
  // align_up((48KiB*2048), 1MiB) = 96MiB (already aligned; checked anyway).
  if (!AlignUpU64(kClientAllocRaw, kGspHeapAlign, out)) return Result::kOverflow;
  return Result::kOk;
}

inline Result ManagementOverhead(uint64_t fbSize, uint64_t *out) {
  // ceil(fb/1G) * 96KiB, aligned to 1MiB. fb==0 -> 0 overhead (no FB).
  uint64_t gb = fbSize / kOneGb + ((fbSize % kOneGb) != 0U ? 1U : 0U);
  if (gb != 0U && kSizePerGb > UINT64_MAX / gb) return Result::kOverflow;
  uint64_t raw = gb * kSizePerGb;
  if (!AlignUpU64(raw, kGspHeapAlign, out)) return Result::kOverflow;
  return Result::kOk;
}

inline Result WprHeapSize(uint64_t carveout, uint64_t baseRm, uint64_t clientAlloc,
                          uint64_t overhead, uint64_t *out) {
  // Checked add (Nova uses saturating_add; model is stricter and reports Overflow).
  if (AddOverflowsU64(carveout, baseRm)) return Result::kOverflow;
  uint64_t s = carveout + baseRm;
  if (AddOverflowsU64(s, clientAlloc)) return Result::kOverflow;
  s += clientAlloc;
  if (AddOverflowsU64(s, overhead)) return Result::kOverflow;
  s += overhead;
  *out = s;
  return Result::kOk;
}

class FifoModel {
 public:
  FifoModel() {
    for (uint32_t i = 0U; i < kMaxChannels; ++i) chans_[i].live = false;
  }

  void NotifyInitDone() { initDone_ = true; }
  void NotifyStaticInfo() { staticInfo_ = true; }
  bool RpcReady() const { return initDone_ && staticInfo_; }

  uint32_t LiveCount() const { return liveCount_; }

  // No-op runlist block/allow (r535_runl): always Ok, never changes channel state.
  Result RunlistBlock() {
#ifdef MUT_BLOCK_CHANGES_STATE
    if (liveCount_ > 0U) chans_[0].state = ChanState::kBound;  // MUTATION TEST-ONLY
#endif
    return Result::kOk;
  }
  Result RunlistAllow() {
#ifdef MUT_ALLOW_CHANGES_STATE
    if (liveCount_ > 0U) chans_[0].state = ChanState::kScheduled;  // MUTATION TEST-ONLY
#endif
    return Result::kOk;
  }

  Result Alloc(const ChannelReq &r) {
#ifdef MUT_SKIP_RPC_READY
    (void)0;  // MUTATION TEST-ONLY: alloc allowed before RpcReady
#else
    if (!RpcReady()) return Result::kInvalidOrder;
#endif
#ifdef MUT_SKIP_CHID_RANGE
    (void)0;
#else
    if (r.chid >= kMaxChannels) return Result::kInvalidChid;
#endif
    if (chans_[r.chid].live) {
      return chans_[r.chid].state == ChanState::kErrored ? Result::kErrorPinned
                                                        : Result::kDuplicate;
    }
#ifdef MUT_SKIP_RUNQ
    (void)0;
#else
    if (r.runq >= kRunqs) return Result::kInvalidRunq;
#endif
    if (!IsKnownEngine(r.engine)) return Result::kInvalidEngine;
    if (r.vasHandle == 0U) return Result::kInvalidHandle;
#ifdef MUT_SKIP_GPFIFO_ALIGN
    (void)0;
#else
    if (r.gpfifoLength == 0U) return Result::kInvalidSize;
    if (r.gpfifoLength % kGpfifoEntrySize != 0U) return Result::kMisaligned;
    if (r.gpfifoOffset % kGpfifoEntrySize != 0U) return Result::kMisaligned;
#endif
    uint64_t entries64 = r.gpfifoLength / kGpfifoEntrySize;
    if (entries64 == 0U || entries64 > UINT32_MAX) return Result::kInvalidSize;
    if (r.instSize == 0U || r.userdSize == 0U || r.mthdbufSize == 0U) return Result::kInvalidSize;
#ifdef MUT_SKIP_INST_ALIGN
    (void)0;
#else
    if (r.instSize % kInstAlign != 0U) return Result::kMisaligned;
#endif
    // Synthetic descriptor overflow checks (base+size wrap).
    if (AddOverflowsU64(r.instAddr, r.instSize)) return Result::kOverflow;
    if (AddOverflowsU64(r.userdAddr, r.userdSize)) return Result::kOverflow;
    if (AddOverflowsU64(r.mthdbufAddr, r.mthdbufSize)) return Result::kOverflow;
    // ramfc aliases instance base with fixed 0x200 size: must fit in u64 (always) and
    // must not exceed... (no upper bound proven; only wrap check).
    if (AddOverflowsU64(r.instAddr, kRamfcSize)) return Result::kOverflow;

    Channel &c = chans_[r.chid];
    c.live = true;
    c.state = ChanState::kAllocated;
    c.req = r;
    c.userdPage = r.chid / kChidPerUserd;
    c.userdIndex = r.chid % kChidPerUserd;
    c.entries = static_cast<uint32_t>(entries64);
    c.doorbell = Doorbell(r.runlId, r.chid);
    c.inst = MemDesc{r.instAddr, r.instSize, 2U, 1U};
    c.userd = MemDesc{r.userdAddr, r.userdSize, 2U, 1U};
    c.ramfc = MemDesc{r.instAddr, kRamfcSize, 2U, 1U};
    c.mthdbuf = MemDesc{r.mthdbufAddr, r.mthdbufSize, 1U, 0U};
    liveCount_ += 1U;
    return Result::kOk;
  }

  Result Bind(uint32_t chid) {
    if (chid >= kMaxChannels || !chans_[chid].live) return Result::kNotFound;
    Channel &c = chans_[chid];
    if (c.state == ChanState::kErrored) return Result::kErrorPinned;
#ifdef MUT_BIND_SKIP_STATE
    (void)0;  // MUTATION TEST-ONLY: bind from any state
#else
    if (c.state != ChanState::kAllocated) return Result::kInvalidOrder;
#endif
    c.state = ChanState::kBound;
    return Result::kOk;
  }

  Result Schedule(uint32_t chid, bool enable) {
    if (chid >= kMaxChannels || !chans_[chid].live) return Result::kNotFound;
    Channel &c = chans_[chid];
    if (c.state == ChanState::kErrored) return Result::kErrorPinned;
    if (enable) {
#ifdef MUT_SCHED_SKIP_STATE
      (void)0;  // MUTATION TEST-ONLY: schedule-enable from any state
#else
      if (c.state != ChanState::kBound) return Result::kInvalidOrder;
#endif
      c.state = ChanState::kScheduled;
      return Result::kOk;
    }
#ifdef MUT_UNSCHED_SKIP_STATE
    (void)0;
#else
    if (c.state != ChanState::kScheduled) return Result::kInvalidOrder;
#endif
    c.state = ChanState::kBound;
    return Result::kOk;
  }

  Result TriggerRc(uint32_t chid) {
    if (chid >= kMaxChannels || !chans_[chid].live) return Result::kNotFound;
    Channel &c = chans_[chid];
    if (c.state == ChanState::kErrored) return Result::kErrorPinned;
    c.state = ChanState::kErrored;
    return Result::kOk;
  }

  Result Free(uint32_t chid) {
    if (chid >= kMaxChannels || !chans_[chid].live) return Result::kNotFound;
    Channel &c = chans_[chid];
    c.live = false;
    c.state = ChanState::kFree;
    c.entries = 0U;
    liveCount_ -= 1U;
    return Result::kOk;
  }

  bool IsLive(uint32_t chid) const {
    return chid < kMaxChannels && chans_[chid].live;
  }
  ChanState StateOf(uint32_t chid) const {
    return chid < kMaxChannels ? chans_[chid].state : ChanState::kFree;
  }
  const Channel &Get(uint32_t chid) const { return chans_[chid]; }

 private:
  Channel chans_[kMaxChannels];
  uint32_t liveCount_ = 0U;
  bool initDone_ = false;
  bool staticInfo_ = false;
};

}  // namespace chsim
