// GA106LabRealOps.hpp — adaptador mínimo de PRODUÇÃO para o shared flow.
//
// Contém SOMENTE operações reais, nenhuma decisão de fluxo:
// discovery/ambiguidade/políticas vivem em GA106LabMmioFlow.hpp.
// Static dispatch (sem virtual, sem function pointers): o build de produção
// continua gerando chamadas diretas; read32() inlina para o mesmo load.
//
// Uso (GA106Lab::readPmcBoot0, wrapper fino):
//   IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, getProvider());
//   GA106LabRealOps ops(pci);
//   return RunFirstMmioReadFlow(ops, *out);

#ifndef GA106LAB_REAL_OPS_HPP
#define GA106LAB_REAL_OPS_HPP

#include <IOKit/IOReturn.h>
#include <IOKit/IOMapTypes.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>

#include "GA106LabProtocol.h"

struct GA106LabRealOps {
    IOPCIDevice *fPci;
    IOMemoryMap *fMap;
    IOVirtualAddress fVA;

    explicit GA106LabRealOps(IOPCIDevice *pci)
    : fPci(pci), fMap(NULL), fVA(0) {}

    bool isProviderPCI() { return fPci != NULL; }

    uint16_t readVendorId()
    {
        return fPci->configRead16(kIOPCIConfigVendorID);
    }
    uint16_t readDeviceId()
    {
        return fPci->configRead16(kIOPCIConfigDeviceID);
    }
    uint16_t readCommandBefore()
    {
        return fPci->configRead16(kIOPCIConfigCommand);
    }
    uint16_t readCommandAfter()
    {
        return fPci->configRead16(kIOPCIConfigCommand);
    }
    uint32_t readBar0Raw()
    {
        return fPci->configRead32(kIOPCIConfigBaseAddress0);
    }

    uint32_t getResourceCount()
    {
        return (uint32_t)fPci->getDeviceMemoryCount();
    }
    bool getResource(uint32_t idx, uint64_t &base, uint64_t &len)
    {
        IODeviceMemory *d = fPci->getDeviceMemoryWithIndex(idx);
        if (!d) {
            return false;
        }
        len = (uint64_t)d->getLength();
        base = (uint64_t)d->getPhysicalAddress();
        return true;
    }

    bool createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut)
    {
        IOMemoryMap *m = fPci->mapDeviceMemoryWithIndex(
            (unsigned int)idx, kIOMapReadOnly | kIOMapInhibitCache);
        if (!m) {
            return false;
        }
        fMap = m;
        mapLenOut = (uint64_t)m->getLength();
        mapOptsOut = (uint32_t)m->getMapOptions();
        return true;
    }

    uint64_t getVA()
    {
        if (!fMap) {
            return 0;
        }
        fVA = fMap->getVirtualAddress();
        return (uint64_t)fVA;
    }

    uint32_t mmioOffset() { return kGA106LabMmio_PmcBoot0_Offset; }
    uint32_t mmioWidth() { return kGA106LabMmio_PmcBoot0_Width; }

    // ÚNICA primitiva de leitura MMIO da produção (inlina para 1 load).
    uint32_t read32(uint32_t offset)
    {
        return OSReadLittleInt32(
            (const volatile void *)(uintptr_t)fVA, offset);
    }

    void releaseMap()
    {
        if (fMap) {
            fMap->release();
            fMap = NULL;
        }
        fVA = 0;
    }
};

#endif /* GA106LAB_REAL_OPS_HPP */
