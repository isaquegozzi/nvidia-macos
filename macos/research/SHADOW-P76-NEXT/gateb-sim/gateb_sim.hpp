// gateb_sim.hpp v2 — M5 Gate B formal state machine (P76-DUAL-OVERNIGHT-V2).
// Isolated host-only model. Zero production wiring, zero real HW.
// Spec: G0004 §2 + G0006 M5 direction.
//
// Phases (terminal: Verified, PartialCommitFailure):
//   Idle -> CommitStarted -> HiCommitted -> LoCommitted -> ReadbackVerified
//        -> Verified | PartialCommitFailure
// HI-post failure (nothing visible yet) returns to Idle (safe retry).
// Any fault at/after HiCommitted (LO fail, read fail, mismatch) is terminal
// fail-closed: pinned page, zero rollback writes, no retry (Busy).
//
// Forbidden transitions (enforced + table-tested):
//   F1: commit from Verified|PartialCommitFailure (Busy, no retry)
//   F2: rollback writes on mismatch (no API exists; count==2 asserted)
//   F3: mid-window abort (stop sampled ONLY pre-commit; window is straight-line)
//   F4: unaligned commit (InvalidArg, zero writes)
//   F5: readback skip (compare is unconditional)
// Barriers: FakeMmio::barrier() after HI and after LO (pre-readback); counted.
#pragma once
#include <cstdint>
#include <vector>

namespace gateb {

inline constexpr uint32_t kHiOffset = 0x100c40u;
inline constexpr uint32_t kLoOffset = 0x100c10u;
inline constexpr uint64_t kAlignMask = 0xFFu;

enum Result : int32_t {
  kOk = 0,
  kInvalidArg = -1,
  kAborted = -2,
  kBusy = -3,
  kIoError = -4,
  kMismatch = -5,
};

enum class Phase : uint8_t {
  kIdle = 0,
  kCommitStarted = 1,
  kHiCommitted = 2,
  kLoCommitted = 3,
  kReadbackVerified = 4,  // transient: reads done, compare pending
  kVerified = 5,          // terminal success
  kPartialCommitFailure = 6,  // terminal fail-closed
};

inline bool IsTerminal(Phase p) {
  return p == Phase::kVerified || p == Phase::kPartialCommitFailure;
}

inline uint32_t EncodeHi(uint64_t dmaAddr) {
  return static_cast<uint32_t>((dmaAddr >> 40u) & 0xFFFFFFu);
}
inline uint32_t EncodeLo(uint64_t dmaAddr) {
  return static_cast<uint32_t>((dmaAddr >> 8u) & 0xFFFFFFFFu);
}
inline uint64_t Reconstruct(uint32_t hi, uint32_t lo) {
  return ((static_cast<uint64_t>(hi & 0xFFFFFFu)) << 40u) |
         (static_cast<uint64_t>(lo) << 8u);
}
inline bool IsAligned(uint64_t dmaAddr) { return (dmaAddr & kAlignMask) == 0u; }

struct FakeMmio {
  struct Write {
    uint32_t off;
    uint32_t val;
  };
  std::vector<Write> writes;
  uint32_t hiReg = 0u, loReg = 0u;
  uint32_t reads = 0u;
  uint32_t barriers = 0u;  // ops.memoryBarrier() call count
  bool dropNextWrite = false;
  uint32_t corruptNextReadMask = 0u;
  bool failWrites = false;

  void memoryBarrier() { ++barriers; }  // models OSSynchronizeIO placement
  bool write32(uint32_t off, uint32_t val) {
    if (failWrites) return false;
    if (dropNextWrite) {
      dropNextWrite = false;
      return true;
    }
    writes.push_back({off, val});
    if (off == kHiOffset) hiReg = val;
    else if (off == kLoOffset) loReg = val;
    return true;
  }
  bool read32(uint32_t off, uint32_t &out) {
    ++reads;
    if (off == kHiOffset) out = hiReg;
    else if (off == kLoOffset) out = loReg;
    else return false;
    if (corruptNextReadMask != 0u) {
      out ^= corruptNextReadMask;
      corruptNextReadMask = 0u;
    }
    return true;
  }
};

using PhaseProbe = void (*)(Phase);

class Transaction {
 public:
  Transaction() = default;
  Phase phase() const { return phase_; }
  bool pinned() const { return pinned_; }
  uint64_t committedAddr() const { return committedAddr_; }
  void setProbe(PhaseProbe p) { probe_ = p; }

