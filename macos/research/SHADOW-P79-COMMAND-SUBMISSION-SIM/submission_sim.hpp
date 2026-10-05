#pragma once
// SHADOW-P79-COMMAND-SUBMISSION-SIM — host-only GPFIFO/pushbuffer/submission model.
// Zero production wiring. No MMIO/PCI/BME/DMA/GSP/VRAM/IRQ/IOKit, no real submission.
// All memory/addresses synthetic. Grounding: P79-COMMAND-SUBMISSION-EVIDENCE/
// (clc56f.h entry/control/method/semaphore fields, ga100 ramfc GPFIFO programming,
// r535 alloc geometry, chan.h lifecycle, rpcfn/msgfn GSP boundary).
// P77 (channel) + P78 (VM) are integrated by the host-only integration test, which
// includes those headers directly — this header copies no logic from them.
#include <cstdint>
#include <map>
#include <vector>

namespace sub79 {

constexpr uint32_t kEntrySize = 8;             // NVC56F_GP_ENTRY__SIZE
constexpr uint32_t kMaxSubchannels = 8;        // NVC56F_NUMBER_OF_SUBCHANNELS
constexpr uint32_t kMaxMethodCount = 0x1FFF;   // COUNT 28:16 (13 bits)
constexpr uint32_t kMaxEntryLength = 0x1FFFFF; // LENGTH 30:10 (21 bits)
constexpr uint32_t kClassAmpereGpfifoA = 0x0000C56F;
constexpr uint32_t kFuncWorkSubmitToken = 186; // CTRL_GPFIFO_GET_WORK_SUBMIT_TOKEN
constexpr uint32_t kFuncWorkSubmitNotif = 187; // CTRL_GPFIFO_SET_WORK_SUBMIT_TOKEN_NOTIF_INDEX

enum class Opcode : uint8_t { kNop = 0, kIllegal = 1, kGpCrc = 2, kPbCrc = 3 };
enum class SecOp : uint8_t {
  kGrp0Tert = 0, kInc = 1, kGrp2Tert = 2, kNonInc = 3,
  kImmd = 4, kOneInc = 5, kReserved6 = 6, kEndSegment = 7,
};
enum class Fault {
  kNone = 0, kInvalidEntry, kAlign, kLengthZero, kLengthOverflow, kRingFull,
  kRingEmpty, kPutOverflow, kUnmappedPb, kStalePb, kPacketOverflow, kUnknownMethod,
  kReservedEncoding, kSubchannelRange, kNoChannel, kUnkicked, kPrematureFree,
  kStaleFence, kSemaphoreMismatch, kTokenMissing, kCountZero, kErrorPinned,
};

struct GpEntry {
  uint32_t get = 0;      // PB offset, 4B units (ENTRY0_GET 31:2)
  uint8_t getHi = 0;     // ENTRY1_GET_HI 7:0
  uint32_t length = 0;   // ENTRY1_LENGTH 30:10, bytes
  bool sub = false;      // LEVEL: MAIN/SUBROUTINE
  bool wait = false;     // SYNC: PROCEED/WAIT
  bool condFetch = false;// FETCH: UNCONDITIONAL/CONDITIONAL
  Opcode op = Opcode::kNop;
};

inline Fault EncodeEntry(const GpEntry& e, uint32_t w[2]) {
  if (e.length == 0) return Fault::kLengthZero;
  if (e.length > kMaxEntryLength) return Fault::kLengthOverflow;
  if (e.get & 0x3U) return Fault::kAlign;
  uint8_t op = static_cast<uint8_t>(e.op);
#ifdef SUB79_MUT_UNKNOWN_OPCODE_OK
  (void)op;
#else
  if (op > 3) return Fault::kInvalidEntry;
  if (op == 1) return Fault::kInvalidEntry;  // ILLEGAL never emitted
#endif
  w[0] = (e.get & ~0x3U) | (e.condFetch ? 1U : 0U);
  w[1] = (static_cast<uint32_t>(e.getHi))
       | (e.sub ? (1U << 9) : 0U)
       | ((e.length & kMaxEntryLength) << 10)
       | (e.wait ? (1U << 31) : 0U)
       | (static_cast<uint32_t>(op) & 0xFFU);
  return Fault::kNone;
}

struct DecodedEntry {
  Fault fault = Fault::kNone;
  GpEntry entry;
};

inline DecodedEntry DecodeEntry(uint32_t w0, uint32_t w1) {
  DecodedEntry r;
  r.entry.condFetch = (w0 & 1U) != 0;
  r.entry.get = w0 & ~0x3U;
  r.entry.getHi = static_cast<uint8_t>(w1 & 0xFFU);
  r.entry.sub = (w1 & (1U << 9)) != 0;
  r.entry.length = (w1 >> 10) & kMaxEntryLength;
  r.entry.wait = (w1 & (1U << 31)) != 0;
  // Bits 7:0 are dual-view (GET_HI address extension vs OPCODE): the header defines
  // both at the same position. Both views are recorded; ILLEGAL(1) always faults.
  uint8_t lo = static_cast<uint8_t>(w1 & 0xFFU);
  if (lo == 0 && (w1 & 0xFFFFFF00U) == 0 && (w0 & ~1U) == 0) {
    // All-zero word pair: indistinguishable from unmapped ring memory.
    r.fault = Fault::kInvalidEntry;
    return r;
  }
  if (lo == static_cast<uint8_t>(Opcode::kIllegal)) {
    r.entry.op = Opcode::kIllegal;
    r.fault = Fault::kInvalidEntry;
    return r;
  }
  r.entry.getHi = lo;
  r.entry.op = (lo <= 3) ? static_cast<Opcode>(lo) : Opcode::kNop;
  if (r.entry.length == 0) r.fault = Fault::kLengthZero;
  return r;
}

// Method addresses documented for class 0xc56f (clc56f.h).
inline bool IsKnownMethod(uint32_t addr) {
  switch (addr) {
    case 0x00: case 0x04: case 0x08:
    case 0x10: case 0x14: case 0x18: case 0x1C:
    case 0x20: case 0x24: case 0x28: case 0x2C: case 0x30: case 0x34:
    case 0x50: case 0x6C: case 0x78: case 0x80: case 0x84:
      return true;
    default:
      return false;
  }
}

struct Packet {
  uint32_t address = 0;   // 12-bit
  uint32_t subchannel = 0;// 0..7
  SecOp sec = SecOp::kInc;
  uint32_t count = 0;     // payload words (INC/NONINC/ONE_INC)
  uint32_t immd = 0;      // IMMD data
};

inline Fault EncodePacketHeader(const Packet& p, uint32_t* out) {
  if (p.address > 0xFFFU) return Fault::kUnknownMethod;
#ifdef SUB79_MUT_UNKNOWN_METHOD_OK
  (void)0;
#else
  if (!IsKnownMethod(p.address)) return Fault::kUnknownMethod;
#endif
#ifndef SUB79_MUT_SUBCHANNEL_OK
  if (p.subchannel >= kMaxSubchannels) return Fault::kSubchannelRange;
#endif
  uint8_t s = static_cast<uint8_t>(p.sec);
#ifndef SUB79_MUT_RESERVED_OK
  if (s == 6) return Fault::kReservedEncoding;
#endif
  if (s > 7) return Fault::kReservedEncoding;
  uint32_t w = (p.address & 0xFFFU) | ((p.subchannel & 7U) << 13);
  if (p.sec == SecOp::kImmd) {
    if (p.immd > 0x1FFFU) return Fault::kPacketOverflow;
    w |= ((p.immd & 0x1FFFU) << 16);
  } else if (p.sec == SecOp::kGrp0Tert || p.sec == SecOp::kGrp2Tert ||
             p.sec == SecOp::kEndSegment) {
    if (p.count != 0) return Fault::kPacketOverflow;
  } else {
#ifndef SUB79_MUT_COUNT_ZERO_OK
    if (p.count == 0 || p.count > kMaxMethodCount) return Fault::kCountZero;
#else
    if (p.count > kMaxMethodCount) return Fault::kCountZero;
#endif
    w |= ((p.count & kMaxMethodCount) << 16);
  }
  w |= (static_cast<uint32_t>(s) << 29);
  *out = w;
  return Fault::kNone;
}

inline uint32_t PacketWordCount(const Packet& p) {
  switch (p.sec) {
    case SecOp::kInc: case SecOp::kNonInc: case SecOp::kOneInc:
      return 1 + p.count;
    default:
      return 1;
  }
}

struct Fence {
  uint64_t id = 0;
  uint32_t payload = 0;
  bool signaled = false;
  bool live = true;
};

class SubmitChannel {
 public:
  SubmitChannel() = default;

