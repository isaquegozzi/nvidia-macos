// GA106LabCoreValidate.h — validadores determinísticos compartilhados.
//
// Usado pela produção (GA106LabMmioFlow.hpp + GA106LabRealOps.hpp, kernel)
// E pelos testes offline (GA106LabMockOps.hpp + harness userspace).
// Lógica pura, sem IOKit/hardware, sem callbacks arbitrários.
//
// Encoding verificado nos headers locais (MacOSX.sdk):
//   IOMapTypes.h:
//     kIOMapCacheMask  = 0x00000f00
//     kIOMapCacheShift = 8
//     kIODefaultCache=0, kIOInhibitCache=1, kIOWriteThru=2,
//     kIOCopyback=3, kIOWriteCombine=4, ...
//     kIOMapDefaultCache     = 0<<8 = 0x0000
//     kIOMapInhibitCache     = 1<<8 = 0x0100
//     kIOMapWriteThruCache   = 2<<8 = 0x0200
//     kIOMapCopybackCache    = 3<<8 = 0x0300
//     kIOMapWriteCombineCache= 4<<8 = 0x0400
//     kIOMapReadOnly         = 0x00001000
//   Portanto:
//     ReadOnly+Inhibit  = 0x1100 (único aceito)
//     ReadOnly+Copyback = 0x1300 (DEVE falhar: contém bit 0x0100 mas
//       campo cache 0x0300 != 0x0100; o check antigo por REQUIRED_BITS
//       aceitava incorretamente porque (0x1300 & 0x1100)==0x1100).
//     ReadOnly+WriteCombine = 0x1400 (DEVE falhar)
//     ReadOnly+Default      = 0x1000 (DEVE falhar)
//     Inhibit sem ReadOnly  = 0x0100 (DEVE falhar)
//     ReadOnly sem Inhibit  = 0x1000 (DEVE falhar)
//
// Política exata (permite bits ortogonais que o framework possa adicionar
// fora de ReadOnly+CacheMask, ex. kIOMapAnywhere normalizado):
//   ((opts & ReadOnly) != 0) && ((opts & CacheMask) == Inhibit)
// Rejeita explicitamente qualquer cache mode != Inhibit.
//
// Sem dependência de headers kernel aqui (constantes numéricas); o
// GA106Lab.cpp faz static_assert contra kIOMap* reais para provar
// equivalência de domínio/shift.

#ifndef GA106LAB_CORE_VALIDATE_H
#define GA106LAB_CORE_VALIDATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Encoding verificado (espelha IOMapTypes.h, já shiftado) ---
#define GA106LAB_MAP_READONLY_BIT      0x00001000u
#define GA106LAB_MAP_CACHE_MASK        0x00000F00u
#define GA106LAB_MAP_CACHE_INHIBIT     0x00000100u
#define GA106LAB_MAP_CACHE_DEFAULT     0x00000000u
#define GA106LAB_MAP_CACHE_WRITETHRU   0x00000200u
#define GA106LAB_MAP_CACHE_COPYBACK    0x00000300u
#define GA106LAB_MAP_CACHE_WRITECOMBINE 0x00000400u

// --- BAR0 baseline (mesma constante da produção) ---
#define GA106LAB_BAR0_EXPECTED_LENGTH  (0x1000000ULL)

// --- Identidade / chip ---
#define GA106LAB_VENDOR_NVIDIA         0x10DEu
#define GA106LAB_DEVICE_GA106          0x2504u
#define GA106LAB_CMD_MSE_BIT           0x2u
#define GA106LAB_MMIO_CHIP_MASK        0x1FF00000u
#define GA106LAB_MMIO_CHIP_SHIFT       20u
#define GA106LAB_MMIO_CHIP_GA106       0x176u

// Validação exata de map options: somente ReadOnly + InhibitCache.
// Retorna 1 = válido, 0 = inválido.
static inline int GA106LabMapOptionsValidROInhibit(uint32_t mapOpts)
{
    int roOk = ((mapOpts & GA106LAB_MAP_READONLY_BIT) != 0u);
    int cacheOk = ((mapOpts & GA106LAB_MAP_CACHE_MASK) ==
                   GA106LAB_MAP_CACHE_INHIBIT);
    return (roOk && cacheOk) ? 1 : 0;
}

// MSE já ligado? (nunca habilitar; abortar se off)
static inline int GA106LabCommandMseOn(uint16_t cmd)
{
    return ((cmd & GA106LAB_CMD_MSE_BIT) != 0u) ? 1 : 0;
}

// BAR0 raw válido? Rejeita I/O (bit0) e 64-bit/prefetch (bits 2:1).
static inline int GA106LabBar0RawValid(uint32_t bar0Raw)
{
    if ((bar0Raw & 0x1u) != 0u) {
        return 0;
    }
    if ((bar0Raw & 0x6u) != 0u) {
        return 0;
    }
    return 1;
}

static inline uint64_t GA106LabBar0BaseFromRaw(uint32_t bar0Raw)
{
    return (uint64_t)(bar0Raw & 0xFFFFFFF0u);
}

// Match estrito de resource: 16 MiB + base == BAR0 atual.
static inline int GA106LabBarResourceMatches(uint64_t len, uint64_t phys,
                                             uint64_t bar0Base)
{
    return (len == GA106LAB_BAR0_EXPECTED_LENGTH && phys == bar0Base) ? 1 : 0;
}

// Alinhamento do offset fixo (4 bytes).
static inline int GA106LabAlignmentValid(uint32_t offset)
{
    return ((offset % 4u) == 0u) ? 1 : 0;
}

// Bounds overflow-safe: offset+width <= mapLen.
static inline int GA106LabBoundsValid(uint32_t offset, uint32_t width,
                                      uint64_t mapLen)
{
    uint64_t end;
    if ((uint64_t)width > mapLen) {
        return 0;
    }
    end = (uint64_t)offset + (uint64_t)width;
    // Detecta wrap (impossível com valores atuais, mas à prova de futuro).
    if (end < (uint64_t)offset) {
        return 0;
    }
    return (end <= mapLen) ? 1 : 0;
}

// Absent-device: ffff / zero (sem retry).
static inline int GA106LabRawIsAbsent(uint32_t raw)
{
    return (raw == 0xFFFFFFFFu || raw == 0x00000000u) ? 1 : 0;
}

static inline uint32_t GA106LabChipFromRaw(uint32_t raw)
{
    return (raw & GA106LAB_MMIO_CHIP_MASK) >> GA106LAB_MMIO_CHIP_SHIFT;
}

static inline int GA106LabChipIsGA106(uint32_t raw)
{
    return (GA106LabChipFromRaw(raw) == GA106LAB_MMIO_CHIP_GA106) ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* GA106LAB_CORE_VALIDATE_H */
