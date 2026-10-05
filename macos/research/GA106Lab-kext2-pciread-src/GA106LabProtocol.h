//
// GA106LabProtocol.h — ABI compartilhada kernel/userspace, protocolo v1.
//
// Regras: fixed-width ints, sem ponteiros, sem STL, sem bool implícito,
// offsets explícitos, static_assert de size/offset nos dois lados.
// KEXT1 = transporte neutro (NÃO compute-specific; sem APIs tinygrad).
//

#ifndef GA106LAB_PROTOCOL_H
#define GA106LAB_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GA106LAB_UC_PROTOCOL_VERSION 1u
#define GA106LAB_DRIVER_VERSION_U32  0x00010200u /* 1.2.0 */

enum {
    kGA106LabSelector_GetProtocolVersion = 0,
    kGA106LabSelector_GetDeviceIdentity = 1,
    kGA106LabSelector_GetServiceState   = 2,
    kGA106LabSelector_GetPciSnapshot   = 3,
    kGA106LabSelector_Count             = 4
};

enum {
    kGA106LabState_Unknown      = 0u,
    kGA106LabState_StartSuccess = 1u
};

struct GA106LabIdentityV1 {
    uint32_t size;            /* @0  = 32 */
    uint32_t version;         /* @4  = 1 */
    uint32_t vendor;          /* @8  (0x10de) */
    uint32_t device;          /* @12 (0x2504) */
    uint64_t providerEntryID; /* @16 */
    uint32_t state;           /* @24: kGA106LabState_* */
    uint32_t reserved;        /* @28 = 0 */
};

struct GA106LabStatusV1 {
    uint32_t size;               /* @0  = 32 */
    uint32_t version;            /* @4  = 1 */
    uint32_t state;              /* @8: kGA106LabState_* */
    uint32_t providerConfirmed;  /* @12: 0/1 */
    uint32_t revision;           /* @16: GA106LAB_DRIVER_VERSION_U32 */
    uint32_t reserved0;          /* @20 = 0 */
    uint32_t reserved1;          /* @24 = 0 */
    uint32_t reserved2;          /* @28 = 0 */
};

/* Bits de GA106LabPciSnapshotV1.validMask: grupo lido com sucesso. */
enum {
    kGA106LabPciValid_Identity = (1u << 0),
    kGA106LabPciValid_Command  = (1u << 1),
    kGA106LabPciValid_Header   = (1u << 2),
    kGA106LabPciValid_Bars     = (1u << 3),
    kGA106LabPciValid_Subsys   = (1u << 4),
    kGA106LabPciValid_CapIrq   = (1u << 5)
};

/* Política all-ones FIELD_AWARE: só Vendor==0xffff invalida o snapshot
   (dispositivo ausente/inacessível). Demais raws preservados + máscara. */
enum {
    kGA106LabPciStatus_Ok          = 0u,
    kGA106LabPciStatus_Unavailable = 1u
};

struct GA106LabPciSnapshotV1 {
    uint32_t size;              /* @0  = 84 */
    uint32_t version;           /* @4  = 1 */
    uint32_t status;            /* @8: kGA106LabPciStatus_* */
    uint32_t validMask;         /* @12: kGA106LabPciValid_* */
    uint16_t vendorId;          /* @16 */
    uint16_t deviceId;          /* @18 */
    uint16_t command;           /* @20 */
    uint16_t statusReg;         /* @22 */
    uint8_t  revisionId;        /* @24 */
    uint8_t  progIf;            /* @25 */
    uint8_t  subclass;          /* @26 */
    uint8_t  classCode;         /* @27 base */
    uint8_t  cacheLineSize;     /* @28 */
    uint8_t  latencyTimer;      /* @29 */
    uint8_t  headerType;        /* @30 */
    uint8_t  bist;              /* @31 */
    uint32_t barRaw[6];         /* @32..56 */
    uint16_t subsystemVendorId; /* @56 */
    uint16_t subsystemId;       /* @58 */
    uint32_t expansionRomRaw;   /* @60 */
    uint8_t  capPointer;        /* @64 */
    uint8_t  interruptLine;     /* @65 */
    uint8_t  interruptPin;      /* @66 */
    uint8_t  reserved0;         /* @67 = 0 */
    uint32_t reserved[4];       /* @68..84 = 0 */
};

#ifdef __cplusplus
} /* extern "C" */

