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

/* Gate B program (selector 10): 48B/v1 + status Success/AlreadyProgrammed
   coerente + latch/writeCount/recovery + reserved zero; sem endereços. */
int ga106ctl_check_gateb_program(const void * buf, size_t len)
{
    const struct GA106LabGateBProgramV1 * info;
    if (!buf) return 1;
    if (len != sizeof(struct GA106LabGateBProgramV1)) return 2;
    info = (const struct GA106LabGateBProgramV1 *) buf;
    if (info->size != sizeof(struct GA106LabGateBProgramV1)) return 3;
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) return 4;
    if (info->status != kGA106LabGateBStatus_Success &&
        info->status != kGA106LabGateBStatus_AlreadyProgrammed) return 5;
    if (info->phase != kGA106LabGateBPhase_Unprogrammed &&
        info->phase != kGA106LabGateBPhase_Programmed) return 6;
    if ((info->status == kGA106LabGateBStatus_Success) !=
        (info->phase == kGA106LabGateBPhase_Programmed)) return 7;
    if (info->writeLatchBefore > kGateBLatch_UnknownPartial) return 8;
    if (info->writeLatchAfter > kGateBLatch_UnknownPartial) return 8;
    if (info->status == kGA106LabGateBStatus_Success &&
        info->writeLatchAfter != kGateBLatch_HIAndLOWritten) return 9;
    if (info->hiWriteAttempted != 0u && info->hiWriteAttempted != 1u) return 10;
    if (info->loWriteAttempted != 0u && info->loWriteAttempted != 1u) return 10;
    if (info->hiWriteCompletedSoftwareSide != 0u &&
        info->hiWriteCompletedSoftwareSide != 1u) return 10;
    if (info->loWriteCompletedSoftwareSide != 0u &&
        info->loWriteCompletedSoftwareSide != 1u) return 10;
    if (info->status == kGA106LabGateBStatus_Success &&
        (info->hiWriteAttempted != 1u || info->loWriteAttempted != 1u ||
         info->hiWriteCompletedSoftwareSide != 1u ||
         info->loWriteCompletedSoftwareSide != 1u)) return 11;
    if (info->writeCount != 0u && info->writeCount != 2u) return 12;
    if (info->status == kGA106LabGateBStatus_Success && info->writeCount != 2u)
        return 12;
    if (info->recoveryRequired != 0u) return 13;
    if (info->bmeEnabled != 0u) return 14;
    if (info->reserved0 != 0u || info->reserved1 != 0u) return 15;
    {
        size_t k;
        const uint8_t *r = (const uint8_t *)info->reserved;
        for (k = 0; k < sizeof(info->reserved); k++) if (r[k] != 0) return 16;
    }
    return 0;
}

int ga106ctl_gateb_program_cli_exit(int krSuccess, const void * buf, size_t len)
{
    if (!krSuccess) {
        return 1;
    }
    return ga106ctl_check_gateb_program(buf, len);
}

/* BME enable (selector 11): 48B/v1 + status Success/AlreadyEnabled coerente +
   boot binding + latch + writeCount 0|1 + readback + reserved zero; sem endereços. */
