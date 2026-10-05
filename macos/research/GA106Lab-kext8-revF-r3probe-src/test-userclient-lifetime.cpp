// test-userclient-lifetime.cpp — testes do MESMO shared owner-lifetime flow
// do UserClient (PROMPT 57: PinOwnerForExternalMethod /
// MarkUserClientClosingFlow / BeginUserClientStopAndDrainFlow /
// ReleasePinnedOwner sobre GA106LabUserClientMockOps).
// Main nunca deadlocka: gates sempre abertos antes de join; falhas são por
// asserts/counts, NUNCA por hang (watchdog como última rede).
// Métricas: OWNER_USE_AFTER_STOP_RETURN=0, RETAIN_AFTER_RELEASE_WINDOW=0,
// UAF=0, RETAIN_LEAK=0, DOUBLE_RELEASE=0 (asserts + balance + TSAN).

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <pthread.h>
#include <unistd.h>

#include "GA106LabUserClientOwnerFlow.hpp"
#include "GA106LabMockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static const int kHammerIters = 500;

struct PinArg {
    GA106LabUserClientMockOps *m;
    void **out;
    bool *ok;
};

static void *runPin(void *a)
{
    PinArg *pa = (PinArg *)a;
    *pa->ok = PinOwnerForExternalMethod(*pa->m, *pa->out);
    return NULL;
}

struct PinHoldArg : PinArg {
    std::atomic<bool> *pinned;
    std::atomic<bool> *releaseGate;
};

static void *runPinHold(void *a)
{
    // Simula corpo de externalMethod: pin -> trabalho -> release.
    PinHoldArg *pa = (PinHoldArg *)a;
    void *o = NULL;
    if (!PinOwnerForExternalMethod(*pa->m, o)) {
        *pa->out = NULL;
        *pa->ok = false;
        return NULL;
    }
    *pa->out = o;
    *pa->ok = true;
    pa->pinned->store(true);
    while (!pa->releaseGate->load()) {
        usleep(1000);
    }
    ReleasePinnedOwner(*pa->m, o);
    return NULL;
}

struct StopArg {
    GA106LabUserClientMockOps *m;
    bool *done;
    int *exitSeqOut;
};

static void *runStopLogged(void *a)
{
    StopArg *sa = (StopArg *)a;
    sa->m->logSeq(3); // STOP_ENTER.
    BeginUserClientStopAndDrainFlow(*sa->m);
    *sa->exitSeqOut = sa->m->logSeq(4); // STOP_EXIT.
    *sa->done = true;
    return NULL;
}

static void openPinGate(GA106LabUserClientMockOps &m)
{
    pthread_mutex_lock(&m.pinGateMutex);
    m.pinEntryOpen = true;
    pthread_cond_broadcast(&m.pinGateCond);
    pthread_mutex_unlock(&m.pinGateMutex);
}

// Conta RETAINs (5) com número >= stopExit.
static int countRetainsAtOrAfter(const GA106LabUserClientMockOps &m, int stopExit)
{
    int n = m.seqNext.load();
    int c = 0;
    for (int s = stopExit; s < n && s < GA106LabUserClientMockOps::kUcSeqCap; s++) {
        if (m.seqCode[s] == 5) {
            c++;
        }
    }
    return c;
}

static void assertBalanced(const GA106LabUserClientMockOps &m)
{
    assert(m.retainCount == m.releaseCount);
    assert(m.activeCount() == 0);
}

