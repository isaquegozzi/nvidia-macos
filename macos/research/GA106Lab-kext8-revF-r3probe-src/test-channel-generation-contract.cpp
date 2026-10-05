// test-channel-generation-contract.cpp — offline R1 P77 stale-handle ledger (HARD-6H T2).
//
// Handle = (index, generation). Free bumps generation; slot reusable.
// Stale handle (old generation) must be detected on every op (bind/schedule/
// submit/free). Double-free detected. Ordering: alloc -> bind -> schedule ->
// submit -> retire -> free; bind-before-alloc and schedule-before-bind rejected.
//
// Pure ledger, deterministic, single-threaded. No IOKit/HW/sudo. R1 test-only.

#include <cassert>
#include <cstdint>
#include <cstdio>

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static const int kSlots = 8;

enum Code : uint8_t { C_OK = 0, C_STALE, C_STATE, C_OOB };

struct Slot {
    uint32_t gen = 0;
    bool allocated = false;
    bool bound = false;
    bool scheduled = false;
};

struct Table {
    Slot s[kSlots];
};

struct Handle {
    int idx;
    uint32_t gen;
};

static Code Check(Table &t, Handle h) {
    if (h.idx < 0 || h.idx >= kSlots) return C_OOB;
    if (!t.s[h.idx].allocated || t.s[h.idx].gen != h.gen) return C_STALE;
    return C_OK;
}

static Code Alloc(Table &t, int idx, Handle &out) {
    if (idx < 0 || idx >= kSlots) return C_OOB;
    if (t.s[idx].allocated) return C_STATE;  // duplicate alloc
    t.s[idx].allocated = true;
    t.s[idx].bound = false;
    t.s[idx].scheduled = false;
    out.idx = idx;
    out.gen = t.s[idx].gen;
    return C_OK;
}

static Code Bind(Table &t, Handle h) {
    Code c = Check(t, h);
    if (c != C_OK) return c;
    if (t.s[h.idx].bound) return C_STATE;
    t.s[h.idx].bound = true;
    return C_OK;
}

static Code Schedule(Table &t, Handle h) {
    Code c = Check(t, h);
    if (c != C_OK) return c;
    if (!t.s[h.idx].bound) return C_STATE;  // schedule-before-bind
    if (t.s[h.idx].scheduled) return C_STATE;
    t.s[h.idx].scheduled = true;
    return C_OK;
}

static Code Free(Table &t, Handle h) {
    Code c = Check(t, h);
    if (c != C_OK) return c;
    if (t.s[h.idx].scheduled) return C_STATE;  // free-while-scheduled: unschedule first
    t.s[h.idx].allocated = false;
    t.s[h.idx].bound = false;
    t.s[h.idx].gen++;  // bump: old handles go stale
    return C_OK;
}

static Code Unschedule(Table &t, Handle h) {
    Code c = Check(t, h);
    if (c != C_OK) return c;
    if (!t.s[h.idx].scheduled) return C_STATE;
    t.s[h.idx].scheduled = false;
    return C_OK;
}

int main(void) {
    // G1: happy lifecycle.
    {
        Table t;
        Handle h{-1, 0};
        assert(Alloc(t, 3, h) == C_OK);
        assert(Bind(t, h) == C_OK);
        assert(Schedule(t, h) == C_OK);
        assert(Unschedule(t, h) == C_OK);
        assert(Free(t, h) == C_OK);
    }
    PASS("G1 HAPPY lifecycle");

    // G2: duplicate alloc rejected.
    {
        Table t;
        Handle a{-1, 0}, b{-1, 0};
        assert(Alloc(t, 1, a) == C_OK);
        assert(Alloc(t, 1, b) == C_STATE);
        assert(Free(t, a) == C_OK);
    }
    PASS("G2 DUP-ALLOC rejected");

    // G3: stale handle after free+realloc (generation reuse).
    {
        Table t;
        Handle old{-1, 0}, fresh{-1, 0};
        assert(Alloc(t, 2, old) == C_OK);
        assert(Free(t, old) == C_OK);
        assert(Alloc(t, 2, fresh) == C_OK);
        assert(fresh.gen == old.gen + 1u);
        assert(Bind(t, old) == C_STALE);     // old generation dead
        assert(Schedule(t, old) == C_STALE);
        assert(Free(t, old) == C_STALE);     // double-free via stale
        assert(Bind(t, fresh) == C_OK);      // fresh works
        assert(Free(t, fresh) == C_OK);
    }
    PASS("G3 STALE-HANDLE + generation-reuse + double-free");

    // G4: bind-before-alloc and schedule-before-bind.
    {
        Table t;
        Handle h{5, 0};  // never allocated (gen 0 matches, allocated false)
        assert(Bind(t, h) == C_STALE);
        Handle a{-1, 0};
        assert(Alloc(t, 5, a) == C_OK);
        assert(Schedule(t, a) == C_STATE);  // not bound yet
        assert(Bind(t, a) == C_OK);
        assert(Schedule(t, a) == C_OK);
        assert(Unschedule(t, a) == C_OK);
        assert(Free(t, a) == C_OK);
    }
    PASS("G4 BIND-BEFORE-ALLOC + SCHEDULE-BEFORE-BIND");

    // G5: free-while-scheduled blocked; OOB handles.
    {
        Table t;
        Handle a{-1, 0};
        assert(Alloc(t, 0, a) == C_OK);
        assert(Bind(t, a) == C_OK);
        assert(Schedule(t, a) == C_OK);
        assert(Free(t, a) == C_STATE);  // must unschedule first
        assert(Unschedule(t, a) == C_OK);
        assert(Free(t, a) == C_OK);
        Handle oob{-1, 0};
        assert(Alloc(t, 8, oob) == C_OOB);
        Handle bad{99, 0};
        assert(Bind(t, bad) == C_OOB);
    }
    PASS("G5 FREE-WHILE-SCHEDULED + OOB");

    // G6: mimo gaps — unschedule-after-free, free-never-allocated,
    // free-bound-not-scheduled. Contract limit note: gen is uint32_t and
    // wraps at UINT32_MAX (a forged gen-0 handle could alias after 2^32
    // frees of one slot; impractical offline, must be re-examined for any
    // future kernel handle table — e.g. never reuse gen 0 or widen to 64b).
    {
        Table t;
        Handle a{-1, 0};
        assert(Alloc(t, 4, a) == C_OK);
        assert(Bind(t, a) == C_OK);
        assert(Free(t, a) == C_OK);          // bound-but-not-scheduled frees fine
        assert(Unschedule(t, a) == C_STALE); // unschedule-after-free
        Handle forged{6, 0};                 // never allocated
        assert(Free(t, forged) == C_STALE);
        assert(Unschedule(t, forged) == C_STALE);
        assert(Schedule(t, forged) == C_STALE);
    }
    PASS("G6 UNSCHED-AFTER-FREE + FREE-NEVER-ALLOC + FREE-BOUND-ONLY");

    printf("test-channel-generation-contract: ALL PASS (%d grupos; P77 ledger)\n", gGroups);
    return 0;
}