int ga106ctl_check_bme_enable(const void * buf, size_t len)
{
    const struct GA106LabBmeEnableV1 * info;
    if (!buf) return 1;
    if (len != sizeof(struct GA106LabBmeEnableV1)) return 2;
    info = (const struct GA106LabBmeEnableV1 *) buf;
    if (info->size != sizeof(struct GA106LabBmeEnableV1)) return 3;
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) return 4;
    if (info->status != kGA106LabBmeStatus_Success &&
        info->status != kGA106LabBmeStatus_AlreadyEnabled) return 5;
    if (info->phase != kGA106LabBmePhase_Idle &&
        info->phase != kGA106LabBmePhase_Enabled) return 6;
    if ((info->status == kGA106LabBmeStatus_Success) !=
        (info->phase == kGA106LabBmePhase_Enabled)) return 7;
    if (info->status == kGA106LabBmeStatus_AlreadyEnabled &&
        (info->phase != kGA106LabBmePhase_Idle || info->writeCount != 0u ||
         info->bmeWriteAttempted != 0u || info->bmeReadbackVerified != 0u ||
         info->bootFresh != 0u || info->sameBootGateA != 0u ||
         info->sameBootGateB != 0u || info->gateBProbeMatch != 0u)) return 7;
    if (info->bootFresh != 0u && info->bootFresh != 1u) return 8;
    if (info->status == kGA106LabBmeStatus_Success && info->bootFresh != 1u)
        return 8;
    if (info->sameBootGateA != 0u && info->sameBootGateA != 1u) return 9;
    if (info->sameBootGateB != 0u && info->sameBootGateB != 1u) return 9;
    if (info->gateBProbeMatch != 0u && info->gateBProbeMatch != 1u) return 9;
    if (info->status == kGA106LabBmeStatus_Success &&
        (info->sameBootGateA != 1u || info->sameBootGateB != 1u ||
         info->gateBProbeMatch != 1u)) return 9;
    if (info->mseEnabledBefore != 0u && info->mseEnabledBefore != 1u) return 10;
    if (info->bmeEnabledBefore != 0u && info->bmeEnabledBefore != 1u) return 10;
    if (info->status == kGA106LabBmeStatus_Success && info->bmeEnabledBefore != 0u)
        return 10;
    if (info->status == kGA106LabBmeStatus_Success && info->mseEnabledBefore != 1u)
        return 10;
    if (info->mseEnabledAfter != 0u && info->mseEnabledAfter != 1u) return 11;
    if (info->bmeEnabledAfter != 0u && info->bmeEnabledAfter != 1u) return 11;
    if (info->status == kGA106LabBmeStatus_Success &&
        (info->mseEnabledAfter != 1u || info->bmeEnabledAfter != 1u)) return 11;
    if (info->bmeWriteAttempted != 0u && info->bmeWriteAttempted != 1u) return 12;
    if (info->bmeReadbackVerified != 0u && info->bmeReadbackVerified != 1u) return 12;
    if (info->status == kGA106LabBmeStatus_Success &&
        (info->bmeWriteAttempted != 1u || info->bmeReadbackVerified != 1u)) return 12;
    if (info->writeCount != 0u && info->writeCount != 1u) return 13;
    if (info->status == kGA106LabBmeStatus_Success && info->writeCount != 1u)
        return 13;
    if (info->status == kGA106LabBmeStatus_AlreadyEnabled && info->writeCount != 0u)
        return 13;
    if (info->recoveryRequired != 0u) return 14;
    if (info->reserved0 != 0u || info->reserved1 != 0u ||
        info->reserved2 != 0u || info->reserved3 != 0u) return 15;
    {
        size_t k;
        const uint8_t *r = (const uint8_t *)info->reserved;
        for (k = 0; k < sizeof(info->reserved); k++) if (r[k] != 0) return 16;
    }
    return 0;
}

int ga106ctl_bme_enable_cli_exit(int krSuccess, const void * buf, size_t len)
{
    if (!krSuccess) {
        return 1;
    }
    return ga106ctl_check_bme_enable(buf, len);
}

/* R3 probes (selectors 12/13/14, revF): 64B/v1 + Pass estrito + predicados
   coerentes + reserved zero + launchAttempted==0. Qualquer non-Pass =>
   CLI exit != 0 (fail-closed; sem retry). */
static int r3_all_reserved_zero(const uint32_t *r, size_t n)
{
    size_t k = 0;
    for (k = 0; k < n; k++) {
        if (r[k] != 0u) {
            return 1;
        }
    }
    return 0;
}

