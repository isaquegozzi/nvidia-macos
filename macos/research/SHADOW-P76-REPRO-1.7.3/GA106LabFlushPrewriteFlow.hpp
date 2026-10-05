// GA106LabFlushPrewriteFlow.hpp — Gate A PREWRITE shared flow (TG-KEXT7.1, P74).
//
// RunVerifyFlushPrewriteFlow<Ops> é O caminho de produção do selector9
// (GA106Lab::verifySysmemFlushPreconditions é wrapper fino) e o MESMO
// template instanciado pelos testes host com MockOps. Static dispatch.
//
// SINGLE-OWNER reservation (fix P74 Blocker A): o método outer detém a
// reserva fFlushBusy durante TODA a operação; as etapas de página NÃO
// tocam no busy bit (sem template aninhado com reserva própria). Nunca os dois.
// Segunda chamada concorrente => Busy definido. Stop espera !busy.
//
// FAILURE-SAFE teardown (fix P74 Blocker B): clearMemoryDescriptor com
// retorno CHECADO. clear != Success => CleanupFailed, SEM releases, SEM
// zerar ponteiros/estado (controlled retention, filosofia XNU). Sem
// double-clear/complete/release. Teardown em Unprepared => no-op.
//
// Fases fFlushState (domínio flush; ABI expõe SÓ 0/1/2):
//   0 Unprepared, 1 AddressReady (interno), 2 PreconditionsReady,
//   20 CleanupFailed (interno; exige reboot/unload, sem retry).
// Diagnósticos HI/LO/WPR2/BOOT omitidos (NotChecked) — ver P68 docs.
//
// Zero MMIO stores / PCI writes / BME / DMA trigger / Falcon/GSP/firmware
// por construção: nenhuma primitiva de escrita existe neste header.

#ifndef GA106LAB_FLUSH_PREWRITE_FLOW_HPP
#define GA106LAB_FLUSH_PREWRITE_FLOW_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

// Gate A pagina dedicada: mesmos constraints da selector7, objetos DISJUNTOS.
#define GA106LAB_FLUSH_PREWRITE_PAGE_SIZE 4096u
#define GA106LAB_FLUSH_PREWRITE_PAGE_ALIGN 4096u
#define GA106LAB_FLUSH_PREWRITE_SEG_LEN 4096u
#define kGA106LabFlushPrewrite_Size 4096u
#define kGA106LabFlushPrewrite_Alignment 4096u

// Fase interna: clear falhou, objetos retidos de propósito. Sem retry.
#define kGA106LabFlushPrewritePhase_CleanupFailed 20u

// Encoding NV_PFB_NISO_FLUSH_* (P65/P66 reconfirmado; P67 endurecido).
static inline uint32_t GA106LabFlushEncodeLo(uint64_t dmaAddr)
{
    return (uint32_t)((dmaAddr >> 8u) & 0xFFFFFFFFu);
}
static inline uint32_t GA106LabFlushEncodeHi(uint64_t dmaAddr)
{
    return (uint32_t)((dmaAddr >> 40u) & 0xFFFFFFu);
}
static inline uint64_t GA106LabFlushDecode(uint32_t lo, uint32_t hi)
{
    return (((uint64_t)(hi & 0xFFFFFFu)) << 40u) | (((uint64_t)lo) << 8u);
}
// Alinhamento minimo HW: low 8 bits ignorados => 256 B.
static inline int GA106LabFlushAlignmentOk(uint64_t dmaAddr)
{
    return ((dmaAddr & 0xFFu) == 0u) ? 1 : 0;
}
// Representavel bits 8..63: HI cabe em 24 bits + roundtrip + sem overflow.
static inline int GA106LabFlushRepresentable(uint64_t dmaAddr)
{
    uint32_t lo;
    uint32_t hi;
    uint64_t rt;
    if (!GA106LabFlushAlignmentOk(dmaAddr)) {
        return 0;
    }
    lo = GA106LabFlushEncodeLo(dmaAddr);
    hi = GA106LabFlushEncodeHi(dmaAddr);
    rt = GA106LabFlushDecode(lo, hi);
    if (rt != dmaAddr) {
        return 0;
    }
    if (!GA106LabSysmemAddressValid(dmaAddr)) {
        return 0;
    }
    if (dmaAddr + (uint64_t)4096u < dmaAddr) {
        return 0;
    }
    return 1;
}
static inline int GA106LabFlushRoundtripOk(uint64_t dmaAddr)
{
    return GA106LabFlushRepresentable(dmaAddr);
}

// Fill ABI zero + versao/tamanho (sem memset).
static inline void GA106LabFlushPrewriteZero(GA106LabFlushPrewriteV1 &out)
{
    unsigned k = 0;
    uint8_t *b = (uint8_t *)&out;
    for (k = 0; k < sizeof(out); k++) {
        b[k] = 0;
    }
    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
}

