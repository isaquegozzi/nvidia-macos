// test-gsp-sysmem.cpp — testes do MESMO shared flow do selector 7
// (FIX-ASTRA-C: prepare-attempt=>complete, clear-falha=>sem-release,
// reserva BUSY, stopping=>Aborted).
//
// Instancia RunPrepareGspSysmemFlow<GA106LabSysmemMockOps> +
// ReleaseGspSysmemFlow (os mesmos templates da produção com
// GA106LabSysmemRealOps). Counters SOMENTE dentro das MockOps.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "GA106LabProtocol.h"
#include "GA106LabGspSysmemFlow.hpp"
#include "GA106LabMockOps.hpp"
#include "ga106ctl-validate.h"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static void assertZeroHw(const GA106LabSysmemMockOps &m)
{
    assert(m.mmioReadCount == 0);
    assert(m.mmioWriteCount == 0);
    assert(m.configWriteCount == 0);
    assert(m.bmeEnableCount == 0);
    assert(m.dmaTriggerCount == 0);
}

static void assertReservedZero(const GA106LabGspSysmemV1 &out)
{
    assert(out.reserved[0] == 0 && out.reserved[1] == 0);
    assert(out.reserved[2] == 0 && out.reserved[3] == 0);
}

int main(void)
{
    // 1. Sucesso: counts exatos, mapping vivo, ABI 48B sem endereços.
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunPrepareGspSysmemFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(m.allocCount == 1 && m.zeroCount == 1);
        assert(m.commandCreateCount == 1 && m.setDescriptorCount == 1);
        assert(m.prepareCount == 1 && m.segmentGenCount == 1);
        assert(m.completeCount == 0 && m.clearCount == 0);
        assert(m.descriptorReleaseCount == 0 && m.commandReleaseCount == 0);
        assertZeroHw(m);
        assert(out.size == 48 && out.version == 1);
        assert(out.status == kGA106LabGspSysmemStatus_AddressReady);
        assert(out.flags == kGA106LabGspSysmemFlag_AddressReady);
        assert(out.bufferSize == 4096 && out.segmentCount == 1);
        assert(out.segmentLength == 4096);
        assert(out.state == kGA106LabGspSysmemState_AddressReady);
        assertReservedZero(out); // IOVA nunca na ABI (pega MUT_EXPOSE_IOVA).
        assert(ga106ctl_check_gsp_sysmem(&out, sizeof(out)) == 0);
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &out, sizeof(out)) == 0);
    }
    PASS("SUCCESS 1/1/1/1/1/1, mapping vivo, ABI 48B, reserved zero");

    // 2. Repeat: ALREADY_PREPARED, zero nova alloc/prepare.
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 out, out2;
        GA106LabSysmemMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnSuccess);
        memset(&out2, 0, sizeof(out2));
        IOReturn kr = RunPrepareGspSysmemFlow(m, out2);
        assert(kr == kIOReturnSuccess);
        assert(out2.status == kGA106LabGspSysmemStatus_AlreadyPrepared);
        assert(m.allocCount == 1 && m.prepareCount == 1);
        assert(m.segmentGenCount == 1);
        assert(out2.bufferSize == 4096 && out2.segmentCount == 1);
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &out2, sizeof(out2)) == 0);
    }
    PASS("REPEAT ALREADY_PREPARED, counts congelados");

    // 2b. BUSY (reserva de outra thread) => kIOReturnBusy, zero efeito.
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        m.busy = true;
        m.state = kSysmemPhase_InProgress;
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnBusy);
        assert(m.allocCount == 0 && m.prepareCount == 0);
        assertZeroHw(m);
    }
    PASS("BUSY => kIOReturnBusy, zero efeito");

    // 2c. Stopping => kIOReturnAborted, zero efeito.
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        m.stopping = true;
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnAborted);
        assert(m.allocCount == 0 && m.prepareCount == 0);
    }
    PASS("STOPPING => kIOReturnAborted, zero efeito");

    // 3. Falhas pré-prepare: complete==0, cleanup exato, EMPTY.
    {
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.allocFail = true;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.completeCount == 0 && m.clearCount == 0);
          assert(m.descriptorReleaseCount == 0 && m.commandReleaseCount == 0);
          assert(m.state == kSysmemPhase_Empty);
          assertZeroHw(m); }
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.bytesNull = true;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.completeCount == 0);
          assert(m.state == kSysmemPhase_Empty); }
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.cmdFail = true;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.completeCount == 0 && m.clearCount == 0);
          assert(m.descriptorReleaseCount == 1 && m.commandReleaseCount == 0);
          assert(m.state == kSysmemPhase_Empty); }
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.bindFail = true;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.completeCount == 0 && m.clearCount == 0);
          assert(m.state == kSysmemPhase_Empty); }
    }
    PASS("FAILURES pré-prepare: complete==0, cleanup exato, EMPTY");

    // 3b. Prepare falho: complete==1 SEMPRE (attempt=>fActive no XNU),
    // tanto antes-de-ativar (mode 1) quanto com-ativo (mode 2).
    {
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.prepareMode = 1;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.prepareCount == 1 && m.completeCount == 1);
          assert(m.clearCount == 1);
          assert(m.descriptorReleaseCount == 1 && m.commandReleaseCount == 1);
          assert(m.state == kSysmemPhase_Empty);
          assertZeroHw(m); }
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.prepareMode = 2;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.prepareCount == 1 && m.completeCount == 1);
          assert(m.clearCount == 1);
          assert(m.state == kSysmemPhase_Empty); }
    }
    PASS("PREPARE_FAIL => complete==1 (attempt rule), clear==1, EMPTY");

    // 4. Segmentos: 0/>1/curto/misaligned/width => FAIL + complete==1.
    {
        const int modes[5] = {1, 2, 3, 4, 5};
        for (int k = 0; k < 5; k++) {
            GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
            GA106LabSysmemMockMakeLive(m); m.segMode = modes[k];
            memset(&out, 0, sizeof(out));
            assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
            assert(m.completeCount == 1 && m.clearCount == 1);
            assert(m.descriptorReleaseCount == 1 && m.commandReleaseCount == 1);
            assert(m.state == kSysmemPhase_Empty);
            assertZeroHw(m);
        }
    }
    PASS("SEGMENTS 0/multi/short/misaligned/width FAIL + complete==1");

    // 5. Stop após ADDRESS_READY: ordem exata [7,8,9,10], 1/1/1/1.
    {
        GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnSuccess);
        m.resetCounts();
        m.opLogCount = 0;
        assert(ReleaseGspSysmemFlow(m) == kIOReturnSuccess);
        assert(m.completeCount == 1 && m.clearCount == 1);
        assert(m.commandReleaseCount == 1 && m.descriptorReleaseCount == 1);
        assert(m.opLogCount == 4);
        assert(m.opLog[0] == 7 && m.opLog[1] == 8);
        assert(m.opLog[2] == 9 && m.opLog[3] == 10);
        assert(m.state == kSysmemPhase_Empty);
        assert(ReleaseGspSysmemFlow(m) == kIOReturnSuccess); // idempotente
        assert(m.completeCount == 1 && m.clearCount == 1);
    }
    PASS("STOP cleanup [complete,clear,relCmd,relDesc] + idempotente");

    // 5b. Clear-falha: CLEANUP_FAILED, zero releases, sem retry.
    {
        GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnSuccess);
        m.clearFail = true;
        m.resetCounts();
        assert(ReleaseGspSysmemFlow(m) != kIOReturnSuccess);
        assert(m.completeCount == 1); // complete ocorreu antes do clear.
        assert(m.commandReleaseCount == 0 && m.descriptorReleaseCount == 0);
        assert(m.state == kSysmemPhase_CleanupFailed);
        assert(ReleaseGspSysmemFlow(m) != kIOReturnSuccess); // sem retry.
        assert(m.commandReleaseCount == 0); // continua sem release.
    }
    PASS("CLEAR_FAIL => CLEANUP_FAILED, zero releases, sem retry");

    // 6. CLI propagation.
    {
        GA106LabSysmemMockOps m; GA106LabGspSysmemV1 good, bad;
        GA106LabSysmemMockMakeLive(m);
        memset(&good, 0, sizeof(good));
        assert(RunPrepareGspSysmemFlow(m, good) == kIOReturnSuccess);
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &good, sizeof(good)) == 0);
        assert(ga106ctl_gsp_sysmem_cli_exit(0, &good, sizeof(good)) != 0);
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &good, 16) != 0);
        bad = good; bad.version = 99;
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.status = kGA106LabGspSysmemStatus_Failed;
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.segmentCount = 2;
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.flags = 0;
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.state = kGA106LabGspSysmemState_Empty;
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &bad, sizeof(bad)) != 0);
    }
    PASS("CLI_PROPAGATION PASS");

    printf("test-gsp-sysmem: ALL PASS (%d grupos; ONE_SHARED_FLOW)\n", gGroups);
    return 0;
}
