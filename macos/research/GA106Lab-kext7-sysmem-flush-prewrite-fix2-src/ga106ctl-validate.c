// ga106ctl-validate.c — mesma unidade usada pelo CLI e pelos testes.
#include "ga106ctl-validate.h"

#include "GA106LabCoreValidate.h"

int ga106ctl_check_identity(const void * buf, size_t len)
{
    const struct GA106LabIdentityV1 * id;
    if (!buf) {
        return 1;
    }
    if (len != sizeof(struct GA106LabIdentityV1)) {
        return 2;
    }
    id = (const struct GA106LabIdentityV1 *) buf;
    if (id->size != sizeof(struct GA106LabIdentityV1)) {
        return 3;
    }
    if (id->version != GA106LAB_UC_PROTOCOL_VERSION) {
        return 4;
    }
    return 0;
}

int ga106ctl_check_status(const void * buf, size_t len)
{
    const struct GA106LabStatusV1 * st;
    if (!buf) {
        return 1;
    }
    if (len != sizeof(struct GA106LabStatusV1)) {
        return 2;
    }
    st = (const struct GA106LabStatusV1 *) buf;
    if (st->size != sizeof(struct GA106LabStatusV1)) {
        return 3;
    }
    if (st->version != GA106LAB_UC_PROTOCOL_VERSION) {
        return 4;
    }
    return 0;
}

int ga106ctl_check_pci_snapshot(const void * buf, size_t len)
{
    const struct GA106LabPciSnapshotV1 * snap;
    if (!buf) {
        return 1;
    }
    if (len != sizeof(struct GA106LabPciSnapshotV1)) {
        return 2;
    }
    snap = (const struct GA106LabPciSnapshotV1 *) buf;
    if (snap->size != sizeof(struct GA106LabPciSnapshotV1)) {
        return 3;
    }
    if (snap->version != GA106LAB_UC_PROTOCOL_VERSION) {
        return 4;
    }
    if (snap->status != kGA106LabPciStatus_Ok) {
        return 5;
    }
    return 0;
}

int ga106ctl_check_bar_map_probe(const void * buf, size_t len)
{
    const struct GA106LabBarMapInfoV1 * info;
    if (!buf) {
        return 1;
    }
    if (len != sizeof(struct GA106LabBarMapInfoV1)) {
        return 2;
    }
    info = (const struct GA106LabBarMapInfoV1 *) buf;
    if (info->size != sizeof(struct GA106LabBarMapInfoV1)) {
        return 3;
    }
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) {
        return 4;
    }
    if (info->status != kGA106LabBarMapStatus_MapReleased) {
        return 5;
    }
    if ((info->validMask &
         kGA106LabBarMapValid_ReleaseConfirmed) == 0) {
        return 6;
    }
    if (info->semanticBar != kGA106LabBarMapSemantic_Bar0) {
        return 7;
    }
    if (info->mappedLength == 0 ||
        info->mappedLength > info->resourceLength) {
        return 8;
    }
    return 0;
}

int ga106ctl_check_mmio_read(const void * buf, size_t len)
{
    const struct GA106LabMmioReadV1 * info;
    if (!buf) {
        return 1;
    }
    if (len != sizeof(struct GA106LabMmioReadV1)) {
        return 2;
    }
    info = (const struct GA106LabMmioReadV1 *) buf;
    if (info->size != sizeof(struct GA106LabMmioReadV1)) {
        return 3;
    }
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) {
        return 4;
    }
    if (info->status != kGA106LabMmioStatus_MapReleased) {
        return 5;
    }
    if ((info->validFlags &
         kGA106LabMmioValid_ReleaseConfirmed) == 0) {
        return 6;
    }
    if (info->registerId != kGA106LabMmioReg_PmcBoot0) {
        return 7;
    }
    if (info->offset != kGA106LabMmio_PmcBoot0_Offset ||
        info->width != kGA106LabMmio_PmcBoot0_Width) {
        return 8;
    }
    /* TG-KEXT4-FIX: validação raw/chip com os MESMOS helpers da produção
       e do seam (defesa em profundidade no userspace; o kernel já só
       retorna Success com chip 0x176 e raw não-absent). */
    if ((info->validFlags & kGA106LabMmioValid_RawValue) == 0) {
        return 9;
    }
    if ((info->validFlags & kGA106LabMmioValid_ChipId) == 0) {
        return 9;
    }
    if (GA106LabRawIsAbsent(info->rawValue)) {
        return 10;
    }
    if (!GA106LabChipIsGA106(info->rawValue)) {
        return 11;
    }
    return 0;
}

