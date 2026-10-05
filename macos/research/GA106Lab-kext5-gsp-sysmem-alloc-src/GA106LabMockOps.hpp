// GA106LabMockOps.hpp — MockOps para testes offline (mesmos shared flows).
//
// Simula hardware via fixtures. Contadores incrementados SOMENTE dentro
// das operações mock equivalentes às reais:
//   read32() -> read32Count++ (+ readOffsets[] log em ordem)
//   createMap() -> mapCreateCount++
//   releaseMap() -> mapReleaseCount++
// Testes NUNCA incrementam manualmente; medem pontos efetivos dos flows
// (selector 5 RunFirstMmioReadFlow, selector 6 RunStaticIdentityReadsFlow
// e selector 7 RunPrepareGspSysmemFlow).
//
// Static dispatch (sem virtual), mesma assinatura das RealOps.

#ifndef GA106LAB_MOCK_OPS_HPP
#define GA106LAB_MOCK_OPS_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

#define GA106LAB_MOCK_MAX_RES 8

struct GA106LabMockRes {
    uint64_t base;
    uint64_t len;
    bool present; // false = getDeviceMemoryWithIndex retorna NULL
};

struct GA106LabMockOps {
    // Fixtures
    bool providerIsPCI;
    uint16_t vendor;
    uint16_t device;
    uint16_t cmdBefore;
    uint16_t cmdAfter;
    uint32_t bar0Raw;
    GA106LabMockRes res[GA106LAB_MOCK_MAX_RES];
    uint32_t nRes;
    bool mapNull;
    uint64_t mapLen;
    uint32_t mapOpts;
    bool vaZero;
    uint32_t mmioRaw; // selector 5 (BOOT_0)
    // Selector 6 fixture (BOOT_42 @0xA00; BOOT_2 removido).
    uint32_t boot42Raw;
    // Offset/width: default fixo; testes podem injetar para alignment/bounds.
    bool useCustomOffset;
    uint32_t customOffset;
    uint32_t customWidth; // 0 = usar fixo 4
    // Overrides do selector 6 (test-only; produção sempre fixa).
    bool useCustomBoot42Offset;
    uint32_t customBoot42Offset;

    // Contadores reais (dentro das ops) + estado do mapa mockado.
    int read32Count;
    int mapCreateCount;
    int mapReleaseCount;
    int configReadCount;
    bool liveMap;
    // Ordem das leituras (para READ_ORDER_FIXED): registra offset de cada read32.
    uint32_t readOffsets[8];
    int readOffsetsCount;

    void resetCounts()
    {
        read32Count = 0;
        mapCreateCount = 0;
        mapReleaseCount = 0;
        configReadCount = 0;
        liveMap = false;
        readOffsetsCount = 0;
    }

    bool isProviderPCI() { return providerIsPCI; }

    uint16_t readVendorId()
    {
        configReadCount++;
        return vendor;
    }
    uint16_t readDeviceId()
    {
        configReadCount++;
        return device;
    }
    uint16_t readCommandBefore()
    {
        configReadCount++;
        return cmdBefore;
    }
    uint16_t readCommandAfter()
    {
        configReadCount++;
        return cmdAfter;
    }
    uint32_t readBar0Raw()
    {
        configReadCount++;
        return bar0Raw;
    }

    uint32_t getResourceCount() { return nRes; }

    bool getResource(uint32_t idx, uint64_t &base, uint64_t &len)
    {
        if (idx >= nRes || idx >= GA106LAB_MOCK_MAX_RES) {
            return false;
        }
        if (!res[idx].present) {
            return false;
        }
        base = res[idx].base;
        len = res[idx].len;
        return true;
    }

