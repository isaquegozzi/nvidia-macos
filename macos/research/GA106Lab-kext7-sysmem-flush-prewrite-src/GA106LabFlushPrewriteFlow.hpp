// GA106LabFlushPrewriteFlow.hpp — Gate A PREWRITE helpers (TG-KEXT7, P68).
//
// Read-only prewrite validation para futuro PROGRAM_SYSMEM_FLUSH_PAGE.
// Producao compoe fluxos ja testados (PCI snapshot, BAR0 probe, GFW readiness,
// sysmem page via RunPrepareGspSysmemFlow com estado DEDICADO fFlush*).
// Este header contem SOMENTE pure helpers compartilhados producao/testes:
// encoding HI/LO, alignment, representabilidade, roundtrip, ABI fill.
// Zero MMIO stores, zero PCI writes, zero BME, zero DMA trigger por construcao
// (nenhuma primitiva de escrita existe neste header).
//
// Decisao diagnostica (P68 §5): HI/LO atuais, WPR2 e BOOT0/42 NAO lidos nesta
// revisao — sem path simples/claro que nao amplie escopo; flushRegistersState
// permanece NotChecked. Documentado em GATE-A-IMPLEMENTATION.md.

#ifndef GA106LAB_FLUSH_PREWRITE_FLOW_HPP
#define GA106LAB_FLUSH_PREWRITE_FLOW_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

// Gate A pagina dedicada: mesmos constraints da selector7, objetos DISJUNTOS.
#define GA106LAB_FLUSH_PREWRITE_PAGE_SIZE 4096u
#define GA106LAB_FLUSH_PREWRITE_PAGE_ALIGN 4096u
#define GA106LAB_FLUSH_PREWRITE_SEG_LEN 4096u
#define kGA106LabFlushPrewrite_Size 4096u
#define kGA106LabFlushPrewrite_Alignment 4096u

// Encoding NV_PFB_NISO_FLUSH_* (P65/P66 reconfirmado; P67 endurecido).
static inline uint32_t GA106LabFlushEncodeLo(uint64_t dmaAddr)
{
    return (uint32_t)((dmaAddr >> 8u) & 0xFFFFFFFFu);
}
static inline uint32_t GA106LabFlushEncodeHi(uint64_t dmaAddr)
{
    return (uint32_t)((dmaAddr >> 40u) & 0xFFFFFFu);
}
static inline uint64_t GA106LabFlushDecode(uint32_t lo, uint32_t hi)
{
    return (((uint64_t)(hi & 0xFFFFFFu)) << 40u) | (((uint64_t)lo) << 8u);
}
// Alinhamento minimo HW: low 8 bits ignorados => 256 B.
static inline int GA106LabFlushAlignmentOk(uint64_t dmaAddr)
{
    return ((dmaAddr & 0xFFu) == 0u) ? 1 : 0;
}
// Representavel bits 8..63: HI cabe em 24 bits + sem overflow + roundtrip.
static inline int GA106LabFlushRepresentable(uint64_t dmaAddr)
{
    uint32_t lo;
    uint32_t hi;
    uint64_t rt;
    if (!GA106LabFlushAlignmentOk(dmaAddr)) {
        return 0;
    }
    lo = GA106LabFlushEncodeLo(dmaAddr);
    hi = GA106LabFlushEncodeHi(dmaAddr);
    rt = GA106LabFlushDecode(lo, hi);
    if (rt != dmaAddr) {
        return 0;
    }
    if (!GA106LabSysmemAddressValid(dmaAddr)) {
        return 0;
    }
    if (dmaAddr + (uint64_t)4096u < dmaAddr) {
        return 0;
    }
    return 1;
}
static inline int GA106LabFlushRoundtripOk(uint64_t dmaAddr)
{
    return GA106LabFlushRepresentable(dmaAddr);
}

// Fill ABI zero + versao/tamanho (sem memset).
static inline void GA106LabFlushPrewriteZero(GA106LabFlushPrewriteV1 &out)
{
    unsigned k = 0;
    uint8_t *b = (uint8_t *)&out;
    for (k = 0; k < sizeof(out); k++) {
        b[k] = 0;
    }
    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
}


// Teardown dedicado Gate A (zero MMIO): complete se houver objetos,
// clear, releases, reset para Unprepared. Chamador detem BUSY.
// Vive no header para que GA106Lab.cpp stop()/verify() nunca chamem
// KPI direto (contrato STOP_USES_SHARED_FLOW).
template <typename Ops>
inline void TeardownFlushPrewritePageFlow(Ops &ops)
{
    (void)ops.completeDma();
    (void)ops.clearDma();
    ops.releaseCommand();
    ops.releaseDescriptor();
    ops.clearAddressRecord();
    ops.lock();
    ops.setState(kGA106LabFlushPrewritePhase_Unprepared);
    ops.unlock();
}

#endif /* GA106LAB_FLUSH_PREWRITE_FLOW_HPP */
