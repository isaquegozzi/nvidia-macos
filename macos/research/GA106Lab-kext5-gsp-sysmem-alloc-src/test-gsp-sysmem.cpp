// test-gsp-sysmem.cpp — testes do MESMO shared flow do selector 7.
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
        assert(ga106ctl_check_gsp_sysmem(&out, sizeof(out)) == 0);
        assert(ga106ctl_gsp_sysmem_cli_exit(1, &out, sizeof(out)) == 0);
    }
    PASS("SUCCESS 1/1/1/1/1/1, mapping vivo, ABI 48B");

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

    // 3. Falhas pré-prepare: complete==0, cleanup correto, EMPTY.
    {
        // allocFail
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.allocFail = true;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.completeCount == 0 && m.clearCount == 0);
          assert(m.descriptorReleaseCount == 0 && m.commandReleaseCount == 0);
          assert(m.state == kGA106LabGspSysmemState_Empty);
          assertZeroHw(m); }
        // bytesNull
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.bytesNull = true;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.completeCount == 0);
          assert(m.state == kGA106LabGspSysmemState_Empty); }
        // cmdFail
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.cmdFail = true;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.completeCount == 0 && m.clearCount == 0);
          assert(m.descriptorReleaseCount == 1 && m.commandReleaseCount == 0);
          assert(m.state == kGA106LabGspSysmemState_Empty); }
        // bindFail
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.bindFail = true;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.completeCount == 0 && m.clearCount == 0);
          assert(m.state == kGA106LabGspSysmemState_Empty); }
        // prepareFail
        { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
          GA106LabSysmemMockMakeLive(m); m.prepareFail = true;
          memset(&out, 0, sizeof(out));
          assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess);
          assert(m.completeCount == 0 && m.clearCount == 1);
          assert(m.state == kGA106LabGspSysmemState_Empty); }
    }
    PASS("FAILURES pré-prepare: complete==0, cleanup exato, EMPTY");

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
            assert(m.state == kGA106LabGspSysmemState_Empty);
            assertZeroHw(m);
        }
    }
    PASS("SEGMENTS 0/multi/short/misaligned/width FAIL + complete==1");

    // 5. Stop após ADDRESS_READY: complete/clear/releases ==1, idempotente.
    {
        GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnSuccess);
        ReleaseGspSysmemFlow(m); // simula GA106Lab::stop
        assert(m.completeCount == 1 && m.clearCount == 1);
        assert(m.commandReleaseCount == 1 && m.descriptorReleaseCount == 1);
        assert(m.state == kGA106LabGspSysmemState_Empty);
        ReleaseGspSysmemFlow(m); // segunda: sem double
        assert(m.completeCount == 1 && m.clearCount == 1);
        assert(m.commandReleaseCount == 1 && m.descriptorReleaseCount == 1);
    }
    PASS("STOP cleanup 1/1/1/1 + idempotente (sem double)");

    // 6. Sensibilidade de mutação: remover complete quebraria o teste 5
    // (completeCount seria 0); segundo prepare quebraria ==1.
    {
        GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
        GA106LabSysmemMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        assert(RunPrepareGspSysmemFlow(m, out) == kIOReturnSuccess);
        assert(m.prepareCount == 1); // 2º prepare => 2 => FAIL
        assert(m.completeCount == 0); // mapping vivo sem complete: correto
    }
    PASS("MUTATION_SENSITIVITY documentada (counts exatos)");

    // 7. CLI propagation.
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
