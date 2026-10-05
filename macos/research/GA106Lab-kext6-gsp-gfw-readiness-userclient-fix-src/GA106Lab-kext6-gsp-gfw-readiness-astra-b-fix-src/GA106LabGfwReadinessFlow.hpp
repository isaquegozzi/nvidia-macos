// GA106LabGfwReadinessFlow.hpp — ONE_SHARED_GFW_READINESS_FLOW (KEXT6.1).
//
// Único fluxo de policy do selector 8 READ_GFW_BOOT_READINESS.
// Produção (GfwRealOps) e testes (GfwMockOps) instanciam O MESMO template
// RunGfwBootReadinessFlow<Ops>. Static dispatch (sem virtual, sem callbacks
// runtime no ABI/UserClient).
//
// Escopo: verificar o estado pré-existente de firmware/boot da GPU (GFW_BOOT)
// por dois registradores fixos (Nova/OpenRM, evidência em
// evidence/upstream-gfw/UPSTREAM-EVIDENCE.md):
//   gate @0x118128 bit 0 (PRIV_LEVEL_MASK.read_protection_level0) — gate de
//     ACESSO, não readiness. Se 0, o registrador de progresso não tem validade.
//   progress @0x118234 bits 7:0 — 0xff = completed; demais = not complete yet
//     (sem estados de erro inventados).
// Polling bounded DENTRO de uma única chamada, com DOIS limites
// INDEPENDENTES: contagem (4000 iterações) e deadline monotônico real (4 s).
// Somente leituras 32-bit (gate <= 4000, progress <= 4000, total <= 8000).
// Zero stores de qualquer tipo; zero PCI/BME/DMA/GSP/firmware; zero selector 7.
//
// Ordem (ASTRA-B-FIX: reserva ANTES de qualquer acesso a provider):
//   reserve BUSY (segunda => Busy; stopping => Aborted; zero acesso antes) ->
//   acquire provider (retain; falha => libera reserva, sem mais acessos) ->
//   vendor/device -> MSE -> BAR0 raw -> base ->
//   discovery (count, match len+base, 0=>FAIL, >=2=>FAIL, 1=>continue) ->
//   resLen/base revalidação -> createMap RO+Inhibit -> mapLen full-resource ->
//   cache EXATA (RO + campo==Inhibit) -> VA -> bounds 0x118234+4 ->
//   deadline = now + 4 s (saturante) ->
//   poll { stop-check; deadline-check; gate read; deadline-check;
//          [progress read]; completed?; budgeted wait } ->
//   fill ABI fixa -> releaseMap -> releaseProvider -> libera reserva -> return.
// Timeout é resultado semântico válido (status TimedOut, kIOReturnSuccess);
// Aborted/MapFailed/Unavailable retornam códigos de erro. Documentação formal
// da semântica transporte-vs-resposta em GA106LabProtocol.h.
//
// Lifetime (ASTRA-B-FIX): a reserva precede getProvider(); o provider é
// retido (retain) entre acquire e release; o mapa é local por chamada.
// stop() observa busy=true e espera (WaitForGfwIdleFlow), logo stop/free não
// retornam enquanto a chamada pode tocar owner/provider/map. Ordem de saída:
// release map -> release provider retido -> libera reserva. Auditoria da
// semântica IOKit no SDK local: getProvider() é estável enquanto o cliente
// está attached; retain()/release() são refcount atômico (OSObject);
// attach retém ambos até detach.
//
// Disciplina de lock: o template chama ops.lock()/unlock() SOMENTE em seções
// curtas (reserve, stop-check, busy-clear); waits e leituras rodam
// DESTRAVADOS. Nenhum map/MMIO/sleep sob trava. A exclusão é o bit fGfwBusy
// (disjointo de fSysmemBusy; mesma trava, vars separadas, sem interferência).
//
// Deadline honesto: 4 s = orçamento do polling ATIVO. Não se promete retorno
// total <= 4.000 ms sob suspensão/preempção arbitrária; se o thread voltar
// após o deadline, a próxima checagem retorna TimedOut sem novas leituras.
//
// Ops deve prover (RealOps = só operações; MockOps = fixtures/counters):
//   lock(); unlock();                       // seções curtas; NUNCA bloquear dentro
//   bool isStopping(); void setStopping(bool);
//   bool isBusy(); void setBusy(bool);
//   uint64_t nowNs();                       // relógio monotônico (nunca wall)
//   void waitBudgetedNs(uint64_t ns);       // espera <= ns, fora de lock
//   void waitMs(unsigned ms);               // só p/ stop-flow; fora de lock
//   bool acquireProvider();                 // retém provider; false = ausente
//   void releaseProvider();                 // solta retenção (no-op se ausente)
//   uint16_t readVendorId(); uint16_t readDeviceId();
//   uint16_t readCommandBefore();
//   uint32_t readBar0Raw();
//   uint32_t getResourceCount();
//   bool     getResource(uint32_t idx, uint64_t &base, uint64_t &len);
//   bool     createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut);
//   uint64_t getVA(); // 0 = falha
//   uint32_t read32(uint32_t offset); // ÚNICA primitiva de leitura
//   void     releaseMap();
// Nenhuma decisão (reserve/discovery/ordem/gate/deadline/poll/bounds) nos Ops.

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