    bool createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut)
    {
        (void)idx;
        mapCreateCount++;
        if (mapNull) {
            return false;
        }
        mapLenOut = mapLen;
        mapOptsOut = mapOpts;
        liveMap = true;
        return true;
    }

    uint64_t getVA()
    {
        if (!liveMap) {
            return 0;
        }
        if (vaZero) {
            return 0;
        }
        return 0x1000u; // VA fake não-zero (nunca dereferenciado; read usa mmioRaw)
    }

    uint32_t mmioOffset()
    {
        if (useCustomOffset) {
            return customOffset;
        }
        return kGA106LabMmio_PmcBoot0_Offset;
    }
    uint32_t mmioWidth()
    {
        if (customWidth != 0) {
            return customWidth;
        }
        return kGA106LabMmio_PmcBoot0_Width;
    }

    uint32_t read32(uint32_t offset)
    {
        if (readOffsetsCount >= 0 &&
            readOffsetsCount < (int)(sizeof(readOffsets) /
                                     sizeof(readOffsets[0]))) {
            readOffsets[readOffsetsCount] = offset;
        }
        readOffsetsCount++;
        read32Count++;
        // Despacha pelo offset efetivo: BOOT_42 ou BOOT_0.
        if (offset == boot42Offset()) {
            return boot42Raw;
        }
        return mmioRaw;
    }

    uint32_t boot42Offset()
    {
        if (useCustomBoot42Offset) {
            return customBoot42Offset;
        }
        return kGA106LabStaticIdentity_Boot42_Offset;
    }
    uint32_t boot42Width() { return kGA106LabStaticIdentity_Boot42_Width; }

    void releaseMap()
    {
        mapReleaseCount++;
        liveMap = false;
    }
};

// Fixture live real: BAR0 16M + BAR1 256M + BAR3 32M + ROM 512K + I/O + sub-range.
inline void GA106LabMockMakeLive(GA106LabMockOps &o)
{
    unsigned i = 0;
    for (i = 0; i < GA106LAB_MOCK_MAX_RES; i++) {
        o.res[i].base = 0;
        o.res[i].len = 0;
        o.res[i].present = false;
    }
    o.providerIsPCI = true;
    o.vendor = 0x10DEu;
    o.device = 0x2504u;
    o.cmdBefore = 0x0003u;
    o.cmdAfter = 0x0003u;
    o.bar0Raw = 0xFB000000u;
    o.nRes = 6;
    o.res[0].present = true;
    o.res[0].base = 0xFB000000ULL;
    o.res[0].len = 0x1000000ULL;
    o.res[1].present = true;
    o.res[1].base = 0x440000000ULL;
    o.res[1].len = 0x10000000ULL;
    o.res[2].present = true;
    o.res[2].base = 0x450000000ULL;
    o.res[2].len = 0x2000000ULL;
    o.res[3].present = true;
    o.res[3].base = 0xFC000000ULL;
    o.res[3].len = 0x80000ULL;
    o.res[4].present = true;
    o.res[4].base = 0x1001ULL;
    o.res[4].len = 0x100ULL;
    o.res[5].present = true;
    o.res[5].base = 0x0ULL;
    o.res[5].len = 0x1000ULL;
    o.mapNull = false;
    o.mapLen = 0x1000000ULL;
    o.mapOpts = 0x1100u;
    o.vaZero = false;
    o.mmioRaw = 0x176000A1u;
    o.boot42Raw = 0x176000A1u; // CHIP_ID 10-bit 0x176 (máscara 0x3FF00000)
    o.useCustomOffset = false;
    o.customOffset = 0;
    o.customWidth = 0;
    o.useCustomBoot42Offset = false;
    o.customBoot42Offset = 0;
    o.resetCounts();
}

// GA106LabSysmemMockOps — MockOps do selector 7 (mesmo shared flow da
// produção). Fixtures por ponto de falha + counters dentro das ops.
// mmio/config/BME/DMA counters existem e devem permanecer 0 (assert).
struct GA106LabSysmemMockOps {
    // Fixtures de falha (false = sucesso).
    bool allocFail;
    bool bytesNull;
    bool cmdFail;
    bool bindFail;
    bool prepareFail;
    // Segmentos: 0=ok(1x4096 alinhado), 1=zero, 2=multi, 3=curto,
    // 4=misaligned, 5=width-overflow.
    int segMode;

    // Estado owner-held espelhado.
    uint32_t state;
    bool bound;
    bool prepared;
    bool descExists;
    bool cmdExists;
    uint64_t dmaAddr;
    uint32_t segLen;

    // Counters efetivos (só dentro das ops).
    int allocCount;
    int zeroCount;
    int commandCreateCount;
    int setDescriptorCount;
    int prepareCount;
    int segmentGenCount;
    int completeCount;
    int clearCount;
    int descriptorReleaseCount;
    int commandReleaseCount;
    // Devem permanecer 0 nesta microfase:
    int mmioReadCount;
    int mmioWriteCount;
    int configWriteCount;
    int bmeEnableCount;
    int dmaTriggerCount;

