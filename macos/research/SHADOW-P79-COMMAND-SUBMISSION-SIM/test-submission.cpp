#include <cstdio>
#include "submission_sim.hpp"

static int pass = 0, fail = 0;
#define CHECK(cond) do { if (cond) ++pass; else { ++fail; std::printf("FAIL %d: %s\n", __LINE__, #cond); } } while (0)
using sub79::Fault;
using sub79::SubmitChannel;

static SubmitChannel freshChan(uint32_t pow2 = 4) {
  SubmitChannel c;
  c.Configure(pow2, 0x100000, 0x200000);
  c.ObtainToken(0xABCD);
  return c;
}
static sub79::GpEntry liveEntry(SubmitChannel& c, uint32_t off, uint32_t len = 64) {
  sub79::GpEntry e;
  e.get = off; e.length = len;
  c.SetPbLive(0x200000 + off, true);
  return e;
}

int main() {
  // §1 happy path
  {
    SubmitChannel c = freshChan();
    CHECK(c.entries() == 16);
    sub79::GpEntry e = liveEntry(c, 0);
    CHECK(c.Append(e) == Fault::kNone);
    CHECK(c.Used() == 1);
    auto k = c.Kick();
    CHECK(k.valid);
    CHECK(c.Consume() == Fault::kNone);
    CHECK(c.Used() == 0);
    CHECK(c.lastConsumed().get == 0);
    uint64_t f = c.Release(0x10);
    CHECK(c.reference() == f);
    CHECK(c.Acquire(f) == Fault::kNone);
    CHECK(c.FreeFence(f) == Fault::kNone);
    CHECK(c.ReleaseToken() == Fault::kNone);
    CHECK(c.Teardown() == Fault::kNone);
  }
  // §2 entry alignment / length / opcode
  {
    uint32_t w[2];
    sub79::GpEntry e;
    e.get = 1; e.length = 64;
    CHECK(sub79::EncodeEntry(e, w) == Fault::kAlign);
    e.get = 0; e.length = 0;
    CHECK(sub79::EncodeEntry(e, w) == Fault::kLengthZero);
    e.length = sub79::kMaxEntryLength + 1;
    CHECK(sub79::EncodeEntry(e, w) == Fault::kLengthOverflow);
    e.length = 64; e.op = sub79::Opcode::kIllegal;
    CHECK(sub79::EncodeEntry(e, w) == Fault::kInvalidEntry);
    e.op = static_cast<sub79::Opcode>(9);
    CHECK(sub79::EncodeEntry(e, w) == Fault::kInvalidEntry);
    e.op = sub79::Opcode::kNop; e.sub = true; e.wait = true; e.condFetch = true;
    e.getHi = 0xAB;
    CHECK(sub79::EncodeEntry(e, w) == Fault::kNone);
    sub79::DecodedEntry d = sub79::DecodeEntry(w[0], w[1]);
    CHECK(d.fault == Fault::kNone && d.entry.sub && d.entry.wait && d.entry.condFetch);
    CHECK(d.entry.getHi == 0xAB && d.entry.length == 64);
    sub79::DecodedEntry z = sub79::DecodeEntry(0, 0);
    CHECK(z.fault == Fault::kInvalidEntry);
    sub79::DecodedEntry zl = sub79::DecodeEntry(0x100, (5U << 10));
    CHECK(zl.fault == Fault::kNone && zl.entry.length == 5);
  }
  // §3 ring full / empty / wrap
  {
    SubmitChannel c = freshChan(2);  // 4 entries, 3 usable
    for (int i = 0; i < 3; ++i) {
      sub79::GpEntry e = liveEntry(c, (uint32_t)(i * 64));
      CHECK(c.Append(e) == Fault::kNone);
    }
    sub79::GpEntry e4 = liveEntry(c, 3 * 64);
    CHECK(c.Append(e4) == Fault::kRingFull);
    c.Kick();
    CHECK(c.Consume() == Fault::kNone);
    CHECK(c.Append(e4) == Fault::kNone);
    c.Kick();
    while (c.Used() > 0) CHECK(c.Consume() == Fault::kNone);
    CHECK(c.Consume() == Fault::kRingEmpty);
    SubmitChannel d;
    CHECK(d.Consume() == Fault::kNoChannel);
    CHECK(d.Append(e4) == Fault::kNoChannel);
  }
  // §4 PUT wrap: fill/drain cycles advance indices mod ring
  {
    SubmitChannel c = freshChan(2);
    for (int round = 0; round < 6; ++round) {
      for (int i = 0; i < 3; ++i) {
        sub79::GpEntry e = liveEntry(c, (uint32_t)((round * 3 + i) * 64));
        CHECK(c.Append(e) == Fault::kNone);
      }
      c.Kick();
      for (int i = 0; i < 3; ++i) CHECK(c.Consume() == Fault::kNone);
    }
    CHECK(c.put() == 18 && c.get() == 18);
    CHECK(c.Used() == 0);
  }
  // §5 pushbuffer residency: unmapped / stale
  {
    SubmitChannel c = freshChan();
    sub79::GpEntry e;
    e.get = 0x400; e.length = 64;
    CHECK(c.Append(e) == Fault::kUnmappedPb);
    c.SetPbLive(0x200000 + 0x400, false);
    CHECK(c.Append(e) == Fault::kStalePb);
    c.SetPbLive(0x200000 + 0x400, true);
    CHECK(c.Append(e) == Fault::kNone);
  }
  // §6 packets: classes, counts, subchannel, methods
  {
    uint32_t w = 0;
    sub79::Packet p;
    p.address = 0x08; p.subchannel = 2; p.sec = sub79::SecOp::kInc; p.count = 4;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kNone);
    CHECK(sub79::PacketWordCount(p) == 5);
    CHECK(((w >> 13) & 7U) == 2U && ((w >> 16) & 0x1FFFU) == 4U && ((w >> 29) & 7U) == 1U);
    p.count = 0;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kCountZero);
    p.count = sub79::kMaxMethodCount + 1;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kCountZero);
    p.count = 1; p.sec = sub79::SecOp::kReserved6;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kReservedEncoding);
    p.sec = sub79::SecOp::kNonInc; p.count = 2; p.subchannel = 8;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kSubchannelRange);
    p.subchannel = 7; p.address = 0xFFF;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kUnknownMethod);
    p.address = 0x1000;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kUnknownMethod);
    p.address = 0x10; p.sec = sub79::SecOp::kImmd; p.immd = 0x100;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kNone);
    CHECK(sub79::PacketWordCount(p) == 1);
    p.immd = 0x2000;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kPacketOverflow);
    p.sec = sub79::SecOp::kEndSegment; p.count = 3;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kPacketOverflow);
    p.count = 0;
    CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kNone);
    for (uint32_t a : {0x00U, 0x04U, 0x08U, 0x10U, 0x14U, 0x18U, 0x1CU, 0x20U,
                       0x24U, 0x28U, 0x2CU, 0x30U, 0x34U, 0x50U, 0x6CU, 0x78U,
                       0x80U, 0x84U}) {
      p.address = a; p.sec = sub79::SecOp::kInc; p.count = 1; p.subchannel = 0;
      CHECK(sub79::EncodePacketHeader(p, &w) == Fault::kNone);
    }
  }
  // §7 lifecycle ordering: token, kick, teardown, error pin
  {
    SubmitChannel c;
    CHECK(c.Configure(0, 0x1000, 0x2000) == Fault::kPutOverflow);
    CHECK(c.Configure(32, 0x1000, 0x2000) == Fault::kPutOverflow);
    CHECK(c.Configure(4, 0x1001, 0x2000) == Fault::kAlign);
    CHECK(c.Configure(4, 0x1000, 0x2000) == Fault::kNone);
    CHECK(c.Configure(4, 0x1000, 0x2000) == Fault::kInvalidEntry);
    CHECK(c.ObtainToken(0) == Fault::kTokenMissing);
    sub79::GpEntry e;
    e.get = 0; e.length = 64;
    c.SetPbLive(0x2000, true);
    CHECK(c.Append(e) == Fault::kTokenMissing);
    CHECK(c.ObtainToken(7) == Fault::kNone);
    CHECK(c.ObtainToken(8) == Fault::kInvalidEntry);
    CHECK(c.Append(e) == Fault::kNone);
    CHECK(c.Consume() == Fault::kUnkicked);
    c.Kick();
    CHECK(c.Teardown() == Fault::kPrematureFree);
    CHECK(c.ReleaseToken() == Fault::kPrematureFree);
    CHECK(c.InjectError() == Fault::kNone);
    CHECK(c.Append(e) == Fault::kErrorPinned);
    CHECK(c.Consume() == Fault::kErrorPinned);
  }
  // §8 fences: mismatch / stale / premature
  {
    SubmitChannel c = freshChan();
    CHECK(c.Acquire(999) == Fault::kStaleFence);
    CHECK(c.FreeFence(999) == Fault::kStaleFence);
    uint64_t f1 = c.Release(1);
    uint64_t f2 = c.Release(2);
    CHECK(f2 == f1 + 1 && c.reference() == f2);
    CHECK(c.Acquire(f1) == Fault::kNone);
    CHECK(c.FreeFence(f1) == Fault::kNone);
    CHECK(c.Acquire(f1) == Fault::kStaleFence);
    CHECK(c.Teardown() == Fault::kPrematureFree);
    CHECK(c.ReleaseToken() == Fault::kNone);
    CHECK(c.Teardown() == Fault::kNone);
  }
  std::printf("submission tests: pass=%d fail=%d\n", pass, fail);
  return fail != 0;
}
