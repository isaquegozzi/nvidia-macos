// test-contract.c — testes de contrato ABI (offline, sem kernel).
// Compilar: clang -o test-contract test-contract.c ga106ctl-validate.c
// Cobre: tamanhos exatos 32, versão, rejeição de size/version errados.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "GA106LabProtocol.h"
#include "ga106ctl-validate.h"

int main(void)
{
    struct GA106LabIdentityV1 goodId;
    struct GA106LabStatusV1 goodSt;
    uint8_t shortBuf[16];
    uint8_t bigBuf[64];

    memset(&goodId, 0, sizeof(goodId));
    goodId.size = (uint32_t) sizeof(goodId);
    goodId.version = GA106LAB_UC_PROTOCOL_VERSION;
    goodId.vendor = 0x10de;
    goodId.device = 0x2504;
    goodId.providerEntryID = 0x100000239ull;
    goodId.state = kGA106LabState_StartSuccess;
    assert(ga106ctl_check_identity(&goodId, sizeof(goodId)) == 0);

    memset(&goodSt, 0, sizeof(goodSt));
    goodSt.size = (uint32_t) sizeof(goodSt);
    goodSt.version = GA106LAB_UC_PROTOCOL_VERSION;
    goodSt.state = kGA106LabState_StartSuccess;
    goodSt.providerConfirmed = 1;
    goodSt.revision = GA106LAB_DRIVER_VERSION_U32;
    assert(ga106ctl_check_status(&goodSt, sizeof(goodSt)) == 0);

    /* size errado / curto / versão errada / NULL. */
    assert(ga106ctl_check_identity(&goodId, 16) != 0);
    assert(ga106ctl_check_identity(&goodId, 64) != 0);
    assert(ga106ctl_check_status(&goodSt, 16) != 0);
    assert(ga106ctl_check_status(&goodSt, 64) != 0);
    goodId.version = 99;
    assert(ga106ctl_check_identity(&goodId, sizeof(goodId)) != 0);
    goodId.version = GA106LAB_UC_PROTOCOL_VERSION;
    goodSt.version = 99;
    assert(ga106ctl_check_status(&goodSt, sizeof(goodSt)) != 0);
    assert(ga106ctl_check_identity(NULL, 32) != 0);
    assert(ga106ctl_check_status(NULL, 32) != 0);
    memset(shortBuf, 0, sizeof(shortBuf));
    assert(ga106ctl_check_identity(shortBuf, sizeof(shortBuf)) != 0);
    memset(bigBuf, 0, sizeof(bigBuf));
    assert(ga106ctl_check_status(bigBuf, sizeof(bigBuf)) != 0);

    printf("test-contract: ALL PASS (12 asserções)\n");
    return 0;
}
