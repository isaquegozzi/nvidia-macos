#include <cstdio>
#include "vm_sim.hpp"

static int pass = 0, fail = 0;
#define CHECK(cond) do { if (cond) ++pass; else { ++fail; std::printf("FAIL %d: %s\n", __LINE__, #cond); } } while (0)
using vm78::Fault;
using vm78::Vm;

static Vm freshInternal() {
  Vm v;
  vm78::VaspaceAllocParams ap;
  if (v.VaspaceAlloc(ap, false) != Fault::kNone) {}
  vm78::CopyPdesParams cp;
  cp.pageSize = vm78::kServerSize;
  cp.virtLo = vm78::kServerStart;
  cp.virtHi = vm78::kServerEnd - 1;
  cp.numLevels = 5;
  cp.rootPhys = 0x10000;
  if (v.CopyServerReservedPdes(cp) != Fault::kNone) {}
  return v;
}

int main() {
  // §1 happy path internal
  {
    Vm v;
    vm78::VaspaceAllocParams ap;
    CHECK(v.VaspaceAlloc(ap, false) == Fault::kNone);
    CHECK(v.VaspaceAlloc(ap, false) == Fault::kDuplicate);
    vm78::CopyPdesParams cp;
    cp.pageSize = vm78::kServerSize; cp.virtLo = vm78::kServerStart;
    cp.virtHi = vm78::kServerEnd - 1; cp.numLevels = 5; cp.rootPhys = 0x10000;
    CHECK(v.CopyServerReservedPdes(cp) == Fault::kNone);
    CHECK(v.CopyServerReservedPdes(cp) == Fault::kDuplicate);
    uint64_t va = 0;
    CHECK(v.VmGet(12, 0x1000, &va) == Fault::kNone);
    CHECK(va == 0);
    CHECK(v.Map(va, 12, 0x10000, vm78::Aperture::kHostCoh, false, false, 0) == Fault::kNone);
    uint64_t pte = 0;
    CHECK(v.Lookup(va, false, false, &pte) == Fault::kNeedsFlush);
    v.Flush();
    CHECK(v.Lookup(va, false, false, &pte) == Fault::kNone);
    CHECK((pte & vm78::kPteValid) != 0);
    CHECK(((pte >> vm78::kPteAperShift) & 3ULL) == 2ULL);
    CHECK(v.Unmap(va) == Fault::kNone);
    CHECK(v.Lookup(va, false, false, nullptr) == Fault::kStale);
    CHECK(v.VmPut(va) == Fault::kNone);
    CHECK(v.VaspaceFree() == Fault::kNone);
  }
  // §2 external path
  {
    Vm v;
    vm78::VaspaceAllocParams ap;
    ap.flags = vm78::kVaspaceFlagExtOwned;
    CHECK(v.VaspaceAlloc(ap, true) == Fault::kNone);
    vm78::SetPdParams sp;
    sp.physAddress = 0x20000; sp.numEntries = 256; sp.aperFlag = 0;
    CHECK(v.SetPageDirectory(sp) == Fault::kNone);
    CHECK(v.SetPageDirectory(sp) == Fault::kDuplicate);
    uint64_t va = 0;
    CHECK(v.VmGet(16, 0x10000, &va) == Fault::kNone);
    CHECK(v.Map(va, 16, 0x1000000, vm78::Aperture::kVram, false, false, 0) == Fault::kNone);
    v.Flush();
    CHECK(v.Lookup(va, true, false, nullptr) == Fault::kNone);
    CHECK(v.Unmap(va) == Fault::kNone);
    CHECK(v.VmPut(va) == Fault::kNone);
    CHECK(v.VaspaceFree() == Fault::kStale);
    CHECK(v.UnsetPageDirectory() == Fault::kNone);
    CHECK(v.VaspaceFree() == Fault::kNone);
  }
  // §3 page sizes leaf ok, dir-only rejected
  {
    Vm v = freshInternal();
    uint64_t va = 0;
    CHECK(v.VmGet(16, 0x10000, &va) == Fault::kNone);
    CHECK(v.VmGet(21, 0x200000, &va) == Fault::kNone);
    CHECK(v.VmGet(29, 0x20000000, &va) == Fault::kNone);
    CHECK(v.VmGet(38, 0x1000, &va) == Fault::kInvalidEncoding);
    CHECK(v.VmGet(47, 0x1000, &va) == Fault::kInvalidEncoding);
    CHECK(v.VmGet(13, 0x2000, &va) == Fault::kInvalidEncoding);
    CHECK(v.Map(0, 38, 0x10000, vm78::Aperture::kVram, false, false, 0) == Fault::kInvalidEncoding);
  }
  // §4 alignment
  {
    Vm v = freshInternal();
    uint64_t va = 0;
    CHECK(v.VmGet(12, 0x1000, &va) == Fault::kNone);
    CHECK(v.Map(1, 12, 0x10000, vm78::Aperture::kVram, false, false, 0) == Fault::kAlignment);
    CHECK(v.Map(va, 12, 0x10001, vm78::Aperture::kVram, false, false, 0) == Fault::kAlignment);
    CHECK(v.Map(va, 12, 0xFFF, vm78::Aperture::kVram, false, false, 0) == Fault::kAlignment);
    CHECK(v.VmGet(12, 0x1001, &va) == Fault::kAlignment);
    CHECK(v.VmGet(16, 0x1000, &va) == Fault::kAlignment);
    vm78::SetPdParams sp;
    sp.physAddress = 0x123; sp.numEntries = 1; sp.aperFlag = 0;
    Vm ve;
    vm78::VaspaceAllocParams ap;
    ap.flags = vm78::kVaspaceFlagExtOwned;
    ve.VaspaceAlloc(ap, true);
    CHECK(ve.SetPageDirectory(sp) == Fault::kAlignment);
  }
  // §5 VA/PA range + wrap + server hole
  {
    Vm v = freshInternal();
    uint64_t va = 0;
    CHECK(v.VmGet(12, 0x1000, &va) == Fault::kNone);
    CHECK(v.Map(vm78::kVaLimit, 12, 0x10000, vm78::Aperture::kVram, false, false, 0) == Fault::kOutOfRange);
    CHECK(v.Map(0xFFFFFFFFFFFFF000ULL, 12, 0x10000, vm78::Aperture::kVram, false, false, 0) == Fault::kWrap);
    CHECK(v.Map(va, 12, vm78::kMaxPa, vm78::Aperture::kVram, false, false, 0) == Fault::kOutOfRange);
    uint64_t sv = 0;
    CHECK(v.VmGet(29, 0x20000000, &sv) == Fault::kNone);
    CHECK(sv + 0x20000000ULL <= vm78::kServerStart || sv >= vm78::kServerEnd);
    CHECK(v.Map(vm78::kServerStart, 12, 0x10000, vm78::Aperture::kVram, false, false, 0) == Fault::kOutOfRange);
  }
  // §6 apertures: encodings + invalid
  {
    vm78::PteFields f;
    f.valid = true; f.pa = 0x10000;
    uint64_t raw = 0;
    f.aper = vm78::Aperture::kVram;
    CHECK(vm78::EncodePte(f, &raw) == Fault::kNone);
    CHECK(((raw >> 1) & 3ULL) == 0ULL);
    f.aper = vm78::Aperture::kHostCoh;
    CHECK(vm78::EncodePte(f, &raw) == Fault::kNone);
    CHECK(((raw >> 1) & 3ULL) == 2ULL);
    f.aper = vm78::Aperture::kNcoh;
    CHECK(vm78::EncodePte(f, &raw) == Fault::kNone);
    CHECK(((raw >> 1) & 3ULL) == 3ULL);
    f.aper = static_cast<vm78::Aperture>(1);
    CHECK(vm78::EncodePte(f, &raw) == Fault::kApertureMismatch);
    vm78::Decoded d = vm78::DecodePte((0x10000ULL >> 4) | (1ULL << 1) | vm78::kPteValid);
    CHECK(d.fault == Fault::kApertureMismatch);
    Vm v = freshInternal();
    uint64_t va = 0;
    v.VmGet(12, 0x1000, &va);
    CHECK(v.Map(va, 12, 0x10000, static_cast<vm78::Aperture>(1), false, false, 0) == Fault::kApertureMismatch);
    vm78::SetPdParams sp;
    sp.physAddress = 0x20000; sp.numEntries = 4; sp.aperFlag = 7;
    Vm ve;
    vm78::VaspaceAllocParams ap;
    ap.flags = vm78::kVaspaceFlagExtOwned;
    ve.VaspaceAlloc(ap, true);
    CHECK(ve.SetPageDirectory(sp) == Fault::kApertureMismatch);
  }
  // §7 permissions
  {
    Vm v = freshInternal();
    uint64_t va = 0, vb = 0;
    v.VmGet(12, 0x1000, &va);
    v.VmGet(12, 0x1000, &vb);
    CHECK(v.Map(va, 12, 0x10000, vm78::Aperture::kVram, true, false, 0) == Fault::kNone);
    CHECK(v.Map(vb, 12, 0x20000, vm78::Aperture::kVram, false, true, 0) == Fault::kNone);
    v.Flush();
    CHECK(v.Lookup(va, true, false, nullptr) == Fault::kPermission);
    CHECK(v.Lookup(va, false, false, nullptr) == Fault::kNone);
    CHECK(v.Lookup(vb, false, true, nullptr) == Fault::kPermission);
    CHECK(v.Lookup(vb, false, false, nullptr) == Fault::kNone);
    uint64_t raw = 0;
    vm78::PteFields f;
    f.valid = true; f.aper = vm78::Aperture::kVram; f.pa = 0x30000; f.ro = true;
    CHECK(vm78::EncodePte(f, &raw) == Fault::kNone);
    CHECK((raw & vm78::kPteRo) != 0);
    f.ro = false; f.priv = true;
    CHECK(vm78::EncodePte(f, &raw) == Fault::kNone);
    CHECK((raw & vm78::kPtePriv) != 0);
  }
  // §8 kind + invalid encodings
  {
    vm78::PteFields f;
    f.valid = true; f.aper = vm78::Aperture::kVram; f.pa = 0x10000;
    uint64_t raw = 0;
    f.kind = 7;
    CHECK(vm78::EncodePte(f, &raw) == Fault::kInvalidEncoding);
    f.kind = 16;
    CHECK(vm78::EncodePte(f, &raw) == Fault::kInvalidEncoding);
    f.kind = 3;
    CHECK(vm78::EncodePte(f, &raw) == Fault::kNone);
    CHECK((raw >> vm78::kPteKindShift) == 3ULL);
    Vm v;
    vm78::VaspaceAllocParams ap;
    ap.index = 99;
    CHECK(v.VaspaceAlloc(ap, false) == Fault::kInvalidEncoding);
    ap.index = 0; ap.flags = 0x10;
    CHECK(v.VaspaceAlloc(ap, false) == Fault::kInvalidEncoding);
    ap.flags = 0;
    CHECK(v.VaspaceAlloc(ap, false) == Fault::kNone);
    vm78::CopyPdesParams cp;
    cp.pageSize = 0x1000; cp.virtLo = vm78::kServerStart;
    cp.virtHi = vm78::kServerEnd - 1; cp.numLevels = 5; cp.rootPhys = 0x10000;
    CHECK(v.CopyServerReservedPdes(cp) == Fault::kInvalidEncoding);
    cp.pageSize = vm78::kServerSize; cp.numLevels = 0;
    CHECK(v.CopyServerReservedPdes(cp) == Fault::kInvalidEncoding);
    cp.numLevels = 7;
    CHECK(v.CopyServerReservedPdes(cp) == Fault::kInvalidEncoding);
  }
  // §9 lifetime errors: duplicate/overlap/unmap-missing/stale/sequencing
  {
    Vm v = freshInternal();
    CHECK(v.Map(0, 12, 0x10000, vm78::Aperture::kVram, false, false, 0) == Fault::kOutOfRange);
    uint64_t va = 0;
    v.VmGet(12, 0x2000, &va);
    CHECK(v.Map(va, 12, 0x10000, vm78::Aperture::kVram, false, false, 0) == Fault::kNone);
    CHECK(v.Map(va, 12, 0x20000, vm78::Aperture::kVram, false, false, 0) == Fault::kDuplicate);
    CHECK(v.Map(va + 0x1000, 12, 0x20000, vm78::Aperture::kVram, false, false, 0) == Fault::kNone);
    CHECK(v.Unmap(va + 0x5000) == Fault::kUnmapMissing);
    CHECK(v.VmPut(va + 0x9000) == Fault::kUnmapMissing);
    CHECK(v.VmPut(va) == Fault::kStale);
    CHECK(v.VaspaceFree() == Fault::kStale);
    v.Unmap(va); v.Unmap(va + 0x1000);
    CHECK(v.VmPut(va) == Fault::kNone);
    CHECK(v.Unmap(va) == Fault::kUnmapMissing);
    Vm w;
    uint64_t x = 0;
    CHECK(w.VmGet(12, 0x1000, &x) == Fault::kStale);
    CHECK(w.Map(0, 12, 0x10000, vm78::Aperture::kVram, false, false, 0) == Fault::kStale);
    CHECK(w.VaspaceFree() == Fault::kStale);
  }
  // §10 decode sentinels + round-trip oracle
  {
    vm78::Decoded z = vm78::DecodePte(0);
    CHECK(z.fault == Fault::kInvalidPte);
    vm78::Decoded s = vm78::DecodePte(vm78::kPteVol);
    CHECK(s.sparse && s.fault == Fault::kNone);
    vm78::Decoded l = vm78::DecodePte(vm78::kPtePriv | (4ULL << 4));
    CHECK(l.invalidLpte);
    int combos = 0;
    for (int sh : {12, 16, 21, 29}) {
      for (uint64_t a : {0ULL, 2ULL, 3ULL}) {
        for (int ro = 0; ro < 2; ++ro) {
          for (int pr = 0; pr < 2; ++pr) {
            vm78::PteFields f;
            f.valid = true;
            f.aper = static_cast<vm78::Aperture>(a);
            f.vol = (a == 2);
            f.ro = ro; f.priv = pr;
            f.pa = 0x10000; f.kind = 0;
            uint64_t raw = 0;
            CHECK(vm78::EncodePte(f, &raw) == Fault::kNone);
            vm78::Decoded d = vm78::DecodePte(raw);
            CHECK(d.fault == Fault::kNone && d.fields.pa == 0x10000ULL);
            CHECK(d.fields.ro == (ro != 0) && d.fields.priv == (pr != 0));
            (void)sh;
            ++combos;
          }
        }
      }
    }
    CHECK(combos == 48);
  }
  std::printf("vm tests: pass=%d fail=%d\n", pass, fail);
  return fail != 0;
}
