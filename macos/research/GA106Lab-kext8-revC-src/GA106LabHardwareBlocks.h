//
// GA106LabHardwareBlocks.h — P80 §14: stubs fail-closed para toda operação
// sensível futura. Cada stub: sem side effect, retorna kIOReturnNotPermitted,
// incrementa o contador de tentativas (latch para LiveBlocked). Determinístico.
// Kernel + host-test compatível (só IOReturn; sem objetos IOKit).
// AUDIT: este header NÃO pode conter símbolos de HW (ver build.sh
// HARDWARE_BLOCKS_ZERO_HW_SOURCE). Stubs nunca chamam RealOps.
//

#ifndef GA106LAB_HARDWARE_BLOCKS_H
#define GA106LAB_HARDWARE_BLOCKS_H

#include <IOKit/IOReturn.h>
#include <stdint.h>

struct GA106LabBlockLog {
    uint32_t mmioWrite;
    uint32_t pciConfigWrite;
    uint32_t bmeEnable;
    uint32_t realDma;
    uint32_t gspLive;
    uint32_t firmwareUpload;
    uint32_t vramWrite;
    uint32_t interruptEnable;
    uint32_t reset;
    uint32_t realGpfifoKick;
    uint32_t realPutWrite;
    uint32_t realDoorbell;
    uint32_t realCommandSubmission;
};

inline void GA106LabBlockLogInit(GA106LabBlockLog * log) {
    if (!log) return;
    log->mmioWrite = 0; log->pciConfigWrite = 0; log->bmeEnable = 0;
    log->realDma = 0; log->gspLive = 0; log->firmwareUpload = 0;
    log->vramWrite = 0; log->interruptEnable = 0; log->reset = 0;
    log->realGpfifoKick = 0; log->realPutWrite = 0; log->realDoorbell = 0;
    log->realCommandSubmission = 0;
}

inline uint32_t GA106LabBlockLogTotal(const GA106LabBlockLog * log) {
    if (!log) return 0;
    return log->mmioWrite + log->pciConfigWrite + log->bmeEnable + log->realDma +
           log->gspLive + log->firmwareUpload + log->vramWrite + log->interruptEnable +
           log->reset + log->realGpfifoKick + log->realPutWrite + log->realDoorbell +
           log->realCommandSubmission;
}

// Cada stub abaixo: ++contador, retorna NotPermitted. Nada mais.
inline IOReturn GA106LabBlockMmioWrite(GA106LabBlockLog * log) {
    if (log) log->mmioWrite++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockPciConfigWrite(GA106LabBlockLog * log) {
    if (log) log->pciConfigWrite++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockBmeEnable(GA106LabBlockLog * log) {
    if (log) log->bmeEnable++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockRealDma(GA106LabBlockLog * log) {
    if (log) log->realDma++;
#ifdef PROD_MUT_BLOCK_RETURNS_SUCCESS
    return kIOReturnSuccess; // MUTATION TEST-ONLY
#endif
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockGspLive(GA106LabBlockLog * log) {
    if (log) log->gspLive++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockFirmwareUpload(GA106LabBlockLog * log) {
    if (log) log->firmwareUpload++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockVramWrite(GA106LabBlockLog * log) {
    if (log) log->vramWrite++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockInterruptEnable(GA106LabBlockLog * log) {
    if (log) log->interruptEnable++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockReset(GA106LabBlockLog * log) {
    if (log) log->reset++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockRealGpfifoKick(GA106LabBlockLog * log) {
    if (log) log->realGpfifoKick++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockRealPutWrite(GA106LabBlockLog * log) {
    if (log) log->realPutWrite++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockRealDoorbell(GA106LabBlockLog * log) {
    if (log) log->realDoorbell++;
    return kIOReturnNotPermitted;
}
inline IOReturn GA106LabBlockRealCommandSubmission(GA106LabBlockLog * log) {
    if (log) log->realCommandSubmission++;
    return kIOReturnNotPermitted;
}

#endif /* GA106LAB_HARDWARE_BLOCKS_H */