// Preenche ABI de timeout (helper compartilhado pelos 3 pontos de exaustão).
template <typename Ops>
inline IOReturn GfwFillTimeout(Ops &ops, GA106LabGfwReadinessV1 &out,
                               uint32_t itersEntered, uint32_t gate,
                               uint32_t prog)
{
    ops.releaseMap();
    ops.releaseProvider();
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
    out.gateReady = gate;
    out.progress = prog;
    out.pollCount = itersEntered;
    return kIOReturnSuccess;
}

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
    uint64_t startNs = 0;
    uint64_t deadlineNs = 0;
    bool haveMap = false;
    bool haveProv = false;

    // Zero total primeiro (sem bytes não inicializados).
    out.size = 0;
    out.version = 0;
    out.status = kGA106LabGfwStatus_NotAttempted;
    out.gateReady = 0;
    out.progress = 0;
    out.pollCount = 0;
    out.timedOut = 0;
    out.reserved = 0;

    // 1. reserva PRIMEIRO: nenhum acesso a provider/hardware antes disto.
    // (ASTRA-B-FIX blocker 1: stop() observa busy e espera; sem reserva
    // antecipada, stop poderia retornar com a chamada ainda por começar.)
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

#define GFW_FAIL_RELEASE_ALL()   \
    do {                         \
        if (haveMap) {           \
            ops.releaseMap();    \
            haveMap = false;     \
        }                        \
        if (haveProv) {          \
            ops.releaseProvider(); \
            haveProv = false;    \
        }                        \
        ops.lock();              \
        ops.setBusy(false);      \
        ops.unlock();            \
    } while (0)

    // 2. acquire provider (retain; primeira e única aquisição).
    if (!ops.acquireProvider()) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNotReady;
    }
    haveProv = true;

    // 3. identidade (mesmo gate 1.2.1; provider já retido).
    vendor = ops.readVendorId();
    if (vendor == 0xFFFFu) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNotReady;
    }
    device = ops.readDeviceId();
    if (vendor != GA106LAB_VENDOR_NVIDIA ||
        device != GA106LAB_DEVICE_GA106) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNoDevice;
    }

    // 4. MSE (nunca habilitar; abortar se off — leituras MMIO sem validade).
    // Documentado: sem compare-after por desenho (Astra não exigiu); a
    // janela inteira de uso do provider é sincronizada por lifetime
    // (reserva + retain), e selector 8 nunca escreve PCI Command.
    commandBefore = ops.readCommandBefore();
    if (!GA106LabCommandMseOn(commandBefore)) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNotReady;
    }

    // 5. BAR0 raw
    bar0Raw = ops.readBar0Raw();
    if (!GA106LabBar0RawValid(bar0Raw)) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNoDevice;
    }
    bar0Base = GA106LabBar0BaseFromRaw(bar0Raw);

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
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNotFound; // 0 ou >=2: FAIL, sem fallback.
    }
    if (resLen != GA106LAB_BAR0_EXPECTED_LENGTH) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNoDevice;
    }
    if (resPhys != bar0Base) {
        out.status = kGA106LabGfwStatus_Unavailable;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNoDevice;
    }

    // 7. map RO+Inhibit (single map por chamada; liberado antes de retornar).
    if (!ops.createMap((uint32_t)matchIndex, mapLen, mapOpts)) {
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNoResources;
    }
    haveMap = true;

    // 8. map metadata: full-resource v1 + cache EXATA + VA.
    if (mapLen > resLen || mapLen != resLen) {
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNoMemory;
    }
    if (!GA106LabMapOptionsValidROInhibit(mapOpts)) {
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNotPermitted;
    }
    va = ops.getVA();
    if (va == 0) {
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNoResources;
    }

    // 9. bounds: 0x118234+4 dentro do mapa (overflow-safe; sem acesso OOR).
    if (!GA106LabBoundsValid(kGfwFlowProgressOffset,
                             GA106LAB_GFW_READ_WIDTH, mapLen)) {
        out.status = kGA106LabGfwStatus_MapFailed;
        GFW_FAIL_RELEASE_ALL();
        return kIOReturnNoMemory;
    }

    // 10. deadline monotônico real (independente do contador de iterações).
    startNs = ops.nowNs();
    if (startNs > UINT64_MAX - GA106LAB_GFW_POLL_TIMEOUT_NS) {
        deadlineNs = UINT64_MAX; // saturante; sem wrap.
    } else {
        deadlineNs = startNs + GA106LAB_GFW_POLL_TIMEOUT_NS;
    }

    // 11. poll bounded duplo: contagem (4000) E deadline (4 s).
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
            ops.waitBudgetedNs(GA106LAB_GFW_POLL_INTERVAL_NS);
        }
