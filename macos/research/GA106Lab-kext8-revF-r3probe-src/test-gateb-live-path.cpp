// test-gateb-live-path.cpp — offline R2 tests of RunGateBProgramFlow<GateBMockOps>.
// Covers §18 list (UC-vs-WC, success, order, counts, BME, BAR/map/mode, width,
// align, seg/len, roundtrip, stop/duplicate/teardown/close/release, unknown)
// + precedence + validator/CLI + latch one-way. Deterministic; 1 pthread group.
// No IOKit live, no HW, no sudo. R2 test-only (production in flow header).

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include "GA106LabProtocol.h"
#include "GA106LabGateBWriteFlow.hpp"
#include "GA106LabMockOps.hpp"
#include "ga106ctl-validate.h"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

struct ThreadFlowArg {
    GateBMockOps *m;
    GA106LabGateBProgramV1 *out;
    IOReturn *kr;
};

static void *runGateBFlowThread(void *a) {
    ThreadFlowArg *fa = (ThreadFlowArg *)a;
    *fa->kr = RunGateBProgramFlow(*fa->m, *fa->out);
    return NULL;
}

// Local encode copies (same contract as FakeBar harness; production encodes inline in flow).
static inline uint32_t TEncodeLo(uint64_t dma) { return (uint32_t)((dma >> 8u) & 0xFFFFFFFFu); }
static inline uint32_t TEncodeHi(uint64_t dma) { return (uint32_t)((dma >> 40u) & 0xFFFFFFu); }

static void assertBalanced(GateBMockOps &m, int expectCreates, int expectHi, int expectLo) {
    assert(m.mapCreateCount == expectCreates);
    assert(m.mapReleaseCount == expectCreates);
    assert(m.acquireCount == m.providerReleaseCount);  // pin 1:1 em todo caminho pós-reserva
    assert(m.bmeEnableCount == 0 && m.configWriteCount == 0 && m.mmioOtherWrites == 0);
    assert(m.genericWrites == 0);
    assert(m.hiWrites == expectHi && m.loWrites == expectLo);
    assert(m.isBusy() == false);
}

