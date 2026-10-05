// test-bme-race-contract.cpp — offline R1 race probe + regression (HARD-6H T3).
//
// History: proved BME checked once pre-reserve (checkTarget) and never
// re-checked after prepareDma unblocks (gap: flip invisible, stale success).
// Fix (HARD6H-T3): commandAfter re-read after prepareDma unblocks, before
// markAddressReady — BME ON or MSE OFF now => Success + Failed + teardown
// (fail_teardown_semantic), with out.bmeEnabled/out.mseEnabled refreshed.
// Transport failure of the re-read => Error + Failed + teardown.
// This test LOCKS the fix: the race case must now FAIL closed, never succeed.
//
// No IOKit, no HW, no sudo. R1 test-only, no live.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include "GA106LabProtocol.h"
#include "GA106LabFlushPrewriteFlow.hpp"
#include "GA106LabMockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

struct FlowArg {
    VerifyFlushMockOps *m;
    GA106LabFlushPrewriteV1 *out;
    IOReturn *kr;
};

static void *runFlow(void *a) {
    FlowArg *fa = (FlowArg *)a;
    *fa->kr = RunVerifyFlushPrewriteFlow(*fa->m, *fa->out);
    return NULL;
}

static void waitPrepareEntered(VerifyFlushMockOps &m) {
    for (int i = 0; i < 5000 && m.prepareCount < 1; i++) {
        usleep(1000);
    }
    assert(m.prepareCount >= 1);
    usleep(5000);  // grace: flow blocked inside prepareDma gate.
}

static void openPrepareGate(VerifyFlushMockOps &m) {
    pthread_mutex_lock(&m.gateMutex);
    m.gateOpen = true;
    pthread_cond_broadcast(&m.gateCond);
    pthread_mutex_unlock(&m.gateMutex);
}

int main(void) {
    // Control: no flip -> PreconditionsReady, bme 0.
    {
        VerifyFlushMockOps m;
        GA106LabFlushPrewriteV1 out;
        IOReturn kr = kIOReturnError;
        VerifyFlushMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        kr = RunVerifyFlushPrewriteFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabFlushPrewriteStatus_PreconditionsReady);
        assert(out.bmeEnabled == 0);
        assert(m.prepareCount == 1 && m.segmentGenCount == 1);
    }
    PASS("R0 CONTROL no-flip PreconditionsReady");

    // Race: BME flips ON while prepareDma blocked => must FAIL closed.
    {
        VerifyFlushMockOps m;
        GA106LabFlushPrewriteV1 out;
        IOReturn kr = kIOReturnError;
        pthread_t th;
        FlowArg fa;
        VerifyFlushMockMakeLive(m);
        m.prepareBlocks = true;
        m.gateOpen = false;
        memset(&out, 0, sizeof(out));
        fa.m = &m;
        fa.out = &out;
        fa.kr = &kr;
        assert(pthread_create(&th, NULL, runFlow, &fa) == 0);
        waitPrepareEntered(m);
        // Flip BME ON mid-prepare (MSE stays ON).
        m.command.store(0x0006u);
        openPrepareGate(m);
        assert(pthread_join(th, NULL) == 0);
        // FIXED behavior: semantic Failed with refreshed evidence, teardown.
        printf("race result: kr=%d status=%u bmeOut=%u mseOut=%u phase=%u\n",
               (int)kr, (unsigned)out.status, (unsigned)out.bmeEnabled,
               (unsigned)out.mseEnabled, (unsigned)out.phase);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabFlushPrewriteStatus_Failed);
        assert(out.phase == kGA106LabFlushPrewritePhase_Unprepared);
        assert(out.bmeEnabled == 1);  // refreshed: flip visible
        assert(out.mseEnabled == 1);
        assert(m.prepareCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("R1 BME-FLIP-DURING-PREPARE fails closed (bme=1 observed)");

    // Transport failure of the re-read => Error + Failed + teardown.
    {
        VerifyFlushMockOps m;
        GA106LabFlushPrewriteV1 out;
        IOReturn kr = kIOReturnError;
        VerifyFlushMockMakeLive(m);
        m.commandAfterFail = true;
        memset(&out, 0, sizeof(out));
        kr = RunVerifyFlushPrewriteFlow(m, out);
        assert(kr == kIOReturnError);
        assert(out.status == kGA106LabFlushPrewriteStatus_Failed);
        assert(out.phase == kGA106LabFlushPrewritePhase_Unprepared);
        assert(m.isBusy() == false);
    }
    PASS("R2 COMMANDAFTER-TRANSPORT-FAIL Error + teardown");

    // Flip + clearFail: semantic teardown retains objects (CleanupFailed),
    // still returns the semantic kr (td ignored by design, same as IOVA path).
    {
        VerifyFlushMockOps m;
        GA106LabFlushPrewriteV1 out;
        IOReturn kr = kIOReturnError;
        pthread_t th;
        FlowArg fa;
        VerifyFlushMockMakeLive(m);
        m.prepareBlocks = true;
        m.gateOpen = false;
        m.clearFail = true;
        memset(&out, 0, sizeof(out));
        fa.m = &m;
        fa.out = &out;
        fa.kr = &kr;
        assert(pthread_create(&th, NULL, runFlow, &fa) == 0);
        waitPrepareEntered(m);
        m.command.store(0x0006u);
        openPrepareGate(m);
        assert(pthread_join(th, NULL) == 0);
        assert(kr == kIOReturnSuccess);  // semantic kr preserved despite clear fail
        assert(out.status == kGA106LabFlushPrewriteStatus_Failed);
        assert(m.state == kGA106LabFlushPrewritePhase_CleanupFailed);
        assert(m.commandReleaseCount == 0 && m.descriptorReleaseCount == 0);
        assert(m.clearCount == 1);  // no double-clear
        assert(m.isBusy() == false);
    }
    PASS("R3 FLIP+CLEARFAIL CleanupFailed retention, semantic kr");

    printf("test-bme-race-contract: ALL PASS (%d grupos; race gate locked)\n", gGroups);
    return 0;
}
