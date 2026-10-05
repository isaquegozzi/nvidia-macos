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
