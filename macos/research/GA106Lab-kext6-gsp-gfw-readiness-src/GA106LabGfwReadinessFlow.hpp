// GA106LabGfwReadinessFlow.hpp — ONE_SHARED_GFW_READINESS_FLOW (KEXT6).
//
// Único fluxo de policy do selector 8 READ_GFW_BOOT_READINESS.
// Produção (GfwRealOps) e testes (GfwMockOps) instanciam O MESMO template
// RunGfwBootReadinessFlow<Ops>. Static dispatch (sem virtual, sem callbacks
// runtime no ABI/UserClient).
//
// Escopo: verificar o estado pré-existente de firmware/boot da GPU (GFW_BOOT)
// por dois registradores fixos (Nova/OpenRM, reconfirmado PROMPT 52):
//   gate @0x118128 bit 0 (PRIV_LEVEL_MASK.read_protection_level0) — gate de
//     ACESSO, não readiness. Se 0, o registrador de progresso não tem validade.
//   progress @0x118234 bits 7:0 — 0xff = completed; demais = not complete yet
//     (sem estados de erro inventados).
// Polling bounded 1ms/4s (4000 iterações) DENTRO de uma única chamada.
// Somente leituras 32-bit (gate <= 4000, progress <= 4000, total <= 8000).
// Zero stores de qualquer tipo; zero PCI/BME/DMA/GSP/firmware; zero selector 7.
//
// Ordem (reserva primeiro, como sysmem; mapa local por chamada):
//   provider -> vendor/device -> MSE -> BAR0 raw -> base ->
//   reserve BUSY (segunda => Busy; stopping => Aborted) ->
//   discovery (count, match len+base, 0=>FAIL, >=2=>FAIL, 1=>continue) ->
//   resLen/base revalidação -> createMap RO+Inhibit -> mapLen full-resource ->
//   cache EXATA (RO + campo==Inhibit) -> VA -> bounds 0x118234+4 ->
//   poll { stop-check; gate read; [progress read]; completed?; wait 1ms } ->
//   fill ABI fixa -> releaseMap + libera reserva -> return.
// Timeout é resultado semântico válido (status TimedOut, kIOReturnSuccess);
// Aborted/MapFailed/Unavailable retornam códigos de erro.
//
// Disciplina de lock: o template chama ops.lock()/unlock() SOMENTE em seções
// curtas (reserve, stop-check); waitMs e leituras rodam DESTRAVADOS. A exclusão
// é o bit fGfwBusy do owner (disjointo de fSysmemBusy; mesma trava, vars
// separadas, sem interferência). stop() espera !BUSY via WaitForGfwIdleFlow
// (poll 1ms, sem lock retido); o mapa é local-por-chamada, logo stop nunca
// libera recurso em uso pelo poll (sem UAF por construção).
//
// Ops deve prover (RealOps = só operações; MockOps = fixtures/counters):
//   lock(); unlock();                       // seções curtas; NUNCA bloquear dentro
//   bool isStopping(); void setStopping(bool);
//   bool isBusy(); void setBusy(bool);
//   void waitMs(unsigned ms);               // IODelay (Real) / contado (Mock)
//   bool isProviderPCI();
//   uint16_t readVendorId(); uint16_t readDeviceId();
//   uint16_t readCommandBefore();
//   uint32_t readBar0Raw();
//   uint32_t getResourceCount();
//   bool     getResource(uint32_t idx, uint64_t &base, uint64_t &len);
//   bool     createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut);
//   uint64_t getVA(); // 0 = falha
//   uint32_t read32(uint32_t offset); // ÚNICA primitiva de leitura
//   void     releaseMap();
// Nenhuma decisão (discovery/ordem/gate/poll/bounds) vive nos Ops.

#ifndef GA106LAB_GFW_READINESS_FLOW_HPP
#define GA106LAB_GFW_READINESS_FLOW_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

