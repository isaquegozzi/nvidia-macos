// GA106LabBmeFlow.hpp — BME ENABLE shared flow (R2, ENABLE_BUS_MASTER).
//
// Único fluxo decisório do selector 11. Produção (BmeRealOps) e testes
// (BmeMockOps) instanciam O MESMO template RunBmeEnableFlow<Ops>.
// Static dispatch, sem virtual, sem callbacks runtime no ABI/UserClient.
//
// Ordem (fail-closed, sem retry, sem DMA kick, sem auto-disable):
//   zero ABI -> stopping?Abort -> reserve BUSY (Busy) ->
//   latch BME != Disabled? AlreadyEnabled/RecoveryRequired ->
//   boot binding (svcNonce!=0; GateA nonce==svc; GateB nonce==svc;
//     GateB latch==HIAndLOWritten; epoch existe==svc)
//     (BootMismatch/StaleGateA/StaleGateB/LatchMismatch) ->
//   probe HI/LO fixa (ProbeMismatch) ->
//   pre-read COMMAND (transport=>InternalError; BME set=>BmeAlreadySet;
//     MSE clear=>MseDisabled) ->
//   GFW check (GfwNotReady) ->
//   stopping recheck (Aborted, zero writes) ->
//   latch=EnableAttempted -> WRITE RMW (cur|0x4) ->
//   readback (transport=>UnknownPartial+reboot; BME/MSE/bits=>UnknownPartial+reboot;
//     match=>EnabledSoftwareSide) ->
//   phase Enabled, Success.
//
// Transporte: Success para desfechos determinados (ABI carrega status; validator
// decide exit do CLI). Exceções: Busy e Aborted. BmeAlreadySet é resposta válida
// (Success, sem write). Sem DMA kick em qualquer caminho. Sem disable automático
// em qualquer caminho (disable = review separada; reboot-class se preciso).
// Latch BME separado, monotônico, reboot-only (nunca confundir com latch Gate B).
//
// Ops deve prover (static dispatch, sem virtual):
//   lock(); unlock();
//   bool isStopping(); bool isBusy(); void setBusy(bool);
//   uint32_t getBmeLatch(); void setBmeLatch(uint32_t);
//   uint64_t getServiceNonce();          // nonce do objeto serviço (init);
//     binding usa igualdade de nonces + época, nunca relógio
//   bool acquireProvider(); void releaseProvider();
//   bool getGateARecord(uint64_t &nonceOut, uint64_t &dmaOut);
//   bool getGateBRecord(uint64_t &nonceOut, uint64_t &dmaOut);
//   uint32_t getGateBLatch();
//   bool getProviderEpoch(bool &hasOut, uint64_t &epochOut);
//   bool probeFlushPair(uint32_t &hiOut, uint32_t &loOut);  // RO, offsets fixos
//   bool readCommand(uint16_t &cmdOut);
//   IOReturn checkGfw(bool &ready);
//   bool writeBmeCommand(uint16_t v);    // RMW já calculado pelo flow; false = recusado
// Nenhuma decisão vive nos Ops.

#ifndef GA106LAB_BME_FLOW_HPP
#define GA106LAB_BME_FLOW_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

// Bit BME no PCI COMMAND (SDK: kIOPCICommandBusLead = 0x4; BusMaster é alias
// deprecado do mesmo bit). MSE = 0x2 (nunca mexido aqui).
#define GA106LAB_BME_BIT 0x0004u
#define GA106LAB_MSE_BIT 0x0002u