    void resetCounts()
    {
        allocCount = 0;
        zeroCount = 0;
        commandCreateCount = 0;
        setDescriptorCount = 0;
        prepareCount = 0;
        segmentGenCount = 0;
        completeCount = 0;
        clearCount = 0;
        descriptorReleaseCount = 0;
        commandReleaseCount = 0;
        mmioReadCount = 0;
        mmioWriteCount = 0;
        configWriteCount = 0;
        bmeEnableCount = 0;
        dmaTriggerCount = 0;
    }

    uint32_t getState() { return state; }
    bool isBound() { return bound; }
    bool isPrepared() { return prepared; }

    bool allocBuffer()
    {
        allocCount++;
        if (allocFail) {
            return false;
        }
        descExists = true;
        state = kGA106LabGspSysmemState_Allocated;
        return true;
    }
    bool zeroBuffer()
    {
        zeroCount++;
        if (!descExists || bytesNull) {
            return false;
        }
        return true;
    }
    bool createCommand()
    {
        commandCreateCount++;
        if (cmdFail || !descExists) {
            return false;
        }
        cmdExists = true;
        return true;
    }
    bool bindDescriptor()
    {
        setDescriptorCount++;
        if (bindFail || !descExists || !cmdExists) {
            return false;
        }
        bound = true;
        return true;
    }
    bool prepareDma()
    {
        prepareCount++;
        if (prepareFail || !bound) {
            return false;
        }
        prepared = true;
        state = kGA106LabGspSysmemState_Prepared;
        return true;
    }
    bool genSegments(uint32_t &countOut, uint64_t &addrOut,
                     uint64_t &lenOut)
    {
        segmentGenCount++;
        countOut = 0;
        addrOut = 0;
        lenOut = 0;
        if (!prepared) {
            return false;
        }
        switch (segMode) {
        case 0:
            countOut = 1;
            addrOut = 0x1000u;
            lenOut = 4096u;
            return true;
        case 1:
            countOut = 0;
            return true;
        case 2:
            countOut = 2;
            addrOut = 0x1000u;
            lenOut = 4096u;
            return true;
        case 3:
            countOut = 1;
            addrOut = 0x1000u;
            lenOut = 2048u;
            return true;
        case 4:
            countOut = 1;
            addrOut = 0x1001u;
            lenOut = 4096u;
            return true;
        case 5:
        default:
            countOut = 1;
            addrOut = 0xFFFFFFFFFFFFF000ULL; // +4096 faz wrap.
            lenOut = 4096u;
            return true;
        }
    }
    bool validateAddressWidth(uint64_t addr)
    {
        return GA106LabSysmemAddressValid(addr) ? true : false;
    }
    void markAddressReady(uint64_t addr, uint32_t len)
    {
        dmaAddr = addr;
        segLen = len;
        state = kGA106LabGspSysmemState_AddressReady;
    }
    void completeDma()
    {
        if (!prepared) {
            return;
        }
        completeCount++;
        prepared = false;
    }
    void clearDma()
    {
        if (!bound) {
            return;
        }
        clearCount++;
        bound = false;
    }
    void releaseCommand()
    {
        if (!cmdExists) {
            return;
        }
        commandReleaseCount++;
        cmdExists = false;
    }
    void releaseDescriptor()
    {
        if (!descExists) {
            return;
        }
        descriptorReleaseCount++;
        descExists = false;
    }
    void resetState()
    {
        dmaAddr = 0;
        segLen = 0;
        state = kGA106LabGspSysmemState_Empty;
        bound = false;
        prepared = false;
    }
};

inline void GA106LabSysmemMockMakeLive(GA106LabSysmemMockOps &o)
{
    o.allocFail = false;
    o.bytesNull = false;
    o.cmdFail = false;
    o.bindFail = false;
    o.prepareFail = false;
    o.segMode = 0;
    o.state = kGA106LabGspSysmemState_Empty;
    o.bound = false;
    o.prepared = false;
    o.descExists = false;
    o.cmdExists = false;
    o.dmaAddr = 0;
    o.segLen = 0;
    o.resetCounts();
}

#endif /* GA106LAB_MOCK_OPS_HPP */
