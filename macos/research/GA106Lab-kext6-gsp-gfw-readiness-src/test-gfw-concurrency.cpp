// test-gfw-concurrency.cpp — concorrência real (pthreads) sobre o MESMO
// shared GFW readiness flow (RunGfwBootReadinessFlow + WaitForGfwIdleFlow).
// Determinístico via gate no Mock (waitMs bloqueia até waitGateOpen).
// TSan/ASan observam quando disponíveis (ver build.sh).
//
// T9: selector8 x selector8 — A bloqueado no poll, B => BUSY, sem leituras de B.
// T10: selector8 x stop — stop espera o in-flight (não libera cedo), abort
// limpo com mapa liberado. B/thread nunca deadlocka: main sempre abre o gate
// antes de join; mutações que quebram a exclusão falham por counts, NUNCA
// por deadlock do harness.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <pthread.h>
#include <unistd.h>

#include "GA106LabProtocol.h"
#include "GA106LabGfwReadinessFlow.hpp"
#include "GA106LabMockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

struct FlowArg {
    GA106LabGfwMockOps *m;
    GA106LabGfwReadinessV1 *out;
    IOReturn *kr;
};

static void *runFlow(void *a)
{
    FlowArg *fa = (FlowArg *)a;
    *fa->kr = RunGfwBootReadinessFlow(*fa->m, *fa->out);
    return NULL;
}

struct FlowArgDone : FlowArg {
    std::atomic<bool> *done;
};

static void *runFlowDone(void *a)
{
    FlowArgDone *fa = (FlowArgDone *)a;
    *fa->kr = RunGfwBootReadinessFlow(*fa->m, *fa->out);
    fa->done->store(true);
    return NULL;
}

struct StopArg {
    GA106LabGfwMockOps *m;
    bool *done;
};

static void *runStop(void *a)
{
    StopArg *sa = (StopArg *)a;
    WaitForGfwIdleFlow(*sa->m);
    *sa->done = true;
    return NULL;
}

static void waitWaitEntered(GA106LabGfwMockOps &m)
{
    for (int i = 0; i < 5000 && m.waitMsCount < 1; i++) {
        usleep(1000);
    }
    assert(m.waitMsCount >= 1);
    usleep(5000); // graça: A já bloqueado no gate (gate fechado).
}

static void openWaitGate(GA106LabGfwMockOps &m)
{
    pthread_mutex_lock(&m.gateMutex);
    m.waitGateOpen = true;
    pthread_cond_broadcast(&m.gateCond);
    pthread_mutex_unlock(&m.gateMutex);
}

int main(void)
{
    // T9: A em poll bloqueado, B => BUSY imediato, zero leituras de B.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 outA, outB;
        IOReturn krA = 0, krB = -1;
        pthread_t thA, thB;
        FlowArg fa;
        FlowArgDone fb;
        std::atomic<bool> bDone(false);
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 1;
        m.progMode = 1; // nunca completa: A permanece no poll.
        m.progValue = 0x00u;
        m.waitBlocks = true;
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
        waitWaitEntered(m); // A dentro do poll (reserva detida).
        assert(pthread_create(&thB, NULL, runFlowDone, &fb) == 0);
        for (int i = 0; i < 2000 && !bDone.load(); i++) {
            usleep(1000); // B deve retornar BUSY rápido (sem bloquear).
        }
        assert(bDone.load() == true);
        assert(krB == kIOReturnBusy);
        assert(outB.status == kGA106LabGfwStatus_NotAttempted);
        openWaitGate(m); // libera A (4000 iters sem sleep: instantâneo).
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thB, NULL) == 0);
        assert(krA == kIOReturnSuccess); // A: timeout válido após gate aberto.
        assert(outA.status == kGA106LabGfwStatus_TimedOut);
        assert(outA.pollCount == 4000);
        // B não leu nada: todos os reads são de A (4000 gate + 4000 progress).
        assert((int)m.gateReadCount == 4000);
        assert((int)m.progressReadCount == 4000);
        assert(m.storeCount == 0 && m.sysmemTouchCount == 0);
        assert(m.waitedUnderLock == false);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
    }
    PASS("T9 selector8xselector8: B BUSY, zero leituras de B");

    // T10: stop espera o in-flight; abort limpo, mapa liberado, sem UAF.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 outA;
        IOReturn krA = 0;
        pthread_t thA, thStop;
        FlowArg fa;
        StopArg sa;
        bool stopDone = false;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0; // nunca completaria sozinho.
        m.waitBlocks = true;
        m.stopAtWait = -1; // stopping vem do stop-flow real.
        memset(&outA, 0, sizeof(outA));
        fa.m = &m;
        fa.out = &outA;
        fa.kr = &krA;
        sa.m = &m;
        sa.done = &stopDone;
        assert(pthread_create(&thA, NULL, runFlow, &fa) == 0);
        waitWaitEntered(m);
        assert(pthread_create(&thStop, NULL, runStop, &sa) == 0);
        usleep(50000); // stop deve estar esperando (A ainda bloqueado).
        assert(stopDone == false); // NÃO retornou cedo (sem release antecipado).
        assert(m.mapReleaseCount == 0); // mapa de A ainda vivo: sem UAF.
        openWaitGate(m);
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        assert(krA == kIOReturnAborted); // A viu stopping e aborteou.
        assert(outA.status == kGA106LabGfwStatus_Aborted);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.isBusy() == false);
        assert(m.storeCount == 0 && m.sysmemTouchCount == 0);
    }
    PASS("T10 selector8xstop: stop espera, abort limpo, sem UAF");

    printf("test-gfw-concurrency: ALL PASS (%d grupos)\n", gGroups);
    return 0;
}
