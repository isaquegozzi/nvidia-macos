// GA106LabMmioFlow.hpp — ONE_SHARED_MMIO_FLOW (header-only template).
//
// Único fluxo decisório do selector 5 READ_FIRST_MMIO_REGISTER.
// Produção (RealOps) e testes (MockOps) instanciam O MESMO template
// RunFirstMmioReadFlow<Ops>, com static dispatch (sem vtable, sem
// tabela de callbacks runtime no ABI/UserClient).
//
// Ordem (idêntica à produção anterior, agora compartilhada):
//   provider -> vendor/device -> MSE -> BAR0 raw -> base ->
//   discovery (count, match len+base, 0=>FAIL, >=2=>FAIL, 1=>continue) ->
//   resLen/base revalidação -> ResourceFound ->
//   createMap RO+Inhibit -> MapCreated ->
//   mapLen full-resource -> mapLen<width ->
//   cache EXATA (RO + campo==Inhibit) ->
//   VA -> alignment -> bounds ->
//   exactly 1x read32 -> absent -> chip 0x176 ->
//   fill ABI fixa -> releaseMap -> command-after compare -> return.
//
// Ops deve prover (static dispatch, sem virtual):
//   bool     isProviderPCI();
//   uint16_t readVendorId(); uint16_t readDeviceId();
//   uint16_t readCommandBefore(); uint16_t readCommandAfter();
//   uint32_t readBar0Raw();
//   uint32_t getResourceCount();
//   bool     getResource(uint32_t idx, uint64_t &base, uint64_t &len);
//            // false = entrada NULL / ausente (skip)
//   bool     createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut);
//            // false = map null
//   uint64_t getVA(); // 0 = falha
//   uint32_t mmioOffset(); uint32_t mmioWidth(); // Real: fixos; Mock: fixture
//   uint32_t read32(uint32_t offset); // ÚNICA primitiva de leitura
//   void     releaseMap();
//
// RealOps só executa operações; MockOps só simula + conta dentro das ops.
// Nenhuma decisão (discovery/ambiguity/policy) vive nos Ops.

#ifndef GA106LAB_MMIO_FLOW_HPP
#define GA106LAB_MMIO_FLOW_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

