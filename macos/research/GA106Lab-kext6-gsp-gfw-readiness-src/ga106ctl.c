// ga106ctl.c — CLI mínima KEXT1 (userspace, sem daemon).
//
// ga106ctl version|identity|status — abre GA106Lab via IOKit e imprime.
// Erros limpos: uso desconhecido, serviço ausente, open negado (não-root),
// falha de chamada. Sem acesso a hardware (só IOServiceOpen/CallMethod/Close).
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <IOKit/IOKitLib.h>

#include "GA106LabProtocol.h"
#include "ga106ctl-validate.h"

#define GA106LAB_SERVICE_NAME "GA106Lab"

static io_connect_t openService(void)
{
    kern_return_t   kr;
    io_iterator_t   iter = IO_OBJECT_NULL;
    io_service_t    service = IO_OBJECT_NULL;
    io_connect_t    conn = IO_OBJECT_NULL;
    CFMutableDictionaryRef match;

    match = IOServiceMatching(GA106LAB_SERVICE_NAME);
    if (!match) {
        fprintf(stderr, "ga106ctl: no-memory building match dictionary\n");
        return IO_OBJECT_NULL;
    }
    service = IOServiceGetMatchingService(kIOMainPortDefault, match);
    /* match consumido pela chamada acima em qualquer caso. */
    if (service == IO_OBJECT_NULL) {
        fprintf(stderr, "ga106ctl: service '%s' not found\n",
                GA106LAB_SERVICE_NAME);
        return IO_OBJECT_NULL;
    }
    kr = IOServiceOpen(service, mach_task_self(), 0, &conn);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        if (geteuid() != 0) {
            fprintf(stderr, "ga106ctl: open denied (%d); try running as root\n",
                    (int) kr);
        } else {
            fprintf(stderr, "ga106ctl: IOServiceOpen failed (%d)\n", (int) kr);
        }
        return IO_OBJECT_NULL;
    }
    return conn;
}

static int cmdVersion(void)
{
    uint64_t out[2];
    uint32_t outCount = 2;
    io_connect_t conn = openService();
    kern_return_t kr;
    if (conn == IO_OBJECT_NULL) {
        return 1;
    }
    kr = IOConnectCallMethod(conn, kGA106LabSelector_GetProtocolVersion,
                             NULL, 0, NULL, 0,
                             out, &outCount, NULL, NULL);
    IOServiceClose(conn);
    if (kr != KERN_SUCCESS || outCount < 2) {
        fprintf(stderr, "ga106ctl: version call failed (%d)\n", (int) kr);
        return 1;
    }
    printf("protocol: %llu\ndriver: 0x%08llx\n",
           (unsigned long long) out[0], (unsigned long long) out[1]);
    return 0;
}

