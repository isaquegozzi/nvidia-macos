// fbdma_sim.hpp — M7 First-DMA/Falcon-FBDMA host-only model (P76-DUAL-OVERNIGHT-V2).
// Isolated. Zero production wiring, zero real DMA. Spec: G0004 section 4 + G0008 M7.
// Engine: 256B chunks; TRANSCFG{target=CoherentSysmem, memtype=Physical};
// regs DMATRFBASE/DMATRFMOFFS/DMATRFFBOFFS/DMATRFCMD; bounded 2s/chunk fake-clock poll.
#pragma once
#include <cstdint>
#include <vector>

namespace fbdma {

inline constexpr uint64_t kChunk = 256u;
inline constexpr uint64_t kChunkBudgetMs = 2000u;
inline constexpr uint64_t kIovaBase = 0x10000000ull;
inline constexpr uint64_t kIovaGrain = 256u;

enum class BufKind : uint8_t {
  kFlushPage = 0, kFrts = 1, kWpr2 = 2, kRadixL0 = 3, kRadixL1 = 4,
  kRadixL2 = 5, kBootloader = 6, kLibOs = 7, kRpcCmdQ = 8, kRpcStatusQ = 9
};
struct BufSpec {
  BufKind kind;
  uint64_t size;
  uint64_t align;
};
struct BufAlloc {
  BufSpec spec;
  uint64_t iova;
};

enum class Target : uint8_t { kCoherentSysmem = 0, kDeviceLocal = 1 };
enum class MemType : uint8_t { kPhysical = 0, kVirtual = 1 };
struct Transcfg {
  Target target;
  MemType memtype;
};

enum Result : int32_t {
  kDone = 0,
  kTimeout = -1,     // chunk poll exhausted; bytesDone accounts completed chunks
  kInvalidArg = -2,  // bad size/align/target/memtype/overlap/unprogrammed
  kOverlap = -3,
  kError = -4,       // backend fault
};

struct FakeClock {
  uint64_t nowMs = 0u;
  void advance(uint64_t ms) { nowMs += ms; }
};

struct DmaRegs {
  uint64_t trfBase = 0u;   // DMATRFBASE: source IOVA
  uint32_t trfMoffs = 0u;  // DMATRFMOFFS: dest FB offset
  uint32_t trfFboffs = 0u; // DMATRFFBOFFS: dest FB offset mirror (model checks equal)
  uint32_t trfCmd = 0u;    // DMATRFCMD: bit0 GO
};

class Inventory {
 public:
  Inventory() : nextIova_(kIovaBase) {}
  // Returns index or -1. Enforces size>0, align>=256, align pow2, overlap-free.
  int reserve(const BufSpec &spec, uint64_t *outIova) {
    if (spec.size == 0u) return -1;
    if (spec.align < kIovaGrain) return -1;
    if ((spec.align & (spec.align - 1u)) != 0u) return -1;  // pow2 required
#ifdef MUT_SKIP_ALIGN_CHECK
    uint64_t base = nextIova_;
#else
    uint64_t base = (nextIova_ + spec.align - 1u) & ~(spec.align - 1u);
#endif
    if (base < nextIova_) return -1;  // R8 (M0041): align-up wrapped: space exhausted
    uint64_t end = base + spec.size;
    if (end < base) return -1;  // R8 (M0041): size addition wrapped; strict tightening
    for (auto &b : bufs_) {
      uint64_t oEnd = b.iova + b.spec.size;
      // Defense-in-depth: unreachable with the bump allocator, kept for
      // future non-monotonic allocators. No mutant (unobservable by design).
      if (base < oEnd && b.iova < end) return -1;  // overlap
    }
    bufs_.push_back({spec, base});
    nextIova_ = end;
    if (outIova) *outIova = base;
    return static_cast<int>(bufs_.size()) - 1;
  }
  size_t size() const { return bufs_.size(); }
  const BufAlloc &at(size_t i) const { return bufs_[i]; }

 private:
  std::vector<BufAlloc> bufs_;
  uint64_t nextIova_;
};

class Transfer {
 public:
  Transfer() = default;
  // Program descriptor. moffs must equal fboffs (mirror check).
  Result program(const Transcfg &cfg, uint64_t srcIova, uint32_t dstOff,
                 uint64_t len) {
#ifdef MUT_ZERO_LEN_OK
    if ((len % kChunk) != 0u) return kInvalidArg;  // MUTATION: zero-len accepted
#else
    if (len == 0u || (len % kChunk) != 0u) return kInvalidArg;
#endif
    if ((srcIova % kIovaGrain) != 0u) return kInvalidArg;
#ifdef MUT_WRONG_TARGET_OK
    (void)cfg;
#else
    if (cfg.target != Target::kCoherentSysmem) return kInvalidArg;
    if (cfg.memtype != MemType::kPhysical) return kInvalidArg;
#endif
    regs_.trfBase = srcIova;
    regs_.trfMoffs = dstOff;
    regs_.trfFboffs = dstOff;
    regs_.trfCmd = 1u;  // GO
    total_ = len;
    done_ = 0u;
    programmed_ = true;
    return kDone;  // program-ack (not completion)
  }
  bool programmed() const { return programmed_; }
  uint64_t bytesDone() const { return done_; }
  const DmaRegs &regs() const { return regs_; }
  // Run to completion against fake clock. faultChunk>=0 stalls that chunk past
  // budget -> Timeout with bytesDone accounting. backendFail -> Error.
  Result run(FakeClock &clk, int faultChunk, bool backendFail) {
    if (!programmed_) return kInvalidArg;
    if (backendFail) return kError;
#ifdef MUT_CHUNK_SIZE_MISMATCH
    const uint64_t chunk = 512u;  // MUTATION TEST-ONLY: wrong granularity
#else
    const uint64_t chunk = kChunk;
#endif
    uint64_t n = total_ / chunk;
    for (uint64_t c = 0u; c < n; ++c) {
      uint64_t spent = 0u;
      while (spent < kChunkBudgetMs) {
        clk.advance(100u);
        spent += 100u;
#ifdef MUT_IGNORE_TIMEOUT
        (void)c;
#else
        if (faultChunk >= 0 && c == static_cast<uint64_t>(faultChunk)) continue;  // never completes
#endif
        break;  // chunk completes within first quantum
      }
#ifndef MUT_IGNORE_TIMEOUT
      if (faultChunk >= 0 && c == static_cast<uint64_t>(faultChunk)) return kTimeout;
#endif
      done_ += chunk;
    }
    programmed_ = false;
    return kDone;
  }

 private:
  DmaRegs regs_;
  uint64_t total_ = 0u;
  uint64_t done_ = 0u;
  bool programmed_ = false;
};

}  // namespace fbdma
