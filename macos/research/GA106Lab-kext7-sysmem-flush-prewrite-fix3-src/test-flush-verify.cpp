// test-flush-verify.cpp — REAL production path test (P74 §5, TG-KEXT7.1).
// Instancia RunVerifyFlushPrewriteFlow<VerifyFlushMockOps>: o MESMO template
// que GA106Lab::verifySysmemFlushPreconditions usa em produção (wrapper fino).
// Cobre: first-call-not-busy, concorrência, AlreadyReady, falhas, stop/close,
// clear-fail, teardown repetido, matriz §10. Zero HW; sanitizers no build.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include "GA106LabProtocol.h"
#include "GA106LabFlushPrewriteFlow.hpp"
#include "GA106LabMockOps.hpp"
#include "ga106ctl-validate.h"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static void assertZeroHw(const VerifyFlushMockOps &m)
{
    assert(m.mmioWriteCount == 0);
    assert(m.configWriteCount == 0);
    assert(m.bmeEnableCount == 0);
    assert(m.dmaTriggerCount == 0);
}
static void assertBalancedTeardown(const VerifyFlushMockOps &m)
{
    // complete/clear chamados; releases só se objetos existiam; sem double.
    assert(m.completeCount <= 1 + m.prepareCount);
}

struct ThreadArg {
    VerifyFlushMockOps *m;
    GA106LabFlushPrewriteV1 *out;
    IOReturn kr;
};
static void *runVerify(void *a)
{
    ThreadArg *t = (ThreadArg *)a;
    t->kr = RunVerifyFlushPrewriteFlow(*t->m, *t->out);
    return NULL;
}

