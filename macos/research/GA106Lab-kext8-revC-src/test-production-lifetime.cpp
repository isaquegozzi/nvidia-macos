// test-production-lifetime.cpp — P80 §5/§10/§11/§15: ownership, teardown,
// fault injection (sem UAF/double-release/stale/leak; estado final seguro).
// Double autocontido no estilo MockOps: objetos kernel modelados por contadores.
#include <cassert>
#include <cstdio>
#include "GA106LabProductionPhase.h"
#include "GA106LabModelContracts.h"

struct MockKobj {
    int retains = 1;
    int releases = 0;
    bool alive() const { return releases < retains; }
    void release() { if (alive()) releases++; }
};

struct ServiceDouble {
    uint32_t phase = kProdPhase_Detached;
    bool stopping = false;
    bool busy = false;
    bool clientOpen = false;
    MockKobj * desc = NULL;
    MockKobj * cmd = NULL;
    int lastDescRetains = 0; // ledger: contagens finais p/ auditoria pós-delete
    int lastDescReleases = 0;
    bool allocFail = false;
    bool mapFail = false;
    GA106LabChannelContract ch{};
    bool chOk = false;
    GA106LabVmContract vm{};
    bool vmOk = false;

    bool start() {
        if (phase != kProdPhase_Detached) return false;
        phase = kProdPhase_Attached;
        return true;
    }
    bool openClient() {
        if (stopping || clientOpen || phase < kProdPhase_Attached) return false;
        clientOpen = true;
        return true;
    }
    void clientClose() { clientOpen = false; } // F-P80-02: NÃO libera (só stop libera; idempotente)
    bool reserve() {
        if (busy || stopping) return false;
        busy = true;
        return true;
    }
    void unreserve() { busy = false; }
    bool allocPage() {
        if (!reserve()) return false;
        bool ok = false;
        if (!allocFail) { desc = new MockKobj(); cmd = new MockKobj(); ok = true; }
        unreserve();
        return ok;
    }
    bool mapPage() {
        if (!reserve()) return false;
        bool ok = (desc && desc->alive() && !mapFail);
        unreserve();
        return ok;
    }
    // stop(): flag terminal + espera !busy (aqui: checagem) + teardown 1x.
    bool stop() {
        stopping = true;
        if (busy) return false; // dreno: chamador retenta (sem lock retido no kernel)
        teardown();
        if (ProductionPhaseTeardownOk(phase)) phase = kProdPhase_Attached;
        return true;
    }
    void teardown() {
        if (cmd) { cmd->release(); if (!cmd->alive()) { delete cmd; } cmd = NULL; }
        if (desc) {
            desc->release();
            lastDescRetains = desc->retains; lastDescReleases = desc->releases;
            if (!desc->alive()) { delete desc; }
            desc = NULL;
        }
        chOk = false; vmOk = false;
    }
    bool prepareChannel(const GA106LabChannelContract & c) {
        if (!GA106LabChannelContractValid(&c)) return false;
        if (ProductionPhaseCanAdvance(phase, kProdPhase_ChannelPrepared) != kPhaseOk)
            return false;
        if (stopping || !desc) return false;
        ch = c; chOk = true; phase = kProdPhase_ChannelPrepared;
        return true;
    }
    // UAF probe: tocar desc após teardown deve ser recusado.
    bool touchDesc() const { return desc && desc->alive() && !stopping; }
};

static int g = 0;
#define PASS(m) do { printf("%s\n", m); g++; } while (0)

