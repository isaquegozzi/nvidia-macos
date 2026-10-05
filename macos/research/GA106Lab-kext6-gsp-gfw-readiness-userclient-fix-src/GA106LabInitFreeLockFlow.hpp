// GA106LabInitFreeLockFlow.hpp — ONE_SHARED_INIT_FREE_LOCK_FLOW (TEST-PATH-FIX).
//
// Único path de policy para lock init/alloc/cleanup + free() + CLEANUP_FAILED.
// Produção (OwnerRealOps) e testes (OwnerMockOps) instanciam OS MESMOS
// templates. Static dispatch, sem virtual.
//
// Regra:
//   production calls shared flow
//   tests call same shared flow
// Nunca:
//   production code A / test replica B
//
// Ops deve prover SOMENTE operações (sem policy/branching):
//   void initNullLocks();                 // zera ambos os ponteiros
//   bool allocFirstLockRaw();             // aloca #1, retorna sucesso
//   bool allocSecondLockRaw();            // aloca #2, retorna sucesso
//   void *getFirstLock(); void *getSecondLock();
//   void freeLockRaw(void *lock);         // assume non-null, só opera
//   void clearFirstLock(); void clearSecondLock();
//   uint32_t getState();
//   bool hasCommand(); bool hasDescriptor();
//   void releaseCommandRaw(); void releaseDescriptorRaw(); // assumem existência
//   void logCleanupFailedLeakSafe(); void logUnexpectedStateInFree();
//
// Toda decisão vive aqui:
//   ordem init, rollback se #2 falha, never free NULL, never double free,
//   CLEANUP_FAILED => sem releases (leak-safe).
//
// Mutations PROD-PATH (afetam shared, logo prod+testes):
//   GA106LAB_INITFREE_MUT_PROD_FREE_RELEASE_CLEANUP_FAILED
//     (+ alias legado GA106LAB_SYSMEM_MUT_FREE_RELEASE_ON_CLEANUP_FAILED)
//     => libera command/descriptor em CLEANUP_FAILED (deve falhar testes).
//   GA106LAB_INITFREE_MUT_PROD_NULL_LOCK_FREE
//     (+ alias legado GA106LAB_SYSMEM_MUT_FREE_NULL_LOCK)
//     => remove NULL guard antes de free (deve falhar testes).

#ifndef GA106LAB_INIT_FREE_LOCK_FLOW_HPP
#define GA106LAB_INIT_FREE_LOCK_FLOW_HPP

#include "GA106LabGspSysmemFlow.hpp"

#include <stdint.h>
#include <stddef.h>

// Aliases legados (1.5.2 test-only) agora disparam o mesmo bug PROD-PATH,
// para preservar cobertura: ou nome antigo ou novo ativa a mutação no
// shared production path.
#if defined(GA106LAB_SYSMEM_MUT_FREE_RELEASE_ON_CLEANUP_FAILED) && \
    !defined(GA106LAB_INITFREE_MUT_PROD_FREE_RELEASE_CLEANUP_FAILED)
#define GA106LAB_INITFREE_MUT_PROD_FREE_RELEASE_CLEANUP_FAILED 1
#endif

#if defined(GA106LAB_SYSMEM_MUT_FREE_NULL_LOCK) && \
    !defined(GA106LAB_INITFREE_MUT_PROD_NULL_LOCK_FREE)
#define GA106LAB_INITFREE_MUT_PROD_NULL_LOCK_FREE 1
#endif

// Init: pointer init + lock #1 + lock #2 + rollback se #2 falha.
// Nunca free NULL; nunca double free; fail limpo.
template <typename Ops>
inline bool InitOwnerLocksFlow(Ops &ops)
{
    ops.initNullLocks();
    if (!ops.allocFirstLockRaw()) {
        return false;
    }
    if (!ops.allocSecondLockRaw()) {
        // Rollback: libera #1 exatamente 1x (sabidamente non-null aqui).
        void *first = ops.getFirstLock();
        ops.freeLockRaw(first);
        ops.clearFirstLock();
        return false;
    }
    return true;
}

// Teardown central de locks: null-safe, sem double-free.
// Mutação PROD_NULL_LOCK_FREE remove os guards (free NULL).
template <typename Ops>
inline void ReleaseOwnerLocksFlow(Ops &ops)
{
#ifdef GA106LAB_INITFREE_MUT_PROD_NULL_LOCK_FREE
    // MUTAÇÃO PROD-PATH TEST-ONLY: remove NULL guard (bug real).
    // Libera mesmo quando nullptr => free(NULL) / crash / contagem.
    ops.freeLockRaw(ops.getFirstLock());
    ops.clearFirstLock();
    ops.freeLockRaw(ops.getSecondLock());
    ops.clearSecondLock();
#else
    if (ops.getFirstLock() != nullptr) {
        ops.freeLockRaw(ops.getFirstLock());
        ops.clearFirstLock();
    }
    if (ops.getSecondLock() != nullptr) {
        ops.freeLockRaw(ops.getSecondLock());
        ops.clearSecondLock();
    }
#endif
}

// Free: CLEANUP_FAILED => leak-safe (sem releases, preserva ponteiros).
// Mutação PROD_FREE_RELEASE reintroduz releases em CLEANUP_FAILED.
// CLEAR_FAILURE_RELEASE_POLICY = LEAK_SAFE_NO_RELEASE
template <typename Ops>
inline void FreeOwnerResourcesFlow(Ops &ops)
{
    uint32_t st = ops.getState();
    if (st == kSysmemPhase_CleanupFailed) {
        ops.logCleanupFailedLeakSafe();
#ifdef GA106LAB_INITFREE_MUT_PROD_FREE_RELEASE_CLEANUP_FAILED
        // MUTAÇÃO PROD-PATH TEST-ONLY: reintroduz bug (release em CLEANUP_FAILED).
        ops.releaseCommandRaw();
        ops.releaseDescriptorRaw();
#else
        // Correto: NÃO release command/descriptor, preserva (leak deliberado).
#endif
        // Locks auxiliares podem ser liberados (null-safe, sem UAF).
        ReleaseOwnerLocksFlow(ops);
        return;
    }
    if (st != kSysmemPhase_Empty) {
        ops.logUnexpectedStateInFree();
    }
    if (ops.hasCommand()) {
        ops.releaseCommandRaw();
    }
    if (ops.hasDescriptor()) {
        ops.releaseDescriptorRaw();
    }
    ReleaseOwnerLocksFlow(ops);
}

#endif /* GA106LAB_INIT_FREE_LOCK_FLOW_HPP */