// Teardown failure-safe (fix Blocker B). Chamador detém BUSY.
// - Unprepared => no-op Success (idempotente; repeated teardown seguro).
// - CleanupFailed => Error sem tocar (sem double-clear/complete/release).
// - Caso contrário: complete (retorno não bloqueia clear) + clear CHECADO:
//   clear != Success => CleanupFailed, SEM releases, SEM zerar ponteiros
//   (controlled retention, NÃO leak normal); clear ok => releases + reset.
// Ops deve prover: lock/unlock/getState/setState/completeDma/clearDma/
// releaseCommand/releaseDescriptor/clearAddressRecord/hasPageObjects.
template <typename Ops>
inline IOReturn TeardownFlushPrewritePageFlowSafe(Ops &ops)
{
    uint32_t st = 0;
    IOReturn clr = kIOReturnSuccess;
    ops.lock();
    st = ops.getState();
    ops.unlock();
    if (st == kGA106LabFlushPrewritePhase_CleanupFailed) {
        return kIOReturnError;
    }
    // Decisão por presença de objetos (o template não usa fases
    // intermediárias): sem objetos => no-op; com objetos => complete+clear.
    if (!ops.hasPageObjects()) {
        if (st != kGA106LabFlushPrewritePhase_Unprepared) {
            ops.lock();
            ops.setState(kGA106LabFlushPrewritePhase_Unprepared);
            ops.unlock();
        }
        return kIOReturnSuccess;
    }
    // complete() decrementa antes de trabalhar (modelo XNU): seu erro nunca
    // impede o clear; clear é o gate de ownership.
    (void)ops.completeDma();
    clr = ops.clearDma();
    if (clr != kIOReturnSuccess) {
#ifdef GA106LAB_FLUSH_MUT_RELEASE_AFTER_FAILED_CLEAR
        // MUTAÇÃO TEST-ONLY (só no caminho de falha): ignora o clear falho
        // e libera mesmo assim. O teste de clear-failure deve FALHAR
        // (pega use-after-free potencial); caminho normal intacto.
        ops.releaseCommand();
        ops.releaseDescriptor();
        ops.clearAddressRecord();
        ops.lock();
        ops.setState(kGA106LabFlushPrewritePhase_Unprepared);
        ops.unlock();
        return kIOReturnSuccess;
#endif
#ifdef GA106LAB_FLUSH_MUT_ZERO_AFTER_FAILED_CLEAR
        // MUTAÇÃO TEST-ONLY (só no caminho de falha): zera ownership sem
        // liberar (dangling). O teste deve FALHAR no address-record retido.
        ops.clearAddressRecord();
        ops.lock();
        ops.setState(kGA106LabFlushPrewritePhase_CleanupFailed);
        ops.unlock();
        return clr;
#endif
        ops.lock();
        ops.setState(kGA106LabFlushPrewritePhase_CleanupFailed);
        ops.unlock();
        return clr;
    }
    ops.releaseCommand();
    ops.releaseDescriptor();
    ops.clearAddressRecord();
    ops.lock();
    ops.setState(kGA106LabFlushPrewritePhase_Unprepared);
    ops.unlock();
    return kIOReturnSuccess;
}

// Alias legado (1.7.0, inseguro): mantido apenas para o repro do Blocker B.
// NÃO usar em produção. Produção usa TeardownFlushPrewritePageFlowSafe.
template <typename Ops>
inline void TeardownFlushPrewritePageFlow(Ops &ops)
{
    (void)ops.completeDma();
    (void)ops.clearDma();
    ops.releaseCommand();
    ops.releaseDescriptor();
    ops.clearAddressRecord();
    ops.lock();
    ops.setState(kGA106LabFlushPrewritePhase_Unprepared);
    ops.unlock();
}

