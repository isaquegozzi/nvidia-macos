#pragma once
// SHADOW-P78-VM-MMU-SIM — host-only Ampere VM/MMU model (scoped, synthetic only).
// Zero production wiring. No MMIO/PCI/BME/DMA/GSP/VRAM/IRQ/IOKit. All addresses synthetic.
// Grounding: P78-VM-MMU-EVIDENCE/ (tu102 dma_bits=47, gp100 PTE, gf100 aper, r535 VASpace RPC).
#include <cstdint>
#include <map>
#include <vector>

namespace vm78 {

constexpr uint64_t kVaBits = 47;
constexpr uint64_t kVaLimit = (1ULL << 47);
constexpr uint64_t kServerStart = 0x100000000ULL;
constexpr uint64_t kServerSize = 0x20000000ULL;
constexpr uint64_t kServerEnd = kServerStart + kServerSize;
constexpr uint32_t kVaspaceClass = 0x000090f1U;
constexpr uint32_t kVaspaceIndexGpuNew = 0x00U;
constexpr uint32_t kVaspaceFlagExtOwned = (1U << 3);
constexpr uint32_t kCmdCopyServerPdes = 0x90f10106U;
constexpr uint32_t kCmdSetPageDir = 0x000813U + 0x800000U;
constexpr uint32_t kCmdUnsetPageDir = 0x000814U + 0x800000U;
constexpr int kPageShiftsLeaf[] = {12, 16, 21, 29};
constexpr int kPageShiftsDirOnly[] = {38, 47};
constexpr uint64_t kMaxPa = (1ULL << 48);

constexpr uint64_t kPteValid = (1ULL << 0);
constexpr unsigned kPteAperShift = 1;
constexpr uint64_t kPteAperMask = (3ULL << 1);
constexpr uint64_t kPteVol = (1ULL << 3);
constexpr uint64_t kPtePriv = (1ULL << 5);
constexpr uint64_t kPteRo = (1ULL << 6);
constexpr uint64_t kPteAtomicDis = (1ULL << 7);
constexpr unsigned kPteKindShift = 56;
constexpr unsigned kPteCtagShift = 36;
constexpr uint8_t kKindInvalid = 0x07;
constexpr int kKindCount = 16;

enum class Aperture : uint64_t { kVram = 0, kHostCoh = 2, kNcoh = 3 };
enum class Fault {
  kNone = 0, kInvalidPte, kPermission, kOutOfRange, kApertureMismatch,
  kAlignment, kDuplicate, kUnmapMissing, kStale, kWrap, kNeedsFlush, kInvalidEncoding,
};

struct PteFields {
  bool valid = false;
  Aperture aper = Aperture::kVram;
  bool vol = false;
  bool priv = false;
  bool ro = false;
  bool atomicDisabled = false;
  uint64_t pa = 0;
  uint8_t kind = 0;
  uint64_t ctag = 0;
};

inline bool IsLeafShift(int shift) {
  for (int s : kPageShiftsLeaf)
    if (s == shift) return true;
  return false;
}
inline bool IsKnownShift(int shift) {
  if (IsLeafShift(shift)) return true;
  for (int s : kPageShiftsDirOnly)
    if (s == shift) return true;
  return false;
}
inline bool IsValidAper(uint64_t v) { return v == 0 || v == 2 || v == 3; }

inline Fault EncodePte(const PteFields& f, uint64_t* out) {
  uint64_t aper = static_cast<uint64_t>(f.aper);
#ifdef VM78_MUT_UNKNOWN_APER_OK
  (void)aper;
#else
  if (!IsValidAper(aper)) return Fault::kApertureMismatch;
#endif
#ifdef VM78_MUT_SKIP_PA_ALIGN
  (void)f;
#else
  if (f.pa & 0xFFFULL) return Fault::kAlignment;
  if (f.pa >= kMaxPa) return Fault::kOutOfRange;
#endif
  if (f.kind >= kKindCount || f.kind == kKindInvalid) return Fault::kInvalidEncoding;
  uint64_t d = 0;
  if (f.valid) d |= kPteValid;
  d |= (aper << kPteAperShift);
  if (f.vol) d |= kPteVol;
  if (f.priv) d |= kPtePriv;
  if (f.ro) d |= kPteRo;
  if (f.atomicDisabled) d |= kPteAtomicDis;
  d |= (f.pa >> 4);
  d |= (static_cast<uint64_t>(f.kind) << kPteKindShift);
  if (f.ctag) d |= (f.ctag << kPteCtagShift);
  *out = d;
  return Fault::kNone;
}

struct Decoded {
  Fault fault = Fault::kNone;
  bool sparse = false;
  bool invalidLpte = false;
  PteFields fields;
};

inline Decoded DecodePte(uint64_t raw) {
  Decoded r;
  if (raw == 0) {
    r.fault = Fault::kInvalidPte;
    return r;
  }
  bool valid = (raw & kPteValid) != 0;
  bool vol = (raw & kPteVol) != 0;
  bool priv = (raw & kPtePriv) != 0;
  uint64_t aper = (raw & kPteAperMask) >> kPteAperShift;
  if (!valid) {
    if (vol && !priv && aper == 0) {
      r.sparse = true;
      r.fields.vol = true;
      return r;
    }
    if (priv && !vol) {
      r.invalidLpte = true;
      r.fault = Fault::kInvalidPte;
      return r;
    }
    r.fault = Fault::kInvalidPte;
    return r;
  }
#ifdef VM78_MUT_UNKNOWN_APER_OK
  (void)aper;
#else
  if (!IsValidAper(aper)) {
    r.fault = Fault::kApertureMismatch;
    return r;
  }
#endif
  r.fields.valid = true;
  r.fields.aper = static_cast<Aperture>(aper);
  r.fields.vol = vol;
  r.fields.priv = priv;
  r.fields.ro = (raw & kPteRo) != 0;
  r.fields.atomicDisabled = (raw & kPteAtomicDis) != 0;
  r.fields.kind = static_cast<uint8_t>(raw >> kPteKindShift);
  if (r.fields.kind >= kKindCount || r.fields.kind == kKindInvalid) {
    r.fault = Fault::kInvalidEncoding;
    return r;
  }
  r.fields.pa = ((raw >> 8) << 12) & (kMaxPa - 1);
  r.fields.ctag = (raw >> kPteCtagShift) & 0xFFULL;
  return r;
}

struct VaspaceAllocParams {
  uint32_t index = kVaspaceIndexGpuNew;
  uint32_t flags = 0;
  uint64_t vaSize = 0;
};
struct CopyPdesParams {
  uint64_t pageSize = 0;
  uint64_t virtLo = 0;
  uint64_t virtHi = 0;
  uint32_t numLevels = 0;
  uint64_t rootPhys = 0;
};
struct SetPdParams {
  uint64_t physAddress = 0;
  uint32_t numEntries = 0;
  uint32_t aperFlag = 0;
};

struct Mapping {
  uint64_t va = 0;
  uint64_t size = 0;
  int shift = 12;
  Aperture aper = Aperture::kVram;
  bool ro = false;
  bool priv = false;
  uint64_t pa = 0;
  uint64_t pte = 0;
  bool needsFlush = true;
};

struct Region {
  uint64_t addr = 0;
  uint64_t size = 0;
};

class Vm {
 public:
  Vm() = default;

