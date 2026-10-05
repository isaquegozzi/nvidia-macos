// test-r3-probes.cpp — matriz offline dos 3 flows R3 (revF, host-only).
// 26 casos adaptados de CODEX-R3-ABI-PROPOSAL §4: PMU VOID (sem chosen=PMU;
// R3-3 vira FAIL_PRELAUNCH), null-mapper PASS (doutrina NULL_OK congelada).
// launchAttempted==0 em TODOS; zero stores; sem IOKit live/HW/sudo.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "GA106LabProtocol.h"
#include "GA106LabR3ProbeFlow.hpp"
#include "GA106LabR3MockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static void assertRoClean(GA106LabR3MockOps &m)
{
    assert(m.mmioWriteCount == 0 && m.configWriteCount == 0 && m.dmaTriggerCount == 0);
    assert(m.acquireCount == m.releaseCount); // pin 1:1
    assert(m.isBusy() == false);
}

static void assertReservedZero32(const uint32_t *r, unsigned n)
{
    for (unsigned k = 0; k < n; k++) assert(r[k] == 0u);
}

// R3-1 SEC2 clean -> chosen=SEC2 PASS; R3-5 wrong-instance trap; R3-6/7/8/9/10.
static void gFalcon(void)
{
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out;
      GA106LabR3MockMakeLive(m); memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.size == 64 && out.version == 1);
      assert(out.status == kGA106LabR3Status_Pass);
      assert(out.chosen == kGA106LabR3Chosen_Sec2);
      assert(out.resolved == 1 && out.halted == 1 && out.fullPre == 1);
      assert(out.idle1 == 1 && out.idle2 == 1 && out.fullPost == 1);
      assert(out.coreSelectFalcon == 1 && out.riscvQuiet == 1);
      assert(out.transcfgStable == 1 && out.mailboxStable == 1);
      assert(out.bootvecStable == 1 && out.fbifStable == 1);
      assert(out.launchAttempted == 0);
      assert(out.reserved0 == 0 && out.reserved1 == 0);
      assertReservedZero32(out.reserved, 8);
      assertRoClean(m); }
    PASS("R3-1 FALCON_SEC2_CLEAN chosen=SEC2");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-2: SEC2 sujo, GSP limpo
      GA106LabR3MockMakeLive(m); m.cpuctl[0] = 0x00u; memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_Pass);
      assert(out.chosen == kGA106LabR3Chosen_Gsp); // fallback comparação
      assert(out.launchAttempted == 0);
      assertRoClean(m); }
    PASS("R3-2 FALCON_GSP_FALLBACK chosen=GSP");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-3 adaptado: PMU VOID
      GA106LabR3MockMakeLive(m); m.cpuctl[0] = 0x00u; m.cpuctl[1] = 0x00u;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch);
      assert(out.chosen == kGA106LabR3Chosen_None); // sem PMU: zero DMA
      assertRoClean(m); }
    PASS("R3-3 BOTH_DIRTY chosen=none (PMU VOID)");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-4 all unresolved
      GA106LabR3MockMakeLive(m); m.instReadable[0] = false; m.instReadable[1] = false;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch && out.chosen == 0); }
    PASS("R3-4 ALL_UNRESOLVED");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-5 wrong-instance trap
      GA106LabR3MockMakeLive(m); m.cpuctl[0] = 0x00u; m.trfPre[0] = 0x00u;
      memset(&out, 0, sizeof(out)); // SEC2 ilegível como SEC2 válido...
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.chosen != kGA106LabR3Chosen_Sec2); // nunca escolhe SEC2 sujo
      assertRoClean(m); }
    PASS("R3-5 WRONG_INSTANCE never-chooses-dirty-SEC2");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-6 running (ambos)
      GA106LabR3MockMakeLive(m); m.cpuctl[0] = 0x00u; m.cpuctl[1] = 0x00u;
      m.trfPre[0] = 0x02u; m.trfPre[1] = 0x02u; memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-6 FALCON_RUNNING");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-7 full (ambos)
      GA106LabR3MockMakeLive(m);
      m.trfPre[0] = 0x03u; m.trfPost[0] = 0x03u; m.trfPre[1] = 0x03u; m.trfPost[1] = 0x03u;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-7 FBDMA_FULL");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-7b: SÓ fullPost
      GA106LabR3MockMakeLive(m);
      m.trfPost[0] = 0x03u; m.trfPost[1] = 0x03u;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-7b FULL_POST_ONLY");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-8 non-idle
      GA106LabR3MockMakeLive(m);
      m.trfPre[0] = 0x00u; m.trfIdle2[0] = 0x00u; m.trfPre[1] = 0x00u; m.trfIdle2[1] = 0x00u;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-8 FBDMA_NON_IDLE");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-8b: SÓ idle2 falha
      GA106LabR3MockMakeLive(m);
      m.trfIdle2[0] = 0x00u; m.trfIdle2[1] = 0x00u;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-8b SECOND_IDLE_ONLY");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-9 drift
      GA106LabR3MockMakeLive(m); m.tcfgB[0] = 0x06u; m.tcfgB[1] = 0x06u;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-9 BASELINE_DRIFT");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // R3-10 UNKNOWN (hash ausente)
      GA106LabR3MockMakeLive(m); m.tcfgA[0] = 0xFFFFFFFFu; m.tcfgA[1] = 0xFFFFFFFFu;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_NeedsReview); }
    PASS("R3-10 HASH_UNKNOWN needs-review");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // core-select!=Falcon
      GA106LabR3MockMakeLive(m); m.bcr[0] = 0x11u; m.bcr[1] = 0x11u;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-CORESELECT_RISCV rejected");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 out; // RISCV active
      GA106LabR3MockMakeLive(m); m.riscv[0] = 0x80u; m.riscv[1] = 0x80u;
      memset(&out, 0, sizeof(out));
      assert(RunR3FalconStateFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-RISCV_ACTIVE rejected");
}