int ga106ctl_check_r3_falcon(const void * buf, size_t len)
{
    const struct GA106LabR3FalconStateV1 * info;
    if (!buf) return 1;
    if (len != sizeof(struct GA106LabR3FalconStateV1)) return 2;
    info = (const struct GA106LabR3FalconStateV1 *) buf;
    if (info->size != sizeof(struct GA106LabR3FalconStateV1)) return 3;
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) return 4;
    if (info->status != kGA106LabR3Status_Pass) return 5;
    if (info->chosen != kGA106LabR3Chosen_Sec2 &&
        info->chosen != kGA106LabR3Chosen_Gsp) return 6;
    if (!info->resolved || !info->halted || !info->fullPre) return 7;
    if (!info->idle1 || !info->idle2 || !info->fullPost) return 8;
    if (!info->coreSelectFalcon || !info->riscvQuiet) return 9;
    if (!info->transcfgStable || !info->mailboxStable) return 10;
    if (!info->bootvecStable || !info->fbifStable) return 11;
    if (info->launchAttempted) return 12;
    if (info->reserved0 || info->reserved1) return 13;
    if (r3_all_reserved_zero(info->reserved, 8)) return 14;
    return 0;
}

int ga106ctl_r3_falcon_cli_exit(int krSuccess, const void * buf, size_t len)
{
    if (!krSuccess) return 1;
    return ga106ctl_check_r3_falcon(buf, len);
}

int ga106ctl_check_r3_mapping(const void * buf, size_t len)
{
    const struct GA106LabR3MappingContractV1 * info;
    if (!buf) return 1;
    if (len != sizeof(struct GA106LabR3MappingContractV1)) return 2;
    info = (const struct GA106LabR3MappingContractV1 *) buf;
    if (info->size != sizeof(struct GA106LabR3MappingContractV1)) return 3;
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) return 4;
    if (info->status != kGA106LabR3Status_Pass) return 5;
    if (!info->providerIdentityStable) return 6;
    if (!info->providerMapperPresent && !info->systemMapperUsed &&
        !info->mapperNullArmed) return 7;
    if (!info->gen64NotRunInR3) return 8;
    if (info->launchAttempted) return 9;
    if (info->reserved0 || info->reserved1) return 10;
    if (r3_all_reserved_zero(info->reserved, 9)) return 11;
    return 0;
}

int ga106ctl_r3_mapping_cli_exit(int krSuccess, const void * buf, size_t len)
{
    if (!krSuccess) return 1;
    return ga106ctl_check_r3_mapping(buf, len);
}

int ga106ctl_check_r3_quies(const void * buf, size_t len)
{
    const struct GA106LabR3QuiescenceV1 * info;
    if (!buf) return 1;
    if (len != sizeof(struct GA106LabR3QuiescenceV1)) return 2;
    info = (const struct GA106LabR3QuiescenceV1 *) buf;
    if (info->size != sizeof(struct GA106LabR3QuiescenceV1)) return 3;
    if (info->version != GA106LAB_UC_PROTOCOL_VERSION) return 4;
    if (info->status != kGA106LabR3Status_Pass) return 5;
    if (!info->quiescencePass || !info->baselineStable) return 6;
    if (info->pendingProjectDmaState != 0) return 7;
    if (!info->halted || !info->fullPre || !info->idle1) return 8;
    if (!info->idle2 || !info->fullPost) return 9;
    if (!info->transcfgStable || !info->mailboxStable) return 10;
    if (!info->bootvecStable || !info->fbifStable) return 11;
    if (!info->schedZero || !info->ceNoPending) return 12;
    if (!info->aerClean || !info->iommuClean || !info->providerEpochOk) return 13;
    if (info->launchAttempted || info->recoveryRequired) return 14;
    if (info->reserved0) return 15;
    if (r3_all_reserved_zero(info->reserved, 7)) return 16;
    return 0;
}

int ga106ctl_r3_quies_cli_exit(int krSuccess, const void * buf, size_t len)
{
    if (!krSuccess) return 1;
    return ga106ctl_check_r3_quies(buf, len);
}
