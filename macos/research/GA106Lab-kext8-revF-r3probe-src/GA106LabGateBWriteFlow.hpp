// GA106LabGateBWriteFlow.hpp — Gate B PROGRAM_SYSMEM_FLUSH shared flow (R2).
//
// Único fluxo decisório do selector 10. Produção (GateBWriteRealOps) e testes
// (GateBMockOps) instanciam O MESMO template RunGateBProgramFlow<Ops>.
// Static dispatch, sem virtual, sem callbacks runtime no ABI/UserClient.
//
// Ordem (fail-closed, sem retry, sem readback automático):
//   zero ABI -> stopping?Abort -> reserve BUSY (Busy) -> latch!=NeverTouched?AlreadyProgrammed ->
//   checkTarget (ProviderInvalid/MseDisabled) -> checkBar0 (BarUnavailable) ->
//   checkGfw (GfwNotReady) -> BME?BmeEnabled ->
//   getStoredPage+seg/len/align/width/representable/roundtrip
//     (PageInvalid/SegmentInvalid/LengthInvalid/AlignmentInvalid/
//      AddressNotRepresentable/EncodingMismatch) ->
//   createUcWriteMap (UcMappingUnavailable) -> opts writable-Inhibit EXATOS
//     (UcSemanticsUnproven) -> VA (UcMappingUnavailable) ->
//   bounds HI+4/LO+4 (UcMappingUnavailable) ->
//   BME re-read (BmeEnabled semântico) ->
//   HI store (hiAttempted) -> latch=HIWritten ->
//   stop-check (UnknownPartial + reboot se stopping) ->
//   LO store (loAttempted) -> latch=HIAndLOWritten ->
//   releaseUcMap -> clearBusy -> Success/Programmed.
//
// Transporte: Success para TODOS os desfechos determinados (a ABI carrega o
// status específico; o validator decide o exit do CLI). Exceções: Busy e
// Aborted (stopping). Sem retry em nenhum caminho. Sem readback automático
// (aprovado separadamente). Sem BME/PCI/DMA/GSP em qualquer caminho.
//
// Latch one-way (service-owned via ops.getLatch/setLatch, seções curtas):
// NeverTouched->HIWritten->HIAndLOWritten; qualquer incerteza pós-store->
// UnknownPartial. NUNCA volta a NeverTouched no mesmo boot (só reboot).
// Teardown aqui = releaseUcMap + clearBusy + (host-side releases); ZERO
// MMIO stores fora dos 2 intencionais — teardown NUNCA escreve registrador.
//
// Ops deve prover (static dispatch, sem virtual):
//   lock(); unlock();
//   bool isStopping(); bool isBusy(); void setBusy(bool);
//   uint32_t getLatch(); void setLatch(uint32_t);
//   bool acquireProvider(); void releaseProvider();
//   IOReturn checkTarget(bool &prov, bool &exact, bool &mse, bool &bme);
//   IOReturn checkBar0(bool &ready);
//   IOReturn checkGfw(bool &ready);
//   bool getStoredPage(uint64_t &addrOut, uint32_t &lenOut, uint32_t &segOut);
//   bool createUcWriteMap(uint64_t &mapLenOut, uint32_t &mapOptsOut);
//   uint64_t getUcVA();
//   void writeFlushHi(uint32_t v);   // offset fixo HI, ÚNICO callsite
//   void writeFlushLo(uint32_t v);   // offset fixo LO, ÚNICO callsite
//   void releaseUcMap();
// Nenhuma decisão vive nos Ops.

#ifndef GA106LAB_GATEB_WRITE_FLOW_HPP
#define GA106LAB_GATEB_WRITE_FLOW_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

// Offsets fixos do par Gate B vivem em GA106LabCoreValidate.h
// (GA106LAB_GATEB_LO/HI_OFFSET/WIDTH; compile-time, nunca do userspace).

