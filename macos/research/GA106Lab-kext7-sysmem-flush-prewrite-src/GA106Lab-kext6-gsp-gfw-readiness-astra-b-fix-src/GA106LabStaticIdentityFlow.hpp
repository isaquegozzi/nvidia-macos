// GA106LabStaticIdentityFlow.hpp — ONE_SHARED_STATIC_IDENTITY_FLOW.
//
// Único fluxo decisório do selector 6 READ_STATIC_IDENTITY.
// Produção (RealOps) e testes (MockOps) instanciam O MESMO template
// RunStaticIdentityReadsFlow<Ops>, static dispatch (sem virtual, sem
// tabela de callbacks runtime no ABI/UserClient).
//
// Fluxo (BOOT_2 REMOVIDO por evidência primária insuficiente; somente
// BOOT_42 — 1 load, fail-fast trivial):
//   provider -> vendor/device -> MSE -> BAR0 raw -> base ->
//   discovery (0=>FAIL, >=2=>FAIL, 1=>continue) -> resLen/base ->
//   ResourceFound -> createMap RO+Inhibit -> MapCreated ->
//   mapLen full -> cache EXATA -> VA ->
//   align/bounds BOOT42 -> read BOOT42 (1x) -> absent ->
//   chip 10-bit (raw&0x3FF00000)>>20 == 0x176 ->
//   fill ABI fixa -> releaseMap -> Command-after compare -> return.
//
// NÃO relê BOOT_0 (helpers de BOOT_0 intactos, política separada).
// Sem retry/polling.
//
// Ops deve prover:
//   bool isProviderPCI();
//   uint16_t readVendorId(); uint16_t readDeviceId();
//   uint16_t readCommandBefore(); uint16_t readCommandAfter();
//   uint32_t readBar0Raw();
//   uint32_t getResourceCount();
//   bool getResource(uint32_t idx, uint64_t &base, uint64_t &len);
//   bool createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut);
//   uint64_t getVA();
//   uint32_t boot42Offset(); uint32_t boot42Width();
//   uint32_t read32(uint32_t offset); // ÚNICA primitiva (1 callsite)
//   void releaseMap();

#ifndef GA106LAB_STATIC_IDENTITY_FLOW_HPP
#define GA106LAB_STATIC_IDENTITY_FLOW_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

template <typename Ops>
inline IOReturn RunStaticIdentityReadsFlow(Ops &ops,
                                           GA106LabStaticIdentityV1 &out)
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
    uint32_t off42 = 0;
    uint32_t w42 = 0;
    uint32_t raw42 = 0;
    uint32_t chip = 0;
    unsigned k = 0;

    out.size = 0;
    out.version = 0;
    out.status = kGA106LabStaticIdentityStatus_Failed;
    out.validMask = 0;
    out.readCount = 0;
    out.boot42Raw = 0;
    out.decodedChipId = 0;
    out.reserved = 0;

    if (!ops.isProviderPCI()) {
        return kIOReturnNotReady;
    }
    vendor = ops.readVendorId();
    if (vendor == 0xFFFFu) {
        return kIOReturnNotReady;
    }
    device = ops.readDeviceId();
    if (vendor != GA106LAB_VENDOR_NVIDIA ||
        device != GA106LAB_DEVICE_GA106) {
        return kIOReturnNoDevice;
    }
    commandBefore = ops.readCommandBefore();
    if (!GA106LabCommandMseOn(commandBefore)) {
        return kIOReturnNotReady;
    }
    bar0Raw = ops.readBar0Raw();
    if (!GA106LabBar0RawValid(bar0Raw)) {
        return kIOReturnNoDevice;
    }
    bar0Base = GA106LabBar0BaseFromRaw(bar0Raw);

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
        return kIOReturnNotFound;
    }
    if (resLen != GA106LAB_BAR0_EXPECTED_LENGTH) {
        return kIOReturnNoDevice;
    }
    if (resPhys != bar0Base) {
        return kIOReturnNoDevice;
    }
    out.status = kGA106LabStaticIdentityStatus_ResourceFound;

    if (!ops.createMap((uint32_t)matchIndex, mapLen, mapOpts)) {
        out.status = kGA106LabStaticIdentityStatus_Failed;
        return kIOReturnNoResources;
    }
    out.status = kGA106LabStaticIdentityStatus_MapCreated;

    if (mapLen > resLen || mapLen != resLen) {
        ops.releaseMap();
        out.status = kGA106LabStaticIdentityStatus_Failed;
        return kIOReturnNoMemory;
    }
    w42 = ops.boot42Width();
    if (mapLen < (uint64_t)w42) {
        ops.releaseMap();
        out.status = kGA106LabStaticIdentityStatus_Failed;
        return kIOReturnNoMemory;
    }
    if (!GA106LabMapOptionsValidROInhibit(mapOpts)) {
        ops.releaseMap();
        out.status = kGA106LabStaticIdentityStatus_Failed;
        return kIOReturnNotPermitted;
    }

    va = ops.getVA();
    if (va == 0) {
        ops.releaseMap();
        out.status = kGA106LabStaticIdentityStatus_Failed;
        return kIOReturnNoResources;
    }

    // READ ÚNICA = BOOT_42 @0xA00 (ordem fixa trivial).
    off42 = ops.boot42Offset();
    w42 = ops.boot42Width();
    if (!GA106LabAlignmentValid(off42)) {
        ops.releaseMap();
        out.status = kGA106LabStaticIdentityStatus_Failed;
        return kIOReturnBadArgument;
    }
    if (!GA106LabBoundsValid(off42, w42, mapLen)) {
        ops.releaseMap();
        out.status = kGA106LabStaticIdentityStatus_Failed;
        return kIOReturnNoMemory;
    }
    raw42 = ops.read32(off42);
    out.status = kGA106LabStaticIdentityStatus_ReadCompleted;
    if (GA106LabRawIsAbsent(raw42)) {
        ops.releaseMap();
        out.status = kGA106LabStaticIdentityStatus_Failed;
        return kIOReturnNoDevice;
    }
    // BOOT_42 CHIP_ID bits 29:20 (máscara 10-bit; helper distinto de BOOT_0).
    chip = GA106LabBoot42ChipFromRaw(raw42);
    if (!GA106LabBoot42IsGA106(raw42)) {
        ops.releaseMap();
        out.status = kGA106LabStaticIdentityStatus_Failed;
        return kIOReturnNoDevice;
    }

    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
    out.status = kGA106LabStaticIdentityStatus_MapReleased;
    out.validMask = kGA106LabStaticIdentityValid_Boot42 |
                    kGA106LabStaticIdentityValid_DecodedChipId |
                    kGA106LabStaticIdentityValid_ReadCount;
    out.readCount = 1;
    out.boot42Raw = raw42;
    out.decodedChipId = chip;

    ops.releaseMap();

    commandAfter = ops.readCommandAfter();
    if (commandAfter != commandBefore) {
        out.status = kGA106LabStaticIdentityStatus_Failed;
        out.validMask &=
            (uint32_t)~kGA106LabStaticIdentityValid_ReleaseConfirmed;
        return kIOReturnError;
    }
    out.validMask |= kGA106LabStaticIdentityValid_ReleaseConfirmed;
    return kIOReturnSuccess;
}

#endif /* GA106LAB_STATIC_IDENTITY_FLOW_HPP */
