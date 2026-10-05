// test-gfw-lifetime.cpp — testes adversariais de lifetime (ASTRA-B-FIX §4).
//
// Reproduzem exatamente o ataque do Astra: barreira antes do primeiro acesso,
// thread A = selector 8 (mesmo shared flow), thread B = stop/close
// (WaitForGfwIdleFlow real). Esperado pós-correção (reserva + retain antes de
// qualquer provider access): stop NÃO retorna enquanto A detém a operação;
// PCI_READS_AFTER_STOP_RETURN = 0; MMIO_READS_AFTER_STOP_RETURN = 0.
// Prova via log de sequência monotônico (seqNext): nenhuma leitura com
// número >= número de saída do stop. Main sempre abre os gates antes de
// join: sem deadlock no harness (falhas são por asserts, nunca por hang).

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

struct StopLogArg {
    GA106LabGfwMockOps *m;
    bool *done;
    int *exitSeqOut;
};

static void *runStopLogged(void *a)
{
    StopLogArg *sa = (StopLogArg *)a;
    sa->m->logSeq(3); // STOP_ENTER.
    WaitForGfwIdleFlow(*sa->m);
    *sa->exitSeqOut = sa->m->logSeq(4); // STOP_EXIT.
    *sa->done = true;
    return NULL;
}

static void openPciGate(GA106LabGfwMockOps &m)
{
    pthread_mutex_lock(&m.pciGateMutex);
    m.pciGateOpen = true;
    pthread_cond_broadcast(&m.pciGateCond);
    pthread_mutex_unlock(&m.pciGateMutex);
}

static void openWaitGate(GA106LabGfwMockOps &m)
{
    pthread_mutex_lock(&m.gateMutex);
    m.waitGateOpen = true;
    pthread_cond_broadcast(&m.gateCond);
    pthread_mutex_unlock(&m.gateMutex);
}

// Espera determinística até A parar na barreira (reserva já detida).
static void waitOpBlocked(GA106LabGfwMockOps &m)
{
    for (int i = 0; i < 5000 && !m.opBlocked.load(); i++) {
        usleep(1000);
    }
    assert(m.opBlocked.load() == true);
    usleep(5000); // graça: A já bloqueado com reserva detida.
}

// Conta leituras PCI(1)/MMIO(2) com número de sequência >= stopExit.
static void countReadsAtOrAfter(const GA106LabGfwMockOps &m, int stopExit,
                                int &pciOut, int &mmioOut)
{
    int n = m.seqNext.load();
    pciOut = 0;
    mmioOut = 0;
    for (int s = stopExit; s < n && s < GA106LabGfwMockOps::kSeqLogCap; s++) {
        if (m.seqCode[s] == 1) {
            pciOut++;
        } else if (m.seqCode[s] == 2) {
            mmioOut++;
        }
    }
}

