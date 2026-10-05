// test-completion-chain-contract.cpp — offline R1 7-state completion model (HARD-6H).
//
// Chain per fence (prompt B3, not reduced to single `done`):
//   accepted -> submitted -> executed -> visible
//   -> completion-published -> notified -> retired
// Forward-only; fault pins to Failed (terminal unless reset model allows).
// Rules under test:
//   retire-before-visible FORBIDDEN; notify-before-publish FORBIDDEN;
//   teardown with pending work drains deterministically (pending->Failed);
//   fault during submit / fault after accept-before-execute -> Failed with code;
//   duplicate advance -> deterministic fault; no success after fatal fault.
//
// No IOKit/HW/sudo. Deterministic, single-threaded. R1 test-only.

#include <cassert>
#include <cstdint>
#include <cstdio>

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

enum Stage : uint8_t { S_NONE = 0, S_ACCEPTED, S_SUBMITTED, S_EXECUTED, S_VISIBLE,
                       S_PUBLISHED, S_NOTIFIED, S_RETIRED, S_FAILED };
enum Code : uint8_t { C_OK = 0, C_ORDER, C_STATE, C_FAULT, C_PENDING };

struct Fence {
    Stage s = S_NONE;
    Code lastFault = C_OK;
};

static Code Advance(Fence &f, Stage to) {
    if (f.s == S_FAILED) { f.lastFault = C_FAULT; return C_FAULT; }
    if (f.s == S_RETIRED) { f.lastFault = C_STATE; return C_STATE; }
    // Legal: exactly next stage, or any-stage -> FAILED (fault injection).
    if (to == S_FAILED) { f.s = S_FAILED; f.lastFault = C_FAULT; return C_OK; }
    if ((uint8_t)to != (uint8_t)f.s + 1u && !(f.s == S_NONE && to == S_ACCEPTED)) {
        f.lastFault = C_ORDER;
        return C_ORDER;
    }
    f.s = to;
    return C_OK;
}

int main(void) {
    // C1: happy path through all 7 states.
    {
        Fence f;
        assert(Advance(f, S_ACCEPTED) == C_OK);
        assert(Advance(f, S_SUBMITTED) == C_OK);
        assert(Advance(f, S_EXECUTED) == C_OK);
        assert(Advance(f, S_VISIBLE) == C_OK);
        assert(Advance(f, S_PUBLISHED) == C_OK);
        assert(Advance(f, S_NOTIFIED) == C_OK);
        assert(Advance(f, S_RETIRED) == C_OK);
        assert(f.s == S_RETIRED);
    }
    PASS("C1 HAPPY 7-state retire");

    // C2: retire-before-visible forbidden (skip from executed).
    {
        Fence f;
        assert(Advance(f, S_ACCEPTED) == C_OK);
        assert(Advance(f, S_SUBMITTED) == C_OK);
        assert(Advance(f, S_EXECUTED) == C_OK);
        assert(Advance(f, S_RETIRED) == C_ORDER);
        assert(f.s == S_EXECUTED);  // state unchanged on order fault
    }
    PASS("C2 RETIRE-BEFORE-VISIBLE forbidden");

    // C3: notify-before-publish forbidden.
    {
        Fence f;
        assert(Advance(f, S_ACCEPTED) == C_OK);
        assert(Advance(f, S_SUBMITTED) == C_OK);
        assert(Advance(f, S_EXECUTED) == C_OK);
        assert(Advance(f, S_VISIBLE) == C_OK);
        assert(Advance(f, S_NOTIFIED) == C_ORDER);
        assert(f.s == S_VISIBLE);
    }
    PASS("C3 NOTIFY-BEFORE-PUBLISH forbidden");

    // C4: fault during submit -> Failed; no further success.
    {
        Fence f;
        assert(Advance(f, S_ACCEPTED) == C_OK);
        assert(Advance(f, S_FAILED) == C_OK);
        assert(f.s == S_FAILED);
        assert(Advance(f, S_SUBMITTED) == C_FAULT);
        assert(Advance(f, S_RETIRED) == C_FAULT);
    }
    PASS("C4 FAULT-DURING-SUBMIT terminal");

    // C5: fault after accept before execute -> Failed with code.
    {
        Fence f;
        assert(Advance(f, S_ACCEPTED) == C_OK);
        assert(Advance(f, S_SUBMITTED) == C_OK);
        assert(Advance(f, S_FAILED) == C_OK);
        assert(f.lastFault == C_FAULT);
        assert(Advance(f, S_EXECUTED) == C_FAULT);
    }
    PASS("C5 FAULT-AFTER-ACCEPT terminal");

    // C6: duplicate advance forbidden (same stage twice).
    {
        Fence f;
        assert(Advance(f, S_ACCEPTED) == C_OK);
        assert(Advance(f, S_ACCEPTED) == C_ORDER);
        assert(Advance(f, S_SUBMITTED) == C_OK);
        assert(Advance(f, S_SUBMITTED) == C_ORDER);
    }
    PASS("C6 DUPLICATE-ADVANCE forbidden");

    // C7: teardown with pending work -> deterministic drain to Failed.
    {
        Fence a, b, c;
        assert(Advance(a, S_ACCEPTED) == C_OK);
        assert(Advance(a, S_SUBMITTED) == C_OK);
        assert(Advance(b, S_ACCEPTED) == C_OK);
        // c untouched (S_NONE).
        Fence *pending[] = {&a, &b};
        int drained = 0;
        for (unsigned i = 0; i < 2; i++) {
            if (pending[i]->s != S_NONE && pending[i]->s != S_RETIRED &&
                pending[i]->s != S_FAILED) {
                assert(Advance(*pending[i], S_FAILED) == C_OK);
                drained++;
            }
        }
        assert(drained == 2);
        assert(a.s == S_FAILED && b.s == S_FAILED && c.s == S_NONE);
    }
    PASS("C7 TEARDOWN-PENDING drain deterministic");

    printf("test-completion-chain-contract: ALL PASS (%d grupos; 7-state, no done-collapse)\n", gGroups);
    return 0;
}