// Endereços efetivos (mutáveis SOMENTE por hooks de mutação test-only).
#ifdef GA106LAB_GFW_MUT_WRONG_GATE_OFFSET
static const uint32_t kGfwFlowGateOffset = 0x00011812Cu; // MUTAÇÃO: offset errado.
#else
static const uint32_t kGfwFlowGateOffset = GA106LAB_GFW_GATE_OFFSET;
#endif
#ifdef GA106LAB_GFW_MUT_WRONG_PROGRESS_OFFSET
static const uint32_t kGfwFlowProgressOffset = 0x000118230u; // MUTAÇÃO: offset errado.
#else
static const uint32_t kGfwFlowProgressOffset = GA106LAB_GFW_PROGRESS_OFFSET;
#endif
#ifdef GA106LAB_GFW_MUT_WRONG_GATE_MASK
static const uint32_t kGfwFlowGateMask = 0x2u; // MUTAÇÃO: máscara errada.
#else
static const uint32_t kGfwFlowGateMask = GA106LAB_GFW_GATE_MASK;
#endif
#ifdef GA106LAB_GFW_MUT_WRONG_COMPLETION_VALUE
static const uint32_t kGfwFlowCompletedValue = 0xFEu; // MUTAÇÃO: valor errado.
#else
static const uint32_t kGfwFlowCompletedValue = GA106LAB_GFW_COMPLETED_VALUE;
#endif

template <typename Ops>
inline IOReturn RunGfwBootReadinessFlow(Ops &ops, GA106LabGfwReadinessV1 &out)
{
    uint16_t vendor = 0;
    uint16_t device = 0;
    uint16_t commandBefore = 0;
    uint32_t bar0Raw = 0;
    uint64_t bar0Base = 0;
    uint32_t count = 0;
    uint32_t i = 0;
    int matchIndex = -1;
    int matchCount = 0;
    uint64_t resLen = 0;
    uint64_t resPhys = 0;
    uint64_t mapLen = 0;
    uint32_t mapOpts = 0;
    uint64_t va = 0;
    uint32_t gateRaw = 0;
    uint32_t progRaw = 0;
    uint32_t lastGate = 0;
    uint32_t lastProgress = 0;
    uint32_t iter = 0;

    // Zero total primeiro (sem bytes não inicializados).
    out.size = 0;
    out.version = 0;
    out.status = kGA106LabGfwStatus_NotAttempted;
    out.gateReady = 0;
    out.progress = 0;
    out.pollCount = 0;
    out.timedOut = 0;
    out.reserved = 0;

    // 1. provider
    if (!ops.isProviderPCI()) {
        out.status = kGA106LabGfwStatus_Unavailable;
        return kIOReturnNotReady;
    }

    // 2. identidade (mesmo gate 1.2.1)
    vendor = ops.readVendorId();
    if (vendor == 0xFFFFu) {
        out.status = kGA106LabGfwStatus_Unavailable;
        return kIOReturnNotReady;
    }
    device = ops.readDeviceId();
    if (vendor != GA106LAB_VENDOR_NVIDIA ||
        device != GA106LAB_DEVICE_GA106) {
        out.status = kGA106LabGfwStatus_Unavailable;
        return kIOReturnNoDevice;
    }

    // 3. MSE (nunca habilitar; abortar se off — leituras MMIO sem validade).
    commandBefore = ops.readCommandBefore();
    if (!GA106LabCommandMseOn(commandBefore)) {
        out.status = kGA106LabGfwStatus_Unavailable;
        return kIOReturnNotReady;
    }

    // 4. BAR0 raw
    bar0Raw = ops.readBar0Raw();
    if (!GA106LabBar0RawValid(bar0Raw)) {
        out.status = kGA106LabGfwStatus_Unavailable;
        return kIOReturnNoDevice;
    }
    bar0Base = GA106LabBar0BaseFromRaw(bar0Raw);

    // 5. reserva BUSY primeiro (seção curta; segunda chamada => Busy).
    // Todo o trabalho abaixo (mapa local + poll) ocorre sob reserva; cada
    // saída de falha libera o que alocou e solta a reserva.
    ops.lock();
#ifdef GA106LAB_GFW_MUT_ALLOW_SECOND_CONCURRENT_CALL
    // MUTAÇÃO TEST-ONLY: sem exclusão (segunda chamada prossegue).
    ops.setBusy(true);
    ops.unlock();
#else
    if (ops.isStopping()) {
        ops.unlock();
        out.status = kGA106LabGfwStatus_Aborted;
        return kIOReturnAborted;
    }
    if (ops.isBusy()) {
        ops.unlock();
        out.status = kGA106LabGfwStatus_NotAttempted;
        return kIOReturnBusy;
    }
    ops.setBusy(true);
    ops.unlock();
#endif

#define GFW_FAIL_RELEASE_BUSY() \
    do {                        \
        ops.lock();             \
        ops.setBusy(false);     \
        ops.unlock();           \
    } while (0)

    // 6. discovery: SOMENTE dentro do count real; match estrito 16MiB+base.
    count = ops.getResourceCount();
    matchIndex = -1;
    matchCount = 0;
    resLen = 0;
    resPhys = 0;
    for (i = 0; i < count; i++) {
        uint64_t base = 0;
        uint64_t len = 0;
        if (!ops.getResource(i, base, len)) {
            continue;
        }
        if (GA106LabBarResourceMatches(len, base, bar0Base)) {
            matchIndex = (int)i;
            matchCount++;
            resLen = len;
            resPhys = base;
        }
    }
    if (matchCount != 1 || matchIndex < 0) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_BUSY();
        return kIOReturnNotFound; // 0 ou >=2: FAIL, sem fallback.
    }
    if (resLen != GA106LAB_BAR0_EXPECTED_LENGTH) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_BUSY();
        return kIOReturnNoDevice;
    }
    if (resPhys != bar0Base) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_BUSY();
        return kIOReturnNoDevice;
    }

    // 7. map RO+Inhibit (single map por chamada; liberado antes de retornar).
    if (!ops.createMap((uint32_t)matchIndex, mapLen, mapOpts)) {
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_BUSY();
        return kIOReturnNoResources;
    }

    // 8. map metadata: full-resource v1 + cache EXATA + VA.
    if (mapLen > resLen || mapLen != resLen) {
        ops.releaseMap();
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_BUSY();
        return kIOReturnNoMemory;
    }
    if (!GA106LabMapOptionsValidROInhibit(mapOpts)) {
        ops.releaseMap();
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_BUSY();
        return kIOReturnNotPermitted;
    }
    va = ops.getVA();
    if (va == 0) {
        ops.releaseMap();
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_BUSY();
        return kIOReturnNoResources;
    }

    // 9. bounds: 0x118234+4 dentro do mapa (overflow-safe; sem acesso OOR).
    if (!GA106LabBoundsValid(kGfwFlowProgressOffset,
                             GA106LAB_GFW_READ_WIDTH, mapLen)) {
        ops.releaseMap();
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_BUSY();
        return kIOReturnNoMemory;
    }

