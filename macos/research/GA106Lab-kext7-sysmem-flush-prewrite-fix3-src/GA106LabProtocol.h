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
#define GA106LAB_DRIVER_VERSION_U32  0x00010703u /* 1.7.3 */

enum {
    kGA106LabSelector_GetProtocolVersion = 0,
    kGA106LabSelector_GetDeviceIdentity = 1,
    kGA106LabSelector_GetServiceState   = 2,
    kGA106LabSelector_GetPciSnapshot   = 3,
    kGA106LabSelector_BarMapProbe      = 4,
    kGA106LabSelector_ReadFirstMmioRegister = 5,
    kGA106LabSelector_ReadStaticIdentity = 6,
    kGA106LabSelector_PrepareGspSysmem = 7,
    kGA106LabSelector_ReadGfwBootReadiness = 8,
    kGA106LabSelector_VerifySysmemFlushPreconditions = 9,
    kGA106LabSelector_Count             = 10
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

/* Selector 4 = BAR_MAP_PROBE (TG-KEXT3). Probe controlado do BAR0 register
   aperture allowlisted. Input void/zero; sem BAR index/resource index/phys/
   offset/length/cache do userspace. Mapeia, valida metadata, desmapeia antes
   de retornar. NUNCA dereferencia MMIO, nunca expõe VA/ponteiro kernel. */
enum {
    kGA106LabBarMapStatus_NotAttempted = 0u,
    kGA106LabBarMapStatus_ResourceFound = 1u,
    kGA106LabBarMapStatus_MapCreated    = 2u,
    kGA106LabBarMapStatus_MapValidated  = 3u,
    kGA106LabBarMapStatus_MapReleased   = 4u,
    kGA106LabBarMapStatus_Failed        = 5u
};

enum {
    kGA106LabBarMapSemantic_Bar0 = 0u
};

enum {
    kGA106LabBarMapValid_Resource         = (1u << 0),
    kGA106LabBarMapValid_PhysicalBase     = (1u << 1),
    kGA106LabBarMapValid_ResourceLength   = (1u << 2),
    kGA106LabBarMapValid_MappedLength     = (1u << 3),
    kGA106LabBarMapValid_ResourceFlags    = (1u << 4),
    kGA106LabBarMapValid_MapOptions       = (1u << 5),
    kGA106LabBarMapValid_ReleaseConfirmed = (1u << 6)
};

struct GA106LabBarMapInfoV1 {
    uint32_t size;            /* @0  = 96 */
    uint32_t version;         /* @4  = 1 */
    uint32_t status;          /* @8: kGA106LabBarMapStatus_* */
    uint32_t validMask;       /* @12: kGA106LabBarMapValid_* */
    uint32_t semanticBar;     /* @16: kGA106LabBarMapSemantic_* (0 = BAR0) */
    uint32_t resourceIndex;   /* @20: index validado por discovery */
    uint64_t physicalBase;    /* @24: resource->getPhysicalAddress() canônico */
    uint64_t resourceLength;  /* @32: resource->getLength() */
    uint64_t mappedLength;    /* @40: map->getLength() (== resourceLength) */
    uint32_t resourceFlags;   /* @48: flags do descriptor (0 se sem fonte) */
    uint32_t mapOptions;      /* @52: eco validado (RO+Inhibit exigidos) */
    uint32_t mapState;        /* @56: 1 = created+validated+released */
    uint32_t reserved0;       /* @60 = 0 */
    uint64_t reserved[4];     /* @64..96 = 0 */
};

/* Selector 5 = READ_FIRST_MMIO_REGISTER (TG-KEXT4). Uma única leitura de
   32 bits semanticamente fixa: NV_PMC_BOOT_0 = BAR0 + 0x00000000.
   Input void/zero; sem BAR/offset/width/address do userspace. Mapeia BAR0
   RO+Inhibit (full, caminho KEXT3), valida, 1x OSReadLittleInt32, valida
   chip-id 0x176, desmapeia antes de retornar. NUNCA escreve. */
enum {
    kGA106LabMmioReg_PmcBoot0 = 1u
};

#define kGA106LabMmio_PmcBoot0_Offset 0x00000000u
#define kGA106LabMmio_PmcBoot0_Width  4u
#define kGA106LabMmio_ChipId_Mask     0x1FF00000u
#define kGA106LabMmio_ChipId_Shift    20u
#define kGA106LabMmio_ChipId_GA106    0x176u

enum {
    kGA106LabMmioStatus_NotAttempted = 0u,
    kGA106LabMmioStatus_ResourceFound = 1u,
    kGA106LabMmioStatus_MapCreated    = 2u,
    kGA106LabMmioStatus_ReadCompleted = 3u,
    kGA106LabMmioStatus_MapReleased   = 4u,
    kGA106LabMmioStatus_Failed        = 5u
};

enum {
    kGA106LabMmioValid_RegisterId       = (1u << 0),
    kGA106LabMmioValid_Offset           = (1u << 1),
    kGA106LabMmioValid_Width            = (1u << 2),
    kGA106LabMmioValid_RawValue         = (1u << 3),
    kGA106LabMmioValid_ChipId           = (1u << 4),
    kGA106LabMmioValid_ReleaseConfirmed = (1u << 5)
};

struct GA106LabMmioReadV1 {
    uint32_t size;          /* @0  = 48 */
    uint32_t version;       /* @4  = 1 */
    uint32_t status;        /* @8: kGA106LabMmioStatus_* */
    uint32_t registerId;    /* @12: kGA106LabMmioReg_* (1 = PMC_BOOT_0) */
    uint32_t offset;        /* @16: fixo 0x00000000 */
    uint32_t width;         /* @20: fixo 4 */
    uint32_t rawValue;      /* @24: valor lido (1x OSReadLittleInt32) */
    uint32_t validFlags;    /* @28: kGA106LabMmioValid_* */
    uint32_t reserved[4];   /* @32..48 = 0 */
};

/* Selector 6 = READ_STATIC_IDENTITY (TG-KEXT4-B). UMA leitura de 32 bits
   semanticamente fixa, sem BOOT_0: NV_PMC_BOOT_42 = BAR0+0xA00.
   (BOOT_2 @0x8 REMOVIDO nesta revisão por evidência primária insuficiente;
   ver relatório.) Input void/zero; sem BAR/offset/width do userspace.
   Um mapa BAR0 RO+Inhibit, 1 release, Command before==after. NUNCA escreve.
   BOOT_42 CHIP_ID = bits 29:20 (máscara 0x3FF00000>>20, distinto de BOOT_0
   bits 28:20 / 0x1FF00000, que permanece intacto). */
#define kGA106LabStaticIdentity_Boot42_Offset 0x00000A00u
#define kGA106LabStaticIdentity_Boot42_Width  4u

enum {
    kGA106LabStaticIdentityStatus_NotAttempted = 0u,
    kGA106LabStaticIdentityStatus_ResourceFound = 1u,
    kGA106LabStaticIdentityStatus_MapCreated    = 2u,
    kGA106LabStaticIdentityStatus_ReadCompleted = 3u,
    kGA106LabStaticIdentityStatus_MapReleased   = 4u,
    kGA106LabStaticIdentityStatus_Failed        = 5u
};

enum {
    kGA106LabStaticIdentityValid_Boot42           = (1u << 0),
    kGA106LabStaticIdentityValid_DecodedChipId    = (1u << 2),
    kGA106LabStaticIdentityValid_ReadCount        = (1u << 3),
    kGA106LabStaticIdentityValid_ReleaseConfirmed = (1u << 5)
};

struct GA106LabStaticIdentityV1 {
    uint32_t size;             /* @0  = 32 */
    uint32_t version;          /* @4  = 1 */
    uint32_t status;           /* @8: kGA106LabStaticIdentityStatus_* */
    uint32_t validMask;        /* @12: kGA106LabStaticIdentityValid_* */
    uint32_t readCount;        /* @16: fixo 1 em sucesso */
    uint32_t boot42Raw;        /* @20: BAR0+0xA00 */
    uint32_t decodedChipId;    /* @24: CHIP_ID de BOOT_42 bits 29:20 (0x176) */
    uint32_t reserved;         /* @28 = 0 */
};

/* Selector 7 = PREPARE_GSP_SYSMEM (TG-KEXT5 GSP-DMA1). Prepara UMA página
   sysmem 4 KiB + mapping DMA (1 segmento) para futuro bootstrap GSP.
   Input void/zero; sem size/address/flags do userspace. BME OFF, zero
   MMIO, zero PCI config writes, zero DMA trigger. Endereço DMA/bus é
   kernel-only e NUNCA aparece na ABI. */
#define kGA106LabFlushPrewrite_Size      4096u
#define kGA106LabFlushPrewrite_Alignment 4096u
#define kGA106LabGspSysmem_Size      4096u
#define kGA106LabGspSysmem_Alignment 4096u

enum {
    kGA106LabGspSysmemState_Empty         = 0u,
    kGA106LabGspSysmemState_Allocated     = 1u,
    kGA106LabGspSysmemState_Prepared      = 2u,
    kGA106LabGspSysmemState_AddressReady  = 3u
};

enum {
    kGA106LabGspSysmemStatus_NotAttempted    = 0u,
    kGA106LabGspSysmemStatus_AddressReady    = 1u,
    kGA106LabGspSysmemStatus_AlreadyPrepared = 2u,
    kGA106LabGspSysmemStatus_Failed          = 3u
};

/* Semântica formal (FIX-ASTRA-C): ADDRESS_READY = mapping resolvido pelo
   KPI e válido segundo constraints locais (1 segmento 4096B alinhado,
   largura verificada). ADDRESS_READY != endpoint DMA live validated:
   a prova de uso pela GA106 exige fase live futura dedicada. IOVA nunca
   é exposta (DMA_ENDPOINT_VALIDATED = NO permanente nesta revisão). */

enum {
    kGA106LabGspSysmemFlag_AddressReady = (1u << 0)
};

struct GA106LabGspSysmemV1 {
    uint32_t size;             /* @0  = 48 */
    uint32_t version;          /* @4  = 1 */
    uint32_t status;           /* @8: kGA106LabGspSysmemStatus_* */
    uint32_t flags;            /* @12: kGA106LabGspSysmemFlag_* */
    uint32_t bufferSize;       /* @16: 4096 */
    uint32_t segmentCount;     /* @20: 1 */
    uint32_t segmentLength;    /* @24: 4096 */
    uint32_t state;            /* @28: kGA106LabGspSysmemState_AddressReady */
    uint32_t reserved[4];      /* @32..48 = 0 */
};

/* Selector 8 = READ_GFW_BOOT_READINESS (TG-KEXT6 GSP-GFW-BOOT-READINESS).
   Verifica o estado pré-existente de firmware/boot da GPU (GFW_BOOT) por
   dois registradores fixos, semanticamente definidos (Nova/OpenRM):
   - NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_PRIV_LEVEL_MASK @0x118128, bit 0
     read_protection_level0 = gate de acesso (NÃO é readiness; se 0, o
     registrador de progresso não pode ser lido com validade).
   - NV_PGC6_AON_SECURE_SCRATCH_GROUP_05[0] @0x118234, bits 7:0 progress;
     0xff = completed. Qualquer outro valor = not complete yet (sem
     estados de erro inventados).
   Input void/zero; sem BAR/offset/mask/timeout do userspace. Polling
   bounded DENTRO de uma única chamada, com DOIS limites independentes:
   contagem (4000 iterações) E deadline monotônico real (4 s). Somente
   leituras 32-bit; zero stores de qualquer tipo. Endereços (BAR/VA/físico/
   IOVA) NUNCA aparecem na ABI.
   MSE é checado ANTES de mapear/ler (sem validade MMIO se off); a janela
   inteira de uso do provider é sincronizada por lifetime (reserva antecipada
   + provider retido + stop espera !busy), e selector 8 nunca escreve PCI
   Command — post-read comparison intentionally absent (nenhuma fonte a
   exige; não adicionado por simetria).
   Semântica formal transporte-vs-resposta (TIMEOUT_TRANSPORT_SEMANTICS_DOCUMENTED):
   transport success (kIOReturnSuccess / CLI exit 0) = resposta ABI entregue
   corretamente; o resultado semântico está SOMENTE em status/timedOut.
   status TimedOut + timedOut 1 = GFW não completou no orçamento (resposta
   válida, NÃO erro de transporte). TimedOut NUNCA é impresso como Completed;
   o CLI não faz retry automático; exit 0 NÃO significa "GFW completed". */
enum {
    kGA106LabGfwStatus_NotAttempted = 0u,
    kGA106LabGfwStatus_Completed    = 1u,
    kGA106LabGfwStatus_TimedOut     = 2u,
    kGA106LabGfwStatus_Aborted      = 3u,
    kGA106LabGfwStatus_MapFailed    = 4u,
    kGA106LabGfwStatus_Unavailable  = 5u
};

/* Selector 9 ABI v1 48B semantica (fixed, versioned). */
enum {
    kGA106LabFlushPrewriteStatus_NotAttempted      = 0u,
    kGA106LabFlushPrewriteStatus_PreconditionsReady = 1u,
    kGA106LabFlushPrewriteStatus_AlreadyReady      = 2u,
    kGA106LabFlushPrewriteStatus_Failed            = 3u
};
enum {
    kGA106LabFlushPrewritePhase_Unprepared        = 0u,
    kGA106LabFlushPrewritePhase_AddressReady      = 1u,
    kGA106LabFlushPrewritePhase_PreconditionsReady = 2u
};
enum {
    kGA106LabFlushRegs_NotChecked = 0u,
    kGA106LabFlushRegs_Zero       = 1u,
    kGA106LabFlushRegs_NonZero    = 2u
};
struct GA106LabFlushPrewriteV1 {
    uint32_t size;                /* @0  = 48 */
    uint32_t version;             /* @4  = 1 */
    uint32_t status;              /* @8  */
    uint32_t phase;               /* @12 */
    uint8_t  providerReady;       /* @16 */
    uint8_t  ga106Exact;          /* @17 */
    uint8_t  bar0Ready;           /* @18 */
    uint8_t  mseEnabled;          /* @19 */
    uint8_t  gfwReady;            /* @20 */
    uint8_t  bmeEnabled;          /* @21 */
    uint8_t  pageReady;           /* @22 */
    uint8_t  segmentCount;        /* @23 esperado 1 */
    uint32_t segmentLength;       /* @24 esperado 4096 */
    uint8_t  alignmentReady;      /* @28 */
    uint8_t  addressRepresentable;/* @29 */
    uint8_t  encodingRoundtripReady; /* @30 */
    uint8_t  flushRegistersState; /* @31 enum NotChecked/Zero/NonZero (sempre NotChecked nesta rev) */
    uint8_t  programmed;          /* @32 sempre 0 em Gate A */
    uint8_t  readbackMatch;       /* @33 sempre 0 em Gate A */
    uint8_t  writeCount;          /* @34 sempre 0 */
    uint8_t  readCount;           /* @35 contagem de reads diagnosticos executados (0 nesta rev) */
    uint8_t  reserved[12];        /* @36..48 = 0 */
};

struct GA106LabGfwReadinessV1 {
    uint32_t size;             /* @0  = 32 */
    uint32_t version;          /* @4  = 1 */
    uint32_t status;           /* @8: kGA106LabGfwStatus_* */
    uint32_t gateReady;        /* @12: boolean semântico (bit0 de 0x118128) */
    uint32_t progress;         /* @16: valor 8-bit de 0x118234[7:0] */
    uint32_t pollCount;        /* @20: iterações executadas (1..4000) */
    uint32_t timedOut;         /* @24: boolean semântico */
    uint32_t reserved;         /* @28 = 0 */
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
static_assert(sizeof(GA106LabBarMapInfoV1) == 96, "BarMap size");
static_assert(alignof(GA106LabBarMapInfoV1) == 8, "BarMap align");
static_assert(offsetof(GA106LabBarMapInfoV1, size) == 0, "BarMap.size");
static_assert(offsetof(GA106LabBarMapInfoV1, version) == 4, "BarMap.version");
static_assert(offsetof(GA106LabBarMapInfoV1, status) == 8, "BarMap.status");
static_assert(offsetof(GA106LabBarMapInfoV1, validMask) == 12, "BarMap.mask");
static_assert(offsetof(GA106LabBarMapInfoV1, semanticBar) == 16, "BarMap.sem");
static_assert(offsetof(GA106LabBarMapInfoV1, resourceIndex) == 20, "BarMap.idx");
static_assert(offsetof(GA106LabBarMapInfoV1, physicalBase) == 24, "BarMap.phys");
static_assert(offsetof(GA106LabBarMapInfoV1, resourceLength) == 32, "BarMap.rlen");
static_assert(offsetof(GA106LabBarMapInfoV1, mappedLength) == 40, "BarMap.mlen");
static_assert(offsetof(GA106LabBarMapInfoV1, resourceFlags) == 48, "BarMap.flags");
static_assert(offsetof(GA106LabBarMapInfoV1, mapOptions) == 52, "BarMap.opts");
static_assert(offsetof(GA106LabBarMapInfoV1, mapState) == 56, "BarMap.state");
static_assert(offsetof(GA106LabBarMapInfoV1, reserved0) == 60, "BarMap.r0");
static_assert(offsetof(GA106LabBarMapInfoV1, reserved) == 64, "BarMap.res");
static_assert(sizeof(GA106LabMmioReadV1) == 48, "MmioRead size");
static_assert(alignof(GA106LabMmioReadV1) == 4, "MmioRead align");
static_assert(offsetof(GA106LabMmioReadV1, size) == 0, "MmioRead.size");
static_assert(offsetof(GA106LabMmioReadV1, version) == 4, "MmioRead.version");
static_assert(offsetof(GA106LabMmioReadV1, status) == 8, "MmioRead.status");
static_assert(offsetof(GA106LabMmioReadV1, registerId) == 12, "MmioRead.reg");
static_assert(offsetof(GA106LabMmioReadV1, offset) == 16, "MmioRead.off");
static_assert(offsetof(GA106LabMmioReadV1, width) == 20, "MmioRead.width");
static_assert(offsetof(GA106LabMmioReadV1, rawValue) == 24, "MmioRead.raw");
static_assert(offsetof(GA106LabMmioReadV1, validFlags) == 28, "MmioRead.flags");
static_assert(offsetof(GA106LabMmioReadV1, reserved) == 32, "MmioRead.res");
static_assert(sizeof(GA106LabStaticIdentityV1) == 32, "StaticIdentity size");
static_assert(alignof(GA106LabStaticIdentityV1) == 4, "StaticIdentity align");
static_assert(offsetof(GA106LabStaticIdentityV1, size) == 0, "StaticIdentity.size");
static_assert(offsetof(GA106LabStaticIdentityV1, version) == 4, "StaticIdentity.version");
static_assert(offsetof(GA106LabStaticIdentityV1, status) == 8, "StaticIdentity.status");
static_assert(offsetof(GA106LabStaticIdentityV1, validMask) == 12, "StaticIdentity.mask");
static_assert(offsetof(GA106LabStaticIdentityV1, readCount) == 16, "StaticIdentity.count");
static_assert(offsetof(GA106LabStaticIdentityV1, boot42Raw) == 20, "StaticIdentity.b42");
static_assert(offsetof(GA106LabStaticIdentityV1, decodedChipId) == 24, "StaticIdentity.chip");
static_assert(offsetof(GA106LabStaticIdentityV1, reserved) == 28, "StaticIdentity.res");
static_assert(sizeof(GA106LabGspSysmemV1) == 48, "GspSysmem size");
static_assert(alignof(GA106LabGspSysmemV1) == 4, "GspSysmem align");
static_assert(offsetof(GA106LabGspSysmemV1, size) == 0, "GspSysmem.size");
static_assert(offsetof(GA106LabGspSysmemV1, version) == 4, "GspSysmem.version");
static_assert(offsetof(GA106LabGspSysmemV1, status) == 8, "GspSysmem.status");
static_assert(offsetof(GA106LabGspSysmemV1, flags) == 12, "GspSysmem.flags");
static_assert(offsetof(GA106LabGspSysmemV1, bufferSize) == 16, "GspSysmem.bufsize");
static_assert(offsetof(GA106LabGspSysmemV1, segmentCount) == 20, "GspSysmem.segcount");
static_assert(offsetof(GA106LabGspSysmemV1, segmentLength) == 24, "GspSysmem.seglen");
static_assert(offsetof(GA106LabGspSysmemV1, state) == 28, "GspSysmem.state");
static_assert(offsetof(GA106LabGspSysmemV1, reserved) == 32, "GspSysmem.res");
static_assert(sizeof(GA106LabGfwReadinessV1) == 32, "GfwReadiness size");
static_assert(sizeof(GA106LabFlushPrewriteV1) == 48, "FlushPrewrite size");
static_assert(alignof(GA106LabFlushPrewriteV1) == 4, "FlushPrewrite align");
static_assert(offsetof(GA106LabFlushPrewriteV1, size) == 0, "FlushPrewrite.size");
static_assert(offsetof(GA106LabFlushPrewriteV1, version) == 4, "FlushPrewrite.version");
static_assert(offsetof(GA106LabFlushPrewriteV1, status) == 8, "FlushPrewrite.status");
static_assert(offsetof(GA106LabFlushPrewriteV1, phase) == 12, "FlushPrewrite.phase");
static_assert(offsetof(GA106LabFlushPrewriteV1, segmentLength) == 24, "FlushPrewrite.seglen");
static_assert(offsetof(GA106LabFlushPrewriteV1, reserved) == 36, "FlushPrewrite.res");
static_assert(alignof(GA106LabGfwReadinessV1) == 4, "GfwReadiness align");
static_assert(offsetof(GA106LabGfwReadinessV1, size) == 0, "GfwReadiness.size");
static_assert(offsetof(GA106LabGfwReadinessV1, version) == 4, "GfwReadiness.version");
static_assert(offsetof(GA106LabGfwReadinessV1, status) == 8, "GfwReadiness.status");
static_assert(offsetof(GA106LabGfwReadinessV1, gateReady) == 12, "GfwReadiness.gate");
static_assert(offsetof(GA106LabGfwReadinessV1, progress) == 16, "GfwReadiness.progress");
static_assert(offsetof(GA106LabGfwReadinessV1, pollCount) == 20, "GfwReadiness.pollCount");
static_assert(offsetof(GA106LabGfwReadinessV1, timedOut) == 24, "GfwReadiness.timedOut");
static_assert(offsetof(GA106LabGfwReadinessV1, reserved) == 28, "GfwReadiness.res");
#else
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
_Static_assert(sizeof(struct GA106LabBarMapInfoV1) == 96, "BarMap size");
_Static_assert(_Alignof(struct GA106LabBarMapInfoV1) == 8, "BarMap align");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, size) == 0, "BarMap.size");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, version) == 4, "BarMap.version");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, status) == 8, "BarMap.status");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, validMask) == 12, "BarMap.mask");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, semanticBar) == 16, "BarMap.sem");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, resourceIndex) == 20, "BarMap.idx");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, physicalBase) == 24, "BarMap.phys");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, resourceLength) == 32, "BarMap.rlen");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, mappedLength) == 40, "BarMap.mlen");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, resourceFlags) == 48, "BarMap.flags");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, mapOptions) == 52, "BarMap.opts");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, mapState) == 56, "BarMap.state");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, reserved0) == 60, "BarMap.r0");
_Static_assert(offsetof(struct GA106LabBarMapInfoV1, reserved) == 64, "BarMap.res");
_Static_assert(sizeof(struct GA106LabMmioReadV1) == 48, "MmioRead size");
_Static_assert(_Alignof(struct GA106LabMmioReadV1) == 4, "MmioRead align");
_Static_assert(offsetof(struct GA106LabMmioReadV1, size) == 0, "MmioRead.size");
_Static_assert(offsetof(struct GA106LabMmioReadV1, version) == 4, "MmioRead.version");
_Static_assert(offsetof(struct GA106LabMmioReadV1, status) == 8, "MmioRead.status");
_Static_assert(offsetof(struct GA106LabMmioReadV1, registerId) == 12, "MmioRead.reg");
_Static_assert(offsetof(struct GA106LabMmioReadV1, offset) == 16, "MmioRead.off");
_Static_assert(offsetof(struct GA106LabMmioReadV1, width) == 20, "MmioRead.width");
_Static_assert(offsetof(struct GA106LabMmioReadV1, rawValue) == 24, "MmioRead.raw");
_Static_assert(offsetof(struct GA106LabMmioReadV1, validFlags) == 28, "MmioRead.flags");
_Static_assert(offsetof(struct GA106LabMmioReadV1, reserved) == 32, "MmioRead.res");
_Static_assert(sizeof(struct GA106LabStaticIdentityV1) == 32, "StaticIdentity size");
_Static_assert(_Alignof(struct GA106LabStaticIdentityV1) == 4, "StaticIdentity align");
_Static_assert(offsetof(struct GA106LabStaticIdentityV1, size) == 0, "StaticIdentity.size");
_Static_assert(offsetof(struct GA106LabStaticIdentityV1, version) == 4, "StaticIdentity.version");
_Static_assert(offsetof(struct GA106LabStaticIdentityV1, status) == 8, "StaticIdentity.status");
_Static_assert(offsetof(struct GA106LabStaticIdentityV1, validMask) == 12, "StaticIdentity.mask");
_Static_assert(offsetof(struct GA106LabStaticIdentityV1, readCount) == 16, "StaticIdentity.count");
_Static_assert(offsetof(struct GA106LabStaticIdentityV1, boot42Raw) == 20, "StaticIdentity.b42");
_Static_assert(offsetof(struct GA106LabStaticIdentityV1, decodedChipId) == 24, "StaticIdentity.chip");
_Static_assert(offsetof(struct GA106LabStaticIdentityV1, reserved) == 28, "StaticIdentity.res");
_Static_assert(sizeof(struct GA106LabGspSysmemV1) == 48, "GspSysmem size");
_Static_assert(_Alignof(struct GA106LabGspSysmemV1) == 4, "GspSysmem align");
_Static_assert(offsetof(struct GA106LabGspSysmemV1, size) == 0, "GspSysmem.size");
_Static_assert(offsetof(struct GA106LabGspSysmemV1, version) == 4, "GspSysmem.version");
_Static_assert(offsetof(struct GA106LabGspSysmemV1, status) == 8, "GspSysmem.status");
_Static_assert(offsetof(struct GA106LabGspSysmemV1, flags) == 12, "GspSysmem.flags");
_Static_assert(offsetof(struct GA106LabGspSysmemV1, bufferSize) == 16, "GspSysmem.bufsize");
_Static_assert(offsetof(struct GA106LabGspSysmemV1, segmentCount) == 20, "GspSysmem.segcount");
_Static_assert(offsetof(struct GA106LabGspSysmemV1, segmentLength) == 24, "GspSysmem.seglen");
_Static_assert(offsetof(struct GA106LabGspSysmemV1, state) == 28, "GspSysmem.state");
_Static_assert(offsetof(struct GA106LabGspSysmemV1, reserved) == 32, "GspSysmem.res");
_Static_assert(sizeof(struct GA106LabGfwReadinessV1) == 32, "GfwReadiness size");
_Static_assert(sizeof(struct GA106LabFlushPrewriteV1) == 48, "FlushPrewrite size");
_Static_assert(_Alignof(struct GA106LabFlushPrewriteV1) == 4, "FlushPrewrite align");
_Static_assert(offsetof(struct GA106LabFlushPrewriteV1, size) == 0, "FlushPrewrite.size");
_Static_assert(offsetof(struct GA106LabFlushPrewriteV1, segmentLength) == 24, "FlushPrewrite.seglen");
_Static_assert(offsetof(struct GA106LabFlushPrewriteV1, reserved) == 36, "FlushPrewrite.res");
_Static_assert(_Alignof(struct GA106LabGfwReadinessV1) == 4, "GfwReadiness align");
_Static_assert(offsetof(struct GA106LabGfwReadinessV1, size) == 0, "GfwReadiness.size");
_Static_assert(offsetof(struct GA106LabGfwReadinessV1, version) == 4, "GfwReadiness.version");
_Static_assert(offsetof(struct GA106LabGfwReadinessV1, status) == 8, "GfwReadiness.status");
_Static_assert(offsetof(struct GA106LabGfwReadinessV1, gateReady) == 12, "GfwReadiness.gate");
_Static_assert(offsetof(struct GA106LabGfwReadinessV1, progress) == 16, "GfwReadiness.progress");
_Static_assert(offsetof(struct GA106LabGfwReadinessV1, pollCount) == 20, "GfwReadiness.pollCount");
_Static_assert(offsetof(struct GA106LabGfwReadinessV1, timedOut) == 24, "GfwReadiness.timedOut");
_Static_assert(offsetof(struct GA106LabGfwReadinessV1, reserved) == 28, "GfwReadiness.res");
#endif

#endif /* GA106LAB_PROTOCOL_H */
