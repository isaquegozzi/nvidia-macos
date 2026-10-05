// test-bar-map-probe.c — modelo userspace da lógica de discovery/validação
// do selector 4 BAR_MAP_PROBE (espelha GA106Lab::probeBar0Map sem kernel).
//
// Cobre: provider errado, MSE off, count 0, missing, ambiguous, BAR0 correto,
// wrong length/base, BAR1/BAR3/ROM/BAR5 rejeitados, map null, mappedLength
// mismatch, options sem ReadOnly/InhibitCache, Command/MSE/BME changed,
// release exactly-once em sucesso e em falha pós-map, zero release sem map,
// CLI failure propagation + success parsing (via ga106ctl_check_bar_map_probe).
// TG-KEXT4-FIX: validação de cache EXATA via GA106LabCoreValidate.h
// (mesmo helper da produção); cobre Copyback/WriteCombine/Default.
//
// Compilar: clang -o test-bar-map-probe test-bar-map-probe.c ga106ctl-validate.c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"
#include "ga106ctl-validate.h"

#define BAR0_LEN 0x1000000ULL
#define BAR0_BASE 0xFB000000ULL
#define OPTS_REQ 0x1100u /* kIOMapReadOnly|kIOMapInhibitCache (só exato passa) */

typedef struct {
    uint64_t base;
    uint64_t len;
    int      kind; /* 0=memory 1=io 2=subrange */
} MockRes;

/* Retorna: 0 ok / 1 wrong-provider / 2 mse-off / 3 missing / 4 ambiguous /
   5 invalid / 6 map-null / 7 len-mismatch / 8 opts-missing /
   9 cmd-changed. releases incrementado por release simulado. */
static int modelProbe(int isRightProvider, int mseOn,
                      const MockRes * rs, int n,
                      int mapNull, uint64_t fakeMapLen, uint32_t fakeOpts,
                      int cmdBefore, int cmdAfter, int * releases,
                      struct GA106LabBarMapInfoV1 * out)
{
    int i, matches = 0, matchIdx = -1;
    uint64_t rlen = 0, rbase = 0;
    memset(out, 0, sizeof(*out));
    out->status = kGA106LabBarMapStatus_Failed;
    if (!isRightProvider) {
        return 1;
    }
    if (!mseOn) {
        return 2;
    }
    for (i = 0; i < n; i++) {
        if (rs[i].kind != 0) {
            continue; /* I/O BAR5 e sub-ranges rejeitados */
        }
        if (rs[i].len == BAR0_LEN && rs[i].base == BAR0_BASE) {
            matches++;
            matchIdx = i;
            rlen = rs[i].len;
            rbase = rs[i].base;
        }
    }
    if (matches == 0) {
        return 3;
    }
    if (matches >= 2) {
        return 4;
    }
    if (rlen != BAR0_LEN || rbase != BAR0_BASE) {
        return 5;
    }
    if (mapNull) {
        return 6;
    }
    if (fakeMapLen > rlen || fakeMapLen != rlen) {
        (*releases)++; /* map criado e liberado no mismatch */
        return 7;
    }
    /* TG-KEXT4-FIX: validação EXATA (mesmo helper da produção).
       Rejeita RO+Copyback (0x1300) que o check antigo por REQUIRED_BITS
       aceitava, além de WC/Default/semRO/semInhibit. */
    if (!GA106LabMapOptionsValidROInhibit(fakeOpts)) {
        (*releases)++;
        return 8;
    }
    out->size = (uint32_t) sizeof(*out);
    out->version = GA106LAB_UC_PROTOCOL_VERSION;
    out->status = kGA106LabBarMapStatus_MapReleased;
    out->validMask = kGA106LabBarMapValid_Resource |
                     kGA106LabBarMapValid_PhysicalBase |
                     kGA106LabBarMapValid_ResourceLength |
                     kGA106LabBarMapValid_MappedLength |
                     kGA106LabBarMapValid_MapOptions |
                     kGA106LabBarMapValid_ReleaseConfirmed;
    out->semanticBar = kGA106LabBarMapSemantic_Bar0;
    out->resourceIndex = (uint32_t) matchIdx;
    out->physicalBase = rbase;
    out->resourceLength = rlen;
    out->mappedLength = fakeMapLen;
    out->mapOptions = fakeOpts;
    out->mapState = 1;
    (*releases)++; /* release no caminho de sucesso */
    if (cmdBefore != cmdAfter) {
        out->status = kGA106LabBarMapStatus_Failed;
        out->validMask &=
            (uint32_t) ~kGA106LabBarMapValid_ReleaseConfirmed;
        return 9;
    }
    return 0;
}