#ifdef GA106LAB_GFW_MUT_CALL_SELECTOR7
    ops.noteSelector7Call(); // MUTAÇÃO TEST-ONLY: interação indevida c/ sysmem.
#endif

    // 10. poll bounded: gate && progress (short-circuit: sem gate, sem progress).
    for (iter = 0; iter < GA106LAB_GFW_POLL_MAX_ITERS; iter++) {
#ifdef GA106LAB_GFW_MUT_UNBOUNDED_POLL
        // MUTAÇÃO TEST-ONLY: sem bound (watchdog deve capturar o hang).
        (void)iter;
        for (;;) {
            gateRaw = ops.read32(kGfwFlowGateOffset);
            lastGate = (gateRaw & kGfwFlowGateMask) != 0u ? 1u : 0u;
            if (lastGate != 0u) {
                progRaw = ops.read32(kGfwFlowProgressOffset);
                lastProgress = progRaw & GA106LAB_GFW_PROGRESS_MASK;
            }
            ops.waitMs(GA106LAB_GFW_POLL_INTERVAL_MS);
        }
#endif
        // 10a. stop-check (seção curta; aborta sem travar).
        ops.lock();
        bool stopping = ops.isStopping();
        ops.unlock();
        if (stopping) {
            ops.releaseMap();
            ops.lock();
            ops.setBusy(false);
            ops.unlock();
            out.status = kGA106LabGfwStatus_Aborted;
            out.gateReady = lastGate;
            out.progress = lastProgress;
            out.pollCount = iter + 1;
            out.timedOut = 0;
            return kIOReturnAborted;
        }

        // 10b. gate read (sempre).
        gateRaw = ops.read32(kGfwFlowGateOffset);
        lastGate = (gateRaw & kGfwFlowGateMask) != 0u ? 1u : 0u;

        // 10c. progress SOMENTE com gate (gate falso => sem leitura).
#ifdef GA106LAB_GFW_MUT_REMOVE_GATE_CHECK
        // MUTAÇÃO TEST-ONLY: lê progresso mesmo sem gate.
        progRaw = ops.read32(kGfwFlowProgressOffset);
        lastProgress = progRaw & GA106LAB_GFW_PROGRESS_MASK;
        if (lastProgress == kGfwFlowCompletedValue) {
#else
        if (lastGate != 0u) {
            progRaw = ops.read32(kGfwFlowProgressOffset);
            lastProgress = progRaw & GA106LAB_GFW_PROGRESS_MASK;
            if (lastProgress == kGfwFlowCompletedValue) {
#endif
                // 10d. completed: fill + release + libera reserva.
#ifdef GA106LAB_GFW_MUT_LEAK_MAP
                // MUTAÇÃO TEST-ONLY: pula o release (mapa vaza).
#else
                ops.releaseMap();
#endif
                ops.lock();
                ops.setBusy(false);
                ops.unlock();
                out.size = (uint32_t)sizeof(out);
                out.version = GA106LAB_UC_PROTOCOL_VERSION;
                out.status = kGA106LabGfwStatus_Completed;
                out.gateReady = lastGate;
                out.progress = lastProgress;
                out.pollCount = iter + 1;
                out.timedOut = 0;
#ifdef GA106LAB_GFW_MUT_EXPOSE_MMIO_ADDRESS
                out.reserved = (uint32_t)va; // MUTAÇÃO TEST-ONLY: vaza endereço.
#endif
                return kIOReturnSuccess;
            }
#ifndef GA106LAB_GFW_MUT_REMOVE_GATE_CHECK
        }
#endif

#ifdef GA106LAB_GFW_MUT_WRITE_INSTEAD_OF_READ
        ops.write32(kGfwFlowGateOffset, gateRaw); // MUTAÇÃO TEST-ONLY: store.
#endif

        // 10e. espera 1ms fora de qualquer lock (última iteração não espera).
        if (iter + 1 < GA106LAB_GFW_POLL_MAX_ITERS) {
#ifdef GA106LAB_GFW_MUT_WAIT_UNDER_LOCK
            ops.lock(); // MUTAÇÃO TEST-ONLY: bloqueia sob trava.
            ops.waitMs(GA106LAB_GFW_POLL_INTERVAL_MS);
            ops.unlock();
#else
            ops.waitMs(GA106LAB_GFW_POLL_INTERVAL_MS);
#endif
        }
    }

    // 11. timeout: resultado semântico VÁLIDO (status TimedOut, Success).
    // Últimos valores observados preservados; nenhuma falha de transporte.
    ops.releaseMap();
    ops.lock();
    ops.setBusy(false);
    ops.unlock();
    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
#ifdef GA106LAB_GFW_MUT_REMOVE_TIMEOUT
    // MUTAÇÃO TEST-ONLY: exaustão reportada como sucesso/completed.
    out.status = kGA106LabGfwStatus_Completed;
    out.timedOut = 0;
#else
    out.status = kGA106LabGfwStatus_TimedOut;
    out.timedOut = 1;
#endif
    out.gateReady = lastGate;
    out.progress = lastProgress;
    out.pollCount = GA106LAB_GFW_POLL_MAX_ITERS;
    return kIOReturnSuccess;
}

// Espera de stop: flag terminal + poll !BUSY sem lock retido (o in-flight é
// bounded <= ~4s e sempre alcança o abort/commit). O mapa do poll é local
// por chamada — stop nunca libera recurso em uso (sem UAF por construção).
template <typename Ops>
inline void WaitForGfwIdleFlow(Ops &ops)
{
    ops.lock();
    ops.setStopping(true);
#ifdef GA106LAB_GFW_MUT_STOP_RELEASE_EARLY
    ops.unlock(); // MUTAÇÃO TEST-ONLY: retorna sem esperar o in-flight.
    return;
#else
    while (ops.isBusy()) {
        ops.unlock();
        ops.waitMs(1); // 1ms, sem lock; IODelay (Real) / contado (Mock).
        ops.lock();
    }
    ops.unlock();
#endif
}

#endif /* GA106LAB_GFW_READINESS_FLOW_HPP */