  Fault VaspaceAlloc(const VaspaceAllocParams& p, bool external) {
    if (vaspaceLive_) return Fault::kDuplicate;
    if (p.index != kVaspaceIndexGpuNew) return Fault::kInvalidEncoding;
    bool ext = (p.flags & kVaspaceFlagExtOwned) != 0;
    if (ext != external) return Fault::kInvalidEncoding;
    if (p.flags & ~kVaspaceFlagExtOwned) return Fault::kInvalidEncoding;
    vaspaceLive_ = true;
    external_ = external;
    bound_ = false;
    return Fault::kNone;
  }
  Fault CopyServerReservedPdes(const CopyPdesParams& p) {
    if (!vaspaceLive_ || external_) return Fault::kStale;
#ifndef VM78_MUT_BIND_SKIP_STATE
    if (bound_) return Fault::kDuplicate;
#endif
    if (p.pageSize != kServerSize) return Fault::kInvalidEncoding;
    if (p.virtLo != kServerStart) return Fault::kOutOfRange;
    if (p.virtHi != kServerEnd - 1) return Fault::kOutOfRange;
    if (p.numLevels == 0 || p.numLevels > 6) return Fault::kInvalidEncoding;
    if ((p.rootPhys & 0xFFFULL) != 0) return Fault::kAlignment;
    bound_ = true;
    return Fault::kNone;
  }
  Fault SetPageDirectory(const SetPdParams& p) {
    if (!vaspaceLive_ || !external_) return Fault::kStale;
    if (bound_) return Fault::kDuplicate;
    if ((p.physAddress & 0xFFFULL) != 0) return Fault::kAlignment;
    if (p.numEntries == 0) return Fault::kInvalidEncoding;
    if (p.aperFlag != 0) return Fault::kApertureMismatch;
    bound_ = true;
    return Fault::kNone;
  }
  Fault UnsetPageDirectory() {
    if (!vaspaceLive_ || !external_ || !bound_) return Fault::kStale;
    if (!mappings_.empty() || !regions_.empty()) return Fault::kStale;
    bound_ = false;
    return Fault::kNone;
  }
  Fault VaspaceFree() {
    if (!vaspaceLive_) return Fault::kStale;
    if (!mappings_.empty() || !regions_.empty()) return Fault::kStale;
    if (external_ && bound_) return Fault::kStale;
    vaspaceLive_ = false;
    bound_ = false;
    return Fault::kNone;
  }

