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

struct GA106LabOwnerRealOps;
struct GA106LabGfwRealOps;

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
// (selector 7, GSP-DMA1, FIX-ASTRA-C). SOMENTE operações sobre o estado
// owner-held do GA106Lab; nenhuma policy (fases/order/cleanup vivem no
// template). Static dispatch: chamadas diretas, sem callbacks runtime.
// Nenhum método bloqueia sob spinlock (o template tranca só seções curtas).
struct GA106LabSysmemRealOps {
    GA106Lab *fOwner;

    explicit GA106LabSysmemRealOps(GA106Lab *owner) : fOwner(owner) {}

    void lock();
    void unlock();

    uint32_t getState();
    void setState(uint32_t s);
    bool isStopping();
    void setStopping(bool b);
    bool isBusy();
    void setBusy(bool b);
    void waitMs(unsigned ms); // IODelay; fora de qualquer lock.

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

    IOReturn completeDma(); // retorno REAL do KPI (teardown decide)
    IOReturn clearDma();    // retorno REAL; falha => sem releases
    void releaseCommand();  // null-safe
    void releaseDescriptor(); // null-safe
    void clearAddressRecord();
};

// GA106LabOwnerRealOps — adaptador de PRODUÇÃO para o shared init/free/lock
// flow (TEST-PATH-FIX). SOMENTE operações brutas sobre o owner GA106Lab;
// TODA policy (ordem, rollback, guards, fase terminal) vive em
// GA106LabInitFreeLockFlow.hpp. Métodos implementados em GA106Lab.cpp.
// POLICY_IN_OPS = NO por construção (sem if de policy aqui).
struct GA106LabOwnerRealOps {
    GA106Lab *fOwner;

    explicit GA106LabOwnerRealOps(GA106Lab *owner) : fOwner(owner) {}

    // Locks (raw, sem policy).
    void initNullLocks();
    bool allocFirstLockRaw();
    bool allocSecondLockRaw();
    void *getFirstLock();
    void *getSecondLock();
    void freeLockRaw(void *lock);
    void clearFirstLock();
    void clearSecondLock();

    // Free resources (raw).
    uint32_t getState();
    bool hasCommand();
    bool hasDescriptor();
    void releaseCommandRaw();
    void releaseDescriptorRaw();
    void logCleanupFailedLeakSafe();
    void logUnexpectedStateInFree();
};

// GA106LabGfwRealOps — adaptador de PRODUÇÃO para o shared GFW readiness
// flow (selector 8, KEXT6). SOMENTE operações brutas: estado fGfwBusy/
// fGfwStopping do owner (seções curtas sob fSysmemLock) + primitivas MMIO
// BAR0 (delegadas ao membro GA106LabRealOps, mesma descoberta/mapa RO+
// Inhibit dos selectors 5/6). TODA policy (gate/progress/poll/bounds)
// vive em GA106LabGfwReadinessFlow.hpp. Nenhum método bloqueia sob spinlock
// (waitMs roda destravado). Sem selector 7, sem DMA, sem firmware.
struct GA106LabGfwRealOps {
    GA106Lab *fOwner;
    GA106LabRealOps fMmio;
    IOPCIDevice *fRetPci; // provider retido entre acquire/release (pin).

    explicit GA106LabGfwRealOps(GA106Lab *owner)
    : fOwner(owner), fMmio(NULL), fRetPci(NULL) {}

    // Exclusão / stopping (raw; policy no template).
    void lock();
    void unlock();
    bool isStopping();
    void setStopping(bool b);
    bool isBusy();
    void setBusy(bool b);
    // Relógio monotônico real (mach_absolute_time; nunca wall clock).
    uint64_t nowNs();
    // Espera orçada (<= ns) fora de qualquer lock; ns==0 => retorno imediato.
    void waitBudgetedNs(uint64_t ns);
    void waitMs(unsigned ms); // IODelay; fora de qualquer lock (stop-flow).
    // Aquisição com pin de lifetime (retain balanceado; policy no template).
    bool acquireProvider();
    void releaseProvider();

    // MMIO BAR0 (raw; delegam ao membro fMmio, vinculado ao retido).
    uint16_t readVendorId();
    uint16_t readDeviceId();
    uint16_t readCommandBefore();
    uint32_t readBar0Raw();
    uint32_t getResourceCount();
    bool getResource(uint32_t idx, uint64_t &base, uint64_t &len);
    bool createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut);
    uint64_t getVA();
    uint32_t read32(uint32_t offset); // ÚNICA primitiva de leitura; zero stores.
    void releaseMap();
};


// GA106LabFlushRealOps — adaptador de PRODUCAO para pagina DEDICADA do Gate A
// (selector 9). Mesma disciplina do SysmemRealOps, estado DISJUNTO fFlush*.
// SOMENTE operacoes; policy no metodo verifySysmemFlushPreconditions.
// Reusa RunPrepareGspSysmemFlow<Ops> com este Ops para obter 1x4096 valido.
struct GA106LabFlushRealOps {
    GA106Lab *fOwner;
    explicit GA106LabFlushRealOps(GA106Lab *owner) : fOwner(owner) {}
    void lock();
    void unlock();
    uint32_t getState();
    void setState(uint32_t s);
    bool isStopping();
    void setStopping(bool b);
    bool isBusy();
    void setBusy(bool b);
    void waitMs(unsigned ms);
    bool allocBuffer();
    bool zeroBuffer();
    bool createCommand();
    bool bindDescriptor();
    bool prepareDma();
    bool genSegments(uint32_t &countOut, uint64_t &addrOut, uint64_t &lenOut);
    bool validateAddressWidth(uint64_t addr)
    {
        return GA106LabSysmemAddressValid(addr) ? true : false;
    }
    void markAddressReady(uint64_t addr, uint32_t len);
    IOReturn completeDma();
    IOReturn clearDma();
    void releaseCommand();
    void releaseDescriptor();
    void clearAddressRecord();
    bool hasPageObjects();
    bool getStoredPage(uint64_t &addrOut, uint32_t &lenOut);
};

#endif /* GA106LAB_REAL_OPS_HPP */