template <typename Ops>
inline IOReturn RunFirstMmioReadFlow(Ops &ops, GA106LabMmioReadV1 &out)
{
    uint16_t vendor = 0;
    uint16_t device = 0;
    uint16_t commandBefore = 0;
    uint16_t commandAfter = 0;
    uint32_t bar0Raw = 0;
    uint64_t bar0Base = 0;
    uint32_t count = 0;
    uint32_t i = 0;
    int matchIndex = -1;
    int matchCount = 0;
    uint64_t resLen = 0;
    uint64_t resPhys = 0;
    uint64_t mapLen = 0;
    uint32_t mapOpts = 0;
    uint64_t va = 0;
    uint32_t off = 0;
    uint32_t w = 0;
    uint32_t raw = 0;
    uint32_t chip = 0;
    unsigned k = 0;

    // Zero total primeiro (sem memset, sem bytes não inicializados).
    out.size = 0;
    out.version = 0;
    out.status = kGA106LabMmioStatus_Failed;
    out.registerId = 0;
    out.offset = 0;
    out.width = 0;
    out.rawValue = 0;
    out.validFlags = 0;
    for (k = 0; k < 4; k++) {
        out.reserved[k] = 0;
    }

    // 1. provider
    if (!ops.isProviderPCI()) {
        return kIOReturnNotReady;
    }

    // 2. identidade (mesmo gate 1.2.1)
    vendor = ops.readVendorId();
    if (vendor == 0xFFFFu) {
        return kIOReturnNotReady;
    }
    device = ops.readDeviceId();
    if (vendor != GA106LAB_VENDOR_NVIDIA ||
        device != GA106LAB_DEVICE_GA106) {
        return kIOReturnNoDevice;
    }

    // 3. MSE (nunca habilitar)
    commandBefore = ops.readCommandBefore();
    if (!GA106LabCommandMseOn(commandBefore)) {
        return kIOReturnNotReady;
    }

    // 4. BAR0 raw
    bar0Raw = ops.readBar0Raw();
    if (!GA106LabBar0RawValid(bar0Raw)) {
        return kIOReturnNoDevice;
    }
    bar0Base = GA106LabBar0BaseFromRaw(bar0Raw);

    // 5. discovery: SOMENTE dentro do count real; match estrito.
    count = ops.getResourceCount();
    matchIndex = -1;
    matchCount = 0;
    resLen = 0;
    resPhys = 0;
    for (i = 0; i < count; i++) {
        uint64_t base = 0;
        uint64_t len = 0;
        if (!ops.getResource(i, base, len)) {
            continue;
        }
        if (GA106LabBarResourceMatches(len, base, bar0Base)) {
            matchIndex = (int)i;
            matchCount++;
            resLen = len;
            resPhys = base;
        }
    }
    if (matchCount != 1 || matchIndex < 0) {
        return kIOReturnNotFound; // 0 ou >=2: FAIL, sem fallback.
    }
    if (resLen != GA106LAB_BAR0_EXPECTED_LENGTH) {
        return kIOReturnNoDevice;
    }
    if (resPhys != bar0Base) {
        return kIOReturnNoDevice;
    }
    out.status = kGA106LabMmioStatus_ResourceFound;

    // 6. map RO+Inhibit (política fixa dentro dos Ops; flow só decide).
    if (!ops.createMap((uint32_t)matchIndex, mapLen, mapOpts)) {
        out.status = kGA106LabMmioStatus_Failed;
        return kIOReturnNoResources;
    }
    out.status = kGA106LabMmioStatus_MapCreated;

    // 7. map metadata: full-resource v1.
    if (mapLen > resLen || mapLen != resLen) {
        ops.releaseMap();
        out.status = kGA106LabMmioStatus_Failed;
        return kIOReturnNoMemory;
    }
    w = ops.mmioWidth();
    if (mapLen < (uint64_t)w) {
        ops.releaseMap();
        out.status = kGA106LabMmioStatus_Failed;
        return kIOReturnNoMemory;
    }

    // 8. cache EXATA (aprovada): RO + campo==Inhibit. Rejeita
    // RO+Copyback (0x1300), RO+WC, RO+Default, sem-RO, sem-Inhibit.
    if (!GA106LabMapOptionsValidROInhibit(mapOpts)) {
        ops.releaseMap();
        out.status = kGA106LabMmioStatus_Failed;
        return kIOReturnNotPermitted;
    }

    // 9. VA interno (nunca exposto).
    va = ops.getVA();
    if (va == 0) {
        ops.releaseMap();
        out.status = kGA106LabMmioStatus_Failed;
        return kIOReturnNoResources;
    }

    // 10. alignment + bounds (helpers compartilhados, overflow-safe).
    off = ops.mmioOffset();
    w = ops.mmioWidth();
    if (!GA106LabAlignmentValid(off)) {
        ops.releaseMap();
        out.status = kGA106LabMmioStatus_Failed;
        return kIOReturnBadArgument;
    }
    if (!GA106LabBoundsValid(off, w, mapLen)) {
        ops.releaseMap();
        out.status = kGA106LabMmioStatus_Failed;
        return kIOReturnNoMemory;
    }

    // 11. A ÚNICA leitura (1x load 32-bit, offset fixo 0 na produção).
    raw = ops.read32(off);
    out.status = kGA106LabMmioStatus_ReadCompleted;

    // 12. absent-device (sem retry).
    if (GA106LabRawIsAbsent(raw)) {
        ops.releaseMap();
        out.status = kGA106LabMmioStatus_Failed;
        return kIOReturnNoDevice;
    }

    // 13. chip 0x176 (só passa; raw NÃO hardcoded).
    chip = GA106LabChipFromRaw(raw);
    if (chip != kGA106LabMmio_ChipId_GA106) {
        ops.releaseMap();
        out.status = kGA106LabMmioStatus_Failed;
        return kIOReturnNoDevice;
    }

    // 14. fill ABI fixa (sempre offset 0 / width 4, sem VA).
    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
    out.registerId = kGA106LabMmioReg_PmcBoot0;
    out.offset = kGA106LabMmio_PmcBoot0_Offset;
    out.width = kGA106LabMmio_PmcBoot0_Width;
    out.rawValue = raw;
    out.validFlags = kGA106LabMmioValid_RegisterId |
                     kGA106LabMmioValid_Offset |
                     kGA106LabMmioValid_Width |
                     kGA106LabMmioValid_RawValue |
                     kGA106LabMmioValid_ChipId;

    // 15. release (single path; nunca persiste).
    ops.releaseMap();
    out.status = kGA106LabMmioStatus_MapReleased;

    // 16. Command after (qualquer mudança = failure, sem restaurar).
    commandAfter = ops.readCommandAfter();
    if (commandAfter != commandBefore) {
        out.status = kGA106LabMmioStatus_Failed;
        out.validFlags &=
            (uint32_t)~kGA106LabMmioValid_ReleaseConfirmed;
        return kIOReturnError;
    }
    out.validFlags |= kGA106LabMmioValid_ReleaseConfirmed;
    return kIOReturnSuccess;
}

#endif /* GA106LAB_MMIO_FLOW_HPP */
