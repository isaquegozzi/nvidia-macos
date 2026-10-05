// GA106LabRealOps.hpp — adaptador mínimo de PRODUÇÃO para os shared flows.
//
// Contém SOMENTE operações reais, nenhuma decisão de fluxo:
// discovery/ambiguidade/políticas vivem em GA106LabMmioFlow.hpp (selector 5)
// e GA106LabStaticIdentityFlow.hpp (selector 6).
// Static dispatch (sem virtual, sem function pointers): o build de produção
// continua gerando chamadas diretas; read32() inlina para loads diretos.
//
// Uso (wrappers finos em GA106Lab.cpp):
//   IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, getProvider());
//   GA106LabRealOps ops(pci);
//   return RunFirstMmioReadFlow(ops, *out); // selector 5
//   return RunStaticIdentityReadsFlow(ops, *out); // selector 6

#ifndef GA106LAB_REAL_OPS_HPP
#define GA106LAB_REAL_OPS_HPP

#include <IOKit/IOReturn.h>
#include <IOKit/IOMapTypes.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

class GA106Lab;

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

    // Allowlist fixa do selector 6 (compile-time): somente BOOT_42.
    // (BOOT_2 removido por evidência primária insuficiente.)
    uint32_t boot42Offset() { return kGA106LabStaticIdentity_Boot42_Offset; }
    uint32_t boot42Width() { return kGA106LabStaticIdentity_Boot42_Width; }

    // Primitivas de leitura MMIO da produção (inlinam para loads diretos):
    // 1 callsite em RunFirstMmioReadFlow (BOOT_0) + 1 callsite em
    // RunStaticIdentityReadsFlow (BOOT_42). Zero stores.
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

// GA106LabSysmemRealOps — adaptador de PRODUÇÃO para o shared sysmem flow
// (selector 7, GSP-DMA1). SOMENTE operações sobre o estado owner-held do
// GA106Lab; nenhuma policy (alloc-count/order/cleanup vivem no template).
// Static dispatch: chamadas diretas, sem callbacks runtime.
struct GA106LabSysmemRealOps {
    GA106Lab *fOwner;

    explicit GA106LabSysmemRealOps(GA106Lab *owner) : fOwner(owner) {}

    uint32_t getState();
    bool isBound();
    bool isPrepared();

    bool allocBuffer();
    bool zeroBuffer();
    bool createCommand();
    bool bindDescriptor(); // setMemoryDescriptor(desc,false): NUNCA auto
    bool prepareDma();     // prepare(0,4096,true,true) exatamente 1x
    bool genSegments(uint32_t &countOut, uint64_t &addrOut,
                     uint64_t &lenOut);
    bool validateAddressWidth(uint64_t addr)
    {
        return GA106LabSysmemAddressValid(addr) ? true : false;
    }
    void markAddressReady(uint64_t addr, uint32_t len);

    void completeDma(); // complete(...) — SÓ chamado se preparado
    void clearDma();    // clearMemoryDescriptor(false) — SÓ se bound
    void releaseCommand();
    void releaseDescriptor();
    void resetState();
};

#endif /* GA106LAB_REAL_OPS_HPP */