static int cmdStruct(uint32_t selector, const char * what)
{
    io_connect_t conn;
    kern_return_t kr;
    conn = openService();
    if (conn == IO_OBJECT_NULL) {
        return 1;
    }
    if (selector == kGA106LabSelector_GetDeviceIdentity) {
        struct GA106LabIdentityV1 out;
        size_t outSize = sizeof(out);
        const struct GA106LabIdentityV1 * id = &out;
        memset(&out, 0, sizeof(out));
        kr = IOConnectCallMethod(conn, selector,
                                 NULL, 0, NULL, 0,
                                 NULL, NULL, &out, &outSize);
        IOServiceClose(conn);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "ga106ctl: %s call failed (%d)\n", what, (int) kr);
            return 1;
        }
        /* Contrato exato: 32 bytes (não buffer genérico) + ABI validada. */
        if (ga106ctl_check_identity(&out, outSize) != 0) {
            fprintf(stderr, "ga106ctl: %s incompatible reply\n", what);
            return 1;
        }
        printf("vendor: %04x\ndevice: %04x\nproviderEntryID: %llu\nstate: %u\n",
               (unsigned) id->vendor, (unsigned) id->device,
               (unsigned long long) id->providerEntryID,
               (unsigned) id->state);
        return 0;
    }
    if (selector == kGA106LabSelector_GetServiceState) {
        struct GA106LabStatusV1 out;
        size_t outSize = sizeof(out);
        const struct GA106LabStatusV1 * st = &out;
        memset(&out, 0, sizeof(out));
        kr = IOConnectCallMethod(conn, selector,
                                 NULL, 0, NULL, 0,
                                 NULL, NULL, &out, &outSize);
        IOServiceClose(conn);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "ga106ctl: %s call failed (%d)\n", what, (int) kr);
            return 1;
        }
        if (ga106ctl_check_status(&out, outSize) != 0) {
            fprintf(stderr, "ga106ctl: %s incompatible reply\n", what);
            return 1;
        }
        printf("state: %s\nproviderConfirmed: %s\nrevision: 0x%08x\n",
               st->state == kGA106LabState_StartSuccess ? "START_SUCCESS"
                                                       : "UNKNOWN",
               st->providerConfirmed ? "yes" : "no",
               (unsigned) st->revision);
        return 0;
    }
    /* NOTA: este ramo só é alcançado com selector GetPciSnapshot. */
    if (selector == kGA106LabSelector_GetPciSnapshot) {
        struct GA106LabPciSnapshotV1 out;
        size_t outSize = sizeof(out);
        const struct GA106LabPciSnapshotV1 * snap = &out;
        int i;
        memset(&out, 0, sizeof(out));
        kr = IOConnectCallMethod(conn, selector,
                                 NULL, 0, NULL, 0,
                                 NULL, NULL, &out, &outSize);
        IOServiceClose(conn);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "ga106ctl: %s call failed (%d)\n", what, (int) kr);
            return 1;
        }
        if (ga106ctl_check_pci_snapshot(&out, outSize) != 0) {
            fprintf(stderr, "ga106ctl: %s incompatible reply\n", what);
            return 1;
        }
        /* RAW primeiro; interpretação separada e marcada como tal. */
        printf("RAW vendor=%04x device=%04x class=%02x%02x%02x rev=%02x\n",
               (unsigned) snap->vendorId, (unsigned) snap->deviceId,
               (unsigned) snap->classCode, (unsigned) snap->subclass,
               (unsigned) snap->progIf, (unsigned) snap->revisionId);
        printf("RAW command=%04x status=%04x\n",
               (unsigned) snap->command, (unsigned) snap->statusReg);
        for (i = 0; i < 6; i++) {
            printf("RAW bar%d=%08x\n", i, (unsigned) snap->barRaw[i]);
        }
        printf("RAW subsys=%04x:%04x rom=%08x capPtr=%02x intLine=%02x intPin=%02x\n",
               (unsigned) snap->subsystemVendorId,
               (unsigned) snap->subsystemId,
               (unsigned) snap->expansionRomRaw,
               (unsigned) snap->capPointer,
               (unsigned) snap->interruptLine,
               (unsigned) snap->interruptPin);
        printf("DECODED identity: vendor=%04x device=%04x class=%02x%02x%02x\n",
               (unsigned) snap->vendorId, (unsigned) snap->deviceId,
               (unsigned) snap->classCode, (unsigned) snap->subclass,
               (unsigned) snap->progIf);
        printf("DECODED mse=%s bme=%s\n",
               (snap->command & 0x2) ? "on" : "off",
               (snap->command & 0x4) ? "on" : "off");
        return 0;
    }
    /* Selector 4 BAR_MAP_PROBE: só metadata segura (sem VA/ponteiro). */
    if (selector == kGA106LabSelector_BarMapProbe) {
        struct GA106LabBarMapInfoV1 out;
        size_t outSize = sizeof(out);
        const struct GA106LabBarMapInfoV1 * info = &out;
        memset(&out, 0, sizeof(out));
        kr = IOConnectCallMethod(conn, selector,
                                 NULL, 0, NULL, 0,
                                 NULL, NULL, &out, &outSize);
        IOServiceClose(conn);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "ga106ctl: %s call failed (%d)\n", what, (int) kr);
            return 1;
        }
        if (ga106ctl_check_bar_map_probe(&out, outSize) != 0) {
            fprintf(stderr, "ga106ctl: %s incompatible reply\n", what);
            return 1;
        }
        printf("BAR semantic=%u resourceIndex=%u\n",
               (unsigned) info->semanticBar,
               (unsigned) info->resourceIndex);
        printf("BAR physBase=0x%016llx resourceLength=0x%016llx mappedLength=0x%016llx\n",
               (unsigned long long) info->physicalBase,
               (unsigned long long) info->resourceLength,
               (unsigned long long) info->mappedLength);
        printf("BAR mapOptions=0x%08x mapState=%u validMask=0x%08x status=%u\n",
               (unsigned) info->mapOptions, (unsigned) info->mapState,
               (unsigned) info->validMask, (unsigned) info->status);
        return 0;
    }
    /* Selector 5 READ_FIRST_MMIO: só metadata + rawValue (sem VA/ponteiro).
       Decisão de exit via ga106ctl_mmio_cli_exit (mesma função exercitada
       pelos testes offline com kr+ABI mockados: prova propagation real). */
    if (selector == kGA106LabSelector_ReadFirstMmioRegister) {
        struct GA106LabMmioReadV1 out;
        size_t outSize = sizeof(out);
        const struct GA106LabMmioReadV1 * info = &out;
        uint32_t chip;
        int cliRc;
        memset(&out, 0, sizeof(out));
        kr = IOConnectCallMethod(conn, selector,
                                 NULL, 0, NULL, 0,
                                 NULL, NULL, &out, &outSize);
        cliRc = ga106ctl_mmio_cli_exit(kr == KERN_SUCCESS, &out, outSize);
        IOServiceClose(conn);
        if (cliRc != 0) {
            if (kr != KERN_SUCCESS) {
                fprintf(stderr, "ga106ctl: %s call failed (%d)\n", what, (int) kr);
            } else {
                fprintf(stderr, "ga106ctl: %s incompatible reply\n", what);
            }
            return 1;
        }
        chip = (info->rawValue & kGA106LabMmio_ChipId_Mask) >>
               kGA106LabMmio_ChipId_Shift;
        printf("register: NV_PMC_BOOT_0\n");
        printf("offset: 0x%08x width: %u\n",
               (unsigned) info->offset, (unsigned) info->width);
        printf("rawValue: 0x%08x\n", (unsigned) info->rawValue);
        printf("chipId: 0x%03x%s\n", (unsigned) chip,
               chip == kGA106LabMmio_ChipId_GA106 ? " (GA106)" : " (UNEXPECTED)");
        printf("status: %u validFlags: 0x%08x\n",
               (unsigned) info->status, (unsigned) info->validFlags);
        return 0;
    }
    /* Selector 6 READ_STATIC_IDENTITY: BOOT42 único (sem VA/ponteiro).
       Exit via ga106ctl_static_identity_cli_exit (mesma dos testes). */
    if (selector == kGA106LabSelector_ReadStaticIdentity) {
        struct GA106LabStaticIdentityV1 out;
        size_t outSize = sizeof(out);
        const struct GA106LabStaticIdentityV1 * info = &out;
        int cliRc;
        memset(&out, 0, sizeof(out));
        kr = IOConnectCallMethod(conn, selector,
                                 NULL, 0, NULL, 0,
                                 NULL, NULL, &out, &outSize);
        cliRc = ga106ctl_static_identity_cli_exit(kr == KERN_SUCCESS,
                                                  &out, outSize);
        IOServiceClose(conn);
        if (cliRc != 0) {
            if (kr != KERN_SUCCESS) {
                fprintf(stderr, "ga106ctl: %s call failed (%d)\n", what, (int) kr);
            } else {
                fprintf(stderr, "ga106ctl: %s incompatible reply\n", what);
            }
            return 1;
        }
        printf("boot42: 0x%08x (chip 0x%03x%s)\n",
               (unsigned) info->boot42Raw, (unsigned) info->decodedChipId,
               info->decodedChipId == kGA106LabMmio_ChipId_GA106 ? " GA106" : " UNEXPECTED");
        printf("readCount: %u status: %u validMask: 0x%08x\n",
               (unsigned) info->readCount, (unsigned) info->status,
               (unsigned) info->validMask);
        return 0;
    }
    /* Selector 7 PREPARE_GSP_SYSMEM: só tamanhos/estado (sem endereços).
       Exit via ga106ctl_gsp_sysmem_cli_exit (mesma dos testes). */
    if (selector == kGA106LabSelector_PrepareGspSysmem) {
        struct GA106LabGspSysmemV1 out;
        size_t outSize = sizeof(out);
        const struct GA106LabGspSysmemV1 * info = &out;
        int cliRc;
        memset(&out, 0, sizeof(out));
        kr = IOConnectCallMethod(conn, selector,
                                 NULL, 0, NULL, 0,
                                 NULL, NULL, &out, &outSize);
        cliRc = ga106ctl_gsp_sysmem_cli_exit(kr == KERN_SUCCESS,
                                             &out, outSize);
        IOServiceClose(conn);
        if (cliRc != 0) {
            if (kr != KERN_SUCCESS) {
                fprintf(stderr, "ga106ctl: %s call failed (%d)\n", what, (int) kr);
            } else {
                fprintf(stderr, "ga106ctl: %s incompatible reply\n", what);
            }
            return 1;
        }
        printf("bufferSize: %u segmentCount: %u segmentLength: %u\n",
               (unsigned) info->bufferSize, (unsigned) info->segmentCount,
               (unsigned) info->segmentLength);
        printf("state: %u status: %u flags: 0x%08x\n",
               (unsigned) info->state, (unsigned) info->status,
               (unsigned) info->flags);
        return 0;
    }
    /* Selector 8 READ_GFW_BOOT_READINESS: só semântica gate/progresso
       (sem endereços). Exit via ga106ctl_gfw_readiness_cli_exit. */
    if (selector == kGA106LabSelector_ReadGfwBootReadiness) {
        struct GA106LabGfwReadinessV1 out;
        size_t outSize = sizeof(out);
        const struct GA106LabGfwReadinessV1 * info = &out;
        int cliRc;
        memset(&out, 0, sizeof(out));
        kr = IOConnectCallMethod(conn, selector,
                                 NULL, 0, NULL, 0,
                                 NULL, NULL, &out, &outSize);
        cliRc = ga106ctl_gfw_readiness_cli_exit(kr == KERN_SUCCESS,
                                                &out, outSize);
        IOServiceClose(conn);
        if (cliRc != 0) {
            if (kr != KERN_SUCCESS) {
                fprintf(stderr, "ga106ctl: %s call failed (%d)\n", what, (int) kr);
            } else {
                fprintf(stderr, "ga106ctl: %s incompatible reply\n", what);
            }
            return 1;
        }
        printf("gateReady: %u progress: 0x%02x pollCount: %u\n",
               (unsigned) info->gateReady, (unsigned) info->progress,
               (unsigned) info->pollCount);
        printf("state: status=%u timedOut=%u\n",
               (unsigned) info->status, (unsigned) info->timedOut);
        return 0;
    }
    fprintf(stderr, "ga106ctl: unknown selector %u\n", (unsigned) selector);
    return 1;
}

