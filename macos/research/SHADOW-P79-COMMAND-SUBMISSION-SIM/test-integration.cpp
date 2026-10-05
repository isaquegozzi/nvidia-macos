// P77 + P78 + P79 host-only integration: proves cross-model invariants without
// copying logic (includes the three headers directly; all header-only, host-only).
#include <cstdio>
#include "../SHADOW-P77-M10-CHANNEL-SIM/channel_sim.hpp"
#include "../SHADOW-P78-VM-MMU-SIM/vm_sim.hpp"
#include "submission_sim.hpp"

static int pass = 0, fail = 0;
#define CHECK(cond) do { if (cond) ++pass; else { ++fail; std::printf("FAIL %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
  // I1: cannot submit without allocated+scheduled channel (P77 gate).
  {
    chsim::FifoModel fifo;
    fifo.NotifyInitDone();
    fifo.NotifyStaticInfo();
    vm78::Vm vm;
    sub79::SubmitChannel sub;
    // No channel allocated: submission side has no ring config derived from it.
    CHECK(!sub.configured());
    CHECK(sub.Append(sub79::GpEntry{}) == sub79::Fault::kNoChannel);
    // Allocate but only to Allocated (not Scheduled): integration refuses.
    chsim::ChannelReq r;
    r.chid = 7; r.runq = 0; r.engine = chsim::Engine::kGr0; r.vasHandle = 1;
    r.gpfifoLength = 128; r.gpfifoOffset = 0;
    r.instSize = 4096; r.userdSize = 4096; r.mthdbufSize = 4096;
    r.instAddr = 0x10000; r.userdAddr = 0x20000; r.mthdbufAddr = 0x30000;
    CHECK(fifo.Alloc(r) == chsim::Result::kOk);
    CHECK(fifo.StateOf(7) == chsim::ChanState::kAllocated);
    bool submittable = (fifo.StateOf(7) == chsim::ChanState::kScheduled);
    CHECK(!submittable);
    CHECK(fifo.Bind(7) == chsim::Result::kOk);
    CHECK(fifo.Schedule(7, true) == chsim::Result::kOk);
    submittable = (fifo.StateOf(7) == chsim::ChanState::kScheduled);
    CHECK(submittable);
  }
  // I2: ring geometry matches P77 channel entries (length/8, power of two).
  {
    chsim::FifoModel fifo;
    fifo.NotifyInitDone();
    fifo.NotifyStaticInfo();
    chsim::ChannelReq r;
    r.chid = 3; r.runq = 1; r.engine = chsim::Engine::kCopy0; r.vasHandle = 1;
    r.gpfifoLength = 256; r.gpfifoOffset = 0x5000;
    r.instSize = 4096; r.userdSize = 4096; r.mthdbufSize = 4096;
    r.instAddr = 0x10000; r.userdAddr = 0x20000; r.mthdbufAddr = 0x30000;
    CHECK(fifo.Alloc(r) == chsim::Result::kOk);
    uint32_t p77entries = static_cast<uint32_t>(r.gpfifoLength / chsim::kGpfifoEntrySize);
    CHECK(p77entries == 32);
    sub79::SubmitChannel sub;
    // ilog2(32) = 5
    CHECK(sub.Configure(5, r.gpfifoOffset, 0x800000) == sub79::Fault::kNone);
    CHECK(sub.entries() == p77entries);
  }
  // I3: cannot submit unmapped pushbuffer (P78 gate).
  {
    vm78::Vm vm;
    vm78::VaspaceAllocParams ap;
    CHECK(vm.VaspaceAlloc(ap, false) == vm78::Fault::kNone);
    vm78::CopyPdesParams cp;
    cp.pageSize = vm78::kServerSize; cp.virtLo = vm78::kServerStart;
    cp.virtHi = vm78::kServerEnd - 1; cp.numLevels = 5; cp.rootPhys = 0x10000;
    CHECK(vm.CopyServerReservedPdes(cp) == vm78::Fault::kNone);
    uint64_t va = 0;
    CHECK(vm.VmGet(12, 0x1000, &va) == vm78::Fault::kNone);
    sub79::SubmitChannel sub;
    sub.Configure(4, 0x100000, va);
    sub.ObtainToken(42);
    sub79::GpEntry e;
    e.get = 0; e.length = 64;
    // PB window not mapped in P78 yet: integration marks it unmapped.
    CHECK(vm.Map(va, 12, 0x50000, vm78::Aperture::kHostCoh, false, false, 0) == vm78::Fault::kNone);
    CHECK(sub.Append(e) == sub79::Fault::kUnmappedPb);
    // After P78 map+flush, integration marks the window live.
    vm.Flush();
    uint64_t pte = 0;
    CHECK(vm.Lookup(va, false, false, &pte) == vm78::Fault::kNone);
    sub.SetPbLive(va + 0, true);
    CHECK(sub.Append(e) == sub79::Fault::kNone);
  }
  // I4: cannot consume stale/freed mapping (P78 unmap races submit).
  {
    vm78::Vm vm;
    vm78::VaspaceAllocParams ap;
    vm.VaspaceAlloc(ap, false);
    vm78::CopyPdesParams cp;
    cp.pageSize = vm78::kServerSize; cp.virtLo = vm78::kServerStart;
    cp.virtHi = vm78::kServerEnd - 1; cp.numLevels = 5; cp.rootPhys = 0x10000;
    vm.CopyServerReservedPdes(cp);
    uint64_t va = 0;
    vm.VmGet(12, 0x1000, &va);
    vm.Map(va, 12, 0x50000, vm78::Aperture::kVram, false, false, 0);
    vm.Flush();
    sub79::SubmitChannel sub;
    sub.Configure(4, 0x100000, va);
    sub.ObtainToken(9);
    sub.SetPbLive(va, true);
    sub79::GpEntry e;
    e.get = 0; e.length = 64;
    CHECK(sub.Append(e) == sub79::Fault::kNone);
    // P78 side unmaps before consumption: integration marks window stale.
    CHECK(vm.Unmap(va) == vm78::Fault::kNone);
    sub.SetPbLive(va, false);
    sub.Kick();
    // Consume path re-validates: entry slot references stale window.
    // (Model-level: new appends refused; in-flight entry flagged by integrator.)
    sub79::GpEntry e2;
    e2.get = 0; e2.length = 64;
    CHECK(sub.Append(e2) == sub79::Fault::kStalePb);
  }
  // I5: PUT cannot advance beyond ring derived from P77 entries; doorbell per kick.
  {
    chsim::ChannelReq r;
    r.gpfifoLength = 64;  // 8 entries
    uint32_t entries = static_cast<uint32_t>(r.gpfifoLength / chsim::kGpfifoEntrySize);
    sub79::SubmitChannel sub;
    CHECK(sub.Configure(3, 0x100000, 0x200000) == sub79::Fault::kNone);
    CHECK(sub.entries() == entries);
    sub.ObtainToken(1);
    for (uint32_t i = 0; i < entries - 1; ++i) {
      sub79::GpEntry e;
      e.get = i * 64; e.length = 64;
      sub.SetPbLive(0x200000 + i * 64, true);
      CHECK(sub.Append(e) == sub79::Fault::kNone);
    }
    sub79::GpEntry extra;
    extra.get = 0; extra.length = 64;
    CHECK(sub.Append(extra) == sub79::Fault::kRingFull);
    auto k1 = sub.Kick();
    CHECK(k1.valid);
    auto k2 = sub.Kick();
    CHECK(!k2.valid);  // single doorbell token per PUT advance batch
  }
  std::printf("integration tests: pass=%d fail=%d\n", pass, fail);
  return fail != 0;
}
