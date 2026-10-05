// rpc_sim.hpp — M9 GSP-RPC cmdq framing host-only model (P76-DUAL-OVERNIGHT-V2).
// Isolated. Zero production wiring, zero real FW/queues. Semantics from Nova
// nova-core-gsp-cmdq.rs + nova-core-gsp-fw.rs (GspMsgElement) via local
// GSP-NEXTSTEP-P65/UPSTREAM-EVIDENCE; NO code copied, algorithm reimplemented.
// Same checksum ALGORITHM as Nova; own envelope LAYOUT (offsets differ on purpose):
//   off 0: checksum u32 LE | off 4: seq u32 LE | off 8: function u32 LE
//   off 12: length u32 LE (total bytes incl. 16B header) | off 16+: payload.
// Emit rule (Nova-identical): checksum = fold(message with checksum field zeroed).
// Validate rule (Nova-identical): fold(whole message) must equal 0.
// Fixed-point note: field at offset 0 lands in lanes 0..3 with lanes 4..7 zero, so
// check(F)==c exactly and emit->validate round-trips (proven by lane analysis;
// also asserted by the roundtrip + fuzz tests, not by faith).
// NOT-MODELED: exact Nova GSP_MSG_QUEUE_ELEMENT/rpc_message_header_v offsets,
// elemCount/page rounding, shared-memory pointers, QUEUE_HEAD notify MMIO write,
// queue lock held across send+receive (single-threaded model), split multi-part
// commands, variable-payload init callbacks, seq validation on receive (Nova logs
// seq; matching is by function).
#pragma once
#include <cstdint>
#include <vector>

namespace rpc {

inline constexpr uint64_t kHeaderLen = 16u;
inline constexpr uint64_t kMaxPayload = 1024u;
inline constexpr uint64_t kPollQuantumMs = 1u;  // Nova wait_for_msg polls each 1ms
inline constexpr uint32_t kFuncInvalid = 0u;    // model-reserved "unrecognized"

enum class Result : int32_t {
  kOk = 0,
  kEmsgsize = -1,    // payload exceeds maximum (send side)
  kTimedOut = -2,    // no message within budget (poll side)
  kEioShort = -3,    // bytes available < advertised length
  kEioChecksum = -4, // fold != 0
  kEinvalFunction = -5,  // unrecognized function code
  kErangeMismatch = -6,  // recognized but not the expected function (consumed)
};

struct FakeClock {
  uint64_t nowMs = 0u;
  void advance(uint64_t ms) { nowMs += ms; }
};

inline uint64_t Rotl64(uint64_t v, unsigned b) {
  b &= 63u;
  return b == 0u ? v : ((v << b) | (v >> (64u - b)));
}

// Nova Cmdq::calculate_checksum, reimplemented: XOR-fold of rotate_left(byte,(i%8)*8),
// result (sum>>32)^(u32)sum.
inline uint32_t FoldChecksum(const uint8_t *data, uint64_t len) {
  uint64_t acc = 0u;
  for (uint64_t i = 0u; i < len; ++i)
    acc ^= Rotl64((uint64_t)data[i], (unsigned)((i % 8u) * 8u));
  return (uint32_t)(acc >> 32) ^ (uint32_t)acc;
}

inline void PutLe32(std::vector<uint8_t> &b, uint64_t off, uint32_t v) {
  b[(size_t)off] = (uint8_t)(v & 0xFFu);
  b[(size_t)off + 1] = (uint8_t)((v >> 8) & 0xFFu);
  b[(size_t)off + 2] = (uint8_t)((v >> 16) & 0xFFu);
  b[(size_t)off + 3] = (uint8_t)((v >> 24) & 0xFFu);
}
inline uint32_t GetLe32(const uint8_t *b, uint64_t off) {
  return (uint32_t)b[(size_t)off] | ((uint32_t)b[(size_t)off + 1] << 8) |
         ((uint32_t)b[(size_t)off + 2] << 16) | ((uint32_t)b[(size_t)off + 3] << 24);
}

class Cmdq {
 public:
  Cmdq() = default;
  uint32_t nextSeq() const { return seq_; }
  uint64_t readPtr() const { return readPtr_; }  // consumed-message counter

