// test-lock-failure.cpp — TEST-PATH-FIX (usa MESMO shared flow da produção).
//
// NÃO duplica init()/free()/lock policy. Usa:
//   GA106LabInitFreeLockFlow.hpp::InitOwnerLocksFlow<Ops>
//   GA106LabInitFreeLockFlow.hpp::ReleaseOwnerLocksFlow<Ops>
// com GA106LabOwnerMockOps (operações brutas + contadores).
// Produção usa os MESMOS templates com GA106LabOwnerRealOps.
// ONE_SHARED_INIT_FREE_LOCK_FLOW = YES, DUPLICATED_INIT_FREE_TEST_LOGIC = NO.
//
// Mutations PROD-PATH (no shared header) devem falhar estes testes:
//   MUT_PROD_NULL_LOCK_FREE => free NULL => CAUGHT.

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "GA106LabGspSysmemFlow.hpp"
#include "GA106LabInitFreeLockFlow.hpp"
#include "GA106LabMockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

int main(void)
{
    // 1. Primeira alocação falha: fail limpo, zero free, sem NULL free.
    {
        GA106LabOwnerMockOps m;
        GA106LabOwnerMockMakeLive(m);
        m.failFirst = true;
        m.failSecond = false;
        // Lixo proposital antes do init (prova initNullLocks no shared flow).
        m.firstLock = (GA106LabOwnerMockOps::FakeLock *)0x1;
        m.secondLock = (GA106LabOwnerMockOps::FakeLock *)0x1;
        bool ok = InitOwnerLocksFlow(m);
        assert(ok == false);
        assert(m.getFirstLock() == nullptr && m.getSecondLock() == nullptr);
        assert(m.allocCount == 1);
        assert(m.freeCount == 0);
        assert(m.freeNullCount == 0);
        // Teardown adicional deve ser no-op seguro (shared flow).
        ReleaseOwnerLocksFlow(m);
        assert(m.freeCount == 0 && m.freeNullCount == 0);
    }
    PASS("FIRST_LOCK_ALLOC_FAILURE_TEST PASS (0 free, no NULL free)");

    // 2. Segunda alocação falha: free #1 1x, nunca free NULL/double.
    {
        GA106LabOwnerMockOps m;
        GA106LabOwnerMockMakeLive(m);
        m.failFirst = false;
        m.failSecond = true;
        bool ok = InitOwnerLocksFlow(m);
        assert(ok == false);
        assert(m.getFirstLock() == nullptr); // liberado e zerado pelo shared
        assert(m.getSecondLock() == nullptr);
        assert(m.allocCount == 2);
        assert(m.freeCount == 1); // exatamente 1 (lock #1)
        assert(m.freeNullCount == 0);
        ReleaseOwnerLocksFlow(m);
        assert(m.freeCount == 1 && m.freeNullCount == 0);
    }
    PASS("SECOND_LOCK_ALLOC_FAILURE_TEST PASS (free #1 1x, zero NULL/double)");

    // 3. Sucesso: ambos alocados, teardown 1x cada, zero NULL/double.
    {
        GA106LabOwnerMockOps m;
        GA106LabOwnerMockMakeLive(m);
        bool ok = InitOwnerLocksFlow(m);
        assert(ok == true);
        assert(m.getFirstLock() != nullptr && m.getSecondLock() != nullptr);
        assert(m.allocCount == 2);
        ReleaseOwnerLocksFlow(m);
        assert(m.getFirstLock() == nullptr && m.getSecondLock() == nullptr);
        assert(m.freeCount == 2);
        assert(m.freeNullCount == 0);
        ReleaseOwnerLocksFlow(m);
        assert(m.freeCount == 2 && m.freeNullCount == 0);
    }
    PASS("LOCK_SUCCESS_TEARDOWN PASS (2 alloc, 2 free, zero NULL/double)");

    // 4. Helper nunca libera NULL diretamente.
    {
        GA106LabOwnerMockOps m;
        GA106LabOwnerMockMakeLive(m);
        ReleaseOwnerLocksFlow(m);
        assert(m.freeCount == 0);
        assert(m.freeNullCount == 0);
    }
    PASS("NULL_LOCK_FREE_COUNT=0 DOUBLE_LOCK_FREE_COUNT=0");

    printf("test-lock-failure: ALL PASS (%d grupos)\n", gGroups);
    return 0;
}