static_assert(sizeof(GA106LabIdentityV1) == 32, "IdentityV1 size");
static_assert(alignof(GA106LabIdentityV1) == 8, "IdentityV1 align");
static_assert(offsetof(GA106LabIdentityV1, size) == 0, "IdentityV1.size");
static_assert(offsetof(GA106LabIdentityV1, version) == 4, "IdentityV1.version");
static_assert(offsetof(GA106LabIdentityV1, vendor) == 8, "IdentityV1.vendor");
static_assert(offsetof(GA106LabIdentityV1, device) == 12, "IdentityV1.device");
static_assert(offsetof(GA106LabIdentityV1, providerEntryID) == 16, "IdentityV1.eid");
static_assert(offsetof(GA106LabIdentityV1, state) == 24, "IdentityV1.state");
static_assert(offsetof(GA106LabIdentityV1, reserved) == 28, "IdentityV1.res");
static_assert(sizeof(GA106LabStatusV1) == 32, "StatusV1 size");
static_assert(alignof(GA106LabStatusV1) == 4, "StatusV1 align"); // só uint32
static_assert(offsetof(GA106LabStatusV1, size) == 0, "StatusV1.size");
static_assert(offsetof(GA106LabStatusV1, version) == 4, "StatusV1.version");
static_assert(offsetof(GA106LabStatusV1, state) == 8, "StatusV1.state");
static_assert(offsetof(GA106LabStatusV1, providerConfirmed) == 12, "StatusV1.conf");
static_assert(offsetof(GA106LabStatusV1, revision) == 16, "StatusV1.rev");
static_assert(offsetof(GA106LabStatusV1, reserved0) == 20, "StatusV1.r0");
static_assert(offsetof(GA106LabStatusV1, reserved1) == 24, "StatusV1.r1");
static_assert(offsetof(GA106LabStatusV1, reserved2) == 28, "StatusV1.r2");
static_assert(sizeof(GA106LabPciSnapshotV1) == 84, "PciSnap size");
static_assert(alignof(GA106LabPciSnapshotV1) == 4, "PciSnap align");
static_assert(offsetof(GA106LabPciSnapshotV1, vendorId) == 16, "PciSnap.vendor");
static_assert(offsetof(GA106LabPciSnapshotV1, barRaw) == 32, "PciSnap.bars");
static_assert(offsetof(GA106LabPciSnapshotV1, subsystemVendorId) == 56, "PciSnap.sub");
static_assert(offsetof(GA106LabPciSnapshotV1, expansionRomRaw) == 60, "PciSnap.rom");
static_assert(offsetof(GA106LabPciSnapshotV1, capPointer) == 64, "PciSnap.cap");
static_assert(offsetof(GA106LabPciSnapshotV1, reserved) == 68, "PciSnap.res");
#else
_Static_assert(sizeof(struct GA106LabIdentityV1) == 32, "IdentityV1 size");
_Static_assert(_Alignof(struct GA106LabIdentityV1) == 8, "IdentityV1 align");
_Static_assert(offsetof(struct GA106LabIdentityV1, size) == 0, "IdentityV1.size");
_Static_assert(offsetof(struct GA106LabIdentityV1, version) == 4, "IdentityV1.version");
_Static_assert(offsetof(struct GA106LabIdentityV1, vendor) == 8, "IdentityV1.vendor");
_Static_assert(offsetof(struct GA106LabIdentityV1, device) == 12, "IdentityV1.device");
_Static_assert(offsetof(struct GA106LabIdentityV1, providerEntryID) == 16, "IdentityV1.eid");
_Static_assert(offsetof(struct GA106LabIdentityV1, state) == 24, "IdentityV1.state");
_Static_assert(offsetof(struct GA106LabIdentityV1, reserved) == 28, "IdentityV1.res");
_Static_assert(sizeof(struct GA106LabStatusV1) == 32, "StatusV1 size");
_Static_assert(_Alignof(struct GA106LabStatusV1) == 4, "StatusV1 align");
_Static_assert(offsetof(struct GA106LabStatusV1, size) == 0, "StatusV1.size");
_Static_assert(offsetof(struct GA106LabStatusV1, version) == 4, "StatusV1.version");
_Static_assert(offsetof(struct GA106LabStatusV1, state) == 8, "StatusV1.state");
_Static_assert(offsetof(struct GA106LabStatusV1, providerConfirmed) == 12, "StatusV1.conf");
_Static_assert(offsetof(struct GA106LabStatusV1, revision) == 16, "StatusV1.rev");
_Static_assert(offsetof(struct GA106LabStatusV1, reserved0) == 20, "StatusV1.r0");
_Static_assert(offsetof(struct GA106LabStatusV1, reserved1) == 24, "StatusV1.r1");
_Static_assert(offsetof(struct GA106LabStatusV1, reserved2) == 28, "StatusV1.r2");
_Static_assert(sizeof(struct GA106LabPciSnapshotV1) == 84, "PciSnap size");
_Static_assert(_Alignof(struct GA106LabPciSnapshotV1) == 4, "PciSnap align");
_Static_assert(offsetof(struct GA106LabPciSnapshotV1, vendorId) == 16, "PciSnap.vendor");
_Static_assert(offsetof(struct GA106LabPciSnapshotV1, barRaw) == 32, "PciSnap.bars");
_Static_assert(offsetof(struct GA106LabPciSnapshotV1, subsystemVendorId) == 56, "PciSnap.sub");
_Static_assert(offsetof(struct GA106LabPciSnapshotV1, expansionRomRaw) == 60, "PciSnap.rom");
_Static_assert(offsetof(struct GA106LabPciSnapshotV1, capPointer) == 64, "PciSnap.cap");
_Static_assert(offsetof(struct GA106LabPciSnapshotV1, reserved) == 68, "PciSnap.res");
#endif

#endif /* GA106LAB_PROTOCOL_H */