  Result commit(FakeMmio &mmio, uint64_t dmaAddr, bool stopRequested) {
    if (phase_ != Phase::kIdle) return kBusy;  // F1 enforced
#ifdef MUT_UNALIGNED_ACCEPT
    (void)IsAligned;
#else
    if (!IsAligned(dmaAddr)) return kInvalidArg;  // F4 enforced
#endif
#ifdef MUT_ABORT_MID_WINDOW
    // MUTATION TEST-ONLY: pre-commit check deferred (forbidden mid-window abort below)
#else
    if (stopRequested) return kAborted;
#endif
    setPhase(Phase::kCommitStarted);
    const uint32_t hi = EncodeHi(dmaAddr);
    const uint32_t lo = EncodeLo(dmaAddr);
    // ---- non-abortable window: no stop checks, no allocs (F3) ----
#ifdef MUT_SWAP_ORDER
    const bool hiFirst = false;
    if (!mmio.write32(kLoOffset, lo)) return toIdle();
    mmio.memoryBarrier();
    setPhase(Phase::kLoCommitted);
    if (!mmio.write32(kHiOffset, hi)) return toPartial(dmaAddr, kIoError);
    mmio.memoryBarrier();
    setPhase(Phase::kHiCommitted);
#else
    const bool hiFirst = true;
    if (!mmio.write32(kHiOffset, hi)) return toIdle();  // nothing visible
    mmio.memoryBarrier();
    setPhase(Phase::kHiCommitted);
#ifdef MUT_ABORT_MID_WINDOW
    // MUTATION TEST-ONLY: forbidden mid-window abort (1 write posted, phase stuck)
    if (stopRequested) return kAborted;
#endif
    if (!mmio.write32(kLoOffset, lo)) return toPartial(dmaAddr, kIoError);
    mmio.memoryBarrier();
    setPhase(Phase::kLoCommitted);
#endif
    (void)hiFirst;
    uint32_t rhi = 0u, rlo = 0u;
    if (!mmio.read32(kHiOffset, rhi) || !mmio.read32(kLoOffset, rlo))
      return toPartial(dmaAddr, kIoError);
    setPhase(Phase::kReadbackVerified);
#ifdef MUT_SKIP_READBACK
    (void)rhi;
    (void)rlo;
#else
    if (rhi != hi || rlo != lo || Reconstruct(rhi, rlo) != dmaAddr) {
#ifdef MUT_ROLLBACK_ON_MISMATCH
      mmio.write32(kHiOffset, 0u);  // MUTATION TEST-ONLY: blind rollback
      mmio.write32(kLoOffset, 0u);
#endif
#ifdef MUT_RETRY_AFTER_FAILURE
      phase_ = Phase::kIdle;  // MUTATION TEST-ONLY: allows retry
      pinned_ = false;
      return kMismatch;
#else
      return toPartial(dmaAddr, kMismatch);  // F2: no rollback writes, pinned, terminal
#endif
    }
#endif
    phase_ = Phase::kVerified;
    if (probe_) probe_(phase_);
    pinned_ = true;
    committedAddr_ = dmaAddr;
    return kOk;
  }

  void resetForTestOnly() {
    phase_ = Phase::kIdle;
    pinned_ = false;
    committedAddr_ = 0u;
  }

 private:
  Phase phase_ = Phase::kIdle;
  bool pinned_ = false;
  uint64_t committedAddr_ = 0u;
  PhaseProbe probe_ = nullptr;

  void setPhase(Phase p) {
    phase_ = p;
    if (probe_) probe_(p);
  }
  Result toIdle() {
    phase_ = Phase::kIdle;
    if (probe_) probe_(phase_);
    return kIoError;
  }
  Result toPartial(uint64_t dmaAddr, Result r) {
    phase_ = Phase::kPartialCommitFailure;
    if (probe_) probe_(phase_);
    pinned_ = true;
    committedAddr_ = dmaAddr;
    return r;
  }
};

}  // namespace gateb
