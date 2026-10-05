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

static int cmdStruct(uint32_t selector, size_t expectSize, const char * what)
{
    uint8_t out[64];
    size_t  outSize = sizeof(out);
    io_connect_t conn;
    kern_return_t kr;
    if (expectSize > sizeof(out)) {
        fprintf(stderr, "ga106ctl: internal buffer too small\n");
        return 1;
    }
    conn = openService();
    if (conn == IO_OBJECT_NULL) {
        return 1;
    }
    memset(out, 0, sizeof(out));
    kr = IOConnectCallMethod(conn, selector,
                             NULL, 0, NULL, 0,
                             NULL, NULL, out, &outSize);
    IOServiceClose(conn);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "ga106ctl: %s call failed (%d)\n", what, (int) kr);
        return 1;
    }
    if (outSize < expectSize) {
        fprintf(stderr, "ga106ctl: %s short reply (%lu < %lu)\n",
                what, (unsigned long) outSize, (unsigned long) expectSize);
        return 1;
    }
    if (selector == kGA106LabSelector_GetDeviceIdentity) {
        const struct GA106LabIdentityV1 * id =
            (const struct GA106LabIdentityV1 *) out;
        printf("vendor: %04x\ndevice: %04x\nproviderEntryID: %llu\nstate: %u\n",
               (unsigned) id->vendor, (unsigned) id->device,
               (unsigned long long) id->providerEntryID,
               (unsigned) id->state);
    } else {
        const struct GA106LabStatusV1 * st =
            (const struct GA106LabStatusV1 *) out;
        printf("state: %s\nproviderConfirmed: %s\nrevision: 0x%08x\n",
               st->state == kGA106LabState_StartSuccess ? "START_SUCCESS"
                                                       : "UNKNOWN",
               st->providerConfirmed ? "yes" : "no",
               (unsigned) st->revision);
    }
    return 0;
}

static int usage(const char * prog)
{
    fprintf(stderr, "usage: %s version|identity|status\n", prog);
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
        return cmdStruct(kGA106LabSelector_GetDeviceIdentity,
                         sizeof(struct GA106LabIdentityV1), "identity");
    }
    if (strcmp(argv[1], "status") == 0) {
        return cmdStruct(kGA106LabSelector_GetServiceState,
                         sizeof(struct GA106LabStatusV1), "status");
    }
    return usage(argv[0]);
}
