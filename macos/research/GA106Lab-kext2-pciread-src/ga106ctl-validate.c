// ga106ctl-validate.c — mesma unidade usada pelo CLI e pelos testes.
#include "ga106ctl-validate.h"

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
