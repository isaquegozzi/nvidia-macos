// test-stop-free-regression.cpp — FIX-ASTRA-B2 §5+§6.
//
// Teste integrado exato do fluxo real owner stop() seguido de free():
//   prepare/address state → force clearMemoryDescriptor(false) failure
//   → stop() [WaitForSysmemIdleAndTeardownFlow] → CLEANUP_FAILED → free()
//
// Esperado:
//   complete count = exactly per attempted prepare (1)
//   clear count = exactly 1
//   command release = 0, descriptor release = 0, retry = 0
//   panic = NO, UAF = NO
//
// Espelha GA106Lab::stop() (template compartilhado) + GA106Lab::free()
// CLEANUP_FAILED branch (LEAK_SAFE_NO_RELEASE). Mesmos counters das MockOps.
// Zero HW. Mutação MUT_FREE_RELEASE_ON_CLEANUP_FAILED reintroduz o bug e
// DEVE ser CAUGHT (releases 1/1 != 0/0).

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "GA106LabProtocol.h"
#include "GA106LabGspSysmemFlow.hpp"
#include "GA106LabMockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

// Espelho userspace de GA106Lab::free() — política CLEANUP_FAILED.
// Deve permanecer idêntico ao kernel (GA106Lab.cpp) em semântica:
//   CLEANUP_FAILED => sem complete/clear/releases, preserva ponteiros.
// Retorna releases observados em free() via out-params.
static void MockOwnerFree(GA106LabSysmemMockOps &m,
                          int &freeCmdRelOut, int &freeDescRelOut)
{
    int cmdBefore = (int)m.commandReleaseCount.load();
    int descBefore = (int)m.descriptorReleaseCount.load();
    freeCmdRelOut = 0;
    freeDescRelOut = 0;

    if (m.state == kSysmemPhase_CleanupFailed) {
#ifdef GA106LAB_SYSMEM_MUT_FREE_RELEASE_ON_CLEANUP_FAILED
        // MUTAÇÃO TEST-ONLY: reintroduz bug (release em CLEANUP_FAILED).
        m.releaseCommand();
        m.releaseDescriptor();
        freeCmdRelOut = (int)m.commandReleaseCount.load() - cmdBefore;
        freeDescRelOut = (int)m.descriptorReleaseCount.load() - descBefore;
        return;
#else
        // Correto: preserva leak deliberado, sem releases.
        freeCmdRelOut = 0;
        freeDescRelOut = 0;
        return;
#endif
    }
    // Estados não-CLEANUP_FAILED: free() defensivo só liberaria se objetos
    // ainda existissem (stop já limpou; aqui não liberamos pois o teardown
    // central já fez releases; apenas observa).
    freeCmdRelOut = 0;
    freeDescRelOut = 0;
}

static void assertZeroHw(const GA106LabSysmemMockOps &m)
{
    assert(m.mmioReadCount == 0);
    assert(m.mmioWriteCount == 0);
    assert(m.configWriteCount == 0);
    assert(m.bmeEnableCount == 0);
    assert(m.dmaTriggerCount == 0);
}

int main(void)
{
    // 1. Integrado: prepare OK → clearFail → stop() → CLEANUP_FAILED → free().
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnSuccess);
        assert(m.state == kSysmemPhase_AddressReady);
        assert(m.allocCount == 1 && m.prepareCount == 1);
        // Força falha de clear (como clearMemoryDescriptor(false) NotReady).
        m.clearFail = true;
        m.resetCounts();
        m.opLogCount = 0;
        // stop(): mesmo template da produção (flag terminal + espera + teardown).
        WaitForSysmemIdleAndTeardownFlow(m);
        // stop deve ter feito complete 1x + clear 1x, zero releases.
        assert(m.completeCount == 1);
        assert(m.clearCount == 1);
        assert(m.commandReleaseCount == 0);
        assert(m.descriptorReleaseCount == 0);
        assert(m.state == kSysmemPhase_CleanupFailed);
        // free(): preserva leak, zero releases adicionais.
        int freeCmd = -1, freeDesc = -1;
        MockOwnerFree(m, freeCmd, freeDesc);
        assert(freeCmd == 0);
        assert(freeDesc == 0);
        assert(m.commandReleaseCount == 0);
        assert(m.descriptorReleaseCount == 0);
        // Sem retry: segundo teardown continua erro, sem releases.
        int cmdBefore = (int)m.commandReleaseCount.load();
        int descBefore = (int)m.descriptorReleaseCount.load();
        assert(ReleaseGspSysmemFlow(m) != kIOReturnSuccess);
        assert((int)m.commandReleaseCount.load() == cmdBefore);
        assert((int)m.descriptorReleaseCount.load() == descBefore);
        assert(m.state == kSysmemPhase_CleanupFailed);
        assertZeroHw(m);
        // Objetos ainda existem (leak deliberado, sem UAF): desc+cmd vivos.
        assert(m.descExists && m.cmdExists);
    }
    PASS("CLEAR_FAILURE_STOP_FREE_REGRESSION PASS (1/1/0/0, leak-safe)");

    // 2. Stop normal (sem falha) → EMPTY → free() sem releases extras.
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnSuccess);
        m.resetCounts();
        WaitForSysmemIdleAndTeardownFlow(m);
        assert(m.completeCount == 1 && m.clearCount == 1);
        assert(m.commandReleaseCount == 1 && m.descriptorReleaseCount == 1);
        assert(m.state == kSysmemPhase_Empty);
        int freeCmd = -1, freeDesc = -1;
        MockOwnerFree(m, freeCmd, freeDesc);
        assert(freeCmd == 0 && freeDesc == 0);
        assertZeroHw(m);
    }
    PASS("STOP_NORMAL_FREE_EMPTY PASS (cleanup 1x, free 0/0)");

    // 3. EMPTY → stop → free: zero ops, zero releases.
    {
        GA106LabSysmemMockOps m;
        GA106LabSysmemMockMakeLive(m);
        m.resetCounts();
        WaitForSysmemIdleAndTeardownFlow(m);
        assert(m.completeCount == 0 && m.clearCount == 0);
        assert(m.commandReleaseCount == 0 && m.descriptorReleaseCount == 0);
        int freeCmd = -1, freeDesc = -1;
        MockOwnerFree(m, freeCmd, freeDesc);
        assert(freeCmd == 0 && freeDesc == 0);
    }
    PASS("EMPTY_STOP_FREE PASS (zero ops)");

    printf("test-stop-free-regression: ALL PASS (%d grupos)\n", gGroups);
    return 0;
}
