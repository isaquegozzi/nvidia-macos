// test-rpc.cpp — M9 RPC cmdq framing host-only tests. Deterministic, no HW.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "rpc_sim.hpp"

using namespace rpc;
static int gGroups = 0;
#define PASS(m) do { std::printf("PASS(%s)\n", m); gGroups++; } while (0)

static uint32_t gLcg = 0x9050C0u;
static uint32_t Next() { gLcg = gLcg * 1664525u + 1013904223u; return gLcg >> 8; }

static std::vector<uint8_t> Payload(uint64_t n, uint32_t seed) {
  std::vector<uint8_t> p((size_t)n);
  for (uint64_t i = 0u; i < n; ++i) p[(size_t)i] = (uint8_t)((seed + i * 31u) & 0xFFu);
  return p;
}

int main() {
  {  // 1. roundtrip: seq starts 0, advances, checksum validates to 0
    Cmdq q;
    std::vector<uint8_t> env;
    std::vector<uint8_t> p = Payload(64, 7);
    assert(q.send(0x47465349u, p.data(), p.size(), env) == Result::kOk);
    assert(env.size() == 16u + p.size());
    assert(GetLe32(env.data(), 4u) == 0u);
    assert(FoldChecksum(env.data(), (uint64_t)env.size()) == 0u);
    assert(q.receive(env.data(), (uint64_t)env.size(), 0x47465349u) == Result::kOk);
    assert(q.readPtr() == 1u);
    std::vector<uint8_t> env2;
    assert(q.send(0x47465345u, nullptr, 0u, env2) == Result::kOk);  // empty payload ok
    assert(GetLe32(env2.data(), 4u) == 1u);
    assert(q.nextSeq() == 2u);
    PASS("roundtrip-seq-checksum-zero");
  }
  {  // 2. tamper anywhere -> BadChecksum
    Cmdq q;
    std::vector<uint8_t> env;
    std::vector<uint8_t> p = Payload(128, 99);
    assert(q.send(0x1234u, p.data(), p.size(), env) == Result::kOk);
    for (uint64_t off : {0ull, 5ull, 9ull, 16ull, 40ull, (uint64_t)env.size() - 1u}) {
      std::vector<uint8_t> bad = env;
      bad[(size_t)off] ^= 0xFFu;
      assert(q.receive(bad.data(), (uint64_t)bad.size(), 0x1234u) == Result::kEioChecksum);
    }
    assert(q.readPtr() == 6u);  // consumed even on checksum failure
    PASS("tamper-checksum-consumed");
  }
  {  // 3. length defects -> EIO class, no consume
    Cmdq q;
    std::vector<uint8_t> env;
    std::vector<uint8_t> p = Payload(32, 1);
    assert(q.send(0x1234u, p.data(), p.size(), env) == Result::kOk);
    assert(q.receive(env.data(), 10u, 0x1234u) == Result::kEioShort);  // no header
    assert(q.receive(env.data(), (uint64_t)env.size() - 1u, 0x1234u) == Result::kEioShort);
    std::vector<uint8_t> badLen = env;
    PutLe32(badLen, 12u, 4u);  // declared < header
    assert(q.receive(badLen.data(), (uint64_t)badLen.size(), 0x1234u) == Result::kEioShort);
    std::vector<uint8_t> hugeLen = env;
    PutLe32(hugeLen, 12u, 0xFFFFFF00u);
    assert(q.receive(hugeLen.data(), (uint64_t)hugeLen.size(), 0x1234u) == Result::kEioShort);
    assert(q.readPtr() == 0u);  // nothing consumable: short reads don't advance
    PASS("length-defects-no-consume");
  }
  {  // 4. function handling: invalid, mismatch
    Cmdq q;
    std::vector<uint8_t> env;
    std::vector<uint8_t> p = Payload(16, 5);
    assert(q.send(kFuncInvalid, p.data(), p.size(), env) == Result::kEinvalFunction);
    assert(q.nextSeq() == 0u);  // rejected send does not consume seq
    assert(q.send(0xA1u, p.data(), p.size(), env) == Result::kOk);
    assert(q.receive(env.data(), (uint64_t)env.size(), 0xB2u) == Result::kErangeMismatch);
    assert(q.readPtr() == 1u);  // mismatch consumed (ERANGE) like Nova
    PASS("function-invalid-mismatch-consumed");
  }
  {  // 5. oversize send rejected
    Cmdq q;
    std::vector<uint8_t> env;
    std::vector<uint8_t> big = Payload(kMaxPayload + 1u, 3);
    assert(q.send(0xA1u, big.data(), big.size(), env) == Result::kEmsgsize);
    assert(q.nextSeq() == 0u);
    std::vector<uint8_t> maxp = Payload(kMaxPayload, 3);
    assert(q.send(0xA1u, maxp.data(), maxp.size(), env) == Result::kOk);
    assert(q.receive(env.data(), (uint64_t)env.size(), 0xA1u) == Result::kOk);
    PASS("oversize-rejected-max-ok");
  }
  {  // 6. poll timeout vs delivery
    Cmdq q;
    FakeClock clk;
    std::vector<uint8_t> env;
    std::vector<uint8_t> p = Payload(8, 11);
    assert(q.send(0xC0u, p.data(), p.size(), env) == Result::kOk);
    assert(q.pollReceive(clk, nullptr, 0u, false, 5u, 0xC0u) == Result::kTimedOut);
    assert(clk.nowMs == 5u);
    assert(q.pollReceive(clk, env.data(), (uint64_t)env.size(), true, 5000u, 0xC0u) ==
           Result::kOk);
    assert(clk.nowMs == 6u);  // 1ms quantum then delivered
    PASS("poll-timeout-delivery-clock");
  }
  {  // 7. 5k fuzz with exact oracle
    for (int i = 0; i < 5000; ++i) {
      Cmdq q;
      FakeClock clk;
      uint64_t n = Next() % 1300u;
      uint32_t func = 1u + (Next() % 0xFFFFu);
      std::vector<uint8_t> p = Payload(n, Next());
      std::vector<uint8_t> env;
      int mode = (int)(Next() % 8u);
      Result rs = q.send(func, p.data(), n, env);
      if (n > kMaxPayload) { assert(rs == Result::kEmsgsize); continue; }
      assert(rs == Result::kOk);
      bool tamper = (mode == 1), trunc = (mode == 2), mismatch = (mode == 3);
      bool noMsg = (mode == 4);
      uint64_t toff = 0u;
      if (tamper) {
        toff = Next() % env.size();
        env[(size_t)toff] ^= (uint8_t)(1u + Next() % 255u);
      }
      // Tamper inside the length field (bytes 12..16) can reframe the message:
      // then Short is the correct EIO-class answer instead of Checksum.
      bool lenHit = tamper && toff >= 12u && toff < 16u;
      uint64_t avail = trunc ? (uint64_t)env.size() - 1u - (Next() % 8u) : (uint64_t)env.size();
      if (trunc && avail < kHeaderLen) avail = 0u;
      uint32_t expect = mismatch ? (func ^ 0xFFFFu) : func;
      if (expect == kFuncInvalid) expect = 0xBEEFu;
      Result rr = q.pollReceive(clk, env.data(), avail, !noMsg, 10u, expect);
      if (noMsg) { assert(rr == Result::kTimedOut); continue; }
      if (trunc) { assert(rr == Result::kEioShort); continue; }
      if (tamper && !lenHit) { assert(rr == Result::kEioChecksum); continue; }
      if (lenHit) {
        assert(rr == Result::kEioChecksum || rr == Result::kEioShort);
        continue;
      }
      if (mismatch) { assert(rr == Result::kErangeMismatch); continue; }
      assert(rr == Result::kOk);
    }
    PASS("fuzz-5k-oracle");
  }
  std::printf("RPC_GROUPS=%d PASS\n", gGroups);
  return gGroups == 7 ? 0 : 1;
}