/* Fluxo real do CLI: IOReturn + ABI. Testes exercitam esta função com
   kr mockado + ABI mockada, provando propagation de ponta a ponta. */
int ga106ctl_mmio_cli_exit(int krSuccess, const void * buf, size_t len)
{
    if (!krSuccess) {
        return 1;
    }
    return ga106ctl_check_mmio_read(buf, len);
}

int ga106ctl_check_static_identity(const void * buf, size_t len)
{
    const struct GA106LabStaticIdentityV1 * info;
    if (!buf) {
        return 1;
    }
    if (len != sizeof(struct GA106LabStaticIdentityV1)) {
        return 2;
    }
    info = (const struct GA106LabStaticIdentityV1 *) buf;
    if (info->size != sizeof(struct GA106LabStaticIdentityV1)) {
        return 3;
    }
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) {
        return 4;
    }
    if (info->status != kGA106LabStaticIdentityStatus_MapReleased) {
        return 5;
    }
    if ((info->validMask &
         kGA106LabStaticIdentityValid_ReleaseConfirmed) == 0) {
        return 6;
    }
    if ((info->validMask & kGA106LabStaticIdentityValid_Boot42) == 0) {
        return 7;
    }
    if (info->readCount != 1) {
        return 8;
    }
    if ((info->validMask &
         kGA106LabStaticIdentityValid_DecodedChipId) == 0) {
        return 9;
    }
    if (GA106LabRawIsAbsent(info->boot42Raw)) {
        return 10;
    }
    // BOOT_42 usa máscara 10-bit dedicada (helper distinto de BOOT_0).
    if (!GA106LabBoot42IsGA106(info->boot42Raw)) {
        return 11;
    }
    if (info->decodedChipId != kGA106LabMmio_ChipId_GA106) {
        return 13;
    }
    return 0;
}

int ga106ctl_static_identity_cli_exit(int krSuccess, const void * buf,
                                      size_t len)
{
    if (!krSuccess) {
        return 1;
    }
    return ga106ctl_check_static_identity(buf, len);
}

int ga106ctl_check_gsp_sysmem(const void * buf, size_t len)
{
    const struct GA106LabGspSysmemV1 * info;
    if (!buf) {
        return 1;
    }
    if (len != sizeof(struct GA106LabGspSysmemV1)) {
        return 2;
    }
    info = (const struct GA106LabGspSysmemV1 *) buf;
    if (info->size != sizeof(struct GA106LabGspSysmemV1)) {
        return 3;
    }
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) {
        return 4;
    }
    if (info->status != kGA106LabGspSysmemStatus_AddressReady &&
        info->status != kGA106LabGspSysmemStatus_AlreadyPrepared) {
        return 5;
    }
    if ((info->flags & kGA106LabGspSysmemFlag_AddressReady) == 0) {
        return 6;
    }
    if (info->bufferSize != kGA106LabGspSysmem_Size) {
        return 7;
    }
    if (info->segmentCount != 1 || info->segmentLength != kGA106LabGspSysmem_Size) {
        return 8;
    }
    if (info->state != kGA106LabGspSysmemState_AddressReady) {
        return 9;
    }
    return 0;
}

