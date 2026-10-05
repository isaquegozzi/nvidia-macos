//
// GA106Lab.hpp — KEXT1: serviço + cache imutável + newUserClient root-only.
// ATTACH_ONLY preservado. Sem mappings, DMA, interrupts, timers, workloops.
//

#ifndef GA106LAB_HPP
#define GA106LAB_HPP

#include <IOKit/IOService.h>

#include "GA106LabProtocol.h"
#include "GA106LabProductionPhase.h"
#include "GA106LabHardwareBlocks.h"
#include "GA106LabModelContracts.h"

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
    friend struct GA106LabOwnerRealOps;
    friend struct GA106LabGfwRealOps;
    friend struct GA106LabFlushRealOps;
    friend struct GA106LabGateBWriteRealOps;
    friend struct GA106LabBmeRealOps;
    friend struct GA106LabR3RealOps;

private:
    GA106LabCachedState fCache;
    bool                fCacheValid;
    GA106LabUserClient * fActiveClient; // NÃO retido (evita ciclo); limpo em close.
    IOSimpleLock *      fSlotLock;      // protege check+reserva do slot (spin, curto).

    // P80 production architecture (offline): fase explícita + latch de bloco
    // live + contratos de modelo (puros, sem HW). fSlotLock protege fase em
    // seções curtas (mesma disciplina das flags busy/stopping).
    uint32_t                fProductionPhase;   // GA106LabProductionPhase
    GA106LabBlockLog        fBlockLog;          // tentativas de op real (todas fail-closed)
    GA106LabChannelContract fChannelContract;   // válido se fChannelContractOk
    bool                    fChannelContractOk;
    GA106LabVmContract      fVmContract;        // válido se fVmContractOk
    bool                    fVmContractOk;

    // Estado sysmem GSP (selector 7, KEXT5): owner é este serviço.
    // UserClient NUNCA guarda estes objetos/endereços.
    // Disciplina (FIX-ASTRA-C): fSysmemBusy é a exclusão — só o detentor
    // toca nos objetos; fSysmemState é a fase; fSysmemStopping é terminal
    // (setado por stop(), nunca limpo). Locks: seções curtas, NUNCA
    // blocking-KPI sob spinlock.
    IOBufferMemoryDescriptor * fSysmemDesc; // owned; release no cleanup/stop
    IODMACommand *             fSysmemCmd;  // owned; release no cleanup/stop
    uint64_t                   fSysmemDmaAddr; // kernel-only; NUNCA na ABI
    uint32_t                   fSysmemSegLen;  // 4096 em ADDRESS_READY
    uint32_t                   fSysmemState;   // fases internas (flow header)
    bool                       fSysmemBusy;     // reserva/IN_PROGRESS
    bool                       fSysmemStopping; // stop() iniciou; terminal
    IOSimpleLock *             fSysmemLock;   // guarda busy/state/stopping

    // Exclusão do selector 8 GFW readiness (KEXT6): vars DISJOINTAS de
    // fSysmemBusy/fSysmemStopping, sob a MESMA trava em seções curtas
    // (sem interferência com selector 7). Mapa do poll é local por chamada.
    bool                       fGfwBusy;      // reserva/IN_PROGRESS do selector 8
    bool                       fGfwStopping;  // stop() iniciou; terminal

    // Gate A flush-prewrite page dedicada (selector 9, TG-KEXT7): objetos
    // DISJUNTOS de fSysmem* (sem reuso de selector7, sem dependencia runtime).
    // Mesma disciplina FIX-ASTRA-C: fFlushBusy e exclusao, fFlushState fase,
    // fFlushStopping terminal. Trava compartilhada fSysmemLock em secoes curtas.
    IOBufferMemoryDescriptor * fFlushDesc;
    IODMACommand *             fFlushCmd;
    uint64_t                   fFlushDmaAddr; // kernel-only; NUNCA na ABI
    uint32_t                   fFlushSegLen;  // 4096 em AddressReady/PreconditionsReady
    uint32_t                   fFlushState;   // 0 Unprepared, 1 AddressReady, 2 PreconditionsReady, 10+ internas
    bool                       fFlushBusy;
    bool                       fFlushStopping;

    // Latch one-way do Gate B (selector 10, PROGRAM_SYSMEM_FLUSH): service-owned,
    // sob fSysmemLock em seções curtas. init() = NeverTouched. stop()/free()/
    // clientClose() NUNCA limpam (só reboot recria o serviço). fGateBBusy é a
    // exclusão do Gate B (disjunta das demais, mesma trava).
    uint32_t                   fGateBLatch;   // kGateBLatch_* (NeverTouched no init)
    bool                       fGateBBusy;    // reserva/IN_PROGRESS do selector 10

    // Boot binding + BME (selector 11, ENABLE_BUS_MASTER): nonces de
    // mach_absolute_time (monotônico; zera no reboot; fora do userspace).
    // fBootNonce fixado no init; Gate A/B gravam (nonce,dma) no sucesso dos
    // wrappers; fBmeLatch/fBmeBusy exclusão do selector 11. Tudo sob
    // fSysmemLock em seções curtas. NUNCA limpo exceto por objeto novo.
    uint64_t                   fBootNonce;    // init(): mach_absolute_time()
    uint64_t                   fGateABootNonce; // wrapper sel9 em Success (+fGateADma)
    uint64_t                   fGateADma;     // IOVA kernel-only; NUNCA na ABI
    uint64_t                   fGateBBootNonce; // wrapper sel10 em Success (+fGateBDma)
    uint64_t                   fGateBDma;     // IOVA kernel-only; NUNCA na ABI
    uint32_t                   fBmeLatch;     // kBmeLatch_* (Disabled no init)
    bool                       fBmeBusy;      // reserva/IN_PROGRESS do selector 11

    // R3 probes (selectors 12/13/14, revF): exclusão + invoked-mask sob
    // fSysmemLock em seções curtas. Latches reboot-only (nunca limpos).
    bool                       fR3Busy;       // reserva única dos 3 probes
    bool                       fR3Stopping;   // terminal setado por stop()
    uint32_t                   fR3InvokedMask; // bit (sel-12): 1x cada probe

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

    // Gate A prewrite (selector 9, KEXT7 VERIFY_SYSMEM_FLUSH_PRECONDITIONS):
    // validacao read-only das precondicoes do futuro Gate B. Zero MMIO stores,
    // zero PCI writes, zero BME, zero DMA trigger, zero Falcon/GSP/firmware.
    // Pagina/mapping dedicados persistem no owner apos sucesso. Diagnosticos
    // HI/LO/WPR2/BOOT omitidos nesta revisao (NotChecked). Reutilizavel:
    // segunda chamada revalida e retorna AlreadyReady sem realocar.
    IOReturn verifySysmemFlushPreconditions(GA106LabFlushPrewriteV1 * out);

    // GFW boot readiness (selector 8, KEXT6 GSP-GFW-BOOT-READINESS): verifica
    // o estado pré-existente de firmware/boot (gate @0x118128 + progresso
    // @0x118234, polling bounded 1ms/4s) em mapa BAR0 RO+Inhibit local.
    // Zero stores; nunca chama selector 7; mapping sysmem (se existir) intocado.
    IOReturn readGfwBootReadiness(GA106LabGfwReadinessV1 * out);

    // Gate B program (selector 10, PROGRAM_SYSMEM_FLUSH): escreve o endereço
    // sysmem validado em Gate A nos registradores fixos HI @0x100c40 + LO
    // @0x100c10 (IMPLEMENTATION_ORDERING), em mapa BAR0 WRITABLE+Inhibit (UC)
    // local por chamada. Exatamente 2 stores, sem readback automático, sem
    // retry, sem BME/PCI/DMA/GSP. Latch one-way fGateBLatch (nunca limpo
    // exceto por reboot). Sem reset/teardown com stores no live path.
    IOReturn programSysmemFlush(GA106LabGateBProgramV1 * out);

    // BME enable (selector 11, ENABLE_BUS_MASTER): RMW 16-bit fixo que liga
    // SOMENTE o bit BusLead (0x4) do PCI COMMAND, preservando demais bits.
    // Zero-input; sem DMA kick/GSP; latch BME separado reboot-only.
    IOReturn enableBusMaster(GA106LabBmeEnableV1 * out);

    // R3 probes (selectors 12/13/14, revF, READ-ONLY): Falcon state, mapping
    // contract (metadata-only, zero IOVAs), quiescence. Zero-input; só
    // predicados/enums; nunca endereços; launchAttempted sempre 0.
    IOReturn probeR3FalconState(GA106LabR3FalconStateV1 * out);
    IOReturn probeR3MappingContract(GA106LabR3MappingContractV1 * out);
    IOReturn probeR3Quiescence(GA106LabR3QuiescenceV1 * out);

    // P80: fase de produção (somente leitura) + prepares de modelo offline
    // (validam contratos, nunca tocam HW) + latch fail-closed de operação real.
    uint32_t productionPhase(void) const;
    IOReturn prepareChannelModel(const GA106LabChannelContract * in);
    IOReturn prepareVmModel(const GA106LabVmContract * in);
    IOReturn prepareSubmissionModel(const GA106LabSubmissionContract * in,
                                    uint64_t pbBase, uint64_t pbLength);
    IOReturn blockRealOperation(uint32_t op);
    const GA106LabBlockLog * blockLog(void) const;

private:
    // Helpers P80 (fSlotLock, seções curtas; nunca blocking-KPI).
    bool noteProductionMilestone(uint32_t to);
    void resetProductionPhaseForTeardown(void);

public:

    // Cleanup centralizado do sysmem vive no template
    // ReleaseGspSysmemFlow (stop + falhas internas): complete 1x se
    // prepare tentado, clear 1x se bound com retorno checado
    // (falha => CLEANUP_FAILED sem releases), releases null-safe.
    // ClientClose NÃO libera. free() só asserta + soltura defensiva.
};

#endif /* GA106LAB_HPP */
