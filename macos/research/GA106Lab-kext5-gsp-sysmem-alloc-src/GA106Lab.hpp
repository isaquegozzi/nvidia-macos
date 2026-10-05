//
// GA106Lab.hpp — KEXT1: serviço + cache imutável + newUserClient root-only.
// ATTACH_ONLY preservado. Sem mappings, DMA, interrupts, timers, workloops.
//

#ifndef GA106LAB_HPP
#define GA106LAB_HPP

#include <IOKit/IOService.h>

#include "GA106LabProtocol.h"

class GA106LabUserClient;

class IOBufferMemoryDescriptor;
class IODMACommand;

struct GA106LabSysmemRealOps;

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
    friend struct GA106LabSysmemRealOps;

private:
    GA106LabCachedState fCache;
    bool                fCacheValid;
    GA106LabUserClient * fActiveClient; // NÃO retido (evita ciclo); limpo em close.
    IOSimpleLock *      fSlotLock;      // protege check+reserva do slot (spin, curto).

    // Estado sysmem GSP (selector 7, KEXT5): owner é este serviço.
    // UserClient NUNCA guarda estes objetos/endereços.
    IOBufferMemoryDescriptor * fSysmemDesc; // owned; release no cleanup/stop
    IODMACommand *             fSysmemCmd;  // owned; release no cleanup/stop
    uint64_t                   fSysmemDmaAddr; // kernel-only; NUNCA na ABI
    uint32_t                   fSysmemSegLen;  // 4096 em ADDRESS_READY
    uint32_t                   fSysmemState;   // kGA106LabGspSysmemState_*
    bool                       fSysmemBound;    // setMemoryDescriptor ok
    bool                       fSysmemPrepared; // prepare ok (mapping vivo)

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

    // Identidade estática (selector 6, TG-KEXT4-B): UMA leitura 32-bit
    // semanticamente fixa (BOOT_42 @0xA00; BOOT_2 removido). Um mapa
    // RO+Inhibit, 1 release. NUNCA escreve. NÃO relê BOOT_0.
    IOReturn readStaticIdentity(GA106LabStaticIdentityV1 * out);

    // Sysmem GSP (selector 7, KEXT5 GSP-DMA1): aloca 4 KiB, zera, binda
    // IODMACommand (autoPrepare=false), prepare explícito 1x, resolve
    // exatamente 1 segmento DMA, mantém mapping vivo no owner.
    // Zero MMIO, zero PCI config writes, BME OFF, zero DMA trigger.
    // Segunda chamada retorna ALREADY_PREPARED sem realocar.
    IOReturn prepareGspSysmem(GA106LabGspSysmemV1 * out);

    // Cleanup centralizado do sysmem (stop + falhas internas).
    // Respeita estado real: complete só se preparado; clear só se bound;
    // releases só do que existe; nunca duplo. ClientClose NÃO libera.
    void releaseGspSysmem(void);
};

#endif /* GA106LAB_HPP */