int ga106ctl_gsp_sysmem_cli_exit(int krSuccess, const void * buf, size_t len)
{
    if (!krSuccess) {
        return 1;
    }
    return ga106ctl_check_gsp_sysmem(buf, len);
}

/* GFW readiness (selector 8): size+version 32B/v1 + status Completed/TimedOut
   coerente (gate/progress/pollCount/timedOut) + reserved zero + sem endereços
   (a ABI sequer possui campos de endereço). */
int ga106ctl_check_gfw_readiness(const void * buf, size_t len)
{
    const struct GA106LabGfwReadinessV1 * info;
    if (!buf) {
        return 1;
    }
    if (len != sizeof(struct GA106LabGfwReadinessV1)) {
        return 2;
    }
    info = (const struct GA106LabGfwReadinessV1 *) buf;
    if (info->size != sizeof(struct GA106LabGfwReadinessV1)) {
        return 3;
    }
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) {
        return 4;
    }
    if (info->status != kGA106LabGfwStatus_Completed &&
        info->status != kGA106LabGfwStatus_TimedOut) {
        return 5;
    }
    if (info->gateReady != 0u && info->gateReady != 1u) {
        return 6;
    }
    if (info->progress > 0xFFu) {
        return 7;
    }
    if (info->pollCount == 0u ||
        info->pollCount > GA106LAB_GFW_POLL_MAX_ITERS) {
        return 8;
    }
    if (info->timedOut != 0u && info->timedOut != 1u) {
        return 9;
    }
    if ((info->status == kGA106LabGfwStatus_Completed) !=
        (info->timedOut == 0u)) {
        return 10;
    }
    if (info->status == kGA106LabGfwStatus_Completed &&
        info->progress != GA106LAB_GFW_COMPLETED_VALUE) {
        return 11;
    }
    if (info->reserved != 0u) {
        return 12;
    }
    return 0;
}

int ga106ctl_gfw_readiness_cli_exit(int krSuccess, const void * buf, size_t len)
{
    if (!krSuccess) {
        return 1;
    }
    return ga106ctl_check_gfw_readiness(buf, len);
}

int ga106ctl_check_flush_prewrite(const void * buf, size_t len)
{
    const struct GA106LabFlushPrewriteV1 * info;
    size_t k;
    const uint8_t *r;
    if (!buf) return 1;
    if (len != sizeof(struct GA106LabFlushPrewriteV1)) return 2;
    info = (const struct GA106LabFlushPrewriteV1 *) buf;
    if (info->size != sizeof(struct GA106LabFlushPrewriteV1)) return 3;
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) return 4;
    if (info->status != kGA106LabFlushPrewriteStatus_PreconditionsReady &&
        info->status != kGA106LabFlushPrewriteStatus_AlreadyReady) return 5;
    if (info->phase != kGA106LabFlushPrewritePhase_PreconditionsReady) return 6;
    if (!info->providerReady || !info->ga106Exact || !info->bar0Ready) return 7;
    if (!info->mseEnabled || !info->gfwReady) return 8;
    if (info->bmeEnabled) return 9;
    if (!info->pageReady) return 10;
    if (info->segmentCount != 1 || info->segmentLength != kGA106LabFlushPrewrite_Size) return 11;
    if (!info->alignmentReady || !info->addressRepresentable || !info->encodingRoundtripReady) return 12;
    if (info->flushRegistersState != kGA106LabFlushRegs_NotChecked) return 13;
    if (info->programmed || info->readbackMatch || info->writeCount || info->readCount) return 14;
    r = info->reserved;
    for (k = 0; k < sizeof(info->reserved); k++) if (r[k] != 0) return 15;
    return 0;
}

int ga106ctl_flush_prewrite_cli_exit(int krSuccess, const void * buf, size_t len)
{
    if (!krSuccess) {
        return 1;
    }
    return ga106ctl_check_flush_prewrite(buf, len);
}