int main(void)
{
    // 1. First valid call: NOT Busy, PreconditionsReady, mapping persiste.
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        VerifyFlushMockMakeLive(m); memset(&out, 0, sizeof(out));
        IOReturn kr = RunVerifyFlushPrewriteFlow(m, out);
        assert(kr == kIOReturnSuccess); // FIRST_SELECTOR9_RETURNS_BUSY = NO
        assert(out.status == kGA106LabFlushPrewriteStatus_PreconditionsReady);
        assert(out.phase == kGA106LabFlushPrewritePhase_PreconditionsReady);
        assert(m.allocCount == 1 && m.prepareCount == 1);
        assert(m.descExists && m.cmdExists); // persiste após retorno
        assert(m.state == kGA106LabFlushPrewritePhase_PreconditionsReady);
        assertZeroHw(m);
        assert(ga106ctl_check_flush_prewrite(&out, sizeof(out)) == 0);
    }
    PASS("first call success, mapping persists");

    // 2. Concurrent second call => Busy definido; primeira completa ok.
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 o1, o2;
        pthread_t th;
        ThreadArg a;
        VerifyFlushMockMakeLive(m);
        m.prepareBlocks = true; m.gateOpen = false;
        memset(&o1, 0, sizeof(o1)); memset(&o2, 0, sizeof(o2));
        a.m = &m; a.out = &o1; a.kr = kIOReturnError;
        assert(pthread_create(&th, NULL, runVerify, &a) == 0);
        // Espera a primeira entrar no gate (poll allocCount).
        for (int i = 0; i < 5000 && m.allocCount == 0; i++) usleep(1000);
        assert(m.allocCount == 1);
        IOReturn kr2 = RunVerifyFlushPrewriteFlow(m, o2);
        assert(kr2 == kIOReturnBusy); // serialização definida
        assert(o2.status == kGA106LabFlushPrewriteStatus_Failed);
        pthread_mutex_lock(&m.gateMutex); m.gateOpen = true;
        pthread_cond_broadcast(&m.gateCond); pthread_mutex_unlock(&m.gateMutex);
        assert(pthread_join(th, NULL) == 0);
        assert(a.kr == kIOReturnSuccess);
        assert(o1.status == kGA106LabFlushPrewriteStatus_PreconditionsReady);
        assert(m.descExists && m.cmdExists);
    }
    PASS("concurrent Busy + serialization");

    // 3. AlreadyReady: reusa, sem realocar.
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 o1, o2;
        VerifyFlushMockMakeLive(m);
        memset(&o1, 0, sizeof(o1)); memset(&o2, 0, sizeof(o2));
        assert(RunVerifyFlushPrewriteFlow(m, o1) == kIOReturnSuccess);
        assert(RunVerifyFlushPrewriteFlow(m, o2) == kIOReturnSuccess);
        assert(o2.status == kGA106LabFlushPrewriteStatus_AlreadyReady);
        assert((int)m.allocCount == 1 && (int)m.prepareCount == 1);
        assert(ga106ctl_check_flush_prewrite(&o2, sizeof(o2)) == 0);
    }
    PASS("AlreadyReady reuse, no second mapping");

    // 4. Falhas de recurso/página.
    {
        struct { bool af, bf, cf, df; int pm, sm; } cases[] = {
            {true,false,false,false,0,0}, {false,false,true,false,0,0},
            {false,false,false,true,0,0}, {false,false,false,false,2,0},
            {false,false,false,false,0,1}, {false,false,false,false,0,2},
            {false,false,false,false,0,3},
        };
        for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
            VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
            VerifyFlushMockMakeLive(m);
            m.allocFail = cases[i].af; m.cmdFail = cases[i].cf;
            m.bindFail = cases[i].df; m.prepareMode = cases[i].pm;
            m.segMode = cases[i].sm;
            memset(&out, 0, sizeof(out));
            IOReturn kr = RunVerifyFlushPrewriteFlow(m, out);
            assert(kr != kIOReturnSuccess || out.status == kGA106LabFlushPrewriteStatus_Failed);
            assert(out.status == kGA106LabFlushPrewriteStatus_Failed);
            assert(!m.busy); // reserva sempre liberada
            assertZeroHw(m);
        }
    }
    PASS("alloc/map/segment failures");

    // 5. Falhas semânticas (resposta válida, sem teardown de página alheia).
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        VerifyFlushMockMakeLive(m); m.command = 0x0006u; // BME ON
        memset(&out, 0, sizeof(out));
        assert(RunVerifyFlushPrewriteFlow(m, out) == kIOReturnSuccess);
        assert(out.status == kGA106LabFlushPrewriteStatus_Failed);
        assert(out.bmeEnabled == 1 && m.allocCount == 0);
    }
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        VerifyFlushMockMakeLive(m); m.command = 0x0000u; // MSE OFF
        memset(&out, 0, sizeof(out));
        assert(RunVerifyFlushPrewriteFlow(m, out) == kIOReturnSuccess);
        assert(out.status == kGA106LabFlushPrewriteStatus_Failed);
        assert(m.allocCount == 0);
    }
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        VerifyFlushMockMakeLive(m); m.gfwReady = false;
        memset(&out, 0, sizeof(out));
        assert(RunVerifyFlushPrewriteFlow(m, out) == kIOReturnSuccess);
        assert(out.status == kGA106LabFlushPrewriteStatus_Failed);
        assert(m.allocCount == 0);
    }
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        VerifyFlushMockMakeLive(m); m.device = 0x2507u; // wrong target
        memset(&out, 0, sizeof(out));
        assert(RunVerifyFlushPrewriteFlow(m, out) == kIOReturnSuccess);
        assert(out.status == kGA106LabFlushPrewriteStatus_Failed);
        assert(m.allocCount == 0);
    }
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        VerifyFlushMockMakeLive(m); m.providerPresent = false;
        memset(&out, 0, sizeof(out));
        assert(RunVerifyFlushPrewriteFlow(m, out) != kIOReturnSuccess);
        assert(!m.busy);
    }
    PASS("semantic failures BME/MSE/GFW/target/provider");

    // 6. Stop na entrada: Aborted, sem efeitos, reserva liberada.
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        VerifyFlushMockMakeLive(m); m.stopping = true;
        memset(&out, 0, sizeof(out));
        assert(RunVerifyFlushPrewriteFlow(m, out) == kIOReturnAborted);
        assert(m.allocCount == 0 && !m.busy);
    }
    PASS("stop at entry Aborted");

    // 6b. Stop durante checks pré-reserva (F-L1): Aborted, sem página, sem busy.
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        VerifyFlushMockMakeLive(m); m.stopDuringChecks = true;
        memset(&out, 0, sizeof(out));
        assert(RunVerifyFlushPrewriteFlow(m, out) == kIOReturnAborted);
        assert(m.allocCount == 0 && !m.busy);
        assert(out.status == kGA106LabFlushPrewriteStatus_Failed);
        assert(!m.descExists && !m.cmdExists);
    }
    PASS("stop during pre-reserve checks Aborted");

    // 7. Stop durante prepare bloqueado: Aborted + teardown limpo.
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        pthread_t th; ThreadArg a;
        VerifyFlushMockMakeLive(m);
        m.prepareBlocks = true; m.gateOpen = false;
        memset(&out, 0, sizeof(out));
        a.m = &m; a.out = &out; a.kr = kIOReturnError;
        assert(pthread_create(&th, NULL, runVerify, &a) == 0);
        for (int i = 0; i < 5000 && m.prepareCount == 0; i++) usleep(1000);
        assert(m.prepareCount == 1);
        m.lock(); m.stopping = true; m.unlock();
        pthread_mutex_lock(&m.gateMutex); m.gateOpen = true;
        pthread_cond_broadcast(&m.gateCond); pthread_mutex_unlock(&m.gateMutex);
        assert(pthread_join(th, NULL) == 0);
        assert(a.kr == kIOReturnAborted);
        assert(!m.descExists && !m.cmdExists); // teardown completo (clear ok)
        assert(!m.busy);
        assertBalancedTeardown(m);
    }
    PASS("stop during prepare Aborted + clean");

    // 8. Stop após sucesso: mapping sobrevive; nova chamada com stopping => Aborted.
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 o1, o2;
        VerifyFlushMockMakeLive(m);
        memset(&o1, 0, sizeof(o1)); memset(&o2, 0, sizeof(o2));
        assert(RunVerifyFlushPrewriteFlow(m, o1) == kIOReturnSuccess);
        m.lock(); m.stopping = true; m.unlock();
        assert(RunVerifyFlushPrewriteFlow(m, o2) == kIOReturnAborted);
        assert(m.descExists && m.cmdExists); // stop passivo não destrói
        assert(m.state == kGA106LabFlushPrewritePhase_PreconditionsReady);
        m.lock(); m.stopping = false; m.unlock();
        memset(&o2, 0, sizeof(o2));
        assert(RunVerifyFlushPrewriteFlow(m, o2) == kIOReturnSuccess);
        assert(o2.status == kGA106LabFlushPrewriteStatus_AlreadyReady);
    }
    PASS("post-success stop passive + reuse");

    // 9. Clear-failure: retenção controlada, sem double, sem retry.
    {
        VerifyFlushMockOps m; GA106LabFlushPrewriteV1 out;
        VerifyFlushMockMakeLive(m);
        m.allocFail = true; // força teardown com objetos? não: sem objetos => no-op
        memset(&out, 0, sizeof(out));
        assert(RunVerifyFlushPrewriteFlow(m, out) != kIOReturnSuccess);
        // Caso real: falha após prepare tentado + clear falha.
        VerifyFlushMockOps m2; GA106LabFlushPrewriteV1 o2;
        VerifyFlushMockMakeLive(m2);
        m2.prepareMode = 2; m2.clearFail = true; // prepare falha com active + clear falha
        memset(&o2, 0, sizeof(o2));
        IOReturn kr = RunVerifyFlushPrewriteFlow(m2, o2);
        assert(kr != kIOReturnSuccess);
        assert(o2.status == kGA106LabFlushPrewriteStatus_Failed);
        assert(m2.state == kGA106LabFlushPrewritePhase_CleanupFailed);
        assert((int)m2.descriptorReleaseCount == 0 && (int)m2.commandReleaseCount == 0);
        assert(m2.descExists && m2.cmdExists); // retidos, não zerados
        assert(!m2.busy);
        // Caso com endereço gravado: sucesso, depois corrupção + clear fail.
        // Teardown deve reter TUDO (incl. address record 0x1000).
        VerifyFlushMockOps m3; GA106LabFlushPrewriteV1 o4;
        VerifyFlushMockMakeLive(m3);
        memset(&o4, 0, sizeof(o4));
        assert(RunVerifyFlushPrewriteFlow(m3, o4) == kIOReturnSuccess);
        assert(m3.dmaAddr == 0x1000u);
        m3.segLen = 2048; // corrupção: reuso inválido
        m3.clearFail = true;
        GA106LabFlushPrewriteV1 o5; memset(&o5, 0, sizeof(o5));
        assert(RunVerifyFlushPrewriteFlow(m3, o5) != kIOReturnSuccess);
        assert(m3.state == kGA106LabFlushPrewritePhase_CleanupFailed);
        assert(m3.descExists && m3.cmdExists && m3.dmaAddr == 0x1000u);
        assert((int)m3.descriptorReleaseCount == 0 && (int)m3.commandReleaseCount == 0);
        // Segundo teardown: sem double (helper retorna Error sem tocar).
        IOReturn td2 = TeardownFlushPrewritePageFlowSafe(m2);
        assert(td2 != kIOReturnSuccess);
        assert((int)m2.descriptorReleaseCount == 0 && (int)m2.commandReleaseCount == 0);
        assert((int)m2.clearCount == 1); // sem double-clear
        // Nova operação após CleanupFailed: recusada, sem retry.
        GA106LabFlushPrewriteV1 o3; memset(&o3, 0, sizeof(o3));
        m2.clearFail = false;
        assert(RunVerifyFlushPrewriteFlow(m2, o3) != kIOReturnSuccess);
    }
    PASS("clear-failure retention, no double, no retry");

    // 10. Teardown repetido idempotente (Unprepared => no-op).
    {
        VerifyFlushMockOps m;
        VerifyFlushMockMakeLive(m);
        int c0 = (int)m.completeCount, r0 = (int)m.clearCount;
        assert(TeardownFlushPrewritePageFlowSafe(m) == kIOReturnSuccess);
        assert(TeardownFlushPrewritePageFlowSafe(m) == kIOReturnSuccess);
        assert((int)m.completeCount == c0 && (int)m.clearCount == r0);
    }
    PASS("repeated teardown idempotent");

    // 11. Não-interferência: verify nunca toca bancos 7/8 (só lock compartilhado
    //     em seções curtas). Concorrente com sysmem flow disjunto: ambos ok.
    {
        VerifyFlushMockOps v; GA106LabSysmemMockOps s;
        GA106LabFlushPrewriteV1 ov; GA106LabGspSysmemV1 os;
        pthread_t th; ThreadArg a;
        VerifyFlushMockMakeLive(v);
        GA106LabSysmemMockMakeLive(s);
        v.prepareBlocks = true; v.gateOpen = false;
        memset(&ov, 0, sizeof(ov)); memset(&os, 0, sizeof(os));
        a.m = &v; a.out = &ov; a.kr = kIOReturnError;
        assert(pthread_create(&th, NULL, runVerify, &a) == 0);
        for (int i = 0; i < 5000 && v.allocCount == 0; i++) usleep(1000);
        assert(RunPrepareGspSysmemFlow(s, os) == kIOReturnSuccess);
        assert(os.status == kGA106LabGspSysmemStatus_AddressReady);
        pthread_mutex_lock(&v.gateMutex); v.gateOpen = true;
        pthread_cond_broadcast(&v.gateCond); pthread_mutex_unlock(&v.gateMutex);
        assert(pthread_join(th, NULL) == 0);
        assert(a.kr == kIOReturnSuccess);
        assert((int)v.allocCount == 1 && (int)s.allocCount == 1);
    }
    PASS("no interference with selector7 path");

    printf("FLUSH_VERIFY_GROUPS=%d PASS\n", gGroups);
    return 0;
}
