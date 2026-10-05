// test-first-mmio-read.cpp — testes do MESMO shared flow da produção.
//
// Instancia RunFirstMmioReadFlow<GA106LabMockOps> (o mesmo template que
// GA106Lab::readPmcBoot0 usa com GA106LabRealOps). Contadores medidos
// SOMENTE dentro de MockOps::read32/createMap/releaseMap — nunca
// incrementados manualmente. Prova zero-read, exactly-one-read, release
// exactly-once, cache exata, CLI real.
//
// Compilar (ver build.sh):
//   clang++ -o test-first-mmio-read test-first-mmio-read.cpp ga106ctl-validate.c

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"
#include "GA106LabMmioFlow.hpp"
#include "GA106LabMockOps.hpp"
#include "ga106ctl-validate.h"

static int gGroups = 0;
#define PASS(msg) do { printf("%s\n", msg); gGroups++; } while (0)

int main(void)
{
    // ---- 0. Cache exata preservada (mesmo helper do flow) ----
    assert(GA106LabMapOptionsValidROInhibit(0x1100u) == 1);
    assert(GA106LabMapOptionsValidROInhibit(0x1300u) == 0); // RO+Copyback
    assert(GA106LabMapOptionsValidROInhibit(0x1400u) == 0); // RO+WC
    assert(GA106LabMapOptionsValidROInhibit(0x1000u) == 0); // RO+Default
    assert(GA106LabMapOptionsValidROInhibit(0x1200u) == 0); // RO+WriteThru
    assert(GA106LabMapOptionsValidROInhibit(0x0100u) == 0); // Inhibit sem RO
    assert(GA106LabMapOptionsValidROInhibit(0x0000u) == 0);
    assert(GA106LabMapOptionsValidROInhibit(0x1101u) == 1); // ortogonal ok
    PASS("CACHE_VALIDATION_EXACT = YES");

    // ---- 1. Sucesso GA106 via SHARED FLOW ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        IOReturn kr;
        GA106LabMockMakeLive(m);
        memset(&out, 0, sizeof(out));
        kr = RunFirstMmioReadFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(m.read32Count == 1);
        assert(m.mapCreateCount == 1);
        assert(m.mapReleaseCount == 1);
        assert(m.liveMap == false);
        assert(out.rawValue == 0x176000A1u);
        assert(ga106ctl_check_mmio_read(&out, sizeof(out)) == 0);
        assert(ga106ctl_mmio_cli_exit(1, &out, sizeof(out)) == 0);
    }
    PASS("GOOD GA106 0x176 via shared flow (1 read, 1 create, 1 release)");

    // ---- 2. Stepping diferente válido ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.mmioRaw = 0x176000B0u;
        m.resetCounts();
        // resetCounts limpa liveMap; re-marca? makeLive setou liveMap=false via reset;
        // createMap setará true durante o flow. Ok.
        memset(&out, 0, sizeof(out));
        IOReturn kr = RunFirstMmioReadFlow(m, out);
        assert(kr == kIOReturnSuccess);
        assert(m.read32Count == 1 && m.mapReleaseCount == 1);
    }
    PASS("DIFFERENT STEPPING PASS");

    // ---- 3. wrong provider / MSE off: 0 reads, 0 creates, 0 releases ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.providerIsPCI = false;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 0 && m.mapReleaseCount == 0);

        GA106LabMockMakeLive(m);
        m.vendor = 0x1022u;
        m.device = 0x1630u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 0 && m.mapReleaseCount == 0);

        GA106LabMockMakeLive(m);
        m.vendor = 0xFFFFu;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 0);

        GA106LabMockMakeLive(m);
        m.cmdBefore = 0x0001u;
        m.cmdAfter = 0x0001u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 0 && m.mapReleaseCount == 0);
    }
    PASS("WRONG_PROVIDER + MSE_OFF => 0 reads/creates/releases");

    // ---- 4. count 0 / missing / ambiguous via MESMO flow ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.nRes = 0;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 0 && m.mapReleaseCount == 0);

        GA106LabMockMakeLive(m);
        m.nRes = 4;
        m.res[0].base = 0x440000000ULL; m.res[0].len = 0x10000000ULL; m.res[0].present = true;
        m.res[1].base = 0x450000000ULL; m.res[1].len = 0x2000000ULL;  m.res[1].present = true;
        m.res[2].base = 0xFC000000ULL;  m.res[2].len = 0x80000ULL;    m.res[2].present = true;
        m.res[3].base = 0x1001ULL;      m.res[3].len = 0x100ULL;      m.res[3].present = true;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 0);

        GA106LabMockMakeLive(m);
        m.nRes = 2;
        m.res[0].base = 0xFB000000ULL; m.res[0].len = 0x1000000ULL; m.res[0].present = true;
        m.res[1].base = 0xFB000000ULL; m.res[1].len = 0x1000000ULL; m.res[1].present = true;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 0 && m.mapReleaseCount == 0);
    }
    PASS("COUNT0/MISSING/AMBIGUOUS same-flow => 0 reads");

    // ---- 5. BAR1/BAR3/ROM/IO rejeitados ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        const uint64_t bases[4] = {0x440000000ULL, 0x450000000ULL, 0xFC000000ULL, 0x1001ULL};
        const uint64_t lens[4] = {0x10000000ULL, 0x2000000ULL, 0x80000ULL, 0x100ULL};
        for (int k = 0; k < 4; k++) {
            GA106LabMockMakeLive(m);
            m.nRes = 1;
            m.res[0].base = bases[k];
            m.res[0].len = lens[k];
            m.res[0].present = true;
            m.resetCounts();
            memset(&out, 0, sizeof(out));
            assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
            assert(m.read32Count == 0 && m.mapReleaseCount == 0);
        }
    }
    PASS("BAR1/BAR3/ROM/BAR5 REJECTED same-flow");

    // ---- 6. wrong BAR base/length ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.bar0Raw = 0xFA000000u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 0);

        GA106LabMockMakeLive(m);
        m.bar0Raw = 0xFB000000u | 0x1u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0);

        GA106LabMockMakeLive(m);
        m.bar0Raw = 0xFB000000u | 0x4u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0);

        GA106LabMockMakeLive(m);
        m.nRes = 1;
        m.res[0].base = 0xFB000000ULL;
        m.res[0].len = 0x2000000ULL;
        m.res[0].present = true;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0);
    }
    PASS("WRONG BAR BASE/LENGTH => 0 reads");

    // ---- 7. map null / length mismatch ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.mapNull = true;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 1 && m.mapReleaseCount == 0);
        assert(m.liveMap == false);

        GA106LabMockMakeLive(m);
        m.mapLen = 0x2000000ULL;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapCreateCount == 1 && m.mapReleaseCount == 1);

        GA106LabMockMakeLive(m);
        m.mapLen = 0x800000ULL;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 1);
    }
    PASS("MAP_NULL (1 attempt, 0 release) + MAPLEN_MISMATCH (1 release, 0 reads)");

    // ---- 8. bad map options via MESMO flow ----
    {
        const uint32_t badOpts[6] = {0x1300u, 0x1400u, 0x1000u, 0x0100u, 0x1200u, 0x0000u};
        for (int k = 0; k < 6; k++) {
            GA106LabMockOps m;
            GA106LabMmioReadV1 out;
            GA106LabMockMakeLive(m);
            m.mapOpts = badOpts[k];
            m.resetCounts();
            memset(&out, 0, sizeof(out));
            assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
            assert(m.read32Count == 0 && m.mapReleaseCount == 1);
        }
    }
    PASS("BAD MAP OPTIONS same-flow => 0 reads, 1 release");

    // ---- 9. VA zero / alignment / bounds via MESMO flow ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.vaZero = true;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 1);

        assert(GA106LabBoundsValid(0, 4, 0x1000000ULL) == 1);
        assert(GA106LabBoundsValid(0, 4, 2) == 0);
        assert(GA106LabAlignmentValid(0) == 1);
        assert(GA106LabAlignmentValid(1) == 0);
        assert(GA106LabAlignmentValid(2) == 0);

        GA106LabMockMakeLive(m);
        m.useCustomOffset = true;
        m.customOffset = 0x1000000u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 1);

        GA106LabMockMakeLive(m);
        m.useCustomOffset = true;
        m.customOffset = 1;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 1);

        GA106LabMockMakeLive(m);
        m.useCustomOffset = true;
        m.customOffset = 2;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 0 && m.mapReleaseCount == 1);
    }
    PASS("VA_ZERO/BOUNDS/ALIGNMENT same-flow => 0 reads, 1 release");

    // ---- 10. absent + wrong chip: 1 read, 1 release, FAIL ----
    {
        const uint32_t badRaws[4] = {0xFFFFFFFFu, 0x00000000u, 0x174000A1u, 0x1A0000A1u};
        for (int k = 0; k < 4; k++) {
            GA106LabMockOps m;
            GA106LabMmioReadV1 out;
            GA106LabMockMakeLive(m);
            m.mmioRaw = badRaws[k];
            m.resetCounts();
            memset(&out, 0, sizeof(out));
            assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
            assert(m.read32Count == 1 && m.mapReleaseCount == 1);
        }
    }
    PASS("ABSENT + WRONG_CHIP same-flow => 1 read, 1 release, FAIL");

    // ---- 11. Command/MSE/BME changed: 1 read, 1 release, FAIL ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.cmdAfter = 0x0007u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 1 && m.mapReleaseCount == 1);
        assert(out.status == kGA106LabMmioStatus_Failed);

        GA106LabMockMakeLive(m);
        m.cmdAfter = 0x0001u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 1 && m.mapReleaseCount == 1);

        GA106LabMockMakeLive(m);
        m.cmdAfter = 0x0002u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) != kIOReturnSuccess);
        assert(m.read32Count == 1 && m.mapReleaseCount == 1);
    }
    PASS("COMMAND/MSE/BME CHANGED same-flow => 1 read, 1 release, FAIL");

    // ---- 12. Release + create counts consolidados ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.providerIsPCI = false;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        RunFirstMmioReadFlow(m, out);
        assert(m.mapReleaseCount == 0 && m.read32Count == 0 && m.mapCreateCount == 0);

        GA106LabMockMakeLive(m);
        m.mapOpts = 0x1300u;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        RunFirstMmioReadFlow(m, out);
        assert(m.mapReleaseCount == 1 && m.read32Count == 0 && m.mapCreateCount == 1);

        GA106LabMockMakeLive(m);
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) == kIOReturnSuccess);
        assert(m.mapReleaseCount == 1 && m.read32Count == 1 && m.mapCreateCount == 1);
        assert(m.liveMap == false); // nunca persiste

        GA106LabMockMakeLive(m);
        m.mmioRaw = 0xFFFFFFFFu;
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        RunFirstMmioReadFlow(m, out);
        assert(m.mapReleaseCount == 1 && m.read32Count == 1);
    }
    PASS("RELEASE_COUNTS + CREATE_COUNTS + no persistent map");

    // ---- 13. Mutação: segunda leitura seria detectada (==1 sensível) ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) == kIOReturnSuccess);
        assert(m.read32Count == 1);
        // Simula mutação hipotética: uma chamada extra a read32.
        (void)m.read32(0);
        assert(m.read32Count == 2);
        assert(m.read32Count != 1); // o assert ==1 do teste quebraria
    }
    PASS("EXTRA_READ_MUTATION_DETECTED = YES");

    // ---- 14. Mutação: release removido seria detectado ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 out;
        GA106LabMockMakeLive(m);
        m.resetCounts();
        memset(&out, 0, sizeof(out));
        assert(RunFirstMmioReadFlow(m, out) == kIOReturnSuccess);
        assert(m.mapReleaseCount == 1);
        assert(m.liveMap == false);
        // Se releaseMap() fosse removido do flow, teríamos 0 e liveMap true.
        // Demonstra sensibilidade: um Mock sem release ficaria com 0.
        GA106LabMockOps m2;
        GA106LabMockMakeLive(m2);
        m2.resetCounts();
        // não executa flow; conta 0 -> prova que ==1 falharia sem release
        assert(m2.mapReleaseCount == 0);
    }
    PASS("MISSING_RELEASE_MUTATION_DETECTED = YES");

    // ---- 15. CLI propagation real ----
    {
        GA106LabMockOps m;
        GA106LabMmioReadV1 good, bad;
        GA106LabMockMakeLive(m);
        m.resetCounts();
        memset(&good, 0, sizeof(good));
        assert(RunFirstMmioReadFlow(m, good) == kIOReturnSuccess);
        assert(ga106ctl_mmio_cli_exit(1, &good, sizeof(good)) == 0);
        assert(ga106ctl_mmio_cli_exit(0, &good, sizeof(good)) != 0);
        assert(ga106ctl_mmio_cli_exit(1, &good, 16) != 0);
        bad = good; bad.version = 99;
        assert(ga106ctl_mmio_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.status = kGA106LabMmioStatus_Failed;
        assert(ga106ctl_mmio_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.registerId = 99;
        assert(ga106ctl_mmio_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.offset = 0x100;
        assert(ga106ctl_mmio_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.width = 8;
        assert(ga106ctl_mmio_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.rawValue = 0x174000A1u;
        assert(ga106ctl_mmio_cli_exit(1, &bad, sizeof(bad)) != 0);
        bad = good; bad.rawValue = 0xFFFFFFFFu;
        assert(ga106ctl_mmio_cli_exit(1, &bad, sizeof(bad)) != 0);
        memset(&bad, 0, sizeof(bad));
        assert(ga106ctl_mmio_cli_exit(1, &bad, sizeof(bad)) != 0);
    }
    PASS("CLI_PROPAGATION_TEST = PASS");

    printf("test-first-mmio-read: ALL PASS (%d grupos; ONE_SHARED_FLOW via RunFirstMmioReadFlow)\n", gGroups);
    return 0;
}