#endif
        // 11a. stop-check (seção curta; aborta sem travar).
        ops.lock();
        bool stopping = ops.isStopping();
        ops.unlock();
        if (stopping) {
            out.status = kGA106LabGfwStatus_Aborted;
            out.gateReady = lastGate;
            out.progress = lastProgress;
            out.pollCount = iter + 1;
            out.timedOut = 0;
            GFW_FAIL_RELEASE_ALL();
            return kIOReturnAborted;
        }

        // 11b. deadline antes do gate read: expirou => TimedOut sem ler.
        if (ops.nowNs() >= deadlineNs) {
            return GfwFillTimeout(ops, out, iter + 1, lastGate, lastProgress);
        }

        // 11c. gate read (sempre).
        gateRaw = ops.read32(kGfwFlowGateOffset);
        lastGate = (gateRaw & kGfwFlowGateMask) != 0u ? 1u : 0u;

        // 11d. progress SOMENTE com gate (gate falso => sem leitura),
        // e SOMENTE se o deadline ainda vale após o gate read.
#ifdef GA106LAB_GFW_MUT_REMOVE_GATE_CHECK
        // MUTAÇÃO TEST-ONLY: lê progresso mesmo sem gate.
        progRaw = ops.read32(kGfwFlowProgressOffset);
        lastProgress = progRaw & GA106LAB_GFW_PROGRESS_MASK;
        if (lastProgress == kGfwFlowCompletedValue) {
#else
        if (lastGate != 0u) {
            if (ops.nowNs() >= deadlineNs) {
                return GfwFillTimeout(ops, out, iter + 1, lastGate,
                                      lastProgress);
            }
            progRaw = ops.read32(kGfwFlowProgressOffset);
            lastProgress = progRaw & GA106LAB_GFW_PROGRESS_MASK;
            if (lastProgress == kGfwFlowCompletedValue) {
#endif
                // 11e. completed: fill + teardown ordenado.
                haveMap = false;
                haveProv = false;
#ifdef GA106LAB_GFW_MUT_LEAK_MAP
                // MUTAÇÃO TEST-ONLY: pula o release (mapa vaza).
#else
                ops.releaseMap();
#endif
                ops.releaseProvider();
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

        // 11f. espera orçada fora de qualquer lock (última iteração não espera).
        if (iter + 1 < GA106LAB_GFW_POLL_MAX_ITERS) {
            uint64_t nowB = ops.nowNs();
            uint64_t rem = (nowB >= deadlineNs) ? 0u : (deadlineNs - nowB);
            uint64_t w = (rem < GA106LAB_GFW_POLL_INTERVAL_NS)
                ? rem : GA106LAB_GFW_POLL_INTERVAL_NS;
#ifdef GA106LAB_GFW_MUT_WAIT_UNDER_LOCK
            ops.lock(); // MUTAÇÃO TEST-ONLY: bloqueia sob trava.
            ops.waitBudgetedNs(w);
            ops.unlock();
#else
            ops.waitBudgetedNs(w);
#endif
        }
    }

    // 12. exaustão das 4000 iterações: timeout semântico válido.
    return GfwFillTimeout(ops, out, GA106LAB_GFW_POLL_MAX_ITERS, lastGate,
                          lastProgress);
}

// Espera de stop: flag terminal + poll !BUSY sem lock retido (o in-flight é
// bounded por contagem E deadline, e sempre alcança abort/commit). O mapa do
// poll é local por chamada — stop nunca libera recurso em uso (sem UAF).
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
