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
#define GA106LAB_DRIVER_VERSION_U32  0x00010100u /* 1.1.0 */

enum {
    kGA106LabSelector_GetProtocolVersion = 0,
    kGA106LabSelector_GetDeviceIdentity = 1,
    kGA106LabSelector_GetServiceState   = 2,
    kGA106LabSelector_Count             = 3
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
#endif

#endif /* GA106LAB_PROTOCOL_H */