  Fault Configure(uint32_t entriesPow2, uint64_t ringBase, uint64_t pbBase) {
    if (configured_) return Fault::kInvalidEntry;
    if (entriesPow2 == 0 || entriesPow2 > 31) return Fault::kPutOverflow;
    uint64_t entries = 1ULL << entriesPow2;
    if (entries < 2) return Fault::kPutOverflow;
    if ((ringBase & 7ULL) != 0 || (pbBase & 3ULL) != 0) return Fault::kAlign;
    if (ringBase + entries * kEntrySize < ringBase) return Fault::kPutOverflow;
    entries_ = static_cast<uint32_t>(entries);
    ringBase_ = ringBase;
    pbBase_ = pbBase;
    configured_ = true;
    return Fault::kNone;
  }

  Fault ObtainToken(uint64_t token) {
    if (!configured_) return Fault::kNoChannel;
    if (tokenLive_) return Fault::kInvalidEntry;
    if (token == 0) return Fault::kTokenMissing;
    token_ = token;
    tokenLive_ = true;
    return Fault::kNone;
  }
  Fault ReleaseToken() {
    if (!tokenLive_) return Fault::kTokenMissing;
    if (put_ != get_) return Fault::kPrematureFree;
    tokenLive_ = false;
    return Fault::kNone;
  }

