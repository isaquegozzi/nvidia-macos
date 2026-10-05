// fwsec_sim.hpp — M8 FWSEC/FRTS/WPR2 sequencing host-only model (P76-DUAL-OVERNIGHT-V2).
// Isolated. Zero production wiring, zero real FW/DMA. Spec: G0004 §4 tail + M8-FWSEC-PLAN.md.
// PROVISIONAL: built against FROZEN M7 (fbdma_sim.hpp e33f8c20…); self-contained range
// vocabulary with kind mapping to M7 BufKind (see KindMap comment); R8 overflow guard
// included from the start (NoWrap). No auto-retry: Failed/Done are terminal for the object.
#pragma once
#include <cstdint>

namespace fwsec {

// Kind mapping to frozen M7 inventory: FRTS<-kFrts, WPR2<-kWpr2, RADIX<-kRadixL0/L1/L2,
// SIG<-staged signatures blob (M7 bootloader-adjacent), BOOT<-kBootloader, LIBOS<-kLibOs.
inline constexpr uint64_t kGrain = 256u;
inline constexpr uint64_t kStageBudgetMs = 2000u;
inline constexpr uint64_t kQuantumMs = 100u;

enum class Result : int32_t {
  kOk = 0,
  kBadMeta = -1,        // bad size/align/range-outside-image/unwrapped arithmetic
  kOverlap = -2,        // image/image or meta/meta range intersection
  kOutOfOrder = -3,     // stage precondition not met
  kAlreadyTerminal = -4,// call after Done/Failed (no auto-retry, no auto-reuse)
  kTimeout = -5,        // stage poll exhausted; object terminal Failed
};

enum class Stage : uint8_t {
  kUnloaded = 0,
  kFrtsStaged = 1,
  kWpr2Valid = 2,
  kTablesBuilt = 3,
  kBootloaderReady = 4,
  kBooting = 5,
  kDone = 6,
  kFailed = 7,
};

enum class FailAt : uint8_t { kNone = 0, kBoot = 1 };

struct Range {
  uint64_t base = 0u;
  uint64_t size = 0u;
};

struct Wpr2Meta {
  Range radix;
  Range sig;
  Range boot;
  uint64_t frtsDma = 0u;
  uint64_t wpr2Dma = 0u;
};

struct FakeClock {
  uint64_t nowMs = 0u;
  void advance(uint64_t ms) { nowMs += ms; }
};

inline bool NoWrap(uint64_t base, uint64_t size) {
  return size > 0u && base + size > base;  // R8 guard: rejects wrap + zero size
}
inline bool Aligned256(uint64_t v) { return (v % kGrain) == 0u; }
inline bool Disjoint(const Range &a, const Range &b) {
  return a.base + a.size <= b.base || b.base + b.size <= a.base;
}
inline bool Inside(const Range &inner, const Range &outer) {
  return inner.base >= outer.base &&
         inner.base + inner.size <= outer.base + outer.size;
}

class Sequencer {
 public:
  Sequencer() = default;
  Stage stage() const { return stage_; }
  Result lastResult() const { return last_; }
  FailAt failedAt() const { return failedAt_; }
  uint64_t bootTicks() const { return bootTicks_; }

  // Atomically stage the five images; all must be valid + pairwise disjoint.
  Result stageImages(const Range &frts, const Range &wpr2, const Range &radix,
                     const Range &sig, const Range &boot) {
    if (stage_ != Stage::kUnloaded) return setLast(Result::kOutOfOrder);
    const Range *imgs[5] = {&frts, &wpr2, &radix, &sig, &boot};
    for (int i = 0; i < 5; ++i) {
      if (!NoWrap(imgs[i]->base, imgs[i]->size) || !Aligned256(imgs[i]->base))
        return setLast(Result::kBadMeta);
    }
    for (int i = 0; i < 5; ++i)
      for (int j = i + 1; j < 5; ++j)
        if (!Disjoint(*imgs[i], *imgs[j])) return setLast(Result::kOverlap);
    frts_ = frts;
    wpr2_ = wpr2;
    radix_ = radix;
    sig_ = sig;
    boot_ = boot;
    stage_ = Stage::kFrtsStaged;
    return setLast(Result::kOk);
  }

