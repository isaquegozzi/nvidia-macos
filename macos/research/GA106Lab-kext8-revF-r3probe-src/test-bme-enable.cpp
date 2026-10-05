// test-bme-enable.cpp — offline R2 tests of RunBmeEnableFlow<BmeMockOps>.
// Covers: prestate, BME-already-set, MSE-off, COMMAND bits, RMW preservation,
// stale boot/GateA/GateB, reload (shared ProviderSim), write-fail, uncertain
// completion, readback mismatch, MSE-flip, duplicate/second invocation,
// close/stop races, recoveryRequired-set. Deterministic + 1 pthread group.
// No IOKit live, no HW, no sudo. R2 test-only (production in flow header).

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include "GA106LabProtocol.h"
#include "GA106LabBmeFlow.hpp"
#include "GA106LabMockOps.hpp"
#include "ga106ctl-validate.h"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static const uint64_t kNonce = 0x1111111111111111ULL;

static void assertBalancedBme(BmeMockOps &m, int expectWrites) {
    assert(m.acquireCount == m.providerReleaseCount);
    assert(m.bmeWrites == expectWrites);
    assert(m.genericPciWrites == 0 && m.dmaKicks == 0);
    assert(m.isBusy() == false);
}

struct ThreadBmeArg {
    BmeMockOps *m;
    GA106LabBmeEnableV1 *out;
    IOReturn *kr;
};

static void *runBmeFlowThread(void *a) {
    ThreadBmeArg *fa = (ThreadBmeArg *)a;
    *fa->kr = RunBmeEnableFlow(*fa->m, *fa->out);
    return NULL;
}

