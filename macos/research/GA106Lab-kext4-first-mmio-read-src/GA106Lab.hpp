//
// GA106Lab.hpp — KEXT1: serviço + cache imutável + newUserClient root-only.
// ATTACH_ONLY preservado. Sem mappings, DMA, interrupts, timers, workloops.
//

#ifndef GA106LAB_HPP
#define GA106LAB_HPP

#include <IOKit/IOService.h>

#include "GA106LabProtocol.h"

class GA106LabUserClient;

// Cache preenchido UMA vez em start(); lido pelos selectors (sem polling).
struct GA106LabCachedState {
    uint32_t vendor;           // 0x10de
    uint32_t device;           // 0x2504
    uint64_t providerEntryID;
    uint32_t state;            // kGA106LabState_*
    uint32_t providerConfirmed;// 0/1
};

class GA106Lab : public IOService
{
    OSDeclareDefaultStructors(GA106Lab);

private:
    GA106LabCachedState fCache;
    bool                fCacheValid;
    GA106LabUserClient * fActiveClient; // NÃO retido (evita ciclo); limpo em close.
    IOSimpleLock *      fSlotLock;      // protege check+reserva do slot (spin, curto).

public:
    bool init(OSDictionary * dictionary = 0) override;
    void free() override;
    bool start(IOService * provider) override;
    void stop(IOService * provider) override;

    // IOUserClient (KEXT1): root-only + cliente único. Sem acesso ao PCI aqui.
    IOReturn newUserClient(task_t owningTask, void * securityID, UInt32 type,
                           OSDictionary * properties,
                           IOUserClient ** handler) override;
    void clientClosed(GA106LabUserClient * client);

    // Leitura do cache por valor (cópia; sem expor ponteiros internos).
    bool copyCachedState(GA106LabCachedState * out) const;

    // Snapshot PCI config read-only (sequência FIXA; sem offset de fora).
    // Retorna Success / NotReady (sem provider/dispositivo) /
    // NoDevice (acessível mas não é 10de:2504) / BadArgument (out nulo).
    IOReturn copyPciSnapshot(GA106LabPciSnapshotV1 * out);

    // Probe BAR0 (selector 4, TG-KEXT3): descobre o resource BAR0 por
    // validação runtime (sem index cego), cria mapa RO+InhibitCache,
    // valida metadata, preenche ABI, libera o mapa antes de retornar.
    // NUNCA dereferencia MMIO. Retorna Success só com RELEASE_CONFIRMED;
    // failure em qualquer divergência (sem retry interno).
    IOReturn probeBar0Map(GA106LabBarMapInfoV1 * out);

    // Primeira leitura MMIO (selector 5, TG-KEXT4): UMA leitura 32-bit
    // semanticamente fixa de NV_PMC_BOOT_0 (BAR0+0, via OSReadLittleInt32).
    // Reusa discovery/map RO+Inhibit do probe; valida chip-id 0x176;
    // desmapeia antes de retornar. NUNCA escreve (MMIO nem config).
    IOReturn readPmcBoot0(GA106LabMmioReadV1 * out);
};

#endif /* GA106LAB_HPP */