int main(void) {
    // stop/clientClose em todo estado: sempre seguro, teardown 1x.
    for (uint32_t s = 1; s <= 9; s++) {
        ServiceDouble d;
        assert(d.start());
        d.phase = s;
        d.clientOpen = true;
        d.clientClose();
        assert(!d.clientOpen);
        assert(d.desc == NULL); // clientClose NÃO libera (nada alocado aqui)
        assert(d.stop());
        assert(d.phase == kProdPhase_Attached);
        assert(d.stop()); // stop idempotente
        assert(d.phase == kProdPhase_Attached);
    }
    PASS("STOP_CLOSE_EVERY_STATE_SAFE_IDEMPOTENT");

    // Fault injection por estágio.
    {
        ServiceDouble d;
        assert(d.start());
        d.allocFail = true;
        assert(!d.allocPage());
        assert(d.desc == NULL); // sem leak parcial
        d.allocFail = false;
        assert(d.allocPage());
        d.mapFail = true;
        assert(!d.mapPage());
        d.mapFail = false;
        assert(d.mapPage());
        GA106LabChannelContract bad{};
        assert(!d.prepareChannel(bad)); // contrato inválido
        assert(!d.chOk);
        d.phase = kProdPhase_FlushPreconditionsVerified;
        assert(!d.prepareChannel(bad)); // fase certa + contrato ruim: ainda recusa
        assert(!d.chOk);
        d.phase = kProdPhase_FlushPreconditionsVerified;
        GA106LabChannelContract c{};
        c.chid = 1; c.runq = 0; c.engineKnown = 1; c.vasHandle = 1;
        c.gpfifoLength = 64; c.scheduled = 1;
        assert(d.prepareChannel(c));
        assert(!d.prepareChannel(c)); // double prepare recusado (reentrada)
        assert(d.stop());
        assert(!d.touchDesc()); // UAF impossível: ponteiro nulado + flag
        assert(d.desc == NULL && d.cmd == NULL);
    }
    PASS("FAULT_INJECTION_EVERY_STAGE_NO_UAF_NO_LEAK");

    // Double release / stale handle.
    {
        ServiceDouble d;
        assert(d.start());
        assert(d.allocPage());
        assert(d.stop());
        // Handle antigo: objeto morto e ponteiro do serviço nulado (ledger, sem UAF).
        assert(d.lastDescReleases == d.lastDescRetains && d.lastDescRetains == 1);
        assert(d.desc == NULL);
        assert(!d.touchDesc());
        // Teardown duplo não relibera (ledger estável).
        d.teardown();
        assert(d.lastDescReleases == d.lastDescRetains && d.lastDescRetains == 1);
    }
    PASS("NO_DOUBLE_RELEASE_NO_STALE_HANDLE");

    // Retry após falha: permitido e converge.
    {
        ServiceDouble d;
        assert(d.start());
        d.allocFail = true;
        assert(!d.allocPage());
        d.allocFail = false;
        assert(d.allocPage());
        assert(d.mapPage());
        assert(d.stop());
    }
    PASS("RETRY_AFTER_FAILURE_CONVERGES");

    // Reserva não vaza em falha; busy bloqueia segunda operação.
    {
        ServiceDouble d;
        assert(d.start());
        d.busy = true; // operação longa em curso (simulada)
        assert(!d.reserve());
        assert(!d.allocPage());
        assert(!d.stop()); // stop pede dreno: retentar depois
        d.busy = false;
        assert(d.stop());
    }
    PASS("RESERVATION_BUSY_DRAIN_SEMANTICS");

    // F-P80-02: close nunca libera; stop teardown 1x; idempotente em toda ordem.
    {
        ServiceDouble d;
        assert(d.start());
        assert(d.allocPage());
        MockKobj *held = d.desc;
        assert(held != NULL && held->alive());
        d.clientOpen = true;
        d.clientClose(); // close: só fecha slot, objetos intactos
        assert(!d.clientOpen);
        assert(d.desc == held && held->alive()); // close NÃO liberou
        d.clientClose(); // double close: no-op seguro
        assert(!d.clientOpen);
        assert(d.desc == held && held->alive());
        assert(d.stop()); // stop: teardown 1x
        assert(d.desc == NULL && d.cmd == NULL);
        assert(d.lastDescReleases == d.lastDescRetains && d.lastDescRetains == 1);
        d.clientClose(); // close-após-stop: no-op seguro
        assert(d.stop()); // stop-após-stop: idempotente
        assert(d.stop());
        assert(d.lastDescReleases == 1); // sem double-release
        assert(d.phase == kProdPhase_Attached);
    }
    PASS("F_P80_02_CLOSE_NEVER_RELEASES_STOP_TEARDOWN_ONCE_IDEMPOTENT");

    printf("PRODUCTION_LIFETIME_TESTS = PASS (%d groups)\n", g);
    return 0;
}