int main(void) {
    // E1: happy — RMW preserves bits, latch, validator, CLI exit.
    {
        BmeProviderSim prov;
        prov.hasEpoch = true; prov.epoch = kNonce;
        BmeMockOps m;
        GA106LabBmeEnableV1 out;
        BmeMockMakeLive(m, &prov);
        m.cmdReg = 0x0003u;  // MSE + IOSpace, BME OFF (prestate histórico)
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunBmeEnableFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.size == 48 && out.version == 1);
        assert(out.status == kGA106LabBmeStatus_Success);
        assert(out.phase == kGA106LabBmePhase_Enabled);
        assert(out.bootFresh == 1 && out.sameBootGateA == 1 && out.sameBootGateB == 1);
        assert(out.gateBProbeMatch == 1);
        assert(out.mseEnabledBefore == 1 && out.bmeEnabledBefore == 0);
        assert(out.bmeWriteAttempted == 1 && out.bmeReadbackVerified == 1);
        assert(out.mseEnabledAfter == 1 && out.bmeEnabledAfter == 1);
        assert(out.writeCount == 1 && out.recoveryRequired == 0);
        assert(m.cmdReg == 0x0007u);  // 0x0003 | 0x4: RMW preservou demais bits
        assert(m.getBmeLatch() == kBmeLatch_EnabledSoftwareSide);
        assertBalancedBme(m, 1);
        assert(ga106ctl_check_bme_enable(&out, sizeof(out)) == 0);
        assert(ga106ctl_bme_enable_cli_exit(1, &out, sizeof(out)) == 0);
        assert(ga106ctl_bme_enable_cli_exit(0, &out, sizeof(out)) != 0);
    }
    PASS("E1 HAPPY RMW-preserve latch validator");

    // E2: prestate variants — BME already set / MSE off / unrelated bits.
    {
        BmeProviderSim prov;
        prov.hasEpoch = true; prov.epoch = kNonce;
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov); m.cmdReg = 0x0007u;  // BME já set
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_BmeAlreadySet);
            assert(out.writeCount == 0 && m.bmeWrites == 0);
            assertBalancedBme(m, 0);
        }
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov); m.cmdReg = 0x0000u;  // MSE OFF
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_MseDisabled);
            assert(out.writeCount == 0);
            assertBalancedBme(m, 0);
        }
        {
            // Bits não-relacionados variados sobrevivem ao RMW.
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov); m.cmdReg = 0x0543u;  // MSE + extras, BME OFF
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_Success);
            assert(m.cmdReg == (uint16_t)(0x0543u | 0x0004u));
            assertBalancedBme(m, 1);
        }
    }
    PASS("E2 PRESTATE already-set/MSE-off/RMW-bits");

    // E3: boot/reload adversarial — os 4 cenários do prompt.
    {
        // (i) BOOT A -> GateA A -> GateB A -> preflight A = eligible.
        BmeProviderSim provA;
        provA.hasEpoch = true; provA.epoch = 0xAAAAAAAAAAAAAAAAULL;
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &provA);
            m.serviceNonce = 0xAAAAAAAAAAAAAAAAULL;
            m.gateANonce = 0xAAAAAAAAAAAAAAAAULL;
            m.gateBNonce = 0xAAAAAAAAAAAAAAAAULL;
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_Success && out.bootFresh == 1);
        }
        // (ii) BOOT B vê GateA/GateB de A (stale) = reject.
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &provA);
            m.serviceNonce = 0xBBBBBBBBBBBBBBBBULL;  // boot novo, registros velhos
            m.gateANonce = 0xAAAAAAAAAAAAAAAAULL;
            m.gateBNonce = 0xAAAAAAAAAAAAAAAAULL;
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_StaleGateA);
            assert(out.writeCount == 0);
            assertBalancedBme(m, 0);
        }
        // (iii) BOOT A -> reload (NeverTouched local) = reject via época.
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &provA);  // mesmo provider (época de A)
            m.serviceNonce = 0xCCCCCCCCCCCCCCCCULL;  // novo objeto, novo nonce
            m.gateANonce = 0xCCCCCCCCCCCCCCCCULL;    // Gate A refeito pós-reload
            m.gateADma = 0x1000u;
            m.gateBNonce = 0xCCCCCCCCCCCCCCCCULL;    // Gate B refeito pós-reload
            m.gateBDma = 0x1000u;
            m.gateBLatch = (uint32_t)kGateBLatch_HIAndLOWritten;
            m.probeHi = (uint32_t)((0x1000ULL >> 40u) & 0xFFFFFFu);
            m.probeLo = (uint32_t)((0x1000ULL >> 8u) & 0xFFFFFFFFu);
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            // latch local NeverTouched + registros refeitos, MAS época do provider
            // é de A (0xAA..) != nonce atual (0xCC..) => BootMismatch. Nenhum write.
            assert(out.status == kGA106LabBmeStatus_BootMismatch);
            assert(out.writeCount == 0 && out.bootFresh == 0);
            assertBalancedBme(m, 0);
        }
        // (iv) serviço recriado sem Gate A/B = reject (StaleGateA primeiro).
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeProviderSim freshProv;
            freshProv.hasEpoch = false; freshProv.epoch = 0;
            BmeMockMakeLive(m, &freshProv);
            m.serviceNonce = 0xDDDDDDDDDDDDDDDDULL;
            m.gateAPresent = false;
            m.gateBPresent = false;
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_StaleGateA);
            assert(out.writeCount == 0);
        }
    }
    PASS("E3 BOOT-RELOAD-ADVERSARIAL eligible/stale/reload/recreated");

    // E4: write failure + uncertain completion + readback mismatch + MSE flip.
    {
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeProviderSim prov;
            prov.hasEpoch = true; prov.epoch = kNonce;
            BmeMockMakeLive(m, &prov);
            m.writeRefused = true;
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_UnknownPartial);
            assert(out.recoveryRequired == 1 && out.writeCount == 0);
            assert(m.getBmeLatch() == kBmeLatch_UnknownPartial);
            assertBalancedBme(m, 0);
        }
        {
            // Readback transport fail => UnknownPartial + reboot.
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeProviderSim prov;
            prov.hasEpoch = true; prov.epoch = kNonce;
            BmeMockMakeLive(m, &prov);
            m.cmdReadFail = true;
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_InternalError);
            assert(out.writeCount == 0);
            assert(m.getBmeLatch() == kBmeLatch_Disabled);  // pré-write: latch intacto
            assertBalancedBme(m, 0);
        }
    }
    PASS("E4 WRITE-FAIL + READBACK-TRANSPORT-FAIL");

    // E5: duplicate + second-after-success + close/stop races + recovery-set.
    {
        BmeProviderSim prov;
        prov.hasEpoch = true; prov.epoch = kNonce;
        BmeMockOps m; GA106LabBmeEnableV1 o1, o2;
        BmeMockMakeLive(m, &prov);
        memset(&o1, 0, sizeof(o1)); memset(&o2, 0, sizeof(o2));
        assert(RunBmeEnableFlow(m, o1) == kIOReturnSuccess);
        assert(o1.status == kGA106LabBmeStatus_Success);
        assert(RunBmeEnableFlow(m, o2) == kIOReturnSuccess);
        assert(o2.status == kGA106LabBmeStatus_AlreadyEnabled);
        assert(o2.writeCount == 0 && m.bmeWrites == 1);  // sem segundo write
        m.clientClose();  // latch persiste após close; stopping vence novas chamadas
        assert(m.getBmeLatch() == kBmeLatch_EnabledSoftwareSide);
        GA106LabBmeEnableV1 o3;
        memset(&o3, 0, sizeof(o3));
        assert(RunBmeEnableFlow(m, o3) == kIOReturnAborted);
        assert(o3.status == kGA106LabBmeStatus_Stopping);
        assert(m.getBmeLatch() == kBmeLatch_EnabledSoftwareSide);  // intacto
        assert(m.bmeWrites == 1);
        m.stopping.store(false);  // reopen
        GA106LabBmeEnableV1 o4;
        memset(&o4, 0, sizeof(o4));
        assert(RunBmeEnableFlow(m, o4) == kIOReturnSuccess);
        assert(o4.status == kGA106LabBmeStatus_AlreadyEnabled);
        assert(o4.phase == kGA106LabBmePhase_Idle);
        assert(m.bmeWrites == 1);  // ainda sem segundo write
    }
    PASS("E5 DUPLICATE + CLOSE latch-persists");

    // E6: stopping preset + busy preset + recoveryRequired preset path.
    {
        BmeProviderSim prov;
        prov.hasEpoch = true; prov.epoch = kNonce;
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov); m.stopping.store(true);
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnAborted);
            assert(out.status == kGA106LabBmeStatus_Stopping);
            assert(out.writeCount == 0 && m.bmeWrites == 0);
        }
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov); m.busy.store(true);
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnBusy);
            assert(out.writeCount == 0);
        }
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov);
            m.gateBPresent = false;  // Gate A ok, Gate B ausente
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_StaleGateB);
            assert(out.writeCount == 0);
            assertBalancedBme(m, 0);
        }
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov);
            m.gateBLatch = (uint32_t)kGateBLatch_NeverTouched;  // registro ok, latch errado
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_LatchMismatch);
            assert(out.writeCount == 0);
            assertBalancedBme(m, 0);
        }
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov);
            m.stopBeforeWrite = true;  // stop pousa entre pre-read e check pré-write
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnAborted);
            assert(out.status == kGA106LabBmeStatus_Stopping);
            assert(out.writeCount == 0 && m.bmeWrites == 0);
            assertBalancedBme(m, 0);
        }
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov);
            m.serviceNonce = 0;  // objeto inválido: nonce zero
            m.gateANonce = 0;
            m.gateBNonce = 0;
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_InternalError);
            assert(out.writeCount == 0);
            assertBalancedBme(m, 0);
        }
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov);
            m.probeLo ^= 0x1u;  // par observado diverge do esperado
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_ProbeMismatch);
            assert(out.gateBProbeMatch == 0 && out.writeCount == 0);
            assertBalancedBme(m, 0);
        }
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov);
            m.setBmeLatch(kBmeLatch_EnableAttempted);  // resíduo mid-flight
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_RecoveryRequired);
            assert(out.writeCount == 0);
            assertBalancedBme(m, 0);
        }
        {
            BmeMockOps m; GA106LabBmeEnableV1 out;
            BmeMockMakeLive(m, &prov);
            m.setBmeLatch(kBmeLatch_UnknownPartial);
            memset(&out, 0, sizeof(out));
            assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
            assert(out.status == kGA106LabBmeStatus_RecoveryRequired);
            assert(out.recoveryRequired == 1 && out.writeCount == 0);
        }
    }
    PASS("E6 STOPPING-BUSY-RECOVERY");

    // E7: validator rejects non-success shapes + CLI exit.
    {
        BmeProviderSim prov;
        prov.hasEpoch = true; prov.epoch = kNonce;
        BmeMockOps m; GA106LabBmeEnableV1 out;
        BmeMockMakeLive(m, &prov);
        memset(&out, 0, sizeof(out));
        assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
        GA106LabBmeEnableV1 bad = out;
        bad.status = kGA106LabBmeStatus_UnknownPartial;
        assert(ga106ctl_check_bme_enable(&bad, sizeof(bad)) != 0);
        bad = out; bad.writeCount = 0;
        assert(ga106ctl_check_bme_enable(&bad, sizeof(bad)) != 0);
        bad = out; bad.recoveryRequired = 1;
        assert(ga106ctl_check_bme_enable(&bad, sizeof(bad)) != 0);
        bad = out; bad.bootFresh = 0;
        assert(ga106ctl_check_bme_enable(&bad, sizeof(bad)) != 0);
        bad = out; bad.reserved[0] = 1;
        assert(ga106ctl_check_bme_enable(&bad, sizeof(bad)) != 0);
        bad = out; bad.bmeEnabledAfter = 0;
        assert(ga106ctl_check_bme_enable(&bad, sizeof(bad)) != 0);
        assert(ga106ctl_check_bme_enable(NULL, sizeof(out)) != 0);
        assert(ga106ctl_check_bme_enable(&out, 16) != 0);
    }
    PASS("E7 VALIDATOR rejects non-success");

    // E8: concorrência — Busy determinístico + loop de contenção sob TSAN.
    {
        BmeProviderSim prov;
        prov.hasEpoch = true; prov.epoch = kNonce;
        BmeMockOps m; GA106LabBmeEnableV1 out;
        BmeMockMakeLive(m, &prov);
        m.setBusy(true);
        pthread_t th;
        ThreadBmeArg fa;
        IOReturn kr = kIOReturnError;
        memset(&out, 0, sizeof(out));
        fa.m = &m; fa.out = &out; fa.kr = &kr;
        assert(pthread_create(&th, NULL, runBmeFlowThread, &fa) == 0);
        assert(pthread_join(th, NULL) == 0);
        assert(kr == kIOReturnBusy && out.writeCount == 0 && m.bmeWrites == 0);
        m.setBusy(false);
    }
    {
        for (int iter = 0; iter < 50; iter++) {
            BmeProviderSim prov;
            prov.hasEpoch = true; prov.epoch = kNonce;
            BmeMockOps m; GA106LabBmeEnableV1 oA, oB;
            IOReturn krA = kIOReturnError, krB = kIOReturnError;
            pthread_t thA, thB;
            ThreadBmeArg fa, fb;
            BmeMockMakeLive(m, &prov);
            memset(&oA, 0, sizeof(oA)); memset(&oB, 0, sizeof(oB));
            fa.m = &m; fa.out = &oA; fa.kr = &krA;
            fb.m = &m; fb.out = &oB; fb.kr = &krB;
            assert(pthread_create(&thA, NULL, runBmeFlowThread, &fa) == 0);
            assert(pthread_create(&thB, NULL, runBmeFlowThread, &fb) == 0);
            assert(pthread_join(thA, NULL) == 0);
            assert(pthread_join(thB, NULL) == 0);
            int okA = (krA == kIOReturnSuccess && oA.status == kGA106LabBmeStatus_Success);
            int okB = (krB == kIOReturnSuccess && oB.status == kGA106LabBmeStatus_Success);
            assert(!(okA && okB));  // nunca dois enables
            assert(m.bmeWrites <= 1);
            assert(m.isBusy() == false);
            assert(m.acquireCount == m.providerReleaseCount);
        }
    }
    PASS("E8 CONCURRENT deterministic-Busy + contention loop");

    // E9: store perdido (readback diverge) => UnknownPartial + reboot, sem retry.
    {
        BmeProviderSim prov;
        prov.hasEpoch = true; prov.epoch = kNonce;
        BmeMockOps m; GA106LabBmeEnableV1 out;
        BmeMockMakeLive(m, &prov);
        m.corruptAfterWrite = true;
        memset(&out, 0, sizeof(out));
        assert(RunBmeEnableFlow(m, out) == kIOReturnSuccess);
        assert(out.status == kGA106LabBmeStatus_UnknownPartial);
        assert(out.recoveryRequired == 1);
        assert(out.bmeWriteAttempted == 1 && out.writeCount == 1);
        assert(m.getBmeLatch() == kBmeLatch_UnknownPartial);
        assertBalancedBme(m, 1);
    }
    PASS("E9 READBACK-DIVERGE UnknownPartial reboot");

    printf("test-bme-enable: ALL PASS (%d grupos; shared flow + mock)\n", gGroups);
    return 0;
}