// Generic writer: intencionalmente exige Ops::writeRegisterAny, que SOMENTE
// o Mock implementa (contado). GateBWriteRealOps NÃO o implementa: qualquer
// uso acidental em produção = erro de compilação. Auditoria complementar em
// GATEB-NO-HIDDEN-WRITES-AUDIT.md. Testes afirmam genericWrites==0 sem hook.
template <typename Ops>
inline void GateBWriteRegister32(Ops &ops, uint32_t off, uint32_t v)
{
    ops.writeRegisterAny(off, v);
}

template <typename Ops>
inline IOReturn RunGateBProgramFlow(Ops &ops, GA106LabGateBProgramV1 &out)
{
    bool prov = false;
    bool exact = false;
    bool mse = false;
    bool bme = false;
    bool barReady = false;
    bool gfwReady = false;
    uint64_t addr = 0;
    uint32_t segLen = 0;
    uint32_t segCount = 0;
    uint64_t mapLen = 0;  // 64-bit: sem truncamento de getLength (Smoke NOTE-2)
    uint32_t mapOpts = 0;
    uint64_t va = 0;
    uint32_t latch = 0;
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint16_t cmdAfter = 0;
    IOReturn kr = kIOReturnSuccess;
    bool haveProv = false;

    out.size = 0;
    out.version = 0;
    out.status = kGA106LabGateBStatus_InternalError;
    out.phase = kGA106LabGateBPhase_Unprogrammed;
    out.preconditionsReady = 0;
    out.ucMappingReady = 0;
    out.bmeEnabled = 0;
    out.pageReady = 0;
    out.addressRepresentable = 0;
    out.encodingRoundtripReady = 0;
    out.writeLatchBefore = (uint8_t)kGateBLatch_NeverTouched;
    out.writeLatchAfter = (uint8_t)kGateBLatch_NeverTouched;
    out.hiWriteAttempted = 0;
    out.hiWriteCompletedSoftwareSide = 0;
    out.loWriteAttempted = 0;
    out.loWriteCompletedSoftwareSide = 0;
    out.writeCount = 0;
    out.recoveryRequired = 0;
    out.reserved0 = 0;
    out.reserved1 = 0;
    out.reserved[0] = 0;
    out.reserved[1] = 0;
    out.reserved[2] = 0;
    out.reserved[3] = 0;
    // Header sempre preenchido (todas as saídas carregam size/version válidos,
    // como exige o validator antes mesmo de interpretar o status).
    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;

    // Reserva + stopping (seções curtas).
    ops.lock();
#ifdef GA106LAB_GATEB_MUT_SKIP_ENTRY_STOP
    // MUTAÇÃO TEST-ONLY: ignora stopping na entrada.
#else
    if (ops.isStopping()) {
        ops.unlock();
        out.status = kGA106LabGateBStatus_Stopping;
        return kIOReturnAborted;
    }
#endif
#ifdef GA106LAB_GATEB_MUT_SKIP_BUSY
    // MUTAÇÃO TEST-ONLY: ignora busy (dupla reserva prossegue).
    if (false) {
#else
    if (ops.isBusy()) {
#endif
        ops.unlock();
        out.status = kGA106LabGateBStatus_InternalError;
        return kIOReturnBusy;
    }
    ops.setBusy(true);
    latch = ops.getLatch();
    ops.unlock();
    out.writeLatchBefore = (uint8_t)latch;
    out.writeLatchAfter = (uint8_t)latch;

    // Latch one-way: qualquer estado != NeverTouched recusa sem tocar HW.
#ifdef GA106LAB_GATEB_MUT_SECOND_INVOCATION
    // MUTAÇÃO TEST-ONLY: ignora o latch (segunda invocação prossegue).
#else
    if (latch != kGateBLatch_NeverTouched) {
        out.status = kGA106LabGateBStatus_AlreadyProgrammed;
        out.phase = (latch == kGateBLatch_HIAndLOWritten)
            ? kGA106LabGateBPhase_Programmed
            : kGA106LabGateBPhase_Unprogrammed;
        if (latch == kGateBLatch_UnknownPartial) {
            out.status = kGA106LabGateBStatus_RecoveryRequired;
        }
        out.recoveryRequired = (latch == kGateBLatch_UnknownPartial) ? 1 : 0;
        ops.lock();
        ops.setBusy(false);
        ops.unlock();
        return kIOReturnSuccess;
    }
#endif

    // Aquisição com pin (retain) — após o latch (recusa AlreadyProgrammed não
    // toca HW nem provider). A partir daqui, TODAS as saídas liberam o pin.
    if (!ops.acquireProvider()) {
        out.status = kGA106LabGateBStatus_ProviderInvalid;
        goto fail_no_map;
    }
    haveProv = true;

    // Fase 0: checks read-only pré-map.
#ifdef GA106LAB_GATEB_MUT_SKIP_PROVIDER
    // MUTAÇÃO TEST-ONLY: finge provider válido (quebra gate).
    prov = true; exact = true; mse = true; bme = false;
#else
    if (ops.checkTarget(prov, exact, mse, bme) != kIOReturnSuccess) {
        out.status = kGA106LabGateBStatus_ProviderInvalid;
        goto fail_no_map;
    }
    if (!prov || !exact) {
        out.status = kGA106LabGateBStatus_ProviderInvalid;
        goto fail_no_map;
    }
#endif
#ifdef GA106LAB_GATEB_MUT_SKIP_MSE_PRE
    // MUTAÇÃO TEST-ONLY: ignora MSE pré-map.
#else
    if (!mse) {
        out.status = kGA106LabGateBStatus_MseDisabled;
        goto fail_no_map;
    }
#endif
#ifdef GA106LAB_GATEB_MUT_SKIP_BAR
    // MUTAÇÃO TEST-ONLY: finge BAR pronto.
    barReady = true;
    kr = kIOReturnSuccess;
#else
    kr = ops.checkBar0(barReady);
#endif
    if (kr != kIOReturnSuccess || !barReady) {
        out.status = kGA106LabGateBStatus_BarUnavailable;
        goto fail_no_map;
    }
#ifdef GA106LAB_GATEB_MUT_SKIP_GFW
    // MUTAÇÃO TEST-ONLY: finge GFW pronto.
    gfwReady = true;
    kr = kIOReturnSuccess;
#else
    kr = ops.checkGfw(gfwReady);
#endif
    if (kr != kIOReturnSuccess || !gfwReady) {
        out.status = kGA106LabGateBStatus_GfwNotReady;
        goto fail_no_map;
    }
#ifdef GA106LAB_GATEB_MUT_ALLOW_BME
    // MUTAÇÃO TEST-ONLY: ignora BME ligado.
#else
    if (bme) {
        out.status = kGA106LabGateBStatus_BmeEnabled;
        out.bmeEnabled = 1;
        goto fail_no_map;
    }
#endif
    out.bmeEnabled = 0;

    // Página dedicada (owner-held; nunca do userspace).
    if (!ops.getStoredPage(addr, segLen, segCount)) {
        out.status = kGA106LabGateBStatus_PageInvalid;
        goto fail_no_map;
    }
#ifdef GA106LAB_GATEB_MUT_SEGCOUNT
    // MUTAÇÃO TEST-ONLY: aceita segmentCount != 1 (quebra contrato).
    (void)segCount;
#else
    if (segCount != 1u) {
        out.status = kGA106LabGateBStatus_SegmentInvalid;
        goto fail_no_map;
    }
#endif
#ifdef GA106LAB_GATEB_MUT_SEGLEN
    // MUTAÇÃO TEST-ONLY: aceita qualquer comprimento.
#else
    if (segLen != kGA106LabFlushPrewrite_Size) {
        out.status = kGA106LabGateBStatus_LengthInvalid;
        goto fail_no_map;
    }
#endif
    out.pageReady = 1;
#ifdef GA106LAB_GATEB_MUT_NO_ALIGN
    // MUTAÇÃO TEST-ONLY: pula checagem de alinhamento 256B.
#else
    if ((addr & 0xFFu) != 0u) {
        out.status = kGA106LabGateBStatus_AlignmentInvalid;
        goto fail_no_map;
    }
#endif
#ifdef GA106LAB_GATEB_MUT_NO_VALID47
    // MUTAÇÃO TEST-ONLY: pula bound 47-bit.
#else
    if (!GA106LabSysmemAddressValid47(addr)) {
        out.status = kGA106LabGateBStatus_AddressNotRepresentable;
        goto fail_no_map;
    }
#endif
    out.addressRepresentable = 1;
#ifdef GA106LAB_GATEB_MUT_NO_ALIGN
    // MUTAÇÃO TEST-ONLY: (reservada; alinhamento já checado acima).
#endif
    {
        uint32_t eLo = (uint32_t)((addr >> 8u) & 0xFFFFFFFFu);
        uint32_t eHi = (uint32_t)((addr >> 40u) & 0xFFFFFFu);
        // Round-trip é identidade aritmética para todo addr (bits 0..7 + 8..39
        // + 40..63 particionam os 64 bits); mantido como defesa em profundidade.
        // Sem hook de mutação: seria inkillable por construção (documentado).
        uint64_t rt = (((uint64_t)eHi) << 40u) | (((uint64_t)eLo) << 8u) |
                      (addr & 0xFFu);
        if (rt != addr) {
            out.status = kGA106LabGateBStatus_EncodingMismatch;
            goto fail_no_map;
        }
        lo = eLo;
        hi = eHi;
    }
    out.encodingRoundtripReady = 1;
    out.preconditionsReady = 1;

    // Mapa UC writable (único mapa por chamada; liberado antes de retornar).
    if (!ops.createUcWriteMap(mapLen, mapOpts)) {
        out.status = kGA106LabGateBStatus_UcMappingUnavailable;
        goto fail_no_map;
    }
    {
        bool haveMap = true;
        // Validação EXATA de modo: writable + Inhibit; WC/RO/outros => unproven.
#ifdef GA106LAB_GATEB_MUT_WC_MAP
        // MUTAÇÃO TEST-ONLY: aceita qualquer modo (inclui WC).
        (void)mapOpts;
#else
        if (!GA106LabMapOptionsValidWritableInhibit(mapOpts)) {
            out.status = kGA106LabGateBStatus_UcSemanticsUnproven;
            ops.releaseUcMap();
            haveMap = false;
            goto fail_no_map_inner;
        }
#endif
        if (mapLen < (uint64_t)(GA106LAB_GATEB_HI_OFFSET + GA106LAB_GATEB_WIDTH) ||
            mapLen < (uint64_t)(GA106LAB_GATEB_LO_OFFSET + GA106LAB_GATEB_WIDTH)) {
            out.status = kGA106LabGateBStatus_UcMappingUnavailable;
            if (haveMap) {
                ops.releaseUcMap();
            }
            goto fail_no_map_inner;
        }
        va = ops.getUcVA();
        if (va == 0) {
            out.status = kGA106LabGateBStatus_UcMappingUnavailable;
            ops.releaseUcMap();
            goto fail_no_map_inner;
        }
        out.ucMappingReady = 1;

        // BME re-read imediatamente pré-store (mesma classe do Gate A T3).
#ifdef GA106LAB_GATEB_MUT_SKIP_REREAD
        // MUTAÇÃO TEST-ONLY: pula re-read (flip invisível).
#else
        {
            uint16_t cmdNow = 0;
            if (!ops.readCommandAfter(cmdNow)) {
                // Falha de transporte PRÉ-store: nada escrito; libera o mapa
                // aqui e usa o caminho sem-map (InternalError, sem reboot).
                out.status = kGA106LabGateBStatus_InternalError;
                ops.releaseUcMap();
                goto fail_no_map;
            }
            {
                bool mseNow = ((cmdNow & 0x0002u) != 0);
                bool bmeNow = ((cmdNow & 0x0004u) != 0);
                out.bmeEnabled = bmeNow ? 1 : 0;
                if (!mseNow) {
                    out.status = kGA106LabGateBStatus_MseDisabled;
                    ops.releaseUcMap();
                    goto fail_no_map_inner;
                }
                if (bmeNow) {
                    out.status = kGA106LabGateBStatus_BmeEnabled;
                    ops.releaseUcMap();
                    goto fail_no_map_inner;
                }
            }
        }
#endif

        // Stopping final antes do primeiro store.
#ifdef GA106LAB_GATEB_MUT_SKIP_PRE_HI_STOP
        // MUTAÇÃO TEST-ONLY: ignora stop pré-HI (escreve mesmo parado).
#endif
        ops.lock();
        {
            bool stoppingNow = ops.isStopping();
            ops.unlock();
#ifdef GA106LAB_GATEB_MUT_SKIP_PRE_HI_STOP
            stoppingNow = false;
#endif
            if (stoppingNow) {
                out.status = kGA106LabGateBStatus_Stopping;
                ops.releaseUcMap();
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

        // Store HI (único callsite) — ordem de source; volatile em RealOps;
        // UC em x86 ordena CPU; sem WC por validação acima.
        out.hiWriteAttempted = 1;
#ifdef GA106LAB_GATEB_MUT_SKIP_HI
        // MUTAÇÃO TEST-ONLY: pula HI (vai direto ao LO).
#else
#ifdef GA106LAB_GATEB_MUT_REVERSE_ORDER
        ops.writeFlushLo(lo);
        out.loWriteAttempted = 1;
        out.loWriteCompletedSoftwareSide = 1;
        out.writeCount = 1;
        ops.writeFlushHi(hi);
        out.hiWriteAttempted = 1;
        out.hiWriteCompletedSoftwareSide = 1;
        out.writeCount = 2;
#else
        ops.writeFlushHi(hi);
        out.hiWriteAttempted = 1;
        out.hiWriteCompletedSoftwareSide = 1;
        out.writeCount = 1;
#endif
#endif
#ifdef GA106LAB_GATEB_MUT_GENERIC_WRITER
        GateBWriteRegister32(ops, GA106LAB_GATEB_HI_OFFSET, hi);
#endif
        ops.lock();
        ops.setLatch(kGateBLatch_HIWritten);
        ops.unlock();
        out.writeLatchAfter = (uint8_t)kGateBLatch_HIWritten;

        // Cancellation point: stop entre HI e LO => UnknownPartial + reboot.
        ops.lock();
        {
            bool stoppingMid = ops.isStopping();
            ops.unlock();
#ifdef GA106LAB_GATEB_MUT_SKIP_MID_STOP
            // MUTAÇÃO TEST-ONLY: ignora stop entre HI e LO.
            stoppingMid = false;
#endif
            if (stoppingMid) {
                out.status = kGA106LabGateBStatus_UnknownPartial;
                out.recoveryRequired = 1;
                ops.releaseUcMap();
#ifdef GA106LAB_GATEB_MUT_CLEAR_LATCH_TEARDOWN
                // MUTAÇÃO TEST-ONLY: teardown parcial limpa o latch (quebra one-way).
                ops.lock();
                ops.setLatch(kGateBLatch_NeverTouched);
                ops.setBusy(false);
                ops.unlock();
#else
#ifdef GA106LAB_GATEB_MUT_CLEAR_LATCH_CLOSE
                // MUTAÇÃO TEST-ONLY: close limpa latch (quebra one-way pós-store).
                ops.clearLatchOnClose();
                ops.lock();
                ops.setBusy(false);
                ops.unlock();
#else
                ops.lock();
                ops.setLatch(kGateBLatch_UnknownPartial);
                ops.setBusy(false);
                ops.unlock();
#endif
#endif
                if (haveProv) {
                    ops.releaseProvider();
                    haveProv = false;
                }
                out.writeLatchAfter = (uint8_t)kGateBLatch_UnknownPartial;
                return kIOReturnSuccess;
            }
        }

        // Store LO (único callsite).
        out.loWriteAttempted = 1;
#ifdef GA106LAB_GATEB_MUT_SKIP_LO
        // MUTAÇÃO TEST-ONLY: pula LO (HI-only parcial).
#else
#ifdef GA106LAB_GATEB_MUT_DUP_HI
        ops.writeFlushHi(hi);
        out.writeCount = 2;
#else
#ifdef GA106LAB_GATEB_MUT_DUP_LO
        ops.writeFlushLo(lo);
        out.loWriteCompletedSoftwareSide = 1;
        out.writeCount = 2;
        ops.writeFlushLo(lo);
        out.writeCount = 3;
#else
        ops.writeFlushLo(lo);
        out.loWriteCompletedSoftwareSide = 1;
        out.writeCount = 2;
#endif
#endif
#endif
#ifdef GA106LAB_GATEB_MUT_ALLOW_THIRD
        ops.writeFlushLo(lo);
        out.writeCount = 3;
#endif
#ifdef GA106LAB_GATEB_MUT_RETRY_AFTER_HI
        // MUTAÇÃO TEST-ONLY: re-tenta (reprograma HI+LO de novo).
        ops.writeFlushHi(hi);
        ops.writeFlushLo(lo);
        out.writeCount = 4;
#endif
#ifdef GA106LAB_GATEB_MUT_SKIP_LATCH_SET
        // MUTAÇÃO TEST-ONLY: pula latch final (programado sem registrar).
#else
        ops.lock();
        ops.setLatch(kGateBLatch_HIAndLOWritten);
        ops.unlock();
#endif
        out.writeLatchAfter = (uint8_t)kGateBLatch_HIAndLOWritten;
#ifdef GA106LAB_GATEB_MUT_ROLLBACK_WRITE
        // MUTAÇÃO TEST-ONLY: rollback write após sucesso (proibido).
        ops.writeFlushHi(0);
        ops.writeFlushLo(0);
#endif

#ifdef GA106LAB_GATEB_MUT_SKIP_MAP_RELEASE
        // MUTAÇÃO TEST-ONLY: vaza o mapa no sucesso (quebra pairing).
#else
        ops.releaseUcMap();
#endif
        (void)haveMap;
    }
fail_no_map_inner:
    if (out.status == kGA106LabGateBStatus_InternalError &&
        out.writeCount == 2 &&
        out.loWriteCompletedSoftwareSide != 0) {
        // Caminho nominal chegou aqui só via fall-through: sucesso.
        out.status = kGA106LabGateBStatus_Success;
        out.phase = kGA106LabGateBPhase_Programmed;
        out.recoveryRequired = 0;
    } else if (out.status == kGA106LabGateBStatus_InternalError) {
        // Falha pós-map sem status específico: fail closed defensivo.
        // (Inacessível nos caminhos atuais — todos os erros pós-map carregam
        // status próprio; mantido como rede de segurança, sem hook de mutação
        // porque nenhum teste o alcança por construção.)
        out.status = kGA106LabGateBStatus_UnknownPartial;
        out.recoveryRequired = 1;
        ops.lock();
        ops.setLatch(kGateBLatch_UnknownPartial);
        ops.unlock();
        out.writeLatchAfter = (uint8_t)kGateBLatch_UnknownPartial;
    }
#ifdef GA106LAB_GATEB_MUT_SKIP_PROV_RELEASE
    // MUTAÇÃO TEST-ONLY: vaza o retain do provider.
#else
    if (haveProv) {
        ops.releaseProvider();
        haveProv = false;
    }
#endif
#ifdef GA106LAB_GATEB_MUT_SKIP_BUSY_CLEAR
    // MUTAÇÃO TEST-ONLY: deixa busy setado (trava futuras chamadas).
#else
    ops.lock();
    ops.setBusy(false);
    ops.unlock();
#endif
    return kIOReturnSuccess;

fail_no_map:
    // NOTA: sem hook CLEAR_LATCH aqui por construção — o latch check precede
    // todos os caminhos fail_no_map, logo o latch é sempre NeverTouched aqui;
    // limpar seria no-op (mutação que nunca falha é desonesta e foi removida).
#ifdef GA106LAB_GATEB_MUT_ZEROING_STORE
    // MUTAÇÃO TEST-ONLY: zeroing cleanup em falha (zero stores proibidos).
    ops.writeFlushHi(0);
    ops.writeFlushLo(0);
#endif
    if (haveProv) {
        ops.releaseProvider();
        haveProv = false;
    }
    ops.lock();
    ops.setBusy(false);
    ops.unlock();
    return kIOReturnSuccess;
}

#endif /* GA106LAB_GATEB_WRITE_FLOW_HPP */
