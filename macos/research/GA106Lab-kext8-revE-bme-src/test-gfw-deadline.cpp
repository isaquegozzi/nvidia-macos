// test-gfw-deadline.cpp — testes do deadline monotônico real (ASTRA-B-FIX
// §5-§8) sobre o MESMO shared flow (RunGfwBootReadinessFlow).
// Relógio 100% fake/injetável (mockNowNs): nenhum teste usa relógio real ou
// dorme de verdade (waits exatos por padrão => suítes instantâneas).
// Invariantes provadas:
//   NO_MMIO_READ_STARTED_AFTER_DEADLINE = YES (D4-D7 por saltos/overshoot)
//   READ_COUNTS_WITHIN_BOUNDS = YES (D1-D3, D8: <=4000/<=4000/<=8000)
// Limite temporal e limite de contagem são INDEPENDENTES.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "GA106LabProtocol.h"
#include "GA106LabGfwReadinessFlow.hpp"
#include "GA106LabMockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static const uint64_t kBudgetNs = GA106LAB_GFW_POLL_TIMEOUT_NS; // 4 s.

int main(void)
{
    // D1: custo MMIO = 0 => bound de contagem vincula (4000 iters exatas).
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert(out.pollCount == 4000);
        assert(m.gateReadCount == 4000 && m.progressReadCount == 0);
        assert(m.maxReadNs < kBudgetNs);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("D1 ZERO_MMIO_COST iteration-bound 4000");

    // D2: custo MMIO = 10 us => deadline vincula antes da contagem.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 1;
        m.progMode = 1;
        m.progValue = 0x00u;
        m.mmioCostNs = 10000u;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert(out.timedOut == 1);
        assert((int)m.gateReadCount < 4000 && (int)m.gateReadCount > 0);
        assert(m.progressReadCount == m.gateReadCount);
        assert(out.pollCount == (uint32_t)m.gateReadCount + 1u);
        assert(m.maxReadNs < kBudgetNs);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("D2 MMIO_COST_10US deadline-bound within-bounds");

    // D3: custo MMIO variável (alterno a cada 3a leitura) => bounds valem.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 1;
        m.progMode = 1;
        m.progValue = 0x00u;
        m.mmioCostNs = 5000u;
        m.mmioCostAltNs = 50000u;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert((int)m.gateReadCount <= 4000 && (int)m.progressReadCount <= 4000);
        assert((int)m.gateReadCount + (int)m.progressReadCount <= 8000);
        assert(m.maxReadNs < kBudgetNs);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("D3 VARIABLE_MMIO_COST within-bounds no-read-after-deadline");

    // D4: wait overshoot (2 s por espera) => poucas iterações, sem leitura tardia.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0;
        m.waitAdvanceOverrideNs = 2000000000LL;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert((int)m.gateReadCount == 2); // iter0, iter1; iter2 expira no check.
        assert(out.pollCount == 3);
        assert(m.progressReadCount == 0);
        assert(m.maxReadNs < kBudgetNs);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("D4 WAIT_OVERSHOOT few-iters no-late-read");

    // D5: relógio salta além do deadline após gate read => sem progress read.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 2;
        m.gateFalseCount = 2; // leituras 1,2 falsas; 3a verdadeira.
        m.jumpAfterGateReads = 3;
        m.jumpNs = 10000000000ULL; // +10 s após a 3a leitura de gate.
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert((int)m.gateReadCount == 3);
        assert(m.progressReadCount == 0); // expirou antes do progress read.
        assert(out.pollCount == 3);
        assert(m.maxReadNs < kBudgetNs); // última leitura iniciou antes.
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("D5 JUMP_AFTER_GATE no-progress-read-after-deadline");

    // D6: deadline expira exatamente antes do progress read.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 1;
        m.jumpAfterGateReads = 1;
        m.jumpNs = 10000000000ULL;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert((int)m.gateReadCount == 1);
        assert(m.progressReadCount == 0);
        assert(out.pollCount == 1);
        assert(out.gateReady == 1); // último gate observado preservado.
        assert(m.maxReadNs < kBudgetNs);
        assert(m.isBusy() == false);
    }
    PASS("D6 EXPIRE_BEFORE_PROGRESS gate-only-then-timeout");

    // D7: deadline expira após sleep (overshoot 5 s) => 1 gate, 1 espera.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0;
        m.waitAdvanceOverrideNs = 5000000000LL;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert((int)m.gateReadCount == 1);
        assert((int)m.waitBudgetedCount == 1);
        assert(out.pollCount == 2);
        assert(m.maxReadNs < kBudgetNs);
        assert(m.isBusy() == false);
    }
    PASS("D7 EXPIRE_AFTER_SLEEP single-gate-then-timeout");

    // D8: contagem e deadline competem (0,5 ms/leitura) => vence o deadline,
    // ambos os limites respeitados simultaneamente.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0;
        m.mmioCostNs = 500000u;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(out.status == kGA106LabGfwStatus_TimedOut);
        assert((int)m.gateReadCount > 0 && (int)m.gateReadCount < 4000);
        assert(m.progressReadCount == 0);
        assert(out.pollCount == (uint32_t)m.gateReadCount + 1u);
        assert((int)m.waitBudgetedCount == (int)m.gateReadCount);
        assert(m.maxReadNs < kBudgetNs);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("D8 COUNT_VS_DEADLINE_RACE both-bounds-hold");

    printf("test-gfw-deadline: ALL PASS (%d grupos; monotonic deadline)\n",
           gGroups);
    return 0;
}
