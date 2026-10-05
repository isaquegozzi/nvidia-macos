// test-static-identity.cpp — testes do MESMO shared flow do selector 6.
//
// Instancia RunStaticIdentityReadsFlow<GA106LabMockOps> (o mesmo template
// que GA106Lab::readStaticIdentity usa com GA106LabRealOps). Contadores
// SOMENTE dentro de MockOps::read32/createMap/releaseMap + readOffsets[].
// Revisão single-read: somente BOOT_42 @0xA00 (BOOT_2 removido).
// BOOT_42 usa máscara 10-bit dedicada (helper distinto de BOOT_0).

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"
#include "GA106LabStaticIdentityFlow.hpp"
#include "GA106LabMockOps.hpp"
#include "ga106ctl-validate.h"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

int main(void)
{
    // 0. Allowlist fixa + máscaras distintas.
    {
        GA106LabMockOps m;
        GA106LabMockMakeLive(m);
        assert(m.boot42Offset() == 0xA00u);
        assert(m.boot42Width() == 4);
        // BOOT_0 intacto (9-bit) vs BOOT_42 (10-bit).
        assert(GA106LabChipFromRaw(0x176000A1u) == 0x176u);
        assert(GA106LabBoot42ChipFromRaw(0x176000A1u) == 0x176u);
        // Contraexemplo do Astra: passa em 9-bit, falha em 10-bit.
        assert(GA106LabChipFromRaw(0x376000A1u) == 0x176u);
        assert(GA106LabBoot42ChipFromRaw(0x376000A1u) == 0x376u);
        assert(GA106LabBoot42IsGA106(0x376000A1u) == 0);
        assert(GA106LabBoot42IsGA106(0x176000A1u) == 1);
    }
    PASS("ALLOWLIST_FIXED = {0xA00}; BOOT42_MASK=0x3FF00000; BOOT0 intacto");

    // 1. Sucesso: 1 read [0xA00], 1 create, 1 release, ABI 32B.
    {
        GA106LabMockOps m;
        GA106LabStaticIdentityV1 out;
        GA106LabMockMakeLive(m);
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunStaticIdentityReadsFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(m.read32Count == 1);
        assert(m.readOffsetsCount == 1 && m.readOffsets[0] == 0xA00u);
        assert(m.mapCreateCount == 1 && m.mapReleaseCount == 1);
        assert(m.liveMap == false);
        assert(out.size == 32 && out.version == 1);
        assert(out.status == kGA106LabStaticIdentityStatus_MapReleased);
        assert(out.readCount == 1);
        assert(out.decodedChipId == 0x176u);
        assert((out.validMask & kGA106LabStaticIdentityValid_ReleaseConfirmed) != 0);
        assert(ga106ctl_check_static_identity(&out, sizeof(out)) == 0);
        assert(ga106ctl_static_identity_cli_exit(1, &out, sizeof(out)) == 0);
    }
    PASS("SUCCESS 1 read [0xA00], 1 release, ABI 32B PASS");

    // 2. Preconditions -> 0 reads.
    {
        GA106LabMockOps m;
        GA106LabStaticIdentityV1 out;
        GA106LabMockMakeLive(m);
        m.providerIsPCI = false; m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 0 && m.mapReleaseCount == 0);

        GA106LabMockMakeLive(m);
        m.cmdBefore = 0x1u; m.cmdAfter = 0x1u; m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 0);

        GA106LabMockMakeLive(m);
        m.nRes = 0; m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 0);

        GA106LabMockMakeLive(m);
        m.nRes = 2;
        m.res[0].base = 0xFB000000ULL; m.res[0].len = 0x1000000ULL; m.res[0].present = true;
        m.res[1].base = 0xFB000000ULL; m.res[1].len = 0x1000000ULL; m.res[1].present = true;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 0);

        GA106LabMockMakeLive(m);
        m.mapNull = true; m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 1 && m.mapReleaseCount == 0);

        const uint32_t bad[5] = {0x1300u, 0x1400u, 0x1000u, 0x0100u, 0x1200u};
        for (int k = 0; k < 5; k++) {
            GA106LabMockMakeLive(m);
            m.mapOpts = bad[k]; m.resetCounts();
            memset(&out, 0, sizeof(out));
            assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
            assert(m.read32Count == 0 && m.mapReleaseCount == 1);
        }

        GA106LabMockMakeLive(m);
        m.vaZero = true; m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 1);

        GA106LabMockMakeLive(m);
        m.useCustomBoot42Offset = true; m.customBoot42Offset = 1; m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 1);
    }
    PASS("PRECONDITIONS 0 reads");

    // 3. BOOT42 inválido: 1 read [0xA00], FAIL (inclui contraexemplo).
    {
        const uint32_t bad[4] = {0x00000000u, 0xFFFFFFFFu, 0x174000A1u, 0x376000A1u};
        for (int k = 0; k < 4; k++) {
            GA106LabMockOps m;
            GA106LabStaticIdentityV1 out;
            GA106LabMockMakeLive(m);
            m.boot42Raw = bad[k]; m.resetCounts();
            memset(&out, 0, sizeof(out));
            assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
            assert(m.read32Count == 1 && m.mapReleaseCount == 1);
            assert(m.readOffsetsCount == 1 && m.readOffsets[0] == 0xA00u);
        }
    }
    PASS("BOOT42_FAIL 1 read [0xA00] FAIL (0/absent/wrong-chip/0x376)");

    // 4. Stepping variante com chip 0x176 passa.
    {
        GA106LabMockOps m;
        GA106LabStaticIdentityV1 out;
        GA106LabMockMakeLive(m);
        m.boot42Raw = 0x176000B0u; m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunStaticIdentityReadsFlow(m, out) == kIOReturnSuccess);
        assert(m.read32Count == 1);
    }
    PASS("STEPPING variant PASS");

    // 5. Command after-change: 1 read, 1 release, FAIL.
    {
        GA106LabMockOps m;
        GA106LabStaticIdentityV1 out;
        GA106LabMockMakeLive(m);
        m.cmdAfter = 0x7u; m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunStaticIdentityReadsFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 1 && m.mapReleaseCount == 1);
    }
    PASS("COMMAND_CHANGED 1 read FAIL");

    // 6. CLI propagation.
    {
        GA106LabMockOps m;
        GA106LabStaticIdentityV1 good, bad;
        GA106LabMockMakeLive(m);
        m.resetCounts();
        memset(&good, 0, sizeof(good));
        assert(RunStaticIdentityReadsFlow(m, good) == kIOReturnSuccess);
        assert(ga106ctl_static_identity_cli_exit(1, &good, sizeof(good)) == 0);
        assert(ga106ctl_static_identity_cli_exit(0, &good, sizeof(good)) != 0);
        assert(ga106ctl_static_identity_cli_exit(1, &good, 16) != 0);
        bad = good; bad.version = 99;
        assert(ga106ctl_static_identity_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.status = kGA106LabStaticIdentityStatus_Failed;
        assert(ga106ctl_static_identity_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.readCount = 2;
        assert(ga106ctl_static_identity_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.validMask = 0;
        assert(ga106ctl_static_identity_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.boot42Raw = 0x376000A1u;
        assert(ga106ctl_static_identity_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.boot42Raw = 0xFFFFFFFFu;
        assert(ga106ctl_static_identity_cli_exit(1, &bad, sizeof(bad)) != 0);
    }
    PASS("CLI_PROPAGATION PASS");

    printf("test-static-identity: ALL PASS (%d grupos; ONE_SHARED_FLOW single-read)\n", gGroups);
    return 0;
}