  Fault VmGet(int shift, uint64_t size, uint64_t* outVa) {
#ifndef VM78_MUT_SKIP_RPC_READY
    if (!vaspaceLive_ || !bound_) return Fault::kStale;
#endif
    if (!IsLeafShift(shift)) return Fault::kInvalidEncoding;
    uint64_t ps = 1ULL << shift;
    if (size == 0 || (size & (ps - 1)) != 0) return Fault::kAlignment;
    uint64_t va = 0;
    if (!FirstFit(size, ps, &va)) return Fault::kOutOfRange;
    regions_.push_back({va, size});
    *outVa = va;
    return Fault::kNone;
  }
  Fault VmPut(uint64_t va) {
    for (size_t i = 0; i < regions_.size(); ++i) {
      if (regions_[i].addr == va) {
        for (const auto& kv : mappings_) {
          const Mapping& m = kv.second;
          if (m.va >= regions_[i].addr && m.va < regions_[i].addr + regions_[i].size)
            return Fault::kStale;
        }
        regions_.erase(regions_.begin() + i);
        return Fault::kNone;
      }
    }
    return Fault::kUnmapMissing;
  }

  Fault Map(uint64_t va, int shift, uint64_t pa, Aperture aper, bool ro, bool priv,
            uint8_t kind) {
    if (!vaspaceLive_ || !bound_) return Fault::kStale;
    if (!IsLeafShift(shift)) return Fault::kInvalidEncoding;
    uint64_t ps = 1ULL << shift;
#ifdef VM78_MUT_SKIP_VA_ALIGN
    (void)ps;
#else
    if ((va & (ps - 1)) != 0) return Fault::kAlignment;
#endif
    const uint64_t endWrap = va + ps;
#ifdef VM78_MUT_SKIP_WRAP
    (void)endWrap;
#else
    if (endWrap < va) return Fault::kWrap;
#endif
    if (va >= kVaLimit || (__uint128_t)va + ps > kVaLimit) return Fault::kOutOfRange;
    if (OverlapsServer(va, ps)) return Fault::kOutOfRange;
    if (!InsideRegion(va, ps)) return Fault::kOutOfRange;
#ifndef VM78_MUT_MAP_OVERLAP_OK
    if (OverlapsMapping(va, ps)) return Fault::kDuplicate;
#endif
    uint64_t av = static_cast<uint64_t>(aper);
    if (!IsValidAper(av)) return Fault::kApertureMismatch;
    if (kind >= kKindCount || kind == kKindInvalid) return Fault::kInvalidEncoding;
    PteFields f;
    f.valid = true;
    f.aper = aper;
    f.vol = (aper == Aperture::kHostCoh);
    f.priv = priv;
    f.ro = ro;
    f.pa = pa;
    f.kind = kind;
    uint64_t raw = 0;
    Fault fe = EncodePte(f, &raw);
    if (fe != Fault::kNone) return fe;
    Mapping m;
    m.va = va;
    m.size = ps;
    m.shift = shift;
    m.aper = aper;
    m.ro = ro;
    m.priv = priv;
    m.pa = pa;
    m.pte = raw;
    m.needsFlush = true;
    mappings_[va] = m;
    return Fault::kNone;
  }