template <typename Ops>
inline IOReturn RunBmeEnableFlow(Ops &ops, GA106LabBmeEnableV1 &out)
{
    uint64_t svcNonce = 0;
    uint64_t gateANonce = 0;
    uint64_t gateADma = 0;
    uint64_t gateBNonce = 0;
    uint64_t gateBDma = 0;
    uint32_t gateBLatch = 0;
    bool hasEpoch = false;
    uint64_t epoch = 0;
    uint32_t hiObs = 0;
    uint32_t loObs = 0;
    uint32_t eHi = 0;
    uint32_t eLo = 0;
    uint16_t cmdBefore = 0;
    uint16_t cmdAfter = 0;
    uint32_t bmeLatch = 0;
    bool haveProv = false;

    out.size = 0;
    out.version = 0;
    out.status = kGA106LabBmeStatus_InternalError;
    out.phase = kGA106LabBmePhase_Idle;
    out.bootFresh = 0;
    out.sameBootGateA = 0;
    out.sameBootGateB = 0;
    out.gateBProbeMatch = 0;
    out.mseEnabledBefore = 0;
    out.bmeEnabledBefore = 0;
    out.bmeWriteAttempted = 0;
    out.bmeReadbackVerified = 0;
    out.mseEnabledAfter = 0;
    out.bmeEnabledAfter = 0;
    out.writeCount = 0;
    out.recoveryRequired = 0;
    out.reserved0 = 0;
    out.reserved1 = 0;
    out.reserved2 = 0;
    out.reserved3 = 0;
    out.reserved[0] = 0;
    out.reserved[1] = 0;
    out.reserved[2] = 0;
    out.reserved[3] = 0;
    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;

    // Reserva + stopping (seções curtas).
    ops.lock();
    if (ops.isStopping()) {
        ops.unlock();
        out.status = kGA106LabBmeStatus_Stopping;
        return kIOReturnAborted;
    }
    if (ops.isBusy()) {
        ops.unlock();
        out.status = kGA106LabBmeStatus_InternalError;
        return kIOReturnBusy;
    }
    ops.setBusy(true);
    bmeLatch = ops.getBmeLatch();
    ops.unlock();

    // Latch BME separado: qualquer estado != Disabled recusa sem tocar HW.
#ifdef GA106LAB_BME_MUT_ALLOW_SECOND
    // MUTAÇÃO TEST-ONLY: ignora latch (segunda habilitação prossegue).
    (void)bmeLatch;
#else
    if (bmeLatch != kBmeLatch_Disabled) {
        if (bmeLatch == kBmeLatch_EnabledSoftwareSide) {
            // Fase Idle honesta: esta chamada nada habilitou; o status diz tudo.
            // (phase Enabled aqui quebraria o validator Success<=>Enabled.)
            out.status = kGA106LabBmeStatus_AlreadyEnabled;
            out.phase = kGA106LabBmePhase_Idle;
        } else {
            out.status = kGA106LabBmeStatus_RecoveryRequired;
            out.recoveryRequired = 1;
        }
        ops.lock();
        ops.setBusy(false);
        ops.unlock();
        return kIOReturnSuccess;
    }
#endif

    // Pin do provider (retain). A partir daqui, liberar em TODAS as saídas.
    if (!ops.acquireProvider()) {
        out.status = kGA106LabBmeStatus_ProviderInvalid;
        goto fail_no_write;
    }
    haveProv = true;

    // Amarração de boot: nonce do serviço (nunca zero em objeto válido).
    svcNonce = ops.getServiceNonce();
#ifdef GA106LAB_BME_MUT_NO_BOOT_NONCE
    // MUTAÇÃO TEST-ONLY: aceita nonce zero (objeto inválido prossegue).
#else
    if (svcNonce == 0) {
        out.status = kGA106LabBmeStatus_InternalError;
        goto fail_no_write;
    }
#endif
#ifdef GA106LAB_BME_MUT_LATCH_ONLY
    // MUTAÇÃO TEST-ONLY: só o latch BME decide (amarração ignorada).
    out.sameBootGateA = 1;
    out.sameBootGateB = 1;
    out.bootFresh = 1;
    out.gateBProbeMatch = 1;
#else
    // Gate A: registro presente E mesmo nonce (latch freshness NUNCA substitui).
#ifdef GA106LAB_BME_MUT_SKIP_GATEA
    // MUTAÇÃO TEST-ONLY: finge Gate A válido.
    gateANonce = svcNonce; gateADma = 0x1000u;
#else
    if (!ops.getGateARecord(gateANonce, gateADma)) {
        out.status = kGA106LabBmeStatus_StaleGateA;
        goto fail_no_write;
    }
#endif
    if (gateANonce == 0 || gateANonce != svcNonce) {
        out.status = kGA106LabBmeStatus_StaleGateA;
        goto fail_no_write;
    }
    out.sameBootGateA = 1;
    // Gate B: registro presente E mesmo nonce E latch Gate B programado.
#ifdef GA106LAB_BME_MUT_SKIP_GATEB
    // MUTAÇÃO TEST-ONLY: finge Gate B válido.
    gateBNonce = svcNonce; gateBDma = 0x1000u;
#else
    if (!ops.getGateBRecord(gateBNonce, gateBDma)) {
        out.status = kGA106LabBmeStatus_StaleGateB;
        goto fail_no_write;
    }
#endif
    if (gateBNonce == 0 || gateBNonce != svcNonce) {
        out.status = kGA106LabBmeStatus_StaleGateB;
        goto fail_no_write;
    }
#ifdef GA106LAB_BME_MUT_SKIP_LATCH_CHECK
    // MUTAÇÃO TEST-ONLY: ignora latch do Gate B.
    gateBLatch = kGateBLatch_HIAndLOWritten;
#else
    gateBLatch = ops.getGateBLatch();
#endif
    if (gateBLatch != kGateBLatch_HIAndLOWritten) {
        out.status = kGA106LabBmeStatus_LatchMismatch;
        goto fail_no_write;
    }
    out.sameBootGateB = 1;
    // Época no provider: deve existir E igualar o nonce (detecta reload com
    // DMA coincidente, onde latch+registros sozinhos passariam).
#ifdef GA106LAB_BME_MUT_ALLOW_RELOAD
    // MUTAÇÃO TEST-ONLY: ignora época (reload passa).
#else
    if (!ops.getProviderEpoch(hasEpoch, epoch)) {
        out.status = kGA106LabBmeStatus_InternalError;
        goto fail_no_write;
    }
    if (!hasEpoch || epoch == 0 || epoch != svcNonce) {
        out.status = kGA106LabBmeStatus_BootMismatch;
        goto fail_no_write;
    }
#endif
    out.bootFresh = 1;

    // Probe do par (RO, offsets fixos): deve recompor gateBDma exatamente.
    if (!ops.probeFlushPair(hiObs, loObs)) {
        out.status = kGA106LabBmeStatus_InternalError;
        goto fail_no_write;
    }
    eHi = (uint32_t)((gateBDma >> 40u) & 0xFFFFFFu);
    eLo = (uint32_t)((gateBDma >> 8u) & 0xFFFFFFFFu);
#ifdef GA106LAB_BME_MUT_IGNORE_RECOVERY
    // MUTAÇÃO TEST-ONLY: mismatch ignorado (prossegue sem recuperação).
    out.gateBProbeMatch = 1;
    (void)hiObs; (void)loObs; (void)eHi; (void)eLo;
#else
    if (hiObs != eHi || loObs != eLo) {
        out.status = kGA106LabBmeStatus_ProbeMismatch;
        goto fail_no_write;
    }
    out.gateBProbeMatch = 1;
#endif
#endif

    // Pre-read COMMAND (16-bit).
#ifdef GA106LAB_BME_MUT_SKIP_PREREAD
    // MUTAÇÃO TEST-ONLY: pula pre-read (usa zero).
    cmdBefore = 0;
#else
    if (!ops.readCommand(cmdBefore)) {
        out.status = kGA106LabBmeStatus_InternalError;
        goto fail_no_write;
    }
#endif
    out.mseEnabledBefore = ((cmdBefore & GA106LAB_MSE_BIT) != 0) ? 1 : 0;
    out.bmeEnabledBefore = ((cmdBefore & GA106LAB_BME_BIT) != 0) ? 1 : 0;
    if (out.bmeEnabledBefore != 0) {
        out.status = kGA106LabBmeStatus_BmeAlreadySet;
        goto fail_no_write;
    }
    if (out.mseEnabledBefore == 0) {
        out.status = kGA106LabBmeStatus_MseDisabled;
        goto fail_no_write;
    }
    // GFW ainda pronto (condição de contorno; sem poll aqui).
    {
        bool gfwReady = false;
        if (ops.checkGfw(gfwReady) != kIOReturnSuccess || !gfwReady) {
            out.status = kGA106LabBmeStatus_GfwNotReady;
            goto fail_no_write;
        }
    }
    goto bme_write_phase;

fail_no_write:
    if (haveProv) {
        ops.releaseProvider();
        haveProv = false;
    }
    ops.lock();
    ops.setBusy(false);
    ops.unlock();
    return kIOReturnSuccess;

bme_write_phase:
    // Stopping final antes do único store.
#ifdef GA106LAB_BME_MUT_SKIP_PREWRITE_STOP
    // MUTAÇÃO TEST-ONLY: ignora stop pré-write.
#else
    ops.lock();
    {
        bool stoppingNow = ops.isStopping();
        ops.unlock();
        if (stoppingNow) {
            out.status = kGA106LabBmeStatus_Stopping;
            if (haveProv) {
                ops.releaseProvider();
                haveProv = false;
            }
            ops.lock();
            ops.setBusy(false);
            ops.unlock();
            return kIOReturnAborted;
        }
    }
#endif

    // WRITE único: RMW com MSE + demais bits preservados (só bit2 liga).
    out.bmeWriteAttempted = 1;
    ops.lock();
    ops.setBmeLatch(kBmeLatch_EnableAttempted);
    ops.unlock();
    {
        uint16_t newCmd = (uint16_t)(cmdBefore | GA106LAB_BME_BIT);
#ifdef GA106LAB_BME_MUT_HARDCODE_CMD
        // MUTAÇÃO TEST-ONLY: valor fixo (pode limpar MSE).
        newCmd = 0x0007u;
#endif
#ifdef GA106LAB_BME_MUT_CLOBBER_MSE
        // MUTAÇÃO TEST-ONLY: limpa MSE no write.
        newCmd = (uint16_t)(newCmd & ~GA106LAB_MSE_BIT);
#endif
        if (!ops.writeBmeCommand(newCmd)) {
            out.status = kGA106LabBmeStatus_UnknownPartial;
            out.recoveryRequired = 1;
            if (haveProv) {
                ops.releaseProvider();
                haveProv = false;
            }
            ops.lock();
            ops.setBmeLatch(kBmeLatch_UnknownPartial);
            ops.setBusy(false);
            ops.unlock();
            return kIOReturnSuccess;
        }
    }
    out.writeCount = 1;
#ifdef GA106LAB_BME_MUT_START_DMA
    // MUTAÇÃO TEST-ONLY: kick DMA após BME (proibido; só no Mock).
    ops.dmaKick();
#endif
#ifdef GA106LAB_BME_MUT_GENERIC_PCI
    // MUTAÇÃO TEST-ONLY: writer PCI genérico (só no Mock; produção não compila).
    ops.configWriteAny(0x04u, 0xFFFFu);
#endif

    // Readback: BME set + MSE preservado + demais bits preservados.
#ifdef GA106LAB_BME_MUT_SKIP_READBACK
    // MUTAÇÃO TEST-ONLY: pula readback (confia cegamente).
    out.mseEnabledAfter = out.mseEnabledBefore;
    out.bmeEnabledAfter = 1;
    out.bmeReadbackVerified = 1;
#else
    {
        uint16_t cmdRb = 0;
        if (!ops.readCommand(cmdRb)) {
            out.status = kGA106LabBmeStatus_UnknownPartial;
            out.recoveryRequired = 1;
            if (haveProv) {
                ops.releaseProvider();
                haveProv = false;
            }
            ops.lock();
            ops.setBmeLatch(kBmeLatch_UnknownPartial);
            ops.setBusy(false);
            ops.unlock();
            return kIOReturnSuccess;
        }
        out.mseEnabledAfter = ((cmdRb & GA106LAB_MSE_BIT) != 0) ? 1 : 0;
        out.bmeEnabledAfter = ((cmdRb & GA106LAB_BME_BIT) != 0) ? 1 : 0;
        if (out.bmeEnabledAfter == 0 || out.mseEnabledAfter == 0 ||
            (uint16_t)(cmdRb & ~GA106LAB_BME_BIT) !=
                (uint16_t)(cmdBefore & ~GA106LAB_BME_BIT)) {
            out.status = kGA106LabBmeStatus_UnknownPartial;
            out.recoveryRequired = 1;
            if (haveProv) {
                ops.releaseProvider();
                haveProv = false;
            }
            ops.lock();
            ops.setBmeLatch(kBmeLatch_UnknownPartial);
            ops.setBusy(false);
            ops.unlock();
            return kIOReturnSuccess;
        }
        out.bmeReadbackVerified = 1;
    }
#endif
#ifdef GA106LAB_BME_MUT_AUTO_DISABLE
    // MUTAÇÃO TEST-ONLY: auto-disable em erro/sucesso (proibido sem prova).
    ops.clearBme();
    ops.setBmeLatch(kBmeLatch_Disabled);
#endif
    ops.lock();
    ops.setBmeLatch(kBmeLatch_EnabledSoftwareSide);
    ops.unlock();
    out.status = kGA106LabBmeStatus_Success;
    out.phase = kGA106LabBmePhase_Enabled;
    out.recoveryRequired = 0;
    if (haveProv) {
        ops.releaseProvider();
        haveProv = false;
    }
    ops.lock();
    ops.setBusy(false);
    ops.unlock();
    return kIOReturnSuccess;
}

#endif /* GA106LAB_BME_FLOW_HPP */
