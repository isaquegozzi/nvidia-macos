// test-production-concurrency.cpp — P80 §6: pthreads sobre double com mutex
// (padrão test-sysmem-concurrency). Prova: reserva única, stop drena,
// double-close seguro, sem reserva retida em poll longo, estado final seguro.
#include <cassert>
#include <cstdio>
#include <pthread.h>
#include "GA106LabProductionPhase.h"

struct ConcDouble {
    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    uint32_t phase = kProdPhase_Detached;
    bool stopping = false;
    bool busy = false;
    bool clientOpen = false;
    int prepared = 0;
    int blockedKicks = 0;

    bool start() {
        pthread_mutex_lock(&m);
        bool ok = (phase == kProdPhase_Detached);
        if (ok) phase = kProdPhase_Attached;
        pthread_mutex_unlock(&m);
        return ok;
    }
    bool openClient() {
        pthread_mutex_lock(&m);
        bool ok = (!stopping && !clientOpen && phase >= kProdPhase_Attached);
        if (ok) clientOpen = true;
        pthread_mutex_unlock(&m);
        return ok;
    }
    void clientClose() {
        pthread_mutex_lock(&m);
        clientOpen = false;
        pthread_mutex_unlock(&m);
    }
    // Reserva curta; poll longo SEM mutex retido (disciplina P75/P76).
    bool prepareOnce() {
        pthread_mutex_lock(&m);
        if (busy || stopping) { pthread_mutex_unlock(&m); return false; }
        busy = true;
        pthread_mutex_unlock(&m);
        for (volatile int i = 0; i < 20000; i++) {} // poll longo destravado
        pthread_mutex_lock(&m);
        bool ok = !stopping;
        if (ok) prepared++;
        busy = false;
        pthread_mutex_unlock(&m);
        return ok;
    }
    // stop(): terminal + espera !busy SEM mutex + teardown 1x.
    void stop() {
        pthread_mutex_lock(&m);
        stopping = true;
        pthread_mutex_unlock(&m);
        for (int i = 0; i < 100000; i++) {
            pthread_mutex_lock(&m);
            bool b = busy;
            pthread_mutex_unlock(&m);
            if (!b) break;
        }
        pthread_mutex_lock(&m);
        clientOpen = false;
        if (ProductionPhaseTeardownOk(phase)) phase = kProdPhase_Attached;
        pthread_mutex_unlock(&m);
    }
};

static ConcDouble * gD;
static void * preparer(void *) { for (int i = 0; i < 50; i++) gD->prepareOnce(); return NULL; }
static void * stopper(void *) { gD->stop(); return NULL; }
static void * closer(void *) { for (int i = 0; i < 50; i++) { gD->openClient(); gD->clientClose(); } return NULL; }

static int g = 0;
#define PASS(m) do { printf("%s\n", m); g++; } while (0)

int main(void) {
    // Corrida prepare x stop x close: invariantes finais sempre válidas.
    for (int round = 0; round < 5; round++) {
        ConcDouble d;
        gD = &d;
        assert(d.start());
        pthread_t t1, t2, t3, t4;
        pthread_create(&t1, NULL, preparer, NULL);
        pthread_create(&t2, NULL, preparer, NULL);
        pthread_create(&t3, NULL, stopper, NULL);
        pthread_create(&t4, NULL, closer, NULL);
        pthread_join(t1, NULL); pthread_join(t2, NULL);
        pthread_join(t3, NULL); pthread_join(t4, NULL);
        assert(!d.busy); // reserva nunca vaza
        assert(d.phase == kProdPhase_Attached); // teardown determinístico
        assert(!d.clientOpen); // stop drenou
        assert(d.stopping);
    }
    PASS("RACE_PREPARE_STOP_CLOSE_SAFE");

    // Double stop / double close / start único.
    {
        ConcDouble d;
        gD = &d;
        assert(d.start());
        assert(!d.start());
        pthread_t t1, t2;
        pthread_create(&t1, NULL, stopper, NULL);
        pthread_create(&t2, NULL, stopper, NULL);
        pthread_join(t1, NULL); pthread_join(t2, NULL);
        assert(d.phase == kProdPhase_Attached);
        assert(d.openClient() == false); // stopping nega open (fail-safe)
    }
    PASS("DOUBLE_STOP_CLOSE_SINGLE_START");

    printf("PRODUCTION_CONCURRENCY_TESTS = PASS (%d groups)\n", g);
    return 0;
}