  Fault Unmap(uint64_t va) {
#ifdef VM78_MUT_UNMAP_ALWAYS_OK
    (void)va;
    return Fault::kNone;
#endif
    auto it = mappings_.find(va);
    if (it == mappings_.end()) return Fault::kUnmapMissing;
    mappings_.erase(it);
    return Fault::kNone;
  }
  void Flush() {
    for (auto& kv : mappings_) kv.second.needsFlush = false;
  }
  Fault Lookup(uint64_t va, bool write, bool unprivileged, uint64_t* pteOut) {
    auto it = mappings_.find(va);
    if (it == mappings_.end()) return Fault::kStale;
    const Mapping& m = it->second;
    if (m.needsFlush) return Fault::kNeedsFlush;
#ifdef VM78_MUT_ALLOW_RW_BYPASS
    (void)write;
#else
    if (write && m.ro) return Fault::kPermission;
#endif
#ifdef VM78_MUT_ALLOW_PRIV_BYPASS
    (void)unprivileged;
#else
    if (unprivileged && m.priv) return Fault::kPermission;
#endif
    if (pteOut) *pteOut = m.pte;
    return Fault::kNone;
  }

  bool vaspaceLive() const { return vaspaceLive_; }
  bool bound() const { return bound_; }
  size_t mappingCount() const { return mappings_.size(); }
  size_t regionCount() const { return regions_.size(); }

 private:
  bool OverlapsServer(uint64_t va, uint64_t size) const {
    return va < kServerEnd && va + size > kServerStart;
  }
  bool InsideRegion(uint64_t va, uint64_t size) const {
    for (const auto& r : regions_) {
      if (va >= r.addr && va + size <= r.addr + r.size) return true;
    }
    return false;
  }
  bool OverlapsMapping(uint64_t va, uint64_t size) const {
    for (const auto& kv : mappings_) {
      uint64_t a = kv.second.va, b = a + kv.second.size;
      if (va < b && va + size > a) return true;
    }
    return false;
  }
  bool FirstFit(uint64_t size, uint64_t align, uint64_t* out) const {
    uint64_t cursor = 0;
    const uint64_t gran = 1ULL << 12;
    (void)gran;
    while (cursor < kVaLimit) {
      uint64_t aligned = (cursor + align - 1) & ~(align - 1);
      __uint128_t end = (__uint128_t)aligned + size;
      if (end > kVaLimit) return false;
      if (OverlapsServer(aligned, size)) {
        cursor = kServerEnd;
        continue;
      }
      bool clash = false;
      for (const auto& r : regions_) {
        if (aligned < r.addr + r.size && aligned + size > r.addr) {
          cursor = r.addr + r.size;
          clash = true;
          break;
        }
      }
      if (!clash) {
        *out = aligned;
        return true;
      }
    }
    return false;
  }

  bool vaspaceLive_ = false;
  bool external_ = false;
  bool bound_ = false;
  std::vector<Region> regions_;
  std::map<uint64_t, Mapping> mappings_;
};

}  // namespace vm78