  // Build one envelope. EMSGSIZE when oversize; seq assigned then incremented.
  Result send(uint32_t function, const uint8_t *payload, uint64_t payloadLen,
              std::vector<uint8_t> &out) {
#ifdef MUT_ALLOW_OVERSIZE
    (void)0;  // MUTATION TEST-ONLY: oversize accepted into an envelope
#else
    if (payloadLen > kMaxPayload) return Result::kEmsgsize;
#endif
    if (function == kFuncInvalid) return Result::kEinvalFunction;
    out.assign((size_t)(kHeaderLen + payloadLen), 0u);
    PutLe32(out, 0u, 0u);  // checksum zeroed for the fold
    PutLe32(out, 4u, seq_);
    PutLe32(out, 8u, function);
    PutLe32(out, 12u, (uint32_t)(kHeaderLen + payloadLen));
    for (uint64_t i = 0u; i < payloadLen; ++i) out[(size_t)(kHeaderLen + i)] = payload[i];
    uint32_t c = FoldChecksum(out.data(), (uint64_t)out.size());
    PutLe32(out, 0u, c);
#ifdef MUT_SEQ_REUSE
    (void)0;  // MUTATION TEST-ONLY: seq never advances
#else
    seq_ += 1u;
#endif
    return Result::kOk;
  }

  // Validate one received envelope against the expected function.
  // readPtr_ advances whenever bytes for a full header were present (consumed),
  // mirroring Nova advancing the read pointer even on mismatch/failure.
  Result receive(const uint8_t *bytes, uint64_t availLen, uint32_t expectedFunction) {
    if (availLen < kHeaderLen) return Result::kEioShort;  // not even a header
    uint32_t declared = GetLe32(bytes, 12u);
    if (declared < kHeaderLen || declared > (uint32_t)(kHeaderLen + kMaxPayload))
      return Result::kEioShort;  // insane length: treat as short/inconsistent (EIO class)
    if (availLen < declared) return Result::kEioShort;
    readPtr_ += 1u;  // consumed: pointer advances regardless of outcome below
#ifdef MUT_SKIP_CHECKSUM
    (void)0;
#else
    if (FoldChecksum(bytes, declared) != 0u) return Result::kEioChecksum;
#endif
// Trailing bytes past `declared` belong to the next message; framing holds,
// not an error. (No hook: both branches would be identical by design.)
    uint32_t func = GetLe32(bytes, 8u);
    if (func == kFuncInvalid) return Result::kEinvalFunction;
#ifdef MUT_MISMATCH_OK
    (void)expectedFunction;
    return Result::kOk;
#else
    if (func != expectedFunction) return Result::kErangeMismatch;  // consumed (ERANGE)
#endif
    return Result::kOk;
  }

  // Poll for a message: no bytes within budget -> TimedOut (clock advanced past
  // budget); bytes present -> validate immediately (fake transport delivers whole).
  Result pollReceive(FakeClock &clk, const uint8_t *bytes, uint64_t availLen,
                     bool hasMsg, uint64_t timeoutMs, uint32_t expectedFunction) {
    uint64_t spent = 0u;
    while (spent < timeoutMs) {
      clk.advance(kPollQuantumMs);
      spent += kPollQuantumMs;
      if (hasMsg) break;
#ifdef MUT_SWALLOW_TIMEOUT
      (void)0;
#else
      (void)0;
#endif
    }
#ifndef MUT_SWALLOW_TIMEOUT
    if (!hasMsg) return Result::kTimedOut;
#else
    if (!hasMsg) return Result::kOk;  // MUTATION TEST-ONLY: timeout reported as success
#endif
    return receive(bytes, availLen, expectedFunction);
  }

 private:
  uint32_t seq_ = 0u;
  uint64_t readPtr_ = 0u;
};

}  // namespace rpc
