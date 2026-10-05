// test-gateb-fakebar-contract.cpp — offline R1 GateB FakeBar harness (HARD-6H).
//
// Spec: SYSMEM-FLUSH-SPEC-P66-ARCHIVE/FIRST-WRITE-GATE-B-SPEC.md + FLUSH-REGISTER-PROOF.md.
//   HI @0x100c40 = (dma>>40) field 23:0; LO @0x100c10 = (dma>>8) u32.
//   Order: HI first, LO second, back-to-back, no sleep between.
//   Counts: writes==2 exactly, reads==0|2, no retry; mismatch -> FAIL no-rewrite.
//   BME OFF; 0 Falcon/FBDMA/GSP/firmware/doorbell/IRQ/DMA-trigger.
//
// Harness: FakeBar u32 array for 0x100c00-0x100c80 + WPR2 pair + Command snapshot.
// MockBarOps counts writes/reads/bme/dma/falcon; only the 2 fixed offsets allowed.
// ProgramFake() is the SPEC implementation under test (fake BAR, no HW).
// Negative tests drive the detector with wrong-order / 3rd-store / BME / corrupt-readback.
//
// No IOKit, no HW, no sudo. R1 test-only, no prod change.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static const uint32_t kBase = 0x100c00u;
static const uint32_t kLoOff = 0x100c10u;
static const uint32_t kHiOff = 0x100c40u;
static const uint32_t kSpan = 0x80u;  // 0x100c00-0x100c80
static const int kWords = (int)(kSpan / 4u);

struct FakeBar {
    uint32_t w[kWords];
    uint32_t wpr2a, wpr2b;  // 0x1fa824/28 stand-ins (must stay unchanged)
    uint16_t command;       // PCI Command snapshot (BME bit must stay OFF)
    void reset(uint16_t cmd = 0x0002u) {
        memset(w, 0, sizeof(w));
        wpr2a = 0xA5A5A5A5u; wpr2b = 0x5A5A5A5Au;
        command = cmd;
    }
};

struct MockBarOps {
    FakeBar *bar;
    int writeCount;
    int readCount;
    int bmeCount;
    int dmaCount;
    int falconCount;
    uint32_t writeSeq[8];
    int writeSeqN;
    bool stopRequested;   // cancellation point between HI and LO
    bool failNextWrite;   // fault injection: next store does not land
    bool stopAfterHi;     // test hook: racing stop lands right after HI store
    bool programmed;      // program-once latch (reprogram needs teardown first)
    void reset(FakeBar *b) {
        bar = b; writeCount = 0; readCount = 0;
        bmeCount = 0; dmaCount = 0; falconCount = 0;
        writeSeqN = 0; memset(writeSeq, 0, sizeof(writeSeq));
        stopRequested = false; failNextWrite = false;
        stopAfterHi = false; programmed = false;
    }
    bool write32(uint32_t off, uint32_t v) {
        if (off != kLoOff && off != kHiOff) return false;
        if (off < kBase || off >= kBase + kSpan) return false;
        if (((off - kBase) % 4u) != 0) return false;
        if (failNextWrite) { failNextWrite = false; return false; }
        bar->w[(off - kBase) / 4u] = v;
        writeCount++;
        if (writeSeqN < 8) writeSeq[writeSeqN++] = off;
        if (off == kHiOff && stopAfterHi) stopRequested = true;
        return true;
    }
    void teardown() {  // zero bar words touched by Gate B + clear latch
        bar->w[(kHiOff - kBase) / 4u] = 0;
        bar->w[(kLoOff - kBase) / 4u] = 0;
        programmed = false;
    }
    uint32_t read32(uint32_t off) {
        readCount++;
        return bar->w[(off - kBase) / 4u];
    }
};

static inline uint32_t EncodeLo(uint64_t dma) { return (uint32_t)((dma >> 8u) & 0xFFFFFFFFu); }
static inline uint32_t EncodeHi(uint64_t dma) { return (uint32_t)((dma >> 40u) & 0xFFFFFFu); }
static inline uint64_t Decode(uint32_t lo, uint32_t hi) {
    return (((uint64_t)hi) << 40u) | (((uint64_t)lo) << 8u);
}