int main(void)
{
    struct GA106LabBarMapInfoV1 out;
    int rel = 0, rc;
    /* Live real: BAR0 16M + BAR1 256M + BAR3 32M + ROM 512K + I/O BAR5. */
    const MockRes live[] = {
        { 0xFB000000ULL, 0x1000000ULL, 0 },
        { 0x440000000ULL, 0x10000000ULL, 0 },
        { 0x450000000ULL, 0x2000000ULL, 0 },
        { 0xFC000000ULL, 0x80000ULL, 0 },
        { 0x1001ULL, 0x100ULL, 1 },
        { 0x0ULL, 0x1000ULL, 2 },
    };

    /* 1. correct BAR0. */
    rel = 0;
    rc = modelProbe(1, 1, live, 6, 0, BAR0_LEN, OPTS_REQ,
                    0x0003, 0x0003, &rel, &out);
    assert(rc == 0 && rel == 1);
    assert(ga106ctl_check_bar_map_probe(&out, sizeof(out)) == 0);
    assert(out.resourceIndex == 0 && out.physicalBase == BAR0_BASE);

    /* 2. wrong provider / MSE off / count 0 / missing. */
    assert(modelProbe(0, 1, live, 6, 0, BAR0_LEN, OPTS_REQ,
                      3, 3, &rel, &out) == 1);
    assert(modelProbe(1, 0, live, 6, 0, BAR0_LEN, OPTS_REQ,
                      3, 3, &rel, &out) == 2);
    assert(modelProbe(1, 1, live, 0, 0, BAR0_LEN, OPTS_REQ,
                      3, 3, &rel, &out) == 3);

    /* 3. ambiguous: dois BAR0 idênticos. */
    {
        const MockRes amb[] = {
            { BAR0_BASE, BAR0_LEN, 0 }, { BAR0_BASE, BAR0_LEN, 0 },
        };
        rel = 0;
        assert(modelProbe(1, 1, amb, 2, 0, BAR0_LEN, OPTS_REQ,
                          3, 3, &rel, &out) == 4 && rel == 0);
    }

    /* 4. wrong length/base: só BAR1/BAR3/ROM/IO. */
    {
        const MockRes nobar0[] = {
            { 0x440000000ULL, 0x10000000ULL, 0 },
            { 0x450000000ULL, 0x2000000ULL, 0 },
            { 0xFC000000ULL, 0x80000ULL, 0 },
            { 0x1001ULL, 0x100ULL, 1 },
        };
        assert(modelProbe(1, 1, nobar0, 4, 0, BAR0_LEN, OPTS_REQ,
                          3, 3, &rel, &out) == 3);
    }

    /* 5. BAR1 rejected (256M na base BAR0? não casa), BAR3, ROM, BAR5. */
    {
        const MockRes onlyBar1[] = { { BAR0_BASE, 0x10000000ULL, 0 } };
        assert(modelProbe(1, 1, onlyBar1, 1, 0, BAR0_LEN, OPTS_REQ,
                          3, 3, &rel, &out) == 3);
    }

    /* 6. map null: zero release. */
    rel = 0;
    assert(modelProbe(1, 1, live, 6, 1, BAR0_LEN, OPTS_REQ,
                      3, 3, &rel, &out) == 6 && rel == 0);

    /* 7. mappedLength mismatch: release exactly once. */
    rel = 0;
    assert(modelProbe(1, 1, live, 6, 0, 0x2000000ULL, OPTS_REQ,
                      3, 3, &rel, &out) == 7 && rel == 1);

    /* 8. options sem ReadOnly / sem InhibitCache: release once. */
    rel = 0;
    assert(modelProbe(1, 1, live, 6, 0, BAR0_LEN, 0x0100u,
                      3, 3, &rel, &out) == 8 && rel == 1);
    rel = 0;
    assert(modelProbe(1, 1, live, 6, 0, BAR0_LEN, 0x1000u,
                      3, 3, &rel, &out) == 8 && rel == 1);
    /* 8b. TG-KEXT4-FIX: Copyback/WriteCombine/Default com RO devem falhar. */
    rel = 0;
    assert(modelProbe(1, 1, live, 6, 0, BAR0_LEN, 0x1300u,
                      3, 3, &rel, &out) == 8 && rel == 1);
    rel = 0;
    assert(modelProbe(1, 1, live, 6, 0, BAR0_LEN, 0x1400u,
                      3, 3, &rel, &out) == 8 && rel == 1);
    rel = 0;
    assert(modelProbe(1, 1, live, 6, 0, BAR0_LEN, 0x1200u,
                      3, 3, &rel, &out) == 8 && rel == 1);

    /* 9. Command/MSE/BME changed após release. */
    rel = 0;
    assert(modelProbe(1, 1, live, 6, 0, BAR0_LEN, OPTS_REQ,
                      0x0003, 0x0007, &rel, &out) == 9 && rel == 1);

    /* 10. CLI propagation: failure -> check != 0; success -> 0. */
    {
        struct GA106LabBarMapInfoV1 bad;
        memset(&bad, 0, sizeof(bad));
        assert(ga106ctl_check_bar_map_probe(&bad, sizeof(bad)) != 0);
        assert(ga106ctl_check_bar_map_probe(&out, 16) != 0);
    }

    printf("test-bar-map-probe: ALL PASS (10 grupos + cache-exata)\n");
    return 0;
}
