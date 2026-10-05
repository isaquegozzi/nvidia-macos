// test-stop-free-regression.cpp — TEST-PATH-FIX (usa MESMO shared flow).
//
// NÃO duplica free()/CLEANUP_FAILED policy. Usa:
//   GA106LabGspSysmemFlow.hpp (prepare/stop) +
//   GA106LabInitFreeLockFlow.hpp::FreeOwnerResourcesFlow<OwnerMockOps>
// para o free(), com GA106LabOwnerMockOps sincronizado ao estado sysmem.
// Produção usa o MESMO FreeOwnerResourcesFlow com OwnerRealOps.
// ONE_SHARED_INIT_FREE_LOCK_FLOW = YES, FREE_CLEANUP_FAILED_SHARED_PATH = YES.
//
// Fluxo integrado: prepare/address → clearFail → stop() → CLEANUP_FAILED
// → FreeOwnerResourcesFlow (mesmo da produção).
// Mutação PROD_FREE_RELEASE deve falhar este teste (CAUGHT).

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "GA106LabProtocol.h"
#include "GA106LabGspSysmemFlow.hpp"
#include "GA106LabInitFreeLockFlow.hpp"
#include "GA106LabMockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static void assertZeroHw(const GA106LabSysmemMockOps &m)
{
    assert(m.mmioReadCount == 0);
    assert(m.mmioWriteCount == 0);
    assert(m.configWriteCount == 0);
    assert(m.bmeEnableCount == 0);
    assert(m.dmaTriggerCount == 0);
}

// Sincroniza o owner-mock (locks/free) com o estado sysmem resultante do
// teardown, para chamar o MESMO FreeOwnerResourcesFlow da produção.
// Retorna releases observados no free via contadores do owner-mock.
static void runSharedFreeFromSysmemState(GA106LabOwnerMockOps &owner,
                                         uint32_t sysmemState,
                                         bool sysmemCmdExists,
                                         bool sysmemDescExists)
{
    owner.state = sysmemState;
    owner.cmdExists = sysmemCmdExists;
    owner.descExists = sysmemDescExists;
    // Locks do owner: aloca via shared init para teardown realista.
    // (Se já alocados, mantém; aqui garante alocados para observar free.)
    if (owner.getFirstLock() == nullptr && owner.getSecondLock() == nullptr) {
        GA106LabOwnerMockOps tmp;
        GA106LabOwnerMockMakeLive(tmp);
        // Aloca diretamente via raw (sem policy) para setup, depois o free
        // usa o shared flow completo.
        owner.failFirst = false;
        owner.failSecond = false;
        (void)owner.allocFirstLockRaw();
        (void)owner.allocSecondLockRaw();
    }
    FreeOwnerResourcesFlow(owner);
}

int main(void)
{
    // 1. Integrado: prepare OK → clearFail → stop() → CLEANUP_FAILED → shared free().
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnSuccess);
        assert(m.state == kSysmemPhase_AddressReady);
        assert(m.allocCount == 1 && m.prepareCount == 1);
        m.clearFail = true;
        m.resetCounts();
        m.opLogCount = 0;
        WaitForSysmemIdleAndTeardownFlow(m);
        assert(m.completeCount == 1);
        assert(m.clearCount == 1);
        assert(m.commandReleaseCount == 0);
        assert(m.descriptorReleaseCount == 0);
        assert(m.state == kSysmemPhase_CleanupFailed);
        // free() via MESMO shared path da produção.
        GA106LabOwnerMockOps owner;
        GA106LabOwnerMockMakeLive(owner);
        // Setup locks via shared init (produção faz o mesmo no init real).
        assert(InitOwnerLocksFlow(owner) == true);
        int freeLockBefore = owner.freeCount;
        owner.state = m.state;
        owner.cmdExists = m.cmdExists;
        owner.descExists = m.descExists;
        int cmdBefore = owner.cmdReleaseCount;
        int descBefore = owner.descReleaseCount;
        FreeOwnerResourcesFlow(owner);
        // CLEANUP_FAILED => zero releases de DMA, locks liberados 1x cada.
        assert(owner.cmdReleaseCount - cmdBefore == 0);
        assert(owner.descReleaseCount - descBefore == 0);
        assert(owner.freeCount - freeLockBefore == 2);
        assert(owner.freeNullCount == 0);
        // Objetos sysmem ainda existem (leak deliberado, sem UAF).
        assert(m.descExists && m.cmdExists);
        // Sem retry: segundo teardown continua erro, sem releases.
        int cmdB = (int)m.commandReleaseCount.load();
        int descB = (int)m.descriptorReleaseCount.load();
        assert(ReleaseGspSysmemFlow(m) != kIOReturnSuccess);
        assert((int)m.commandReleaseCount.load() == cmdB);
        assert((int)m.descriptorReleaseCount.load() == descB);
        assert(m.state == kSysmemPhase_CleanupFailed);
        assertZeroHw(m);
    }
    PASS("CLEAR_FAILURE_STOP_FREE_REGRESSION PASS (1/1/0/0, leak-safe)");

    // 2. Stop normal (sem falha) → EMPTY → shared free() sem releases extras de DMA.
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
        GA106LabOwnerMockOps owner;
        GA106LabOwnerMockMakeLive(owner);
        assert(InitOwnerLocksFlow(owner) == true);
        owner.state = m.state;
        owner.cmdExists = false;
        owner.descExists = false;
        int cmdB = owner.cmdReleaseCount;
        int descB = owner.descReleaseCount;
        FreeOwnerResourcesFlow(owner);
        assert(owner.cmdReleaseCount == cmdB && owner.descReleaseCount == descB);
        assertZeroHw(m);
    }
    PASS("STOP_NORMAL_FREE_EMPTY PASS (cleanup 1x, free 0/0)");

    // 3. EMPTY → stop → shared free: zero ops, zero releases.
    {
        GA106LabSysmemMockOps m;
        GA106LabSysmemMockMakeLive(m);
        m.resetCounts();
        WaitForSysmemIdleAndTeardownFlow(m);
        assert(m.completeCount == 0 && m.clearCount == 0);
        GA106LabOwnerMockOps owner;
        GA106LabOwnerMockMakeLive(owner);
        assert(InitOwnerLocksFlow(owner) == true);
        owner.state = m.state;
        owner.cmdExists = false;
        owner.descExists = false;
        FreeOwnerResourcesFlow(owner);
        assert(owner.cmdReleaseCount == 0 && owner.descReleaseCount == 0);
    }
    PASS("EMPTY_STOP_FREE PASS (zero ops)");

    printf("test-stop-free-regression: ALL PASS (%d grupos)\n", gGroups);
    return 0;
}