int main(void)
{
    // A1: ataque exato do Astra — barreira antes da PRIMEIRA leitura PCI,
    // stop/close concorrente. Pós-fix: stop espera (busy já detido).
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 outA;
        IOReturn krA = 0;
        pthread_t thA, thStop;
        FlowArg fa;
        StopLogArg sa;
        bool stopDone = false;
        int stopExit = -1;
        int pciAfter = -1, mmioAfter = -1;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0;
        m.blockPci = true; // A trava antes do primeiro acesso PCI.
        memset(&outA, 0, sizeof(outA));
        fa.m = &m;
        fa.out = &outA;
        fa.kr = &krA;
        sa.m = &m;
        sa.done = &stopDone;
        sa.exitSeqOut = &stopExit;
        assert(pthread_create(&thA, NULL, runFlow, &fa) == 0);
        waitOpBlocked(m); // A bloqueado na barreira (reserva detida).
        assert(pthread_create(&thStop, NULL, runStopLogged, &sa) == 0);
        usleep(50000); // stop deve estar esperando, NÃO retornado.
        assert(stopDone == false); // stop cannot finish while A owns op.
        openPciGate(m); // libera A; verá stopping e abortará.
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        assert(krA == kIOReturnAborted);
        assert(outA.status == kGA106LabGfwStatus_Aborted);
        countReadsAtOrAfter(m, stopExit, pciAfter, mmioAfter);
        assert(pciAfter == 0); // PCI_READS_AFTER_STOP_RETURN = 0.
        assert(mmioAfter == 0); // MMIO_READS_AFTER_STOP_RETURN = 0.
        // A mapeou (preâmbulo curto sem checkpoints) e abortou na iter 0
        // do poll; tudo antes do retorno do stop; teardown balanceado.
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("A1 STOP_DURING_PREAMBLE zero-reads-after-stop-return");

    // A2: stop antes da aquisição — Aborted sem acquire, sem acessos.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 out;
        GA106LabGfwMockMakeLive(m);
        m.stopping = true;
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunGfwBootReadinessFlow(m, out);
        assert(kr == kIOReturnAborted);
        assert(out.status == kGA106LabGfwStatus_Aborted);
        assert(out.pollCount == 0);
        assert(m.acquireCount == 0);
        assert((int)m.configReadCount == 0 && m.read32Count == 0);
        assert(m.mapCreateCount == 0);
        assert(m.isBusy() == false);
    }
    PASS("A2 STOP_BEFORE_ACQUIRE zero-access");

    // A3: stop during map — barreira em createMap; sem uso após stop.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 outA;
        IOReturn krA = 0;
        pthread_t thA, thStop;
        FlowArg fa;
        StopLogArg sa;
        bool stopDone = false;
        int stopExit = -1;
        int pciAfter = -1, mmioAfter = -1;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0;
        m.blockMap = true; // A trava dentro de createMap (reserva detida).
        memset(&outA, 0, sizeof(outA));
        fa.m = &m;
        fa.out = &outA;
        fa.kr = &krA;
        sa.m = &m;
        sa.done = &stopDone;
        sa.exitSeqOut = &stopExit;
        assert(pthread_create(&thA, NULL, runFlow, &fa) == 0);
        waitOpBlocked(m); // A bloqueado em createMap (reserva detida).
        assert(pthread_create(&thStop, NULL, runStopLogged, &sa) == 0);
        usleep(50000);
        assert(stopDone == false);
        assert(m.mapReleaseCount == 0); // mapa ainda não liberado: sem UAF.
        openWaitGate(m); // mesmo gate de createMap.
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        assert(krA == kIOReturnAborted);
        countReadsAtOrAfter(m, stopExit, pciAfter, mmioAfter);
        assert(pciAfter == 0);
        assert(mmioAfter == 0);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("A3 STOP_DURING_MAP zero-reads-after-stop-return balanced");

    // A4: stop during poll com prova de sequência (além do T10).
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 outA;
        IOReturn krA = 0;
        pthread_t thA, thStop;
        FlowArg fa;
        StopLogArg sa;
        bool stopDone = false;
        int stopExit = -1;
        int pciAfter = -1, mmioAfter = -1;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0;
        m.waitBlocks = true;
        memset(&outA, 0, sizeof(outA));
        fa.m = &m;
        fa.out = &outA;
        fa.kr = &krA;
        sa.m = &m;
        sa.done = &stopDone;
        sa.exitSeqOut = &stopExit;
        assert(pthread_create(&thA, NULL, runFlow, &fa) == 0);
        for (int i = 0; i < 5000 && m.waitBudgetedCount < 1; i++) {
            usleep(1000);
        }
        assert(m.waitBudgetedCount >= 1);
        usleep(5000);
        assert(pthread_create(&thStop, NULL, runStopLogged, &sa) == 0);
        usleep(50000);
        assert(stopDone == false);
        openWaitGate(m);
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        assert(krA == kIOReturnAborted);
        countReadsAtOrAfter(m, stopExit, pciAfter, mmioAfter);
        assert(pciAfter == 0);
        assert(mmioAfter == 0);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("A4 STOP_DURING_POLL seq-proven zero-after-return");

    // A5: close durante o preâmbulo + nova chamada concorrente => Aborted
    // rápido; operação original aborta; zeros após retorno do stop.
    {
        GA106LabGfwMockOps m;
        GA106LabGfwReadinessV1 outA, outB;
        IOReturn krA = 0, krB = -1;
        pthread_t thA, thStop;
        FlowArg fa;
        StopLogArg sa;
        bool stopDone = false;
        int stopExit = -1;
        int pciAfter = -1, mmioAfter = -1;
        GA106LabGfwMockMakeLive(m);
        m.gateMode = 0;
        m.blockPci = true;
        memset(&outA, 0, sizeof(outA));
        memset(&outB, 0, sizeof(outB));
        fa.m = &m;
        fa.out = &outA;
        fa.kr = &krA;
        sa.m = &m;
        sa.done = &stopDone;
        sa.exitSeqOut = &stopExit;
        assert(pthread_create(&thA, NULL, runFlow, &fa) == 0);
        waitOpBlocked(m); // A bloqueado no preâmbulo (reserva detida).
        assert(pthread_create(&thStop, NULL, runStopLogged, &sa) == 0);
        usleep(20000);
        // Nova chamada durante o teardown: stopping já setado => Aborted
        // imediato, sem acquire, sem acessos.
        {
            GA106LabGfwReadinessV1 tmp;
            int acqBefore = m.acquireCount;
            int cfgBefore = (int)m.configReadCount;
            memset(&tmp, 0, sizeof(tmp));
            IOReturn kr2 = RunGfwBootReadinessFlow(m, tmp);
            assert(kr2 == kIOReturnAborted);
            assert(m.acquireCount == acqBefore);
            assert((int)m.configReadCount == cfgBefore);
            (void)outB;
            (void)krB;
        }
        openPciGate(m);
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        assert(krA == kIOReturnAborted);
        countReadsAtOrAfter(m, stopExit, pciAfter, mmioAfter);
        assert(pciAfter == 0);
        assert(mmioAfter == 0);
        assert(m.acquireCount == 1 && m.providerReleaseCount == 1);
        assert(m.isBusy() == false);
    }
    PASS("A5 CLOSE_DURING_PREAMBLE concurrent-aborts zeros-hold");

    printf("test-gfw-lifetime: ALL PASS (%d grupos; adversarial lifetime)\n",
           gGroups);
    return 0;
}