static int usage(const char * prog)
{
    fprintf(stderr, "usage: %s version|identity|status|pci|bar-map-probe|first-mmio-read|static-identity|prepare-gsp-sysmem|read-gfw-boot-readiness\n", prog);
    return 2;
}

int main(int argc, char * argv[])
{
    if (argc != 2) {
        return usage(argv[0]);
    }
    if (strcmp(argv[1], "version") == 0) {
        return cmdVersion();
    }
    if (strcmp(argv[1], "identity") == 0) {
        return cmdStruct(kGA106LabSelector_GetDeviceIdentity, "identity");
    }
    if (strcmp(argv[1], "status") == 0) {
        return cmdStruct(kGA106LabSelector_GetServiceState, "status");
    }
    if (strcmp(argv[1], "pci") == 0) {
        return cmdStruct(kGA106LabSelector_GetPciSnapshot, "pci");
    }
    if (strcmp(argv[1], "bar-map-probe") == 0) {
        return cmdStruct(kGA106LabSelector_BarMapProbe, "bar-map-probe");
    }
    if (strcmp(argv[1], "first-mmio-read") == 0) {
        return cmdStruct(kGA106LabSelector_ReadFirstMmioRegister,
                         "first-mmio-read");
    }
    if (strcmp(argv[1], "static-identity") == 0) {
        return cmdStruct(kGA106LabSelector_ReadStaticIdentity,
                         "static-identity");
    }
    if (strcmp(argv[1], "prepare-gsp-sysmem") == 0) {
        return cmdStruct(kGA106LabSelector_PrepareGspSysmem,
                         "prepare-gsp-sysmem");
    }
    if (strcmp(argv[1], "read-gfw-boot-readiness") == 0) {
        return cmdStruct(kGA106LabSelector_ReadGfwBootReadiness,
                         "read-gfw-boot-readiness");
    }
    return usage(argv[0]);
}
