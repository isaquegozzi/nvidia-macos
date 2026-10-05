// test-production-phase.cpp — P80 §10: state machine (transições, teto, teardown).
#include <cassert>
#include <cstdio>
#include "GA106LabProductionPhase.h"

static int g = 0;
#define PASS(m) do { printf("%s\n", m); g++; } while (0)

int main(void) {
    // Happy path completo 0..9.
    {
        uint32_t ph = kProdPhase_Detached;
        for (uint32_t t = 1; t <= kProdPhase_SubmissionPrepared; t++) {
            assert(ProductionPhaseCanAdvance(ph, t) == kPhaseOk);
            ph = t;
        }
        assert(ph == kProdPhase_SubmissionPrepared);
        assert(ProductionPhaseIsCeiling(ph));
    }
    PASS("PHASE_HAPPY_PATH_0_TO_9");

    // Skip / reentrada / mesmo estado.
    {
        assert(ProductionPhaseCanAdvance(1, 3) == kPhaseSkip);
        assert(ProductionPhaseCanAdvance(1, 9) == kPhaseSkip);
        assert(ProductionPhaseCanAdvance(4, 4) == kPhaseReenter);
        assert(ProductionPhaseCanAdvance(5, 3) == kPhaseReenter);
        assert(ProductionPhaseCanAdvance(0, 0) == kPhaseReenter);
        assert(ProductionPhaseCanAdvance(0, 5) == kPhaseSkip);
        assert(ProductionPhaseCanAdvance(99, 1) == kPhaseSkip);
        assert(ProductionPhaseCanAdvance(1, 99) == kPhaseSkip);
    }
    PASS("PHASE_ILLEGAL_SKIP_REENTER");

    // Teto: nada sai de SubmissionPrepared; LiveBlocked só via latch.
    {
        assert(ProductionPhaseCanAdvance(9, 10) == kPhaseOk); // latch permitido
        assert(ProductionPhaseCanAdvance(9, 9) == kPhaseReenter);
        assert(ProductionPhaseCanAdvance(2, 10) == kPhaseOk);
        assert(ProductionPhaseCanAdvance(0, 10) == kPhaseReenter); // Detached sem latch
        assert(ProductionPhaseCanAdvance(10, 1) == kPhaseTerminal);
        assert(ProductionPhaseCanAdvance(10, 10) == kPhaseTerminal);
        assert(ProductionPhaseCanAdvance(10, 9) == kPhaseTerminal);
        assert(!ProductionPhaseIsCeiling(kProdPhase_VmPrepared));
    }
    PASS("PHASE_LIVE_CEILING_TERMINAL");

    // Teardown determinístico por estado (stop/clientClose em todo estado).
    {
        assert(!ProductionPhaseTeardownOk(kProdPhase_Detached));
        for (uint32_t s = 1; s <= 9; s++) assert(ProductionPhaseTeardownOk(s));
        assert(!ProductionPhaseTeardownOk(kProdPhase_LiveBlocked));
        assert(!ProductionPhaseTeardownOk(99));
    }
    PASS("PHASE_TEARDOWN_MAP_EVERY_STATE");

    // Teardown volta a Attached (idempotente por construção: Attached->Attached ok).
    {
        assert(ProductionPhaseCanAdvance(6, kProdPhase_Attached) == kPhaseOk);
        assert(ProductionPhaseCanAdvance(1, kProdPhase_Attached) == kPhaseReenter); // no-op seguro
    }
    PASS("PHASE_TEARDOWN_TARGET_ATTACHED");

    printf("PRODUCTION_PHASE_TESTS = PASS (%d groups)\n", g);
    return 0;
}
