// GA106LabMockOps.hpp — MockOps para testes offline (mesmo shared flow).
//
// Simula hardware via fixtures. Contadores incrementados SOMENTE dentro
// das operações mock equivalentes às reais:
//   read32() -> read32Count++
//   createMap() -> mapCreateCount++
//   releaseMap() -> mapReleaseCount++
// Testes NUNCA incrementam manualmente; medem pontos efetivos do flow.
//
// Static dispatch (sem virtual), mesma assinatura de GA106LabRealOps.

#ifndef GA106LAB_MOCK_OPS_HPP
#define GA106LAB_MOCK_OPS_HPP

#include "GA106LabProtocol.h"

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
    uint32_t mmioRaw;
    // Offset/width: default fixo; testes podem injetar para alignment/bounds.
    bool useCustomOffset;
    uint32_t customOffset;
    uint32_t customWidth; // 0 = usar fixo 4

    // Contadores reais (dentro das ops) + estado do mapa mockado.
    int read32Count;
    int mapCreateCount;
    int mapReleaseCount;
    int configReadCount;
    bool liveMap;

    void resetCounts()
    {
        read32Count = 0;
        mapCreateCount = 0;
        mapReleaseCount = 0;
        configReadCount = 0;
        liveMap = false;
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
        (void)offset;
        read32Count++;
        return mmioRaw;
    }

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
    o.useCustomOffset = false;
    o.customOffset = 0;
    o.customWidth = 0;
    o.resetCounts();
}

#endif /* GA106LAB_MOCK_OPS_HPP */
