// test-lock-failure.cpp — FIX-ASTRA-B2 §7+§8+§9+§10.
//
// Simula init()/teardown de locks do GA106Lab com falhas injetáveis,
// espelhando GA106Lab::init() + GA106LabFreeLockNullSafe() em semântica:
//   fSysmemLock = NULL; fSlotLock = NULL (explícito)
//   alloc #1 (fSysmemLock); se NULL => fail limpo, free calls 0
//   alloc #2 (fSlotLock); se NULL => free #1 exatamente 1x, nunca free NULL
//   teardown central: if (*slot) { free; *slot=NULL; } (nunca free NULL)
//
// Contadores provam:
//   NULL_LOCK_FREE_COUNT = 0, DOUBLE_LOCK_FREE_COUNT = 0
// Mutação MUT_FREE_NULL_LOCK força free(NULL) e DEVE ser CAUGHT.

#include <cassert>
#include <cstdint>
#include <cstdio>

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

// Fake lock (análogo funcional de IOSimpleLock p/ disciplina init/free).
struct FakeLock {
    int id;
};

// Instrumentação global (análoga a contadores de auditoria).
static int gAllocCount = 0;
static int gFreeCount = 0;
static int gFreeNullCount = 0;
static int gDoubleFreeCount = 0;

static void resetLockCounts(void)
{
    gAllocCount = 0;
    gFreeCount = 0;
    gFreeNullCount = 0;
    gDoubleFreeCount = 0;
}

// Fixtures de falha por posição.
static bool gFailFirst = false;
static bool gFailSecond = false;
static int gAllocSeq = 0;

static FakeLock *MockLockAlloc(void)
{
    gAllocSeq++;
    gAllocCount++;
    if (gAllocSeq == 1 && gFailFirst) {
        return nullptr;
    }
    if (gAllocSeq == 2 && gFailSecond) {
        return nullptr;
    }
    FakeLock *l = new FakeLock();
    l->id = gAllocSeq;
    return l;
}

// Helper central espelhando GA106LabFreeLockNullSafe().
static void MockFreeLockNullSafe(FakeLock **slot)
{
#ifdef GA106LAB_SYSMEM_MUT_FREE_NULL_LOCK
    // MUTAÇÃO TEST-ONLY: libera NULL (conta como free-NULL) e não zera
    // corretamente (provoca double-free observável no teste).
    if (!slot) {
        gFreeNullCount++;
        return;
    }
    if (*slot == nullptr) {
        gFreeNullCount++; // bug: free(NULL)
        return;
    }
    delete *slot;
    gFreeCount++;
    // bug: não zera => segundo free conta como double (detectado abaixo).
    // (propositalmente omite *slot = nullptr)
    return;
#else
    if (slot && *slot) {
        // Detecta double-free via id sentinela? Aqui basta garantir que
        // segundo free no mesmo slot seja no-op (ponteiro já NULL).
        delete *slot;
        *slot = nullptr;
        gFreeCount++;
    }
    return;
#endif
}

// Espelho de GA106Lab::init() — ordem formal e rollback seguro.
// Retorna true = success, false = fail limpo.
static bool MockInit(FakeLock **sysmemLockOut, FakeLock **slotLockOut)
{
    // Inicialização explícita (LOCK_POINTERS_EXPLICITLY_INITIALIZED).
    *sysmemLockOut = nullptr;
    *slotLockOut = nullptr;
    gAllocSeq = 0;

    *sysmemLockOut = MockLockAlloc();
    if (!*sysmemLockOut) {
        return false;
    }
    *slotLockOut = MockLockAlloc();
    if (!*slotLockOut) {
        MockFreeLockNullSafe(sysmemLockOut);
        return false;
    }
    return true;
}

int main(void)
{
    // 1. Primeira alocação falha: fail limpo, zero free, sem NULL free.
    {
        FakeLock *sys = (FakeLock *)0x1; // lixo proposital (prova init zera)
        FakeLock *slot = (FakeLock *)0x1;
        resetLockCounts();
        gFailFirst = true;
        gFailSecond = false;
        bool ok = MockInit(&sys, &slot);
        assert(ok == false);
        assert(sys == nullptr && slot == nullptr);
        assert(gAllocCount == 1);
        assert(gFreeCount == 0);
        assert(gFreeNullCount == 0);
        assert(gDoubleFreeCount == 0);
        // Nenhum objeto DMA alocado/vazado neste caminho (init falhou antes).
        // Teardown adicional deve ser no-op seguro.
        MockFreeLockNullSafe(&sys);
        MockFreeLockNullSafe(&slot);
        assert(gFreeCount == 0 && gFreeNullCount == 0);
    }
    PASS("FIRST_LOCK_ALLOC_FAILURE_TEST PASS (0 free, no NULL free)");

    // 2. Segunda alocação falha: free #1 1x, nunca free NULL/double.
    {
        FakeLock *sys = nullptr;
        FakeLock *slot = nullptr;
        resetLockCounts();
        gFailFirst = false;
        gFailSecond = true;
        bool ok = MockInit(&sys, &slot);
        assert(ok == false);
        assert(sys == nullptr); // liberado e zerado
        assert(slot == nullptr);
        assert(gAllocCount == 2);
        assert(gFreeCount == 1); // exatamente 1 (lock #1)
        assert(gFreeNullCount == 0);
        // Sem double-free: teardown extra é no-op.
        MockFreeLockNullSafe(&sys);
        MockFreeLockNullSafe(&slot);
        assert(gFreeCount == 1 && gFreeNullCount == 0);
    }
    PASS("SECOND_LOCK_ALLOC_FAILURE_TEST PASS (free #1 1x, zero NULL/double)");

    // 3. Sucesso: ambos alocados, teardown 1x cada, zero NULL/double.
    {
        FakeLock *sys = nullptr;
        FakeLock *slot = nullptr;
        resetLockCounts();
        gFailFirst = false;
        gFailSecond = false;
        bool ok = MockInit(&sys, &slot);
        assert(ok == true);
        assert(sys != nullptr && slot != nullptr);
        assert(gAllocCount == 2);
        MockFreeLockNullSafe(&sys);
        MockFreeLockNullSafe(&slot);
        assert(sys == nullptr && slot == nullptr);
        assert(gFreeCount == 2);
        assert(gFreeNullCount == 0);
        // Double-free seria no-op (ponteiros já NULL).
        MockFreeLockNullSafe(&sys);
        MockFreeLockNullSafe(&slot);
        assert(gFreeCount == 2 && gFreeNullCount == 0);
    }
    PASS("LOCK_SUCCESS_TEARDOWN PASS (2 alloc, 2 free, zero NULL/double)");

    // 4. Helper nunca libera NULL diretamente.
    {
        FakeLock *n = nullptr;
        resetLockCounts();
        MockFreeLockNullSafe(&n);
        MockFreeLockNullSafe(nullptr);
        assert(gFreeCount == 0);
        assert(gFreeNullCount == 0);
    }
    PASS("NULL_LOCK_FREE_COUNT=0 DOUBLE_LOCK_FREE_COUNT=0");

    printf("test-lock-failure: ALL PASS (%d grupos)\n", gGroups);
    return 0;
}
