// GA106LabGspSysmemFlow.hpp — ONE_SHARED_SYSMEM_FLOW (GSP-DMA1, FIX-ASTRA-C).
//
// Único fluxo de policy do selector 7 PREPARE_GSP_SYSMEM.
// Produção (SysmemRealOps) e testes (SysmemMockOps) instanciam O MESMO
// template RunPrepareGspSysmemFlow<Ops> + ReleaseGspSysmemFlow<Ops>.
// Static dispatch (sem virtual, sem callbacks runtime no ABI/UserClient).
//
// Escopo: alocar 4 KiB, zerar, bindar IODMACommand (autoPrepare=false),
// prepare explícito 1x, resolver exatamente 1 segmento DMA, validar
// largura/alinhamento, manter mapping vivo. Zero MMIO, zero PCI config,
// BME OFF, zero DMA trigger, zero firmware. Endereço DMA/bus é
// kernel-only e NUNCA entra na ABI.
//
// Modelo XNU real (FIX-ASTRA-C, cf. IODMACommand.cpp):
// - prepare() incrementa fActive na entrada e NÃO decrementa em falha:
//   cleanup após prepare TENTADO executa complete() exatamente 1x,
//   inclusive quando prepare() retornou erro.
// - clearMemoryDescriptor(false) falha (NotReady) se ainda ativo:
//   clear!=Success => CLEANUP_FAILED, SEM releases (leak deliberado,
//   filosofia do próprio XNU em IODMACommand::free).
// - complete() decrementa antes de trabalhar: seu erro não impede clear.
//
// Disciplina de lock (FIX-ASTRA-C): o template chama ops.lock()/unlock()
// SOMENTE em seções curtas (check/reserve/commit); todo KPI bloqueante
// (alloc/prepare/complete/clear/release) roda DESTRAVADO. A exclusão é o
// bit BUSY do owner: só o detentor toca nos objetos; stop() espera
// !BUSY (poll IODelay, sem lock retido) e nunca toca objetos alheios.
// Segunda chamada com BUSY => kIOReturnBusy. Stopping => kIOReturnAborted.
//
// Ops deve prover (RealOps = só operações; MockOps = fixtures/counters):
//   lock(); unlock();                       // seções curtas; NUNCA bloquear dentro
//   uint32_t getState(); void setState(uint32_t);
//   bool isStopping(); void setStopping(bool);
//   bool isBusy(); void setBusy(bool);
//   void waitMs(unsigned ms);               // IODelay (Real) / usleep (Mock)
//   bool allocBuffer(); bool zeroBuffer(); bool createCommand();
//   bool bindDescriptor();            // setMemoryDescriptor(desc,false)
//   bool prepareDma();                // prepare(0,4096,true,true) 1x
//   bool genSegments(uint32_t &n, uint64_t &addr, uint64_t &len);
//   bool validateAddressWidth(uint64_t addr);
//   void markAddressReady(uint64_t addr, uint32_t len);
//   IOReturn completeDma();           // SÓ chamado se prepare foi tentado
//   IOReturn clearDma();              // SÓ chamado se bound; retorno CHECADO
//   void releaseCommand(); void releaseDescriptor(); // null-safe, sem double
//   void clearAddressRecord();        // addr=0,len=0

#ifndef GA106LAB_GSP_SYSMEM_FLOW_HPP
#define GA106LAB_GSP_SYSMEM_FLOW_HPP

#include "GA106LabProtocol.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

// Fases internas do owner (ABI expõe SÓ EMPTY e ADDRESS_READY; o resto
// nunca aparece em GA106LabGspSysmemV1.state).
enum {
    kSysmemPhase_Empty            = 0, // == ABI Empty
    kSysmemPhase_AddressReady     = 3, // == ABI ADDRESS_READY
    kSysmemPhase_InProgress       = 10,
    kSysmemPhase_Allocated        = 11,
    kSysmemPhase_Bound            = 12,
    kSysmemPhase_PrepareAttempted = 13,
    kSysmemPhase_Prepared         = 14,
    kSysmemPhase_CleanupFailed    = 20
};

// Precisa complete()? prepare foi tentado (sucesso OU erro): XNU deixou
// fActive==1. Conservative rule: attempt => complete exactly once.
static inline bool SysmemPhaseNeedsComplete(uint32_t phase)
{
    return (phase == kSysmemPhase_PrepareAttempted ||
            phase == kSysmemPhase_Prepared ||
            phase == kSysmemPhase_AddressReady)
        ? true
        : false;
}

// Está bound? (setMemoryDescriptor ok, ainda não desbindado).
static inline bool SysmemPhaseIsBound(uint32_t phase)
{
    return (phase == kSysmemPhase_Bound || SysmemPhaseNeedsComplete(phase))
        ? true
        : false;
}