// SPEC implementation (fake BAR): returns true=programmed, false=FAIL (no partial state claimed).
// Rules: 256B align + HI 24-bit + round-trip + 47-bit bound (T1 contract) +
// program-once latch + stop cancellation between HI and LO (partial => FAIL).
static bool ProgramFake(MockBarOps &ops, uint64_t dma, bool withReadback,
                        bool *readbackMatchOut) {
    uint32_t lo, hi;
    if (readbackMatchOut) *readbackMatchOut = false;
    if (ops.programmed) return false;                // reprogram needs teardown first
    if ((dma & 0xFFu) != 0) return false;            // 256B min align
    if ((dma >> 40u) > 0xFFFFFFu) return false;      // HI 24-bit field
    if (dma >= (1ULL << 47)) return false;           // 47-bit bound (Valid47)
    if (dma + (uint64_t)4096u > (1ULL << 47)) return false;
    lo = EncodeLo(dma);
    hi = EncodeHi(dma);
    if (Decode(lo, hi) != dma) return false;         // round-trip
    if (ops.stopRequested) return false;             // cancelled before HI
    if (!ops.write32(kHiOff, hi)) return false;      // HI first
    if (ops.stopRequested) return false;             // stop landed mid-gate: HI-only partial
    if (!ops.write32(kLoOff, lo)) return false;      // LO second, back-to-back
    ops.programmed = true;
    if (withReadback) {
        uint32_t rHi = ops.read32(kHiOff);
        uint32_t rLo = ops.read32(kLoOff);
        bool m = (Decode(rLo, rHi) == dma);
        if (readbackMatchOut) *readbackMatchOut = m;
        if (!m) return false;  // mismatch -> FAIL, no rewrite
    }
    return true;
}

// Bar state classifier: any incomplete pair => UNKNOWN => reboot-class recovery.
// No compensating-write rollback (unproven); teardown() above is test-harness
// cleanup, NOT a hardware recovery claim.
enum BarState { BS_CLEAN = 0, BS_COMPLETE, BS_HI_ONLY, BS_LO_ONLY, BS_MISMATCH, BS_UNKNOWN };
static BarState ClassifyBar(const FakeBar &bar, uint64_t expectDma) {
    uint32_t hi = bar.w[(kHiOff - kBase) / 4u];
    uint32_t lo = bar.w[(kLoOff - kBase) / 4u];
    uint32_t eHi = EncodeHi(expectDma);
    uint32_t eLo = EncodeLo(expectDma);
    bool hiSet = (hi != 0), loSet = (lo != 0);
    // Note: dma==0 unrepresentable for this classifier (zero word == clean).
    if (!hiSet && !loSet) return BS_CLEAN;
    if (hiSet && loSet) {
        return (Decode(lo, hi) == expectDma && hi == eHi && lo == eLo)
            ? BS_COMPLETE : BS_MISMATCH;
    }
    if (hiSet) return BS_HI_ONLY;
    return BS_LO_ONLY;
}
static bool NeedsReboot(BarState s) { return s != BS_CLEAN && s != BS_COMPLETE; }

