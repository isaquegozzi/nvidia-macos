// test-gfw-readiness.cpp — testes offline do MESMO shared flow do selector 8
// (KEXT6: RunGfwBootReadinessFlow<GA106LabGfwMockOps>).
// waitMs do Mock NÃO dorme (só conta), exceto com waitBlocks (concorrência);
// logo os testes de timeout (4000 iterações) são instantâneos.
// Contadores SOMENTE dentro das MockOps; testes NUNCA incrementam manualmente.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "GA106LabProtocol.h"
#include "GA106LabGfwReadinessFlow.hpp"
#include "GA106LabMockOps.hpp"
#include "ga106ctl-validate.h"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static void assertZeroHwSideEffects(const GA106LabGfwMockOps &m)
{
    assert(m.storeCount == 0);
    assert(m.sysmemTouchCount == 0);
}

int main(void)
{
    // T2: gate true + progress=0xff imediato => 1 gate + 1 progress, Completed.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.size == 32 && out.version == 1);
        assert(out.status == kGA106LabGfwStatus_Completed);
        assert(out.gateReady == 1 && out.progress == 0xFFu);
        assert(out.pollCount == 1 && out.timedOut == 0);
        assert(out.reserved == 0);
        assert(m.gateReadCount == 1 && m.progressReadCount == 1);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.waitedUnderLock == false); // nunca espera sob trava.
        assert(m.readOffsetsCount == 2);
        assert(m.readOffsets[0] == 0x118128u); // gate primeiro
        assert(m.readOffsets[1] == 0x118234u); // progress depois
        assertZeroHwSideEffects(m);
        assert(ga106ctl_check_gfw_readiness(&out, sizeof(out)) == 0);
        assert(ga106ctl_gfw_readiness_cli_exit(1, &out, sizeof(out)) == 0);
    }
    PASS("T2 IMMEDIATE_COMPLETION 1/1 Completed pollCount=1");

    // T1: gate falso nas 4000 iterações => progress 0 reads, timeout.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess); // timeout é resultado semântico válido.
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert(out.timedOut == 1 && out.pollCount == 4000);
        assert(out.gateReady == 0);
        assert(m.gateReadCount == 4000 && m.progressReadCount == 0);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.waitMsCount == 3999); // última iteração não espera.
        assert(m.waitedUnderLock == false); // 3999 esperas, nenhuma sob trava.
        assertZeroHwSideEffects(m);
        assert(ga106ctl_check_gfw_readiness(&out, sizeof(out)) == 0);
    }
    PASS("T1 GATE_FALSE_TIMEOUT gate=4000 progress=0 pollCount=4000");

    // T3: gate falso 3x e depois verdadeiro + completion tardia => ordem ok.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 2;
        m.gateFalseCount = 3;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_Completed);
        assert(out.pollCount == 4);
        assert(m.gateReadCount == 4 && m.progressReadCount == 1);
        assert(m.readOffsetsCount == 5);
        assert(m.readOffsets[0] == 0x118128u);
        assert(m.readOffsets[1] == 0x118128u);
        assert(m.readOffsets[2] == 0x118128u);
        assert(m.readOffsets[3] == 0x118128u);
        assert(m.readOffsets[4] == 0x118234u);
        assertZeroHwSideEffects(m);
    }
    PASS("T3 DELAYED_COMPLETION order gatex4+progress pollCount=4");

    // T4: gate true + progress nunca 0xff => timeout no bound.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.progMode = 1;
        m.progValue = 0x00u;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert(out.timedOut == 1 && out.pollCount == 4000);
        assert(out.gateReady == 1 && out.progress == 0x00u);
        assert(m.gateReadCount == 4000 && m.progressReadCount == 4000);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.waitedUnderLock == false);
        assertZeroHwSideEffects(m);
    }
    PASS("T4 NEVER_COMPLETE timeout gate=4000 progress=4000");

    // T5: progresso não-monotônico/regressivo: só 0xff exato completa.
    {
        // 5a: sequência sem 0xff (inclui 0xfe) => timeout.
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        uint32_t seq[5] = { 0x01u, 0x00u, 0x5Au, 0xFEu, 0x7Fu };
        GA106LabGfwMockMakeLive(m);
        m.progMode = 2;
        m.progSeqLen = 5;
        for (int k = 0; k < 5; k++) {
            m.progSeq[k] = seq[k];
        }
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert(out.progress != 0xFFu);
        assertZeroHwSideEffects(m);
        // 5b: 0xff na 4a posição => completa na iteração 4 (0xfe não completa).
        GA106LabGfwMockOps m2;
        GA106LabGfwReadinessV1 out2;
        uint32_t seq2[4] = { 0x00u, 0xFEu, 0x01u, 0xFFu };
        GA106LabGfwMockMakeLive(m2);
        m2.progMode = 2;
        m2.progSeqLen = 4;
        for (int k = 0; k < 4; k++) {
            m2.progSeq[k] = seq2[k];
        }
        memset(&out2, 0, sizeof(out2));
        kr = RunGfwBootReadinessFlow(m2, out2);
        assert(kr == kIOReturnSuccess);
        assert(out2.status == kGA106LabGfwStatus_Completed);
        assert(out2.progress == 0xFFu && out2.pollCount == 4);
        assertZeroHwSideEffects(m2);
    }
    PASS("T5 PROGRESS_NONMONOTONIC only-exact-0xff-completes");

    // T6: map failure => retorno limpo, zero reads, sem leak de mapa.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.mapNull = true;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnNoResources);
        assert(out.status == kGA106LabGfwStatus_MapFailed);
        assert(out.pollCount == 0);
        assert(m.read32Count == 0);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 0);
        assertZeroHwSideEffects(m);
    }
    PASS("T6 MAP_FAILURE clean zero-reads");

    // T7: recurso/BAR insuficiente => sem acesso fora de alcance.
    {
        // 7a: resource curto (fora do match 16MiB) => NoDevice, zero reads.
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.res[0].len = 0x1000ULL;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnNotFound); // 0 matches: sem fallback.
        assert(out.status == kGA106LabGfwStatus_Unavailable);
        assert(m.read32Count == 0 && m.mapCreateCount == 0);
        assertZeroHwSideEffects(m);
        // 7b: mapa curto (len != resource) => NoMemory + release, zero reads.
        GA106LabGfwMockOps m2;
        GA106LabGfwReadinessV1 out2;
        GA106LabGfwMockMakeLive(m2);
        m2.mapLen = 0x1000ULL;
        memset(&out2, 0, sizeof(out2));
        kr = RunGfwBootReadinessFlow(m2, out2);
        assert(kr == kIOReturnNoMemory);
        assert(out2.status == kGA106LabGfwStatus_MapFailed);
        assert(m2.read32Count == 0);
        assert(m2.mapCreateCount == 1 && m2.mapReleaseCount == 1);
        assertZeroHwSideEffects(m2);
    }
    PASS("T7 SHORT_RESOURCE no-out-of-range-access");

    // T8: stop during poll => abort, mapa liberado, busy limpo, sem UAF.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0; // nunca completaria sozinho.
        m.stopAtWait = 5;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnAborted);
        assert(out.status == kGA106LabGfwStatus_Aborted);
        assert(out.timedOut == 0);
        assert(m.isBusy() == false);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.waitMsCount >= 5);
        assertZeroHwSideEffects(m);
    }
    PASS("T8 STOP_DURING_POLL abort map-released no-UAF");

    // T11+T12: reserved zero + sem exposição de endereço (via ABI 32B).
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunGfwBootReadinessFlow(m, out) == kIOReturnSuccess);
        assert(sizeof(out) == 32);
        assert(out.reserved == 0);
        assert(out.progress <= 0xFFu && out.gateReady <= 1u);
        assert(ga106ctl_gfw_readiness_cli_exit(1, &out, sizeof(out)) == 0);
        // Timeout também tem reserved zero.
        GA106LabGfwMockOps m2;
        GA106LabGfwReadinessV1 out2;
        GA106LabGfwMockMakeLive(m2);
        m2.gateMode = 0;
        memset(&out2, 0, sizeof(out2));
        assert(RunGfwBootReadinessFlow(m2, out2) == kIOReturnSuccess);
        assert(out2.reserved == 0);
        assert(ga106ctl_gfw_readiness_cli_exit(1, &out2, sizeof(out2)) == 0);
    }
    PASS("T11_T12 RESERVED_ZERO NO_ADDRESS_EXPOSURE");

    // T13+T14: sem interação com selector 7, sem writes/PCI/BME/DMA/GSP.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunGfwBootReadinessFlow(m, out) == kIOReturnSuccess);
        assert(m.sysmemTouchCount == 0); // INTERNAL_SELECTOR7_CALL = NO.
        assert(m.storeCount == 0);       // zero MMIO stores.
        assert(m.waitedUnderLock == false);
    }
    PASS("T13_T14 NO_SELECTOR7 NO_STORES");

    // Provider/identidade: NotReady/NoDevice sem leituras MMIO.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.providerIsPCI = false;
        memset(&out, 0, sizeof(out));
        assert(RunGfwBootReadinessFlow(m, out) == kIOReturnNotReady);
        assert(out.status == kGA106LabGfwStatus_Unavailable);
        assert(m.read32Count == 0);
        GA106LabGfwMockMakeLive(m);
        m.vendor = 0x1234u;
        memset(&out, 0, sizeof(out));
        assert(RunGfwBootReadinessFlow(m, out) == kIOReturnNoDevice);
        assert(m.read32Count == 0);
        GA106LabGfwMockMakeLive(m);
        m.cmdBefore = 0x0000u; // MSE off.
        memset(&out, 0, sizeof(out));
        assert(RunGfwBootReadinessFlow(m, out) == kIOReturnNotReady);
        assert(m.read32Count == 0);
    }
    PASS("PROVIDER_IDENTITY MSE-gated zero-MMIO-reads");

    printf("test-gfw-readiness: ALL PASS (%d grupos; ONE_SHARED_GFW_READINESS_FLOW)\n",
           gGroups);
    return 0;
}