// Teardown compartilhado e central. Chamador detém BUSY (fluxo em falha
// ou stop após espera). Ordem única: complete→clear→releases→reset.
// clear!=Success => CLEANUP_FAILED, SEM releases (leak-safe, sem UAF).
template <typename Ops>
inline IOReturn ReleaseGspSysmemFlow(Ops &ops)
{
    uint32_t phase = 0;
    bool needComplete = false;
    bool needClear = false;
    IOReturn clearRc = kIOReturnSuccess;

    ops.lock();
    phase = ops.getState();
    ops.unlock();

    if (phase == kSysmemPhase_Empty) {
        return kIOReturnSuccess; // nada para fazer.
    }
    if (phase == kSysmemPhase_CleanupFailed) {
        return kIOReturnError; // sem retry automático, sem release forçado.
    }

    needComplete = SysmemPhaseNeedsComplete(phase);
    needClear = SysmemPhaseIsBound(phase);

#ifdef GA106LAB_SYSMEM_MUT_REL_CMD_BEFORE_CLEAR
    ops.releaseCommand(); // MUTAÇÃO TEST-ONLY: release antes do clear.
#endif
#ifdef GA106LAB_SYSMEM_MUT_REL_DESC_BEFORE_CLEAR
    ops.releaseDescriptor(); // MUTAÇÃO TEST-ONLY: release antes do clear.
#endif
    if (needComplete) {
#ifdef GA106LAB_SYSMEM_MUT_COMPLETE_ONLY_ON_SUCCESS
        // MUTAÇÃO TEST-ONLY: comportamento antigo (pula complete se o
        // prepare falhou). Deve quebrar testes (fActive vazaria no XNU).
        if (phase == kSysmemPhase_PrepareAttempted) {
            // pula complete de propósito.
        } else {
            (void)ops.completeDma();
        }
#elif defined(GA106LAB_SYSMEM_MUT_NO_COMPLETE)
        // MUTAÇÃO TEST-ONLY: pula complete (deve quebrar testes).
#else
        (void)ops.completeDma(); // decremento já ocorreu; segue p/ clear.
#endif
#ifdef GA106LAB_SYSMEM_MUT_DOUBLE_COMPLETE
        (void)ops.completeDma(); // MUTAÇÃO TEST-ONLY: duplo complete.
#endif
    }
    if (needClear) {
#ifdef GA106LAB_SYSMEM_MUT_NO_CLEAR
        // MUTAÇÃO TEST-ONLY: pula clear (deve quebrar testes).
#else
        clearRc = ops.clearDma();
#endif
#ifdef GA106LAB_SYSMEM_MUT_DOUBLE_CLEAR
        (void)ops.clearDma(); // MUTAÇÃO TEST-ONLY: duplo clear.
#endif
#ifdef GA106LAB_SYSMEM_MUT_IGNORE_CLEAR_FAIL
        (void)clearRc; // MUTAÇÃO TEST-ONLY: ignora falha do clear.
#else
        if (clearRc != kIOReturnSuccess) {
            ops.lock();
            ops.setState(kSysmemPhase_CleanupFailed);
            ops.unlock();
            return kIOReturnError; // SEM releases: command ainda referencia.
        }
#endif
    }
#ifdef GA106LAB_SYSMEM_MUT_REL_CMD_BEFORE_CLEAR
    // (releaseCommand já executado acima, antes do clear.)
#else
    ops.releaseCommand();
#endif
#ifdef GA106LAB_SYSMEM_MUT_REL_DESC_BEFORE_CLEAR
    // (releaseDescriptor já executado acima, antes do clear.)
#else
    ops.releaseDescriptor();
#endif
    ops.lock();
    ops.clearAddressRecord();
    ops.setState(kSysmemPhase_Empty);
    ops.unlock();
    return kIOReturnSuccess;
}