int main(void) {
    // L1: happy success — exactly 2 stores HI,LO + latch + ABI + validator.
    {
        GateBMockOps m;
        GA106LabGateBProgramV1 out;
        GateBMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGateBProgramFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.size == 48 && out.version == 1);
        assert(out.status == kGA106LabGateBStatus_Success);
        assert(out.phase == kGA106LabGateBPhase_Programmed);
        assert(out.preconditionsReady == 1 && out.ucMappingReady == 1);
        assert(out.bmeEnabled == 0 && out.pageReady == 1);
        assert(out.addressRepresentable == 1 && out.encodingRoundtripReady == 1);
        assert(out.writeLatchBefore == kGateBLatch_NeverTouched);
        assert(out.writeLatchAfter == kGateBLatch_HIAndLOWritten);
        assert(out.hiWriteAttempted == 1 && out.hiWriteCompletedSoftwareSide == 1);
        assert(out.loWriteAttempted == 1 && out.loWriteCompletedSoftwareSide == 1);
        assert(out.writeCount == 2 && out.recoveryRequired == 0);
        assert(m.hiWrites == 1 && m.loWrites == 1);
        assert(m.writeSeqN == 2 && m.writeSeq[0] == GA106LAB_GATEB_HI_OFFSET &&
               m.writeSeq[1] == GA106LAB_GATEB_LO_OFFSET);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);  // pin 1:1
        assert(m.barHi == TEncodeHi(0x1000ULL));  // HI == 0 here
        assert(m.barLo == TEncodeLo(0x1000ULL));  // LO == 0x10
        assert(m.wprSentinel == 0xA5A5A5A5u);
        assertBalanced(m, 1, 1, 1);
        assert(ga106ctl_check_gateb_program(&out, sizeof(out)) == 0);
        assert(ga106ctl_gateb_program_cli_exit(1, &out, sizeof(out)) == 0);
        assert(ga106ctl_gateb_program_cli_exit(0, &out, sizeof(out)) != 0);
    }
    PASS("L1 HAPPY success 2-stores latch ABI validator");

    // L2: precondition failures -> zero stores, exact statuses.
    {
        struct Case { const char *name; int setup; uint32_t want; };
        // setup codes exercised below one by one for exact-status asserts.
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.providerPresent = false;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_ProviderInvalid);
            assert(out.writeCount == 0 && m.hiWrites == 0 && m.loWrites == 0);
            assert(m.mapCreateCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.command = 0x0000u; m.commandAfter = 0x0000u;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_MseDisabled);
            assert(out.writeCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.barError = true;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_BarUnavailable);
            assert(out.writeCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.gfwReady = false;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_GfwNotReady);
            assert(out.writeCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.command = 0x0006u; m.commandAfter = 0x0006u;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_BmeEnabled);
            assert(out.bmeEnabled == 1 && out.writeCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.pagePresent = false;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_PageInvalid);
            assert(out.writeCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.segCount = 2;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_SegmentInvalid);
            assert(out.writeCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.pageLen = 2048u;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_LengthInvalid);
            assert(out.writeCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.pageAddr = 0x1001u;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_AlignmentInvalid);
            assert(out.writeCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.pageAddr = (1ULL << 47);
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_AddressNotRepresentable);
            assert(out.writeCount == 0);
            assertBalanced(m, 0, 0, 0);
        }
    }
    PASS("L2 PRECONDITION-FAILS zero-stores exact-status");

    // L3: map/mode failures.
    {
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.mapMode = 3;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_UcMappingUnavailable);
            assert(out.writeCount == 0);
            assert(m.mapCreateCount == 1 && m.mapReleaseCount == 0);
            assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
            assert(m.isBusy() == false);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.mapMode = 1;  // WC must fail exact-mode check
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_UcSemanticsUnproven);
            assert(out.writeCount == 0);
            assertBalanced(m, 1, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.mapLenOverride = 0x1000ULL;  // short map
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_UcMappingUnavailable);
            assert(out.writeCount == 0);
            assertBalanced(m, 1, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.vaZero = true;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_UcMappingUnavailable);
            assert(out.writeCount == 0);
            assertBalanced(m, 1, 0, 0);
        }
    }
    PASS("L3 MAP-MODE-FAILS (null/WC/short/VA0)");

    // L4: stopping/busy/latch.
    {
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.stopping = true;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnAborted);
            assert(out.status == kGA106LabGateBStatus_Stopping);
            assert(out.writeCount == 0 && m.mapCreateCount == 0);
            assert(m.acquireCount == 0 && m.providerReleaseCount == 0);
            assert(m.isBusy() == false);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.busy = true;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnBusy);
            assert(out.writeCount == 0);
            assert(m.acquireCount == 0 && m.providerReleaseCount == 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.latch = kGateBLatch_HIAndLOWritten;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_AlreadyProgrammed);
            assert(out.phase == kGA106LabGateBPhase_Programmed);
            assert(out.writeCount == 0 && m.mapCreateCount == 0);
            assert(m.acquireCount == 0 && m.providerReleaseCount == 0);
            assert(m.isBusy() == false);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.latch = kGateBLatch_UnknownPartial;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_RecoveryRequired);
            assert(out.recoveryRequired == 1 && out.writeCount == 0);
            assert(m.acquireCount == 0 && m.providerReleaseCount == 0);
            assert(m.isBusy() == false);
        }
    }
    PASS("L4 STOPPING-BUSY-LATCH (abort/busy/already/recovery)");

    // L5: BME flip at re-read + transport fail + stop-between + duplicate.
    {
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.commandAfter = 0x0006u;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_BmeEnabled);
            assert(out.bmeEnabled == 1 && out.writeCount == 0);
            assert(out.writeLatchAfter == kGateBLatch_NeverTouched);
            assertBalanced(m, 1, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.commandAfterFail = true;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_InternalError);
            assert(out.writeCount == 0);
            assertBalanced(m, 1, 0, 0);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 out;
            GateBMockMakeLive(m); m.stopAfterHi = true;
            memset(&out, 0, sizeof(out));
            assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabGateBStatus_UnknownPartial);
            assert(out.recoveryRequired == 1);
            assert(out.writeCount == 1 && out.hiWriteAttempted == 1);
            assert(out.writeLatchAfter == kGateBLatch_UnknownPartial);
            assert(m.latch == kGateBLatch_UnknownPartial);  // latch reflete parcial
            assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
            assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
            assert(m.isBusy() == false);
        }
        {
            GateBMockOps m; GA106LabGateBProgramV1 o1, o2;
            GateBMockMakeLive(m);
            memset(&o1, 0, sizeof(o1)); memset(&o2, 0, sizeof(o2));
            assert(RunGateBProgramFlow(m, o1) == kIOReturnSuccess);
            assert(o1.status == kGA106LabGateBStatus_Success);
            assert(RunGateBProgramFlow(m, o2) == kIOReturnSuccess);
            assert(o2.status == kGA106LabGateBStatus_AlreadyProgrammed);
            assert(m.hiWrites == 1 && m.loWrites == 1);  // no second program
        }
    }
    PASS("L5 REREAD-FLIP + TRANSPORT-FAIL + STOP-BETWEEN + DUPLICATE");

    // L6: close-after-success keeps latch; close-after-partial keeps UnknownPartial.
    {
        GateBMockOps m; GA106LabGateBProgramV1 o1;
        GateBMockMakeLive(m);
        memset(&o1, 0, sizeof(o1));
        assert(RunGateBProgramFlow(m, o1) == kIOReturnSuccess);
        m.clientClose();  // stopping=true post-success
        assert(m.latch == kGateBLatch_HIAndLOWritten);  // close never clears latch
        GA106LabGateBProgramV1 o2;
        memset(&o2, 0, sizeof(o2));
        assert(RunGateBProgramFlow(m, o2) == kIOReturnAborted);  // stopping wins pre-latch
        assert(m.latch == kGateBLatch_HIAndLOWritten);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
    }
    PASS("L6 CLOSE-AFTER-SUCCESS latch persists");

    // L7: validator rejects non-success shapes.
    {
        GateBMockOps m; GA106LabGateBProgramV1 out;
        GateBMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunGateBProgramFlow(m, out) == kIOReturnSuccess);
        GA106LabGateBProgramV1 bad = out;
        bad.status = kGA106LabGateBStatus_UnknownPartial;
        assert(ga106ctl_check_gateb_program(&bad, sizeof(bad)) != 0);
        bad = out; bad.writeCount = 3;
        assert(ga106ctl_check_gateb_program(&bad, sizeof(bad)) != 0);
        bad = out; bad.recoveryRequired = 1;
        assert(ga106ctl_check_gateb_program(&bad, sizeof(bad)) != 0);
        bad = out; bad.bmeEnabled = 1;
        assert(ga106ctl_check_gateb_program(&bad, sizeof(bad)) != 0);
        bad = out; bad.reserved[0] = 1;
        assert(ga106ctl_check_gateb_program(&bad, sizeof(bad)) != 0);
        bad = out; bad.writeLatchAfter = 9;
        assert(ga106ctl_check_gateb_program(&bad, sizeof(bad)) != 0);
        assert(ga106ctl_check_gateb_program(NULL, sizeof(out)) != 0);
        assert(ga106ctl_check_gateb_program(&out, 16) != 0);
    }
    PASS("L7 VALIDATOR rejects non-success shapes");

    // L8: duplicata concorrente.
    // L8a determinístico: reserva pré-segurada => thread recebe Busy sem tocar HW.
    {
        GateBMockOps m;
        GA106LabGateBProgramV1 oA;
        IOReturn krA = kIOReturnError;
        pthread_t thA;
        ThreadFlowArg fa;
        GateBMockMakeLive(m);
        m.setBusy(true);  // reserva detida => qualquer chamada recusa
        memset(&oA, 0, sizeof(oA));
        fa.m = &m; fa.out = &oA; fa.kr = &krA;
        assert(pthread_create(&thA, NULL, runGateBFlowThread, &fa) == 0);
        assert(pthread_join(thA, NULL) == 0);
        assert(krA == kIOReturnBusy);
        assert(m.hiWrites == 0 && m.loWrites == 0 && m.mapCreateCount == 0);
        assert(m.acquireCount == 0 && m.providerReleaseCount == 0);
        m.setBusy(false);
    }
    // L8b contenção real sob TSAN: pares concorrentes, N iterações. A reserva é
    // guardada por mutex (inspecionado), logo exatamente-um-prossegue por par;
    // o desfecho por par (Busy vs AlreadyProgrammed) depende do schedule e AMBOS
    // são aceitos; invariantes valem em todo schedule (fail-closed em corrida).
    {
        for (int iter = 0; iter < 50; iter++) {
            GateBMockOps m;
            GA106LabGateBProgramV1 oA, oB;
            IOReturn krA = kIOReturnError, krB = kIOReturnError;
            pthread_t thA, thB;
            ThreadFlowArg fa, fb;
            GateBMockMakeLive(m);
            memset(&oA, 0, sizeof(oA)); memset(&oB, 0, sizeof(oB));
            fa.m = &m; fa.out = &oA; fa.kr = &krA;
            fb.m = &m; fb.out = &oB; fb.kr = &krB;
            assert(pthread_create(&thA, NULL, runGateBFlowThread, &fa) == 0);
            assert(pthread_create(&thB, NULL, runGateBFlowThread, &fb) == 0);
            assert(pthread_join(thA, NULL) == 0);
            assert(pthread_join(thB, NULL) == 0);
            int progA = (krA == kIOReturnSuccess && oA.status == kGA106LabGateBStatus_Success);
            int progB = (krB == kIOReturnSuccess && oB.status == kGA106LabGateBStatus_Success);
            assert(!(progA && progB));  // nunca duas programações
            assert(m.hiWrites <= 1 && m.loWrites <= 1);
            assert(m.isBusy() == false);
            assert(m.acquireCount == m.providerReleaseCount);
            assert(m.mapCreateCount == m.mapReleaseCount);
        }
    }
    PASS("L8 CONCURRENT-DUPLICATE deterministic Busy + contention loop");

    // L9: stop pousa durante o mapa => Aborted pré-HI, zero stores, tudo liberado.
    {
        GateBMockOps m; GA106LabGateBProgramV1 out;
        GateBMockMakeLive(m); m.stopDuringMap = true;
        memset(&out, 0, sizeof(out));
        assert(RunGateBProgramFlow(m, out) == kIOReturnAborted);
        assert(out.status == kGA106LabGateBStatus_Stopping);
        assert(out.writeCount == 0 && m.hiWrites == 0 && m.loWrites == 0);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
        assert(out.writeLatchAfter == kGateBLatch_NeverTouched);
        assert(m.isBusy() == false);
    }
    PASS("L9 STOP-DURING-MAP Aborted pre-HI");

    printf("test-gateb-live-path: ALL PASS (%d grupos; shared flow + mock)\n", gGroups);
    return 0;
}
