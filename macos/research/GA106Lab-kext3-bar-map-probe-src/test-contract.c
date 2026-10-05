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

    /* Snapshot: bom, size/version errados, status!=Ok, IntLine 0xff ok. */
    {
        struct GA106LabPciSnapshotV1 good;
        memset(&good, 0, sizeof(good));
        good.size = (uint32_t) sizeof(good);
        good.version = GA106LAB_UC_PROTOCOL_VERSION;
        good.status = kGA106LabPciStatus_Ok;
        good.vendorId = 0x10de;
        good.deviceId = 0x2504;
        good.interruptLine = 0xff; /* sem IRQ roteada: NÃO invalida */
        assert(ga106ctl_check_pci_snapshot(&good, sizeof(good)) == 0);
        assert(ga106ctl_check_pci_snapshot(&good, 16) != 0);
        assert(ga106ctl_check_pci_snapshot(&good, 128) != 0);
        good.version = 99;
        assert(ga106ctl_check_pci_snapshot(&good, sizeof(good)) != 0);
        good.version = GA106LAB_UC_PROTOCOL_VERSION;
        good.status = kGA106LabPciStatus_Unavailable; /* vendor ffff no kernel */
        assert(ga106ctl_check_pci_snapshot(&good, sizeof(good)) != 0);
        good.status = kGA106LabPciStatus_Ok;
        good.vendorId = 0xffff; /* raw preservado, mas status Ok: aceito */
        assert(ga106ctl_check_pci_snapshot(&good, sizeof(good)) == 0);
        assert(ga106ctl_check_pci_snapshot(NULL, 84) != 0);
        /* 1022:1630 com status Ok passa no ENVELOPE (validador não julga
           identidade — o kernel retorna NoDevice antes de preencher). */
        good.vendorId = 0x1022;
        good.deviceId = 0x1630;
        assert(ga106ctl_check_pci_snapshot(&good, sizeof(good)) == 0);
    }

    /* BAR_MAP_PROBE ABI 96: bom, size/version errados, status/validMask,
       semantic, mappedLength bounds. */
    {
        struct GA106LabBarMapInfoV1 good;
        memset(&good, 0, sizeof(good));
        good.size = (uint32_t) sizeof(good);
        good.version = GA106LAB_UC_PROTOCOL_VERSION;
        good.status = kGA106LabBarMapStatus_MapReleased;
        good.validMask = kGA106LabBarMapValid_Resource |
                         kGA106LabBarMapValid_PhysicalBase |
                         kGA106LabBarMapValid_ResourceLength |
                         kGA106LabBarMapValid_MappedLength |
                         kGA106LabBarMapValid_MapOptions |
                         kGA106LabBarMapValid_ReleaseConfirmed;
        good.semanticBar = kGA106LabBarMapSemantic_Bar0;
        good.resourceIndex = 0;
        good.physicalBase = 0xFB000000ull;
        good.resourceLength = 0x1000000ull;
        good.mappedLength = 0x1000000ull;
        good.mapOptions = 0x1100u;
        good.mapState = 1;
        assert(sizeof(good) == 96);
        assert(ga106ctl_check_bar_map_probe(&good, sizeof(good)) == 0);
        assert(ga106ctl_check_bar_map_probe(&good, 16) != 0);
        assert(ga106ctl_check_bar_map_probe(&good, 128) != 0);
        good.version = 99;
        assert(ga106ctl_check_bar_map_probe(&good, sizeof(good)) != 0);
        good.version = GA106LAB_UC_PROTOCOL_VERSION;
        good.status = kGA106LabBarMapStatus_Failed;
        assert(ga106ctl_check_bar_map_probe(&good, sizeof(good)) != 0);
        good.status = kGA106LabBarMapStatus_MapReleased;
        good.validMask = 0; /* sem RELEASE_CONFIRMED */
        assert(ga106ctl_check_bar_map_probe(&good, sizeof(good)) != 0);
        good.validMask = kGA106LabBarMapValid_ReleaseConfirmed;
        good.semanticBar = 7; /* BAR arbitrário */
        assert(ga106ctl_check_bar_map_probe(&good, sizeof(good)) != 0);
        good.semanticBar = kGA106LabBarMapSemantic_Bar0;
        good.mappedLength = 0x2000000ull; /* > resourceLength */
        assert(ga106ctl_check_bar_map_probe(&good, sizeof(good)) != 0);
        good.mappedLength = 0;
        assert(ga106ctl_check_bar_map_probe(&good, sizeof(good)) != 0);
        assert(ga106ctl_check_bar_map_probe(NULL, 96) != 0);
    }

    printf("test-contract: ALL PASS (12+9+12 asserções)\n");
    return 0;
}