  Result validateWpr2(const Wpr2Meta &m) {
    if (stage_ != Stage::kFrtsStaged) return setLast(Result::kOutOfOrder);
#ifdef MUT_SKIP_META_CHECK
    (void)m;
#else
    const Range *meta[3] = {&m.radix, &m.sig, &m.boot};
    const Range *imgs[3] = {&radix_, &sig_, &boot_};
    for (int i = 0; i < 3; ++i) {
      if (!NoWrap(meta[i]->base, meta[i]->size) || !Aligned256(meta[i]->base))
        return setLast(Result::kBadMeta);
      if (!Inside(*meta[i], *imgs[i])) return setLast(Result::kBadMeta);
    }
// Defense-in-depth: unreachable through the public API (staged images are
    // pairwise disjoint and every meta range must sit Inside its image, so two
    // meta ranges cannot intersect). Kept for future non-disjoint layouts.
    // No mutant: unobservable by design (M7-overlap precedent).
    for (int i = 0; i < 3; ++i)
      for (int j = i + 1; j < 3; ++j)
        if (!Disjoint(*meta[i], *meta[j])) return setLast(Result::kOverlap);
#ifdef MUT_SKIP_DMA_ALIGN_CHECK
    (void)0;
#else
    if (!Aligned256(m.frtsDma) || !Aligned256(m.wpr2Dma))
      return setLast(Result::kBadMeta);
#endif
#endif
    meta_ = m;
    stage_ = Stage::kWpr2Valid;
    return setLast(Result::kOk);
  }

  Result buildTables() {
#ifdef MUT_SKIP_STAGE_ORDER
    (void)0;
#else
    if (stage_ != Stage::kWpr2Valid) {
      if (stage_ == Stage::kDone || stage_ == Stage::kFailed)
        return setLast(Result::kAlreadyTerminal);
      return setLast(Result::kOutOfOrder);
    }
#endif
    stage_ = Stage::kTablesBuilt;
    return setLast(Result::kOk);
  }

  Result readyBootloader() {
#ifdef MUT_SKIP_STAGE_ORDER
    (void)0;
#else
    if (stage_ != Stage::kTablesBuilt) {
      if (stage_ == Stage::kDone || stage_ == Stage::kFailed)
        return setLast(Result::kAlreadyTerminal);
      return setLast(Result::kOutOfOrder);
    }
#endif
    stage_ = Stage::kBootloaderReady;
    return setLast(Result::kOk);
  }

  // faultBoot stalls the boot poll past budget -> Timeout, terminal Failed.
  // Terminal states pin staged buffers; retry needs a fresh Sequencer.
  Result boot(FakeClock &clk, bool faultBoot) {
#ifdef MUT_RETRY_AFTER_FAIL
    (void)0;  // MUTATION TEST-ONLY: terminal-state guard removed
#else
    if (stage_ == Stage::kDone || stage_ == Stage::kFailed)
      return setLast(Result::kAlreadyTerminal);  // no auto-retry, no reuse
#endif
#ifdef MUT_SKIP_STAGE_ORDER
    (void)0;
#else
    if (stage_ != Stage::kBootloaderReady) return setLast(Result::kOutOfOrder);
#endif
    stage_ = Stage::kBooting;
    uint64_t spent = 0u;
    while (spent < kStageBudgetMs) {
      clk.advance(kQuantumMs);
      spent += kQuantumMs;
#ifdef MUT_IGNORE_TIMEOUT
      (void)faultBoot;
#else
      if (faultBoot) continue;  // never completes this poll
#endif
      break;
    }
#ifndef MUT_IGNORE_TIMEOUT
    if (faultBoot) {
      stage_ = Stage::kFailed;
      failedAt_ = FailAt::kBoot;
      return setLast(Result::kTimeout);
    }
#endif
    bootTicks_ = spent;
    stage_ = Stage::kDone;
    return setLast(Result::kOk);
  }

 private:
  Result setLast(Result r) {
    last_ = r;
    return r;
  }
  Stage stage_ = Stage::kUnloaded;
  Result last_ = Result::kOk;
  FailAt failedAt_ = FailAt::kNone;
  uint64_t bootTicks_ = 0u;
  Range frts_, wpr2_, radix_, sig_, boot_;
  Wpr2Meta meta_;
};

}  // namespace fwsec