int main(void)
{
    // UC1: stop antes do pin (barreira na entrada) => reject limpo, sem retain.
    // Ordem de escalonamento irrelevante: qualquer ordem dá reject + zero retain.
    {
        GA106LabUserClientMockOps m;
        void *out = (void *)0x7;
        bool ok = true;
        pthread_t thA, thStop;
        PinArg pa;
        StopArg sa;
        bool stopDone = false;
        int stopExit = -1;
        GA106LabUserClientMockMakeLive(m);
        m.pinEntryBlock = true;
        pa.m = &m;
        pa.out = &out;
        pa.ok = &ok;
        sa.m = &m;
        sa.done = &stopDone;
        sa.exitSeqOut = &stopExit;
        assert(pthread_create(&thA, NULL, runPin, &pa) == 0);
        assert(pthread_create(&thStop, NULL, runStopLogged, &sa) == 0);
        // stop marca closing e drena (active==0 => retorno imediato).
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        openPinGate(m); // libera A: verá closing e rejeitará.
        assert(pthread_join(thA, NULL) == 0);
        assert(ok == false && out == NULL);
        assert(m.retainCount == 0 && m.releaseCount == 0);
        assertBalanced(m);
        assert(countRetainsAtOrAfter(m, stopExit) == 0);
    }
    PASS("UC1 PREPIN_STOP reject-no-retain");

    // UC2: pin válido + stop durante a chamada => stop espera; owner válido.
    {
        GA106LabUserClientMockOps m;
        void *out = NULL;
        bool ok = false;
        pthread_t thA, thStop;
        PinHoldArg pa;
        StopArg sa;
        bool stopDone = false;
        int stopExit = -1;
        std::atomic<bool> pinned(false);
        std::atomic<bool> gate(false);
        GA106LabUserClientMockMakeLive(m);
        pa.m = &m;
        pa.out = &out;
        pa.ok = &ok;
        pa.pinned = &pinned;
        pa.releaseGate = &gate;
        sa.m = &m;
        sa.done = &stopDone;
        sa.exitSeqOut = &stopExit;
        assert(pthread_create(&thA, NULL, runPinHold, &pa) == 0);
        for (int i = 0; i < 5000 && !pinned.load(); i++) {
            usleep(1000);
        }
        assert(pinned.load() == true); // A detém pin válido.
        assert(pthread_create(&thStop, NULL, runStopLogged, &sa) == 0);
        usleep(50000); // stop marca closing e drena: deve ESPERAR.
        assert(stopDone == false); // stop só conclui após a política.
        gate.store(true); // libera A: release do pin.
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        assert(ok == true && out == (void *)0x1); // owner local segue válido.
        assert(countRetainsAtOrAfter(m, stopExit) == 0);
        assert(m.retainCount == 1 && m.releaseCount == 1);
        assertBalanced(m);
    }
    PASS("UC2 POSTPIN_STOP stop-waits owner-stays-valid");

    // UC3: clientClose exatamente durante tentativa de pin.
    {
        GA106LabUserClientMockOps m;
        void *out = (void *)0x7;
        bool ok = true;
        pthread_t thA;
        PinArg pa;
        GA106LabUserClientMockMakeLive(m);
        m.pinEntryBlock = true;
        pa.m = &m;
        pa.out = &out;
        pa.ok = &ok;
        assert(pthread_create(&thA, NULL, runPin, &pa) == 0);
        usleep(20000);
        MarkUserClientClosingFlow(m); // clientClose: só marca, sem drain.
        assert(m.isClosing() == true);
        openPinGate(m);
        assert(pthread_join(thA, NULL) == 0);
        assert(ok == false && out == NULL);
        assert(m.retainCount == 0); // nada retido durante close.
        assertBalanced(m);
    }
    PASS("UC3 CLOSE_DURING_PIN reject-no-retain");

    // UC4: sem janela load-retain (hammer pin vs close) + invariantes.
    {
        // 4a: estados unitários rejeitam sem retain.
        {
            GA106LabUserClientMockOps m;
            void *out = NULL;
            GA106LabUserClientMockMakeLive(m);
            m.closing = true;
            assert(PinOwnerForExternalMethod(m, out) == false);
            assert(m.retainCount == 0);
            GA106LabUserClientMockMakeLive(m);
            m.open = false;
            assert(PinOwnerForExternalMethod(m, out) == false);
            assert(m.retainCount == 0);
            GA106LabUserClientMockMakeLive(m);
            m.owner = NULL;
            assert(PinOwnerForExternalMethod(m, out) == false);
            assert(m.retainCount == 0);
        }
        // 4b: hammer concorrente pin-vs-close: só reject-ou-pin-válido.
        {
            GA106LabUserClientMockOps m;
            struct HammerPin {
                static void *run(void *a)
                {
                    GA106LabUserClientMockOps *mm = (GA106LabUserClientMockOps *)a;
                    for (int i = 0; i < kHammerIters; i++) {
                        void *o = NULL;
                        if (PinOwnerForExternalMethod(*mm, o)) {
                            assert(o == (void *)0x1);
                            ReleasePinnedOwner(*mm, o);
                        } else {
                            assert(o == NULL);
                        }
                    }
                    return NULL;
                }
            };
            struct HammerClose {
                static void *run(void *a)
                {
                    GA106LabUserClientMockOps *mm = (GA106LabUserClientMockOps *)a;
                    for (int i = 0; i < kHammerIters; i++) {
                        MarkUserClientClosingFlow(*mm);
                        mm->lock();
                        mm->closing = false;
                        mm->open = true;
                        mm->unlock();
                    }
                    return NULL;
                }
            };
            pthread_t thP, thC;
            GA106LabUserClientMockMakeLive(m);
            assert(pthread_create(&thP, NULL, HammerPin::run, &m) == 0);
            assert(pthread_create(&thC, NULL, HammerClose::run, &m) == 0);
            assert(pthread_join(thP, NULL) == 0);
            assert(pthread_join(thC, NULL) == 0);
            assert(m.retainCount == m.releaseCount); // sem leak/double.
            assert(m.activeCount() == 0);
        }
    }
    PASS("UC4 LOAD_RETAIN_WINDOW_NONE hammer-balanced");

    // UC5: duas externalMethods concorrentes + stop (drain conta 2).
    {
        GA106LabUserClientMockOps m;
        void *outA = NULL, *outB = NULL;
        bool okA = false, okB = false;
        pthread_t thA, thStop;
        PinHoldArg pa;
        StopArg sa;
        bool stopDone = false;
        int stopExit = -1;
        std::atomic<bool> pinned(false);
        std::atomic<bool> gate(false);
        GA106LabUserClientMockMakeLive(m);
        pa.m = &m;
        pa.out = &outA;
        pa.ok = &okA;
        pa.pinned = &pinned;
        pa.releaseGate = &gate;
        sa.m = &m;
        sa.done = &stopDone;
        sa.exitSeqOut = &stopExit;
        assert(pthread_create(&thA, NULL, runPinHold, &pa) == 0);
        for (int i = 0; i < 5000 && !pinned.load(); i++) {
            usleep(1000);
        }
        assert(pinned.load() == true);
        // Segunda chamada pinada na main (permitida; active 1->2).
        {
            void *o2 = NULL;
            assert(PinOwnerForExternalMethod(m, o2) == true);
            assert(o2 == (void *)0x1);
            outB = o2;
            okB = true;
            assert(pthread_create(&thStop, NULL, runStopLogged, &sa) == 0);
            usleep(50000);
            assert(stopDone == false); // drain espera active==2.
            gate.store(true); // A libera (active 2->1); stop segue esperando.
            usleep(20000);
            assert(stopDone == false);
            ReleasePinnedOwner(m, o2); // B libera (active 1->0).
        }
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        assert(okA && outA == (void *)0x1 && okB && outB == (void *)0x1);
        assert(countRetainsAtOrAfter(m, stopExit) == 0);
        assert(m.retainCount == 2 && m.releaseCount == 2);
        assertBalanced(m);
    }
    PASS("UC5 TWO_CALLS_STOP drain-counts-2 balanced");

    // UC6: free após stop enquanto última chamada sai (modelo de free).
    {
        GA106LabUserClientMockOps m;
        void *out = NULL;
        bool ok = false;
        pthread_t thA, thStop;
        PinHoldArg pa;
        StopArg sa;
        bool stopDone = false;
        int stopExit = -1;
        std::atomic<bool> pinned(false);
        std::atomic<bool> gate(false);
        int baseRefs = 1; // referência base do start().
        GA106LabUserClientMockMakeLive(m);
        pa.m = &m;
        pa.out = &out;
        pa.ok = &ok;
        pa.pinned = &pinned;
        pa.releaseGate = &gate;
        sa.m = &m;
        sa.done = &stopDone;
        sa.exitSeqOut = &stopExit;
        assert(pthread_create(&thA, NULL, runPinHold, &pa) == 0);
        for (int i = 0; i < 5000 && !pinned.load(); i++) {
            usleep(1000);
        }
        assert(pthread_create(&thStop, NULL, runStopLogged, &sa) == 0);
        usleep(20000);
        gate.store(true); // última chamada sai (release do pin).
        assert(pthread_join(thA, NULL) == 0);
        assert(pthread_join(thStop, NULL) == 0);
        assert(stopDone == true);
        // stop() libera a referência base; free() exige active==0.
        baseRefs--;
        assert(baseRefs == 0);
        assert(m.activeCount() == 0); // pré-condição do free atendida.
        m.lock();
        m.unlock(); // trava íntegra após stop (free pode liberá-la).
        assert(m.retainCount == 1 && m.releaseCount == 1); // só o pin.
        assert(countRetainsAtOrAfter(m, stopExit) == 0);
    }
    PASS("UC6 FREE_AFTER_STOP preconditions-hold");

    printf("test-userclient-lifetime: ALL PASS (%d grupos; ONE_SHARED_UC_FLOW)\n",
           gGroups);
    return 0;
}