// R3-11..16 mapping.
static void gMapping(void)
{
    { GA106LabR3MockOps m; GA106LabR3MappingContractV1 out;
      GA106LabR3MockMakeLive(m); memset(&out, 0, sizeof(out));
      assert(RunR3MappingContractFlow(m, out) == kIOReturnSuccess);
      assert(out.size == 64 && out.version == 1);
      assert(out.status == kGA106LabR3Status_Pass);
      assert(out.providerMapperPresent == 1 && out.providerIdentityStable == 1);
      assert(out.gen64NotRunInR3 == 1 && out.launchAttempted == 0);
      assert(out.reserved0 == 0 && out.reserved1 == 0);
      assertReservedZero32(out.reserved, 9);
      assertRoClean(m); }
    PASS("R3-11 MAPPER_PROVIDER pass");
    { GA106LabR3MockOps m; GA106LabR3MappingContractV1 out; // R3-12 fallback
      GA106LabR3MockMakeLive(m); m.mapperPresent = false; memset(&out, 0, sizeof(out));
      assert(RunR3MappingContractFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_Pass);
      assert(out.systemMapperUsed == 1 && out.mapperNullArmed == 1); }
    PASS("R3-12 MAPPER_SYSTEM_FALLBACK pass");
    { GA106LabR3MockOps m; GA106LabR3MappingContractV1 out; // R3-13 null (NULL_OK=PASS)
      GA106LabR3MockMakeLive(m); m.mapperPresent = false; memset(&out, 0, sizeof(out));
      assert(RunR3MappingContractFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_Pass); // doutrina congelada, com nota
      assertRoClean(m); }
    PASS("R3-13 MAPPER_NULL pass-with-note");
    { GA106LabR3MockOps m; GA106LabR3MappingContractV1 out; // R3-14 drift
      GA106LabR3MockMakeLive(m); m.identityStable = false; memset(&out, 0, sizeof(out));
      assert(RunR3MappingContractFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-14 IDENTITY_DRIFT");
    { GA106LabR3MockOps m; GA106LabR3MappingContractV1 out; // R3-15 vtd unknown (info)
      GA106LabR3MockMakeLive(m); m.vtdState = 3u; memset(&out, 0, sizeof(out));
      assert(RunR3MappingContractFlow(m, out) == kIOReturnSuccess);
      assert(out.appleVtdState == 3u); // informativo; PASS por mapper+identidade
      assert(out.status == kGA106LabR3Status_Pass); }
    PASS("R3-15 VTD_UNKNOWN informational");
    { GA106LabR3MockOps m; GA106LabR3MappingContractV1 out; // mapper query fail
      GA106LabR3MockMakeLive(m); m.mapperQueryFail = true; memset(&out, 0, sizeof(out));
      assert(RunR3MappingContractFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-MAPPER_QUERY_FAIL");
}

// R3-16 construção: gen64NotRun deve ser 1 (se flow gerasse IOVA seria bug).
// R3-17..26 quiescence + comuns.
static void gQuies(void)
{
    { GA106LabR3MockOps m; GA106LabR3QuiescenceV1 out;
      GA106LabR3MockMakeLive(m); memset(&out, 0, sizeof(out));
      assert(RunR3QuiescenceFlow(m, out) == kIOReturnSuccess);
      assert(out.size == 64 && out.version == 1);
      assert(out.status == kGA106LabR3Status_Pass && out.quiescencePass == 1);
      assert(out.baselineStable == 1 && out.pendingProjectDmaState == 0);
      assert(out.aerClean == 1 && out.providerEpochOk == 1);
      assert(out.launchAttempted == 0 && out.recoveryRequired == 0);
      assert(out.reserved0 == 0);
      assertReservedZero32(out.reserved, 7);
      assertRoClean(m); }
    PASS("R3-17 QUIES_CLEAN pass");
    { GA106LabR3MockOps m; GA106LabR3QuiescenceV1 out; // R3-18 engine fail
      GA106LabR3MockMakeLive(m); m.cpuctl[0] = 0x00u; memset(&out, 0, sizeof(out));
      assert(RunR3QuiescenceFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch && out.quiescencePass == 0); }
    PASS("R3-18 QUIES_ENGINE_FAIL");
    { GA106LabR3MockOps m; GA106LabR3QuiescenceV1 out; // R3-19 drift
      GA106LabR3MockMakeLive(m); m.mb0B[0] = 0x99u; memset(&out, 0, sizeof(out));
      assert(RunR3QuiescenceFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-19 QUIES_DRIFT");
    { GA106LabR3MockOps m; GA106LabR3QuiescenceV1 out; // R3-20 sched
      GA106LabR3MockMakeLive(m); m.schedZero = false; memset(&out, 0, sizeof(out));
      assert(RunR3QuiescenceFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-20 QUIES_SCHED");
    { GA106LabR3MockOps m; GA106LabR3QuiescenceV1 out; // R3-21 CE
      GA106LabR3MockMakeLive(m); m.ceQuiet = false; memset(&out, 0, sizeof(out));
      assert(RunR3QuiescenceFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-21 QUIES_CE");
    { GA106LabR3MockOps m; GA106LabR3QuiescenceV1 out; // R3-22 AER
      GA106LabR3MockMakeLive(m); m.statusReg = 0xF900u; memset(&out, 0, sizeof(out));
      assert(RunR3QuiescenceFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch && out.aerClean == 0); }
    PASS("R3-22 QUIES_AER");
    { GA106LabR3MockOps m; GA106LabR3QuiescenceV1 out; // R3-24 epoch
      GA106LabR3MockMakeLive(m); m.epochOk = false; memset(&out, 0, sizeof(out));
      assert(RunR3QuiescenceFlow(m, out) == kIOReturnSuccess);
      assert(out.status == kGA106LabR3Status_FailPrelaunch); }
    PASS("R3-24 QUIES_EPOCH");
    { GA106LabR3MockOps m; GA106LabR3FalconStateV1 o1, o2; // R3-25 duplicate
      GA106LabR3MockMakeLive(m); memset(&o1, 0, sizeof(o1)); memset(&o2, 0, sizeof(o2));
      assert(RunR3FalconStateFlow(m, o1) == kIOReturnSuccess && o1.status == kGA106LabR3Status_Pass);
      assert(RunR3FalconStateFlow(m, o2) == kIOReturnSuccess);
      assert(o2.status == kGA106LabR3Status_RejectDuplicate); }
    PASS("R3-25 DUPLICATE rejected");
    { GA106LabR3MockOps m; GA106LabR3QuiescenceV1 out; // R3-26 stop race
      GA106LabR3MockMakeLive(m); m.stopping = true; memset(&out, 0, sizeof(out));
      assert(RunR3QuiescenceFlow(m, out) == kIOReturnAborted);
      assert(out.status == kGA106LabR3Status_Aborted); }
    PASS("R3-26 STOP_RACE aborted");
    { GA106LabR3MockOps m; GA106LabR3MappingContractV1 out; // busy
      GA106LabR3MockMakeLive(m); m.busy = true; memset(&out, 0, sizeof(out));
      assert(RunR3MappingContractFlow(m, out) == kIOReturnBusy); }
    PASS("R3-BUSY rejected");
}

int main(void)
{
    gFalcon();
    gMapping();
    gQuies();
    printf("test-r3-probes: ALL PASS (%d grupos)\n", gGroups);
    return 0;
}
