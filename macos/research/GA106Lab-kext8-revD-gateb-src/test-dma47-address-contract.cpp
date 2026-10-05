// test-dma47-address-contract.cpp — offline DMA47 contract (6H + HARD-6H T1).
//
// Purpose: falsify "any IOVA the KPI returns is GPU-usable" for the
// sysmem/DMA path WITHOUT live execution.
//
// Production: GA106LabSysmemAddressValid47() (CoreValidate) enforces HI/LO
// round-trip + no-wrap + 4KiB page entirely below 2^47; wired into all 4
// validateAddressWidth sites (RealOps sysmem+flush, MockOps mirrors).
// Upstream: nova-hal-tu102-gfw.rs dma_mask 47; P78 VmContract VA<2^47
// (GA106LabModelContracts.h:64, SHADOW-P78-VM-MMU-SIM EVIDENCE-BINDING).
//
// This test proves the legacy gap (old Valid accepts >=2^47) + locks the
// wired behavior (group G ops-level).
// No IOKit, no HW, no sudo, deterministic.

#include <cassert>
#include <cstdint>
#include <cstdio>

#include "GA106LabCoreValidate.h"
#include "GA106LabMockOps.hpp"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

// Candidate future contract (NOT production yet): width-bounded.
static inline int SysmemAddressValid47(uint64_t addr) {
    const uint64_t kLimit = (1ULL << 47);
    if (!GA106LabSysmemAddressValid(addr)) return 0;
    if (addr >= kLimit) return 0;
    if (addr + (uint64_t)4096u > kLimit) return 0;
    if (addr + (uint64_t)4096u < addr) return 0;
    return 1;
}

int main(void) {
    // A. Current validator accepts high addresses (gap proof).
    {
        // 1<<47 is round-trippable and wrap-safe, so current says VALID.
        uint64_t a = (1ULL << 47);
        assert(GA106LabSysmemAddressValid(a) == 1);
        assert(SysmemAddressValid47(a) == 0);
    }
    PASS("A1 CURRENT_ACCEPTS_1ULL47 CANDIDATE_REJECTS (gap proven)");

    {
        uint64_t wmax = 0xFFFFFFFFFFFFF000ULL;  // wraps on +4096 -> rejected
        assert(GA106LabSysmemAddressValid(wmax) == 0);
        assert(SysmemAddressValid47(wmax) == 0);
        // High but non-wrapping address: clean gap proof.
        uint64_t b = (1ULL << 48);
        assert(GA106LabSysmemAddressValid(b) == 1);
        assert(SysmemAddressValid47(b) == 0);
    }
    PASS("A2 CURRENT_ACCEPTS_1ULL48 CANDIDATE_REJECTS");

    // B. Candidate matrix: low addresses unaffected.
    {
        assert(SysmemAddressValid47(0x1000ULL) == 1);
        assert(SysmemAddressValid47(0x0ULL) == 1);
        assert(SysmemAddressValid47(0xfb000000ULL) == 1);  // BAR0-like base
    }
    PASS("B1 LOW_ADDRESSES STILL VALID");

    // C. Boundary: last valid 4KiB page below 2^47.
    {
        const uint64_t kLimit = (1ULL << 47);
        assert(SysmemAddressValid47(kLimit - 4096u) == 1);
        assert(SysmemAddressValid47(kLimit - 4095u) == 0);  // +4096 exceeds limit
        assert(SysmemAddressValid47(kLimit - 1u) == 0);     // +4096 exceeds limit
        assert(SysmemAddressValid47(kLimit) == 0);
        assert(SysmemAddressValid47(kLimit + 0x1000u) == 0);
    }
    PASS("C1 BOUNDARY_LAST_PAGE");

    // D. Consistency with P78 VA limit (<2^47).
    {
        // GA106LabModelContracts.h:64 kVaLimit = 1ULL<<47 — same constant.
        const uint64_t kVaLimit = (1ULL << 47);
        assert(SysmemAddressValid47(kVaLimit - 4096u) == 1);
        // Current sysmem validator alone would allow IOVA the VM layer rejects.
        assert(GA106LabSysmemAddressValid(kVaLimit + 0x1000u) == 1);
    }
    PASS("D1 CONSISTENT_WITH_P78_VA47");

    // E. Wrap safety preserved in candidate.
    {
        uint64_t w = 0xFFFFFFFFFFFFF000ULL;
        assert(SysmemAddressValid47(w) == 0);
        assert(GA106LabSysmemAddressValid(w) == 0);  // wrap check fires
    }
    PASS("E1 WRAP_STILL_REJECTED");

    // F. Production helper (R2 additive) matches candidate on all vectors.
    {
        const uint64_t vec[] = {
            0x0ULL, 0x1000ULL, 0xfb000000ULL,
            (1ULL << 47) - 4096u, (1ULL << 47) - 4095u,
            (1ULL << 47) - 1u, (1ULL << 47), (1ULL << 47) + 0x1000u,
            (1ULL << 48), 0xFFFFFFFFFFFFF000ULL
        };
        for (unsigned i = 0; i < sizeof(vec) / sizeof(vec[0]); i++) {
            assert(GA106LabSysmemAddressValid47(vec[i]) == SysmemAddressValid47(vec[i]));
        }
    }
    PASS("F1 PROD_HELPER_MATCHES_CANDIDATE");

    // G. Ops-level wiring (R2): all 4 validateAddressWidth sites enforce 47-bit.
    {
        GA106LabSysmemMockOps sops;
        GA106LabSysmemMockMakeLive(sops);
        assert(sops.validateAddressWidth(0x1000ULL) == true);
        assert(sops.validateAddressWidth((1ULL << 47) - 4096u) == true);
        assert(sops.validateAddressWidth(1ULL << 47) == false);
        assert(sops.validateAddressWidth(1ULL << 48) == false);
        VerifyFlushMockOps fops;
        VerifyFlushMockMakeLive(fops);
        assert(fops.validateAddressWidth(0x1000ULL) == true);
        assert(fops.validateAddressWidth(1ULL << 47) == false);
    }
    PASS("G1 OPS_WIDTH_WIRED sysmem+flush reject-high");

    printf("test-dma47-address-contract: ALL PASS (%d grupos; gap probe + Valid47 wired)\n", gGroups);
    return 0;
}