// Orquestração Gate A, SINGLE-OWNER (fix Blocker A).
// Ops deve prover (static dispatch, sem virtual):
//   lock(); unlock();
//   bool isStopping(); bool isBusy(); void setBusy(bool);
//   uint32_t getState(); void setState(uint32_t);
//   bool hasPageObjects(); bool getStoredPage(uint64_t &addr, uint32_t &len);
//   IOReturn checkTarget(bool &providerReady, bool &ga106Exact,
//                        bool &mse, bool &bme);   // erro transporte => recurso
//   IOReturn checkBar0(bool &ready);
//   IOReturn checkGfw(bool &ready);
//   bool allocBuffer(); bool zeroBuffer(); bool createCommand();
//   bool bindDescriptor(); bool prepareDma();
//   bool genSegments(uint32_t &n, uint64_t &addr, uint64_t &len);
//   bool validateAddressWidth(uint64_t addr);
//   void markAddressReady(uint64_t addr, uint32_t len);
//   IOReturn completeDma(); IOReturn clearDma();
//   void releaseCommand(); void releaseDescriptor(); void clearAddressRecord();
//
// Contrato transporte-vs-semantica: Success + status Failed = resposta valida
// com precondicao nao atendida; erro IOKit = recurso/Busy/Aborted.
template <typename Ops>
inline IOReturn RunVerifyFlushPrewriteFlow(Ops &ops, GA106LabFlushPrewriteV1 &out)
{
    IOReturn kr = kIOReturnSuccess;
    IOReturn td = kIOReturnSuccess;
    bool stopping = false;
    bool reuse = false;
    bool prov = false;
    bool exact = false;
    bool mse = false;
    bool bme = false;
    bool barReady = false;
    bool gfwReady = false;
    uint32_t n = 0;
    uint64_t addr = 0;
    uint64_t len = 0;
    uint32_t slen = 0;

    GA106LabFlushPrewriteZero(out);
    out.status = kGA106LabFlushPrewriteStatus_Failed;
    out.phase = kGA106LabFlushPrewritePhase_Unprepared;
    out.flushRegistersState = kGA106LabFlushRegs_NotChecked;

    // FASE 0 — checks somente-leitura SEM reserva (fix F-L1: estreita a janela
    // busy para fora do poll bounded do GFW; checks não têm efeitos colaterais,
    // então é seguro executá-los antes de reservar). Nenhum goto daqui toca
    // em busy (reserva ainda não adquirida): retorno direto.
    kr = ops.checkTarget(prov, exact, mse, bme);
    if (kr != kIOReturnSuccess) {
        return kr;
    }
    out.providerReady = prov ? 1 : 0;
    out.ga106Exact = exact ? 1 : 0;
    out.mseEnabled = mse ? 1 : 0;
    out.bmeEnabled = bme ? 1 : 0;
    if (!prov || !exact) {
        return kIOReturnSuccess;
    }
    if (!mse) {
        return kIOReturnSuccess;
    }
    if (bme) {
        return kIOReturnSuccess; // BmeMustBeOff; nunca desliga aqui.
    }

    kr = ops.checkBar0(barReady);
    if (kr != kIOReturnSuccess) {
        return kr;
    }
    out.bar0Ready = barReady ? 1 : 0;
    if (!barReady) {
        return kIOReturnSuccess;
    }

    kr = ops.checkGfw(gfwReady);
    if (kr != kIOReturnSuccess) {
        return kr;
    }
    out.gfwReady = gfwReady ? 1 : 0;
    if (!gfwReady) {
        return kIOReturnSuccess;
    }

    // FASE 1 — reserva ÚNICA da operação (dono: este método; etapas internas
    // NÃO reservam — fix Blocker A: nunca dois donos). Stop durante a fase 0
    // é observado aqui (stopping checado sob trava antes de reservar).
    ops.lock();
    if (ops.isStopping()) {
        ops.unlock();
        return kIOReturnAborted;
    }
    if (ops.getState() == kGA106LabFlushPrewritePhase_CleanupFailed) {
        ops.unlock();
        return kIOReturnError; // sem retry após falha de clear.
    }
    if (ops.isBusy()) {
        ops.unlock();
        return kIOReturnBusy;
    }
#ifdef GA106LAB_FLUSH_MUT_NO_RESERVE
    // MUTAÇÃO TEST-ONLY: sem reserva (teste concorrente pega double-alloc).
#else
    ops.setBusy(true);
#endif
#ifdef GA106LAB_FLUSH_MUT_FORCE_FIRST_BUSY
    // MUTAÇÃO TEST-ONLY: força Busy na primeira chamada.
    ops.unlock();
    return kIOReturnBusy;
#endif
    reuse = (ops.getState() == kGA106LabFlushPrewritePhase_PreconditionsReady &&
             ops.hasPageObjects());
    ops.unlock();

    // 5a. Reuso: revalida mapping persistido, sem realocar.
    if (reuse) {
#ifdef GA106LAB_FLUSH_MUT_SECOND_MAPPING_ON_READY
        // MUTAÇÃO TEST-ONLY: aloca segundo mapping (teste repeat pega alloc==2).
        (void)ops.allocBuffer();
#endif
        if (!ops.getStoredPage(addr, slen)) {
            kr = kIOReturnError;
            goto fail_teardown;
        }
        if (slen != kGA106LabFlushPrewrite_Size ||
            !GA106LabFlushRepresentable(addr)) {
            kr = kIOReturnError;
            goto fail_teardown;
        }
        out.pageReady = 1;
        out.segmentCount = 1;
        out.segmentLength = kGA106LabFlushPrewrite_Size;
        out.alignmentReady = 1;
        out.addressRepresentable = 1;
        out.encodingRoundtripReady = 1;
        out.phase = kGA106LabFlushPrewritePhase_PreconditionsReady;
        out.status = kGA106LabFlushPrewriteStatus_AlreadyReady;
        ops.lock();
        ops.setBusy(false);
        ops.unlock();
        return kIOReturnSuccess;
    }

    // 5b. Preparo novo (etapas SEM reserva própria; dono é este método).
#ifdef GA106LAB_FLUSH_MUT_DOUBLE_RESERVE
    // MUTAÇÃO TEST-ONLY: reintroduz reserva aninhada (primeira chamada => Busy).
    ops.lock();
    if (ops.isBusy()) {
        ops.unlock();
        kr = kIOReturnBusy;
        goto fail_clean;
    }
    ops.unlock();
#endif
    if (!ops.allocBuffer()) {
        kr = kIOReturnNoMemory;
        goto fail_teardown;
    }
    ops.lock();
    stopping = ops.isStopping();
    ops.unlock();
    if (stopping) {
        kr = kIOReturnAborted;
        goto fail_teardown;
    }
    if (!ops.zeroBuffer()) {
        kr = kIOReturnNoMemory;
        goto fail_teardown;
    }
    if (!ops.createCommand()) {
        kr = kIOReturnNoResources;
        goto fail_teardown;
    }
    if (!ops.bindDescriptor()) {
        kr = kIOReturnError;
        goto fail_teardown;
    }
    ops.lock();
    stopping = ops.isStopping();
    ops.unlock();
    if (stopping) {
        kr = kIOReturnAborted;
        goto fail_teardown;
    }
    if (!ops.prepareDma()) {
        kr = kIOReturnError;
        goto fail_teardown;
    }
    // prepareDma pode bloquear (gate); stop pode ter sinalizado no meio.
    ops.lock();
    stopping = ops.isStopping();
    ops.unlock();
    if (stopping) {
        kr = kIOReturnAborted;
        goto fail_teardown;
    }
    if (!ops.genSegments(n, addr, len)) {
        kr = kIOReturnError;
        goto fail_teardown;
    }
    if (n != 1 || len != kGA106LabFlushPrewrite_Size) {
        kr = kIOReturnError;
        goto fail_teardown;
    }
    if (!ops.validateAddressWidth(addr)) {
        kr = kIOReturnError;
        goto fail_teardown;
    }
    if (!GA106LabFlushRepresentable(addr)) {
        kr = kIOReturnSuccess; // IOVA fora do encoding: resposta valida, FAIL.
        goto fail_teardown_semantic;
    }
    ops.markAddressReady(addr, (uint32_t)len);
    out.pageReady = 1;
    out.segmentCount = 1;
    out.segmentLength = kGA106LabFlushPrewrite_Size;
    out.alignmentReady = 1;
    out.addressRepresentable = 1;
    out.encodingRoundtripReady = 1;
    out.phase = kGA106LabFlushPrewritePhase_PreconditionsReady;
    out.status = kGA106LabFlushPrewriteStatus_PreconditionsReady;
    ops.lock();
    ops.setState(kGA106LabFlushPrewritePhase_PreconditionsReady);
    ops.setBusy(false);
    ops.unlock();
    return kIOReturnSuccess;

fail_teardown_semantic:
    td = TeardownFlushPrewritePageFlowSafe(ops);
    (void)td;
    out.status = kGA106LabFlushPrewriteStatus_Failed;
    out.phase = kGA106LabFlushPrewritePhase_Unprepared;
    ops.lock();
    ops.setBusy(false);
    ops.unlock();
    return kr;

fail_teardown:
    td = TeardownFlushPrewritePageFlowSafe(ops);
    if (td != kIOReturnSuccess) {
        // Teardown parcial reteve objetos (clear falhou): propaga o erro de
        // recurso, sem esconder atrás do erro original.
        out.status = kGA106LabFlushPrewriteStatus_Failed;
        out.phase = kGA106LabFlushPrewritePhase_Unprepared;
        ops.lock();
        ops.setBusy(false);
        ops.unlock();
        return td;
    }
    out.status = kGA106LabFlushPrewriteStatus_Failed;
    out.phase = kGA106LabFlushPrewritePhase_Unprepared;
    ops.lock();
    ops.setBusy(false);
    ops.unlock();
    return kr;

fail_clean:
    out.status = kGA106LabFlushPrewriteStatus_Failed;
    out.phase = kGA106LabFlushPrewritePhase_Unprepared;
    ops.lock();
    ops.setBusy(false);
    ops.unlock();
    return kr;
}

#endif /* GA106LAB_FLUSH_PREWRITE_FLOW_HPP */