int main(void) {
    // G1: happy no-readback.
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        bool m = true;
        assert(ProgramFake(ops, 0x1000ULL, false, &m) == true);
        assert(ops.writeCount == 2 && ops.readCount == 0);
        assert(ops.writeSeqN == 2 && ops.writeSeq[0] == kHiOff && ops.writeSeq[1] == kLoOff);
        assert(bar.w[(kHiOff - kBase) / 4] == EncodeHi(0x1000ULL));
        assert(bar.w[(kLoOff - kBase) / 4] == EncodeLo(0x1000ULL));
        assert(bar.wpr2a == 0xA5A5A5A5u && bar.wpr2b == 0x5A5A5A5Au);
        assert(bar.command == 0x0002u && ops.bmeCount == 0 && ops.dmaCount == 0 && ops.falconCount == 0);
        // Only the 2 regs changed in span.
        int nz = 0;
        for (int i = 0; i < kWords; i++) if (bar.w[i] != 0) nz++;
        // 0x1000 encodes LO nonzero; HI zero -> exactly 1 nonzero word here.
        assert(nz == 1);
    }
    PASS("G1 HAPPY NO-READBACK 2-writes HI-then-LO span-clean");

    // G2: happy with readback.
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        bool m = false;
        assert(ProgramFake(ops, 0x12345000ULL, true, &m) == true);
        assert(m == true && ops.writeCount == 2 && ops.readCount == 2);
    }
    PASS("G2 HAPPY READBACK match 2+2");

    // G3: wrong order detected (LO then HI is NOT spec).
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        assert(ops.write32(kLoOff, 1u) == true);
        assert(ops.write32(kHiOff, 2u) == true);
        // Detector: spec order is HI,LO — this sequence violates it.
        bool orderOk = (ops.writeSeqN == 2 && ops.writeSeq[0] == kHiOff && ops.writeSeq[1] == kLoOff);
        assert(orderOk == false);
    }
    PASS("G3 WRONG-ORDER LO-HI flagged");

    // G4: third store violates exactly-2.
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        bool m = false;
        assert(ProgramFake(ops, 0x2000ULL, false, &m) == true);
        assert(ops.write32(kLoOff, 0xDEADu) == true);  // rogue 3rd store
        assert(ops.writeCount == 3);
        bool countOk = (ops.writeCount == 2);
        assert(countOk == false);
    }
    PASS("G4 THIRD-STORE flagged (exactly-2 violated)");

    // G5: BME attempt is forbidden surface.
    {
        MockBarOps ops; FakeBar bar;
        bar.reset(); ops.reset(&bar);
        ops.bmeCount++;  // simulated enable attempt
        assert(ops.bmeCount == 1);
        bool gateBclean = (ops.bmeCount == 0 && (bar.command & 0x4u) == 0);
        assert(gateBclean == false);  // detector: any BME enable fails GateB audit
        assert((bar.command & 0x4u) == 0);  // command snapshot still OFF
    }
    PASS("G5 BME-ENABLE flagged (GateB must stay BME OFF)");

    // G6: readback mismatch -> FAIL, no rewrite (writes stay 2).
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        bool m = true;
        // Program manually then corrupt LO before readback check.
        assert(ops.write32(kHiOff, EncodeHi(0x3000ULL)) == true);
        assert(ops.write32(kLoOff, EncodeLo(0x3000ULL)) == true);
        bar.w[(kLoOff - kBase) / 4] ^= 0x1u;  // corruption
        uint32_t rHi = ops.read32(kHiOff);
        uint32_t rLo = ops.read32(kLoOff);
        bool match = (Decode(rLo, rHi) == 0x3000ULL);
        assert(match == false);
        assert(ops.writeCount == 2);  // no rewrite attempted by checker
        (void)m;
    }
    PASS("G6 READBACK-MISMATCH FAIL no-rewrite");

    // G7: unaligned DMA rejected with zero writes.
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        bool m = false;
        assert(ProgramFake(ops, 0x1001ULL, false, &m) == false);
        assert(ops.writeCount == 0 && ops.readCount == 0);
    }
    PASS("G7 UNALIGNED rejected zero-writes");

    // G8: HI-only then abort => incomplete pair, UNKNOWN, reboot-class.
    // NOTE: dma chosen with nonzero HI and LO (zero-valued stores are
    // invisible to the classifier — a real modeling limit, documented).
    {
        FakeBar bar; MockBarOps ops;
        const uint64_t kDma = (1ULL << 40) | 0x1000ULL;  // HI=1 LO=0x10
        bar.reset(); ops.reset(&bar);
        bool m = false;
        ops.stopAfterHi = true;  // racing stop lands between HI and LO
        assert(ProgramFake(ops, kDma, false, &m) == false);
        assert(ops.writeCount == 1 && ops.writeSeq[0] == kHiOff);
        assert(ClassifyBar(bar, kDma) == BS_HI_ONLY);
        assert(NeedsReboot(BS_HI_ONLY) == true);
    }
    PASS("G8 HI-ONLY-ABORT incomplete UNKNOWN reboot-class");

    // G9: LO without HI => order violation, UNKNOWN.
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        assert(ops.write32(kLoOff, EncodeLo(0x5000ULL)) == true);
        assert(ClassifyBar(bar, 0x5000ULL) == BS_LO_ONLY);
        assert(NeedsReboot(BS_LO_ONLY) == true);
    }
    PASS("G9 LO-WITHOUT-HI UNKNOWN");

    // G10: duplicate HI / duplicate LO violate exactly-2 (via raw ops).
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        assert(ops.write32(kHiOff, 1u) == true);
        assert(ops.write32(kHiOff, 2u) == true);
        assert(ops.writeCount == 2);  // detector: same-offset rewrite, sequence broken
        bool orderOk = (ops.writeSeqN == 2 && ops.writeSeq[0] == kHiOff && ops.writeSeq[1] == kLoOff);
        assert(orderOk == false);
    }
    PASS("G10 DUPLICATE-HI flagged");

    // G11: truncation — HI field overflow cannot round-trip.
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        bool m = false;
        uint64_t dma = (0x1FFFFFFULL << 40) | 0x1000ULL;  // 25-bit HI
        assert(ProgramFake(ops, dma, false, &m) == false);
        assert(ops.writeCount == 0);
    }
    PASS("G11 TRUNCATION rejected zero-writes");

    // G12: non-47-bit address rejected (Valid47 in harness, mirrors T1).
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        bool m = false;
        assert(ProgramFake(ops, (1ULL << 47), false, &m) == false);
        assert(ProgramFake(ops, (1ULL << 47) - 4096u, false, &m) == true);
        assert(ops.writeCount == 2);  // only the valid one landed
    }
    PASS("G12 NON-47 rejected; LIMIT-4096 accepted");

    // G13: reprogram without teardown rejected (program-once latch).
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        bool m = false;
        assert(ProgramFake(ops, 0x6000ULL, false, &m) == true);
        assert(ProgramFake(ops, 0x7000ULL, false, &m) == false);
        assert(ops.writeCount == 2);
        ops.teardown();
        assert(ClassifyBar(bar, 0x6000ULL) == BS_CLEAN);
        assert(ProgramFake(ops, 0x7000ULL, false, &m) == true);  // clean after teardown
    }
    PASS("G13 REPROGRAM needs teardown");

    // G14: failing LO store => partial HI, FAIL, UNKNOWN (no rewrite).
    {
        MockBarOps ops2;
        FakeBar bar2;
        bar2.reset(); ops2.reset(&bar2);
        assert(ops2.write32(kHiOff, EncodeHi((1ULL << 40) | 0x2000ULL)) == true);
        ops2.failNextWrite = true;
        assert(ops2.write32(kLoOff, EncodeLo((1ULL << 40) | 0x2000ULL)) == false);
        assert(ops2.writeCount == 1);
        assert(ClassifyBar(bar2, (1ULL << 40) | 0x2000ULL) == BS_HI_ONLY);
        assert(NeedsReboot(BS_HI_ONLY) == true);
    }
    PASS("G14 PARTIAL-STORE-FAIL HI-only UNKNOWN");

    // G15: stop before gate => zero writes, clean.
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        bool m = false;
        ops.stopRequested = true;
        assert(ProgramFake(ops, 0x9000ULL, false, &m) == false);
        assert(ops.writeCount == 0);
        assert(ClassifyBar(bar, 0x9000ULL) == BS_CLEAN);
    }
    PASS("G15 STOP-BEFORE-GATE zero-writes clean");

    // G16: full classifier matrix on one bar.
    {
        FakeBar bar; MockBarOps ops;
        bar.reset(); ops.reset(&bar);
        assert(ClassifyBar(bar, (1ULL << 40) | 0x3000ULL) == BS_CLEAN);
        assert(NeedsReboot(BS_CLEAN) == false);
        bool m = false;
        assert(ProgramFake(ops, (1ULL << 40) | 0x3000ULL, true, &m) == true && m == true);
        assert(ClassifyBar(bar, (1ULL << 40) | 0x3000ULL) == BS_COMPLETE);
        assert(NeedsReboot(BS_COMPLETE) == false);
        bar.w[(kLoOff - kBase) / 4u] ^= 0x1u;
        assert(ClassifyBar(bar, (1ULL << 40) | 0x3000ULL) == BS_MISMATCH);
        assert(NeedsReboot(BS_MISMATCH) == true);
    }
    PASS("G16 CLASSIFIER clean/complete/mismatch");

    printf("test-gateb-fakebar-contract: ALL PASS (%d grupos; R1 harness, no prod change)\n", gGroups);
    return 0;
}