template <typename Ops>
inline IOReturn RunPrepareGspSysmemFlow(Ops &ops, GA106LabGspSysmemV1 &out)
{
    uint32_t count = 0;
    uint64_t addr = 0;
    uint64_t len = 0;
    uint32_t st = 0;
    bool busy = false;
    unsigned k = 0;
    IOReturn teardownRc = kIOReturnSuccess;

    out.size = 0;
    out.version = 0;
    out.status = kGA106LabGspSysmemStatus_Failed;
    out.flags = 0;
    out.bufferSize = 0;
    out.segmentCount = 0;
    out.segmentLength = 0;
    out.state = kGA106LabGspSysmemState_Empty;
    for (k = 0; k < 4; k++) {
        out.reserved[k] = 0;
    }

    // Reserva atômica: check + IN_PROGRESS sob o mesmo lock.
    ops.lock();
    if (ops.isStopping()) {
        ops.unlock();
        return kIOReturnAborted;
    }
    st = ops.getState();
    busy = ops.isBusy();
#ifdef GA106LAB_SYSMEM_MUT_SECOND_ALLOC
    // MUTAÇÃO TEST-ONLY: ignora repeat-gate E busy-gate (realoca na
    // 2ª chamada; teste de repeat pega alloc==2).
#else
    if (st == kSysmemPhase_AddressReady && !busy) {
        ops.unlock();
        out.size = (uint32_t)sizeof(out);
        out.version = GA106LAB_UC_PROTOCOL_VERSION;
        out.status = kGA106LabGspSysmemStatus_AlreadyPrepared;
        out.flags = kGA106LabGspSysmemFlag_AddressReady;
        out.bufferSize = kGA106LabGspSysmem_Size;
        out.segmentCount = 1;
        out.segmentLength = kGA106LabGspSysmem_Size;
        out.state = kGA106LabGspSysmemState_AddressReady;
        return kIOReturnSuccess;
    }
    if (st != kSysmemPhase_Empty || busy) {
        ops.unlock();
        return (busy || st == kSysmemPhase_InProgress) ? kIOReturnBusy
                                                       : kIOReturnError;
    }
#endif
#ifdef GA106LAB_SYSMEM_MUT_NO_INPROGRESS
    // MUTAÇÃO TEST-ONLY: sem reserva (2ª chamada concorrente entra junto).
#else
    ops.setBusy(true);
    ops.setState(kSysmemPhase_InProgress);
#endif
    ops.unlock();

    if (!ops.allocBuffer()) {
        goto fail;
    }
    ops.lock();
    ops.setState(kSysmemPhase_Allocated);
    ops.unlock();
    if (!ops.zeroBuffer()) {
        goto fail;
    }
    if (!ops.createCommand()) {
        goto fail;
    }
    if (!ops.bindDescriptor()) {
        goto fail;
    }
    ops.lock();
    ops.setState(kSysmemPhase_Bound);
    ops.unlock();
    ops.lock();
    ops.setState(kSysmemPhase_PrepareAttempted);
    ops.unlock();
    if (!ops.prepareDma()) {
        goto fail; // teardown faz complete (attempt => fActive==1 no XNU).
    }
    ops.lock();
    ops.setState(kSysmemPhase_Prepared);
    ops.unlock();
    if (!ops.genSegments(count, addr, len)) {
        goto fail;
    }
#ifdef GA106LAB_SYSMEM_MUT_ACCEPT_MULTI_SEG
    // MUTAÇÃO TEST-ONLY: aceita qualquer count (deve quebrar testes).
#else
    if (count != 1 || len != (uint64_t)kGA106LabGspSysmem_Size) {
        goto fail;
    }
#endif
    if ((addr % (uint64_t)kGA106LabGspSysmem_Alignment) != 0u) {
        goto fail;
    }
    if (!ops.validateAddressWidth(addr)) {
        goto fail;
    }

    ops.lock();
    ops.markAddressReady(addr, (uint32_t)len);
    ops.setState(kSysmemPhase_AddressReady);
    ops.setBusy(false);
    ops.unlock();

    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
    out.status = kGA106LabGspSysmemStatus_AddressReady;
    out.flags = kGA106LabGspSysmemFlag_AddressReady;
    out.bufferSize = kGA106LabGspSysmem_Size;
    out.segmentCount = 1;
    out.segmentLength = (uint32_t)len;
    out.state = kGA106LabGspSysmemState_AddressReady;
#ifdef GA106LAB_SYSMEM_MUT_EXPOSE_IOVA
    out.reserved[0] = (uint32_t)addr; // MUTAÇÃO TEST-ONLY: vaza IOVA.
#endif
    return kIOReturnSuccess;

fail:
    teardownRc = ReleaseGspSysmemFlow(ops);
    ops.lock();
    ops.setBusy(false);
    if (teardownRc != kIOReturnSuccess) {
        ops.setState(kSysmemPhase_CleanupFailed);
    }
    ops.unlock();
    return kIOReturnError;
}

// Espera serializada de stop (MESMA policy na produção e nos testes):
// flag terminal + poll !BUSY SEM lock retido (op in-flight sempre alcança
// um commit point após cada KPI bloqueante) + reserva + teardown central.
// Nunca toca objetos de operação alheia. Chamador (stop) continua com
// slot-cleanup + super::stop depois.
template <typename Ops>
inline void WaitForSysmemIdleAndTeardownFlow(Ops &ops)
{
    ops.lock();
    ops.setStopping(true);
    while (ops.isBusy()) {
        ops.unlock();
        ops.waitMs(1); // 1ms, sem lock; IODelay (Real) / usleep (Mock).
        ops.lock();
    }
    ops.setBusy(true); // reserva para o teardown.
    ops.unlock();
    (void)ReleaseGspSysmemFlow(ops);
    ops.lock();
    ops.setBusy(false);
    ops.unlock();
}

#endif /* GA106LAB_GSP_SYSMEM_FLOW_HPP */
