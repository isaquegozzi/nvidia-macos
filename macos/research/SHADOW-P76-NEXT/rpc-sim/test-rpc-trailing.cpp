// test-rpc-trailing.cpp — F1 finding-closure addendum (M0041). Additive only;
// exercises rpc_sim.hpp read-only. Concatenated envelopes must frame: leading
// message validates with trailing bytes present; partial tail reads Short.
// Deterministic, no HW.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "rpc_sim.hpp"

using namespace rpc;
static int gGroups = 0;
#define PASS(m) do { std::printf("PASS(%s)\n", m); gGroups++; } while (0)

int main() {
  std::vector<uint8_t> pA(48u), pB(96u);
  for (size_t i = 0u; i < pA.size(); ++i) pA[i] = (uint8_t)(i * 7u + 1u);
  for (size_t i = 0u; i < pB.size(); ++i) pB[i] = (uint8_t)(i * 13u + 2u);
  Cmdq q;
  std::vector<uint8_t> envA, envB;
  assert(q.send(0xA1u, pA.data(), pA.size(), envA) == Result::kOk);
  assert(q.send(0xB2u, pB.data(), pB.size(), envB) == Result::kOk);
  uint64_t lenA = (uint64_t)envA.size(), lenB = (uint64_t)envB.size();
  std::vector<uint8_t> concat = envA;
  concat.insert(concat.end(), envB.begin(), envB.end());
  {  // 1. leading message validates despite a full trailing message present
    assert(q.receive(concat.data(), (uint64_t)concat.size(), 0xA1u) == Result::kOk);
    assert(q.readPtr() == 1u);
    PASS("leading-ok-with-trailing");
  }
  {  // 2. second message parses at its offset
    assert(q.receive(concat.data() + lenA, lenB, 0xB2u) == Result::kOk);
    assert(q.readPtr() == 2u);
    PASS("second-at-offset-ok");
  }
  {  // 3. leading message validates with a partial (3-byte) tail present
    assert(q.receive(concat.data(), lenA + 3u, 0xA1u) == Result::kOk);
    assert(q.readPtr() == 3u);
    PASS("leading-ok-with-partial-tail");
  }
  {  // 4. truncated tail read is Short, unconsumed
    uint64_t before = q.readPtr();
    assert(q.receive(concat.data() + lenA, 3u, 0xB2u) == Result::kEioShort);
    assert(q.readPtr() == before);
    PASS("truncated-tail-short-unconsumed");
  }
  std::printf("RPC_TRAILING_GROUPS=%d PASS\n", gGroups);
  return gGroups == 4 ? 0 : 1;
}
