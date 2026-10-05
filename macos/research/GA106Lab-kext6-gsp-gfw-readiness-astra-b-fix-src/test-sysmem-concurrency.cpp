// test-sysmem-concurrency.cpp — concorrência real (pthreads) sobre o MESMO
// shared flow (RunPrepareGspSysmemFlow + WaitForSysmemIdleAndTeardownFlow).
// Determinístico via gate no Mock (prepareDma bloqueia até gateOpen).
// TSan/ASan observam quando disponíveis (ver build.sh).

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <pthread.h>
#include <unistd.h>

#include "GA106LabProtocol.h"
#include "GA106LabGspSysmemFlow.hpp"
#include "GA106LabMockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

struct FlowArg {
    GA106LabSysmemMockOps *m;
    GA106LabGspSysmemV1 *out;
    IOReturn *kr;
};

static void *runFlow(void *a)
{
    FlowArg *fa = (FlowArg *)a;
    *fa->kr = RunPrepareGspSysmemFlow(*fa->m, *fa->out);
    return NULL;
}

struct StopArg {
    GA106LabSysmemMockOps *m;
    bool *done;
};

static void *runStop(void *a)
{
    StopArg *sa = (StopArg *)a;
    WaitForSysmemIdleAndTeardownFlow(*sa->m);
    *sa->done = true;
    return NULL;
}

struct FlowArgDone : FlowArg {
    std::atomic<bool> *done;
};

static void *runFlowDone(void *a)
{
    FlowArgDone *fa = (FlowArgDone *)a;
    *fa->kr = RunPrepareGspSysmemFlow(*fa->m, *fa->out);
    fa->done->store(true);
    return NULL;
}

static void waitPrepareEntered(GA106LabSysmemMockOps &m)
{
    for (int i = 0; i < 5000 && m.prepareCount < 1; i++) {
        usleep(1000);
    }
    assert(m.prepareCount == 1);
    usleep(5000); // graça: A já bloqueado no gate (gate fechado).
}

static void openGate(GA106LabSysmemMockOps &m)
{
    pthread_mutex_lock(&m.gateMutex);
    m.gateOpen = true;
    pthread_cond_broadcast(&m.gateCond);
    pthread_mutex_unlock(&m.gateMutex);
}

int main(void)
{
    // T1: prepare x prepare — A bloqueado, B => BUSY, 1 alloc/prepare total.
    // B roda em thread própria com poll limitado: se B bloquear (mutação
    // que derrota a exclusão), o gate abre e o teste falha por counts,
    // NUNCA por deadlock (main jamais chama o flow sincronamente aqui).
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 outA, outB;
        IOReturn krA = 0, krB = -1;
        pthread_t thA, thB;
        FlowArg fa;
        FlowArgDone fb;
        std::atomic<bool> bDone(false);
        GA106LabSysmemMockMakeLive(m);
        m.prepareBlocks = true;
        memset(&outA, 0, sizeof(outA));
        memset(&outB, 0, sizeof(outB));
        fa.m = &m;
        fa.out = &outA;
        fa.kr = &krA;
        fb.m = &m;
        fb.out = &outB;
        fb.kr = &krB;
        fb.done = &bDone;
        assert(pthread_create(&thA, NULL, runFlow, &fa) == 0);
        waitPrepareEntered(m); // A dentro do prepare (reserva detida).
        assert(pthread_create(&thB, NULL, runFlowDone, &fb) == 0);
        for (int i = 0; i < 2000 && !bDone.load(); i++) {
            usleep(1000); // B deve retornar BUSY rápido (sem bloquear).
        }
        openGate(m); // libera A (e B, se mutação o fez entrar).
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thB, NULL) == 0);
        assert(krB == kIOReturnBusy);
        assert(krA == kIOReturnSuccess);
        assert(m.allocCount == 1 && m.prepareCount == 1);
        assert(m.segmentGenCount == 1);
        assert(m.state == kSysmemPhase_AddressReady);
    }
    PASS("T1 prepare x prepare: B BUSY, 1 alloc/prepare total");

    // T2: prepare x teardown (stop) — stop espera, não libera objetos de A.
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 outA;
        IOReturn krA = 0;
        pthread_t thA, thStop;
        FlowArg fa;
        StopArg sa;
        bool stopDone = false;
        GA106LabSysmemMockMakeLive(m);
        m.prepareBlocks = true;
        memset(&outA, 0, sizeof(outA));
        fa.m = &m;
        fa.out = &outA;
        fa.kr = &krA;
        sa.m = &m;
        sa.done = &stopDone;
        assert(pthread_create(&thA, NULL, runFlow, &fa) == 0);
        waitPrepareEntered(m);
        assert(pthread_create(&thStop, NULL, runStop, &sa) == 0);
        usleep(50000); // stop deve estar esperando (A ainda bloqueado).
        assert(stopDone == false); // NÃO retornou cedo.
        assert(m.commandReleaseCount == 0); // NÃO liberou objetos de A.
        assert(m.descriptorReleaseCount == 0);
        assert(m.descExists && m.cmdExists); // objetos de A intactos.
        openGate(m);
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        assert(krA == kIOReturnSuccess); // A comitou; stop desfez depois.
        assert(m.completeCount == 1 && m.clearCount == 1);
        assert(m.commandReleaseCount == 1 && m.descriptorReleaseCount == 1);
        assert(m.state == kSysmemPhase_Empty);
    }
    PASS("T2 prepare x teardown: stop espera, cleanup 1x, EMPTY, sem UAF");

    // T3: repeat x repeat sobre ADDRESS_READY — zero novas ops.
    {
        GA106LabSysmemMockOps m;
        GA106LabGspSysmemV1 out0, outA, outB;
        IOReturn krA = 0, krB = 0;
        pthread_t thA, thB;
        FlowArg fa, fb;
        GA106LabSysmemMockMakeLive(m);
        memset(&out0, 0, sizeof(out0));
        assert(RunPrepareGspSysmemFlow(m, out0) == kIOReturnSuccess);
        memset(&outA, 0, sizeof(outA));
        memset(&outB, 0, sizeof(outB));
        fa.m = &m;
        fa.out = &outA;
        fa.kr = &krA;
        fb.m = &m;
        fb.out = &outB;
        fb.kr = &krB;
        assert(pthread_create(&thA, NULL, runFlow, &fa) == 0);
        assert(pthread_create(&thB, NULL, runFlow, &fb) == 0);
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thB, NULL) == 0);
        assert(krA == kIOReturnSuccess && krB == kIOReturnSuccess);
        assert(outA.status == kGA106LabGspSysmemStatus_AlreadyPrepared);
        assert(outB.status == kGA106LabGspSysmemStatus_AlreadyPrepared);
        assert(m.allocCount == 1 && m.prepareCount == 1);
        assert(m.state == kSysmemPhase_AddressReady);
    }
    PASS("T3 repeat x repeat: ALREADY_PREPARED, counts congelados");

    // T4: stop sem in-flight — retorno imediato, zero ops.
    {
        GA106LabSysmemMockOps m;
        bool done = false;
        StopArg sa;
        pthread_t thStop;
        GA106LabSysmemMockMakeLive(m);
        sa.m = &m;
        sa.done = &done;
        assert(pthread_create(&thStop, NULL, runStop, &sa) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(done == true);
        assert(m.allocCount == 0 && m.completeCount == 0);
        assert(m.state == kSysmemPhase_Empty);
    }
    PASS("T4 stop sem in-flight: imediato, zero ops");

    printf("test-sysmem-concurrency: ALL PASS (%d grupos)\n", gGroups);
    return 0;
}