  // Pushbuffer residency is provided by the integrator (P78 Vm::Lookup analogue):
  // set every appended entry's PB window live (true) or stale (false).
  void SetPbLive(uint64_t pbAddr, bool live) { pbLive_[pbAddr] = live; }

  Fault Append(const GpEntry& e) {
    if (!configured_) return Fault::kNoChannel;
    if (errorPinned_) return Fault::kErrorPinned;
#ifndef SUB79_MUT_TOKEN_OK
    if (!tokenLive_) return Fault::kTokenMissing;
#endif
    uint32_t w[2];
    Fault fe = EncodeEntry(e, w);
    if (fe != Fault::kNone) return fe;
    uint64_t pbAddr = pbBase_ + e.get;
    auto it = pbLive_.find(pbAddr);
#ifndef SUB79_MUT_PB_RESIDENCY_OK
    if (it == pbLive_.end()) return Fault::kUnmappedPb;
    if (!it->second) return Fault::kStalePb;
#else
    (void)it;
#endif
    uint32_t used = Used();
#ifdef SUB79_MUT_RING_FULL_OK
    (void)used;
#else
    if (used >= entries_ - 1) return Fault::kRingFull;
#endif
    uint32_t slot = put_ % entries_;
    slots_[slot] = e;
    slotLive_[slot] = true;
    put_ = (put_ + 1) % kRingIndexMod;
    kickPending_ = true;
    return Fault::kNone;
  }

  struct KickToken {
    bool valid = false;
  };
  KickToken Kick() {
    KickToken t;
    if (!configured_ || !kickPending_) return t;
    t.valid = true;
    kickPending_ = false;
    kicked_ = true;
    return t;
  }

  Fault Consume() {
    if (!configured_) return Fault::kNoChannel;
    if (errorPinned_) return Fault::kErrorPinned;
#ifdef SUB79_MUT_UNKICKED_OK
    (void)0;
#else
    if (!kicked_ && put_ != get_) return Fault::kUnkicked;
#endif
    if (put_ == get_) return Fault::kRingEmpty;
    uint32_t slot = get_ % entries_;
    if (!slotLive_[slot]) return Fault::kStalePb;
    slotLive_[slot] = false;
    lastConsumed_ = slots_[slot];
    get_ = (get_ + 1) % kRingIndexMod;
    if (put_ == get_) kicked_ = false;
    return Fault::kNone;
  }

  Fault InjectError() {
    if (!configured_) return Fault::kNoChannel;
    errorPinned_ = true;
    return Fault::kNone;
  }

  // Semaphore completion: release(payload) signals; acquire(id) observes.
  uint64_t Release(uint32_t payload) {
    Fence f;
    f.id = ++fenceSeq_;
    f.payload = payload;
    f.signaled = true;
    fences_[f.id] = f;
    reference_ = f.id;
    return f.id;
  }
  Fault Acquire(uint64_t id) {
    auto it = fences_.find(id);
    if (it == fences_.end() || !it->second.live) return Fault::kStaleFence;
    if (!it->second.signaled) return Fault::kSemaphoreMismatch;
    return Fault::kNone;
  }
  Fault FreeFence(uint64_t id) {
    auto it = fences_.find(id);
    if (it == fences_.end() || !it->second.live) return Fault::kStaleFence;
    if (!it->second.signaled) return Fault::kPrematureFree;
    it->second.live = false;
    return Fault::kNone;
  }

  Fault Teardown() {
    if (!configured_) return Fault::kNoChannel;
#ifndef SUB79_MUT_TEARDOWN_OK
    if (put_ != get_) return Fault::kPrematureFree;
    for (const auto& kv : fences_)
      if (kv.second.live && !kv.second.signaled) return Fault::kPrematureFree;
    if (tokenLive_) return Fault::kPrematureFree;
#endif
    configured_ = false;
    return Fault::kNone;
  }

  uint32_t Used() const {
    return (put_ >= get_) ? (put_ - get_) : (kRingIndexMod - get_ + put_);
  }
  uint32_t entries() const { return entries_; }
  uint32_t put() const { return put_; }
  uint32_t get() const { return get_; }
  uint64_t reference() const { return reference_; }
  bool configured() const { return configured_; }
  GpEntry lastConsumed() const { return lastConsumed_; }

 private:
  static constexpr uint32_t kRingIndexMod = 1U << 31;  // 32-bit visible counters
  bool configured_ = false;
  uint32_t entries_ = 0;
  uint64_t ringBase_ = 0;
  uint64_t pbBase_ = 0;
  uint32_t put_ = 0;
  uint32_t get_ = 0;
  bool kickPending_ = false;
  bool kicked_ = false;
  bool errorPinned_ = false;
  uint64_t token_ = 0;
  bool tokenLive_ = false;
  uint64_t fenceSeq_ = 0;
  uint64_t reference_ = 0;
  GpEntry lastConsumed_;
  std::map<uint32_t, GpEntry> slots_;
  std::map<uint32_t, bool> slotLive_;
  std::map<uint64_t, bool> pbLive_;
  std::map<uint64_t, Fence> fences_;
};

}  // namespace sub79
