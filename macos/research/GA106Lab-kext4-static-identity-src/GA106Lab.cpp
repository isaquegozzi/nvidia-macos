//
// GA106Lab.cpp — KEXT0 mínimo de observação GA106 (spec TG-KEXT0).
//
// Fluxo: START_ENTER → super::start → cast IOPCIDevice → copyProperty (registry
// já publicado) → IOLog → START_SUCCESS → return true. Nenhuma chamada toca o
// dispositivo: sem open/close, sem config space, sem BAR, sem DMA, sem GSP.
//
// HARDWARE_MUTATION_EXPECTED = NO por construção (ver auditoria no README).
//

#include "GA106Lab.hpp"
#include "GA106LabUserClient.hpp"
#include "GA106LabCoreValidate.h"
#include "GA106LabMmioFlow.hpp"
#include "GA106LabStaticIdentityFlow.hpp"
#include "GA106LabRealOps.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOMapTypes.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>
#include <mach/kmod.h>

// TG-KEXT4-FIX (Astra FINDING 1): encoding verificado nos headers locais.
// kIOMapCacheMask e kIOMapInhibitCache já estão no mesmo domínio/shift
// (CacheShift=8 aplicado na definição); nenhuma conversão extra necessária.
// Estes asserts provam que GA106LabCoreValidate.h espelha o kernel.
static_assert(kIOMapReadOnly == 0x00001000,
              "kIOMapReadOnly encoding changed");
static_assert(kIOMapCacheMask == 0x00000F00,
              "kIOMapCacheMask encoding changed");
static_assert(kIOMapCacheShift == 8, "kIOMapCacheShift changed");
static_assert(kIOMapInhibitCache == 0x00000100,
              "kIOMapInhibitCache encoding changed");
static_assert(kIOMapCopybackCache == 0x00000300,
              "kIOMapCopybackCache encoding changed");
static_assert(kIOMapWriteCombineCache == 0x00000400,
              "kIOMapWriteCombineCache encoding changed");
static_assert(kIOMapDefaultCache == 0x00000000,
              "kIOMapDefaultCache encoding changed");
static_assert(GA106LAB_MAP_READONLY_BIT == 0x00001000u,
              "core RO mirror diverged");
static_assert(GA106LAB_MAP_CACHE_MASK == 0x00000F00u,
              "core cache mask diverged");
static_assert(GA106LAB_MAP_CACHE_INHIBIT == 0x00000100u,
              "core inhibit diverged");

#undef super
#define super IOService

OSDefineMetaClassAndStructors(GA106Lab, IOService);

// Formata até 4 bytes LE como "xx xx xx xx" para log (sem dumps gigantes).
static void GA106LabLogBytes(const char * label, const OSData * data)
{
    if (!data) {
        IOLog("[GA106Lab] %s=UNAVAILABLE_WITH_ZERO_PCI_ACCESS\n", label);
        return;
    }
    unsigned int len = data->getLength();
    const UInt8 * bytes = (const UInt8 *) data->getBytesNoCopy();
    if (!bytes || len == 0) {
        IOLog("[GA106Lab] %s=UNAVAILABLE_WITH_ZERO_PCI_ACCESS\n", label);
        return;
    }
    if (len > 4) {
        len = 4;
    }
    char buf[16];
    unsigned int pos = 0;
    for (unsigned int i = 0; i < len; i++) {
        pos += (unsigned int) snprintf(buf + pos, sizeof(buf) - pos, "%02x%s",
                                       bytes[i], (i + 1 < len) ? " " : "");
        if (pos >= sizeof(buf)) {
            break;
        }
    }
    buf[sizeof(buf) - 1] = '\0';
    IOLog("[GA106Lab] %s=%s\n", label, buf);
}

// Publica string no PRÓPRIO serviço (dicionário em memória; provider intocado).
static void GA106LabPublishString(GA106Lab * self, const char * key,
                                  const char * value)
{
    if (!self || !key || !value) {
        return;
    }
    OSString * s = OSString::withCString(value);
    if (s) {
        self->setProperty(key, s);
        s->release();
    }
}

bool GA106Lab::init(OSDictionary * dictionary)
{
    if (!super::init(dictionary)) {
        return false;
    }
    fCache.vendor = 0;
    fCache.device = 0;
    fCache.providerEntryID = 0;
    fCache.state = kGA106LabState_Unknown;
    fCache.providerConfirmed = 0;
    fCacheValid = false;
    fActiveClient = NULL;
    // Trava do slot single-client (spin curto; sem workloop/command gate).
    fSlotLock = IOSimpleLockAlloc();
    if (!fSlotLock) {
        return false;
    }
    return true;
}

void GA106Lab::free(void)
{
    if (fSlotLock) {
        IOSimpleLockFree(fSlotLock);
        fSlotLock = NULL;
    }
    super::free();
}

bool GA106Lab::start(IOService * provider)
{
    IOLog("[GA106Lab] START_ENTER\n");
    GA106LabPublishString(this, "GA106LabState", "START_ENTER");
    GA106LabPublishString(this, "GA106LabRevision", "TG-KEXT4-STATIC-IDENTITY-READS");

    uint32_t vendor = 0;
    uint32_t device = 0;
    uint64_t entryID = 0;
    bool     haveVendor = false;
    bool     haveDevice = false;

    if (!super::start(provider)) {
        IOLog("[GA106Lab] START_FAILED super::start\n");
        return false;
    }

    // ATTACH_ONLY: confirma o provider sem abrir/controlar o dispositivo.
    IOPCIDevice * pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci) {
        IOLog("[GA106Lab] START_FAILED provider is not IOPCIDevice\n");
        GA106LabPublishString(this, "GA106LabState", "PROVIDER_CAST_FAILED");
        super::stop(provider);
        return false;
    }
    IOLog("[GA106Lab] PROVIDER_CONFIRMED\n");

    // SOMENTE propriedades já publicadas no IORegistry. Nenhum acesso PCI.
    // copyProperty() retém o objeto publicado; getBytesNoCopy() acessa os bytes
    // já existentes sem copiar e sem tocar hardware. A decodificação u16 LE
    // abaixo é aritmética pura em CPU sobre esses bytes.
    OSObject * obj = NULL;
    OSData * data = NULL;
    const UInt8 * b = NULL;

    obj = pci->copyProperty("vendor-id");
    data = OSDynamicCast(OSData, obj);
    b = (data && data->getLength() >= 2)
        ? (const UInt8 *) data->getBytesNoCopy() : NULL;
    if (b) {
        char hex[8];
        snprintf(hex, sizeof(hex), "%02x%02x", b[1], b[0]);
        IOLog("[GA106Lab] vendor=%s\n", hex);
        GA106LabPublishString(this, "GA106LabProviderVendor", hex);
        vendor = ((uint32_t) b[1] << 8) | b[0];
        haveVendor = true;
    } else {
        IOLog("[GA106Lab] vendor=UNAVAILABLE_WITH_ZERO_PCI_ACCESS\n");
    }
    OSSafeReleaseNULL(obj);

    obj = pci->copyProperty("device-id");
    data = OSDynamicCast(OSData, obj);
    b = (data && data->getLength() >= 2)
        ? (const UInt8 *) data->getBytesNoCopy() : NULL;
    if (b) {
        char hex[8];
        snprintf(hex, sizeof(hex), "%02x%02x", b[1], b[0]);
        IOLog("[GA106Lab] device=%s\n", hex);
        GA106LabPublishString(this, "GA106LabProviderDevice", hex);
        device = ((uint32_t) b[1] << 8) | b[0];
        haveDevice = true;
    } else {
        IOLog("[GA106Lab] device=UNAVAILABLE_WITH_ZERO_PCI_ACCESS\n");
    }
    OSSafeReleaseNULL(obj);

    obj = pci->copyProperty("class-code");
    GA106LabLogBytes("class-code", OSDynamicCast(OSData, obj));
    OSSafeReleaseNULL(obj);

    obj = pci->copyProperty("built-in");
    GA106LabLogBytes("built-in", OSDynamicCast(OSData, obj));
    OSSafeReleaseNULL(obj);

    entryID = pci->getRegistryEntryID();
    IOLog("[GA106Lab] registryEntryID=%llu\n",
          (unsigned long long) entryID);
    OSNumber * eid = OSNumber::withNumber(entryID, 64);
    if (eid) {
        setProperty("GA106LabProviderEntryID", eid);
        eid->release();
    }

    char path[128];
    int pathLen = (int) sizeof(path);
    // getPath formata string do registry (IOService plane), sem acesso ao hardware.
    if (pci->getPath(path, &pathLen, gIOServicePlane)) {
        path[sizeof(path) - 1] = '\0';
        IOLog("[GA106Lab] path=%s\n", path);
    } else {
        IOLog("[GA106Lab] path=UNAVAILABLE_WITH_ZERO_PCI_ACCESS\n");
    }

    IOLog("[GA106Lab] START_SUCCESS\n");
    GA106LabPublishString(this, "GA106LabState", "START_SUCCESS");
    // Cache imutável p/ UserClient (preenchido UMA vez; sem polling depois).
    fCache.vendor = vendor;
    fCache.device = device;
    fCache.providerEntryID = entryID;
    fCache.state = kGA106LabState_StartSuccess;
    fCache.providerConfirmed = 1;
    // Identidade válida SOMENTE com vendor E device (nunca parcial).
    fCacheValid = (haveVendor && haveDevice);
    // Publica o serviço para o matching userspace (ga106ctl). Sem isso,
    // IOServiceGetMatchingService NÃO encontra serviços não-registrados
    // (provado offline: nó ativo 1.0.0 existe mas matching retorna not-found).
    // Publicar ≠ framebuffer/display: serviço IOService puro, sem hardware.
    registerService();
    return true;
}

void GA106Lab::stop(IOService * provider)
{
    IOLog("[GA106Lab] STOP\n");
    // Limpa o slot sob trava: teardown do provider não deixa vaga presa.
    if (fSlotLock) {
        IOSimpleLockLock(fSlotLock);
        fActiveClient = NULL;
        IOSimpleLockUnlock(fSlotLock);
    }
    super::stop(provider);
}

bool GA106Lab::copyCachedState(GA106LabCachedState * out) const
{
    if (!out || !fCacheValid) {
        return false;
    }
    *out = fCache; // cópia por valor; sem expor ponteiros internos.
    return true;
}

// Allowlist FIXA de leitura PCI config, SEMPRE no próprio provider.
// Método: wrappers 1-arg (configRead8/16/32(offset)) — o provider usa seu
// space/endereço interno (provado no header SDK + XNU: inline →
// extendedConfigRead*; forma 2-arg repassa a union ao bridge e com union
// zerada lia 00:00.0 — bug TG-KEXT2 original, sem union aqui).
// Raws preservados; identidade 10de:2504 EXIGIDA (WRONG_DEVICE nunca aceito).
IOReturn GA106Lab::copyPciSnapshot(GA106LabPciSnapshotV1 * out)
{
    IOPCIDevice * pci;
    uint16_t      vendor;
    uint16_t      device;
    int           i;
    if (!out) {
        return kIOReturnBadArgument;
    }
    // Zero total primeiro: sem bytes não inicializados em nenhum caminho.
    out->size = 0;
    out->version = 0;
    out->status = kGA106LabPciStatus_Unavailable;
    out->validMask = 0;
    out->vendorId = 0;
    out->deviceId = 0;
    out->command = 0;
    out->statusReg = 0;
    out->revisionId = 0;
    out->progIf = 0;
    out->subclass = 0;
    out->classCode = 0;
    out->cacheLineSize = 0;
    out->latencyTimer = 0;
    out->headerType = 0;
    out->bist = 0;
    for (i = 0; i < 6; i++) {
        out->barRaw[i] = 0;
    }
    out->subsystemVendorId = 0;
    out->subsystemId = 0;
    out->expansionRomRaw = 0;
    out->capPointer = 0;
    out->interruptLine = 0;
    out->interruptPin = 0;
    out->reserved0 = 0;
    for (i = 0; i < 4; i++) {
        out->reserved[i] = 0;
    }

    pci = OSDynamicCast(IOPCIDevice, getProvider());
    if (!pci) {
        return kIOReturnNotReady; // provider indisponível.
    }

    // Identidade: decide disponibilidade do snapshot.
    vendor = pci->configRead16(kIOPCIConfigVendorID);
    if (vendor == 0xffff) {
        return kIOReturnNotReady; // dispositivo ausente/inacessível.
    }
    device = pci->configRead16(kIOPCIConfigDeviceID);
    if (vendor != 0x10de || device != 0x2504) {
        return kIOReturnNoDevice; // acessível, mas NÃO é a RTX esperada.
    }
    out->vendorId = vendor;
    out->deviceId = device;
    out->validMask |= kGA106LabPciValid_Identity;

    out->command = pci->configRead16(kIOPCIConfigCommand);
    out->statusReg = pci->configRead16(kIOPCIConfigStatus);
    out->validMask |= kGA106LabPciValid_Command;

    out->revisionId = pci->configRead8(kIOPCIConfigRevisionID);
    out->progIf = pci->configRead8(kIOPCIConfigRevisionID + 1);
    out->subclass = pci->configRead8(kIOPCIConfigRevisionID + 2);
    out->classCode = pci->configRead8(kIOPCIConfigRevisionID + 3);
    out->cacheLineSize = pci->configRead8(kIOPCIConfigCacheLineSize);
    out->latencyTimer = pci->configRead8(kIOPCIConfigCacheLineSize + 1);
    out->headerType = pci->configRead8(kIOPCIConfigHeaderType);
    out->bist = pci->configRead8(kIOPCIConfigHeaderType + 1);
    out->validMask |= kGA106LabPciValid_Header;

    // BARs: SOMENTE valor bruto. Write-probe PROIBIDO (sem 0xffffffff).
    for (i = 0; i < 6; i++) {
        out->barRaw[i] = pci->configRead32(
            (UInt8) (kIOPCIConfigBaseAddress0 + i * 4));
    }
    out->validMask |= kGA106LabPciValid_Bars;

    out->subsystemVendorId = pci->configRead16(kIOPCIConfigSubSystemVendorID);
    out->subsystemId = pci->configRead16(kIOPCIConfigSubSystemID);
    out->validMask |= kGA106LabPciValid_Subsys;

    out->expansionRomRaw = pci->configRead32(kIOPCIConfigExpansionROMBase);
    out->capPointer = pci->configRead8(kIOPCIConfigCapabilitiesPtr);
    out->interruptLine = pci->configRead8(kIOPCIConfigInterruptLine);
    out->interruptPin = pci->configRead8(kIOPCIConfigInterruptLine + 1);
    // IntLine 0xff (sem IRQ roteada) NÃO invalida: raw preservado.
    out->validMask |= kGA106LabPciValid_CapIrq;

    out->size = (uint32_t) sizeof(*out);
    out->version = GA106LAB_UC_PROTOCOL_VERSION;
    out->status = kGA106LabPciStatus_Ok;
    return kIOReturnSuccess;
}

// TG-KEXT3 BAR0 probe: descobre o resource BAR0 por validação runtime,
// cria UM mapa RO+InhibitCache, valida metadata, preenche a ABI, libera o
// mapa antes de retornar. NUNCA dereferencia MMIO (nem getVirtualAddress),
// nunca escreve config, nunca altera MSE/BME. Mapa nunca sobrevive ao call.
#define kGA106LabBar0ExpectedLength (0x1000000ULL) /* 16 MiB baseline */
#define kGA106LabBarMapRequiredOpts (kIOMapReadOnly | kIOMapInhibitCache)

IOReturn GA106Lab::probeBar0Map(GA106LabBarMapInfoV1 * out)
{
    IOPCIDevice *     pci;
    uint16_t          vendor;
    uint16_t          device;
    uint16_t          commandBefore;
    uint16_t          commandAfter;
    uint32_t          bar0Raw;
    uint64_t          bar0Base;
    IOItemCount       count;
    unsigned int      i;
    int               matchIndex;
    int               matchCount;
    IODeviceMemory *  matchDesc;
    IOByteCount       resLen;
    IOPhysicalAddress resPhys;
    IOMemoryMap *     map;
    IOByteCount       mapLen;
    IOOptionBits      mapOpts;

    if (!out) {
        return kIOReturnBadArgument;
    }
    // Zero total primeiro: sem bytes não inicializados em nenhum caminho.
    out->size = 0;
    out->version = 0;
    out->status = kGA106LabBarMapStatus_Failed;
    out->validMask = 0;
    out->semanticBar = kGA106LabBarMapSemantic_Bar0;
    out->resourceIndex = 0;
    out->physicalBase = 0;
    out->resourceLength = 0;
    out->mappedLength = 0;
    out->resourceFlags = 0;
    out->mapOptions = 0;
    out->mapState = 0;
    out->reserved0 = 0;
    for (i = 0; i < 4; i++) {
        out->reserved[i] = 0;
    }

    pci = OSDynamicCast(IOPCIDevice, getProvider());
    if (!pci) {
        return kIOReturnNotReady;
    }

    // Identidade: mesmo gate do fix 1.2.1 (wrappers 1-arg, sem union).
    vendor = pci->configRead16(kIOPCIConfigVendorID);
    if (vendor == 0xffff) {
        return kIOReturnNotReady;
    }
    device = pci->configRead16(kIOPCIConfigDeviceID);
    if (vendor != 0x10de || device != 0x2504) {
        return kIOReturnNoDevice;
    }

    // Command before (MSE deve estar ON; BME pode ficar OFF).
    // Helper compartilhado com o seam offline (mesma decisão).
    commandBefore = pci->configRead16(kIOPCIConfigCommand);
    if (!GA106LabCommandMseOn(commandBefore)) {
        return kIOReturnNotReady; // MSE OFF: abortar, nunca habilitar.
    }

    // Base BAR0 atual do provider (32-bit non-prefetch: bits 2:1 == 00).
    // Helper compartilhado; rejeita I/O (bit0) e 64-bit/prefetch (bits 2:1).
    bar0Raw = pci->configRead32(kIOPCIConfigBaseAddress0);
    if (!GA106LabBar0RawValid(bar0Raw)) {
        return kIOReturnNoDevice; // I/O ou 64-bit/prefetch: não é BAR0 32-bit.
    }
    bar0Base = GA106LabBar0BaseFromRaw(bar0Raw);

    // Discovery: iterar SOMENTE dentro do count real (sem index cego).
    count = pci->getDeviceMemoryCount();
    matchIndex = -1;
    matchCount = 0;
    matchDesc = NULL;
    for (i = 0; i < (unsigned int) count; i++) {
        IODeviceMemory *  d;
        IOByteCount       l;
        IOPhysicalAddress p;
        d = pci->getDeviceMemoryWithIndex(i);
        if (!d) {
            continue;
        }
        l = d->getLength();
        p = d->getPhysicalAddress();
        // Match estrito via helper compartilhado: 16 MiB + base == BAR0
        // atual. Rejeita VRAM (~256M), BAR3 (~32M), ROM (~512K),
        // I/O BAR5 (nem aparece aqui) e sub-ranges.
        if (GA106LabBarResourceMatches((uint64_t)l, (uint64_t)p, bar0Base)) {
            matchIndex = (int) i;
            matchCount++;
            matchDesc = d; // borrowed/provider-owned: SEM release.
        }
    }
    if (matchCount != 1 || !matchDesc || matchIndex < 0) {
        return kIOReturnNotFound; // 0 ou >=2: FAIL, sem fallback.
    }
    resLen = matchDesc->getLength();
    resPhys = matchDesc->getPhysicalAddress();
    if (resLen != kGA106LabBar0ExpectedLength) {
        return kIOReturnNoDevice;
    }
    if ((uint64_t) resPhys != bar0Base) {
        return kIOReturnNoDevice;
    }
    out->status = kGA106LabBarMapStatus_ResourceFound;

    // Create: UM mapa, região validada, opções exatas por símbolo.
    map = pci->mapDeviceMemoryWithIndex((unsigned int) matchIndex,
                                        kIOMapReadOnly | kIOMapInhibitCache);
    if (!map) {
        out->status = kGA106LabBarMapStatus_Failed;
        return kIOReturnNoResources;
    }
    out->status = kGA106LabBarMapStatus_MapCreated;

    // Validate metadata SEM dereference (getVirtualAddress NUNCA chamado).
    mapLen = map->getLength();
    mapOpts = map->getMapOptions();
    if (mapLen > resLen) {
        map->release();
        map = NULL;
        out->status = kGA106LabBarMapStatus_Failed;
        return kIOReturnNoMemory;
    }
    if (mapLen != resLen) { // full-resource policy v1.
        map->release();
        map = NULL;
        out->status = kGA106LabBarMapStatus_Failed;
        return kIOReturnNoMemory;
    }
    // REQUIRED_BITS REMOVIDO (Astra FINDING 1: aceitava 0x1300
    // ReadOnly+Copyback porque (0x1300 & 0x1100)==0x1100).
    // Validação EXATA via helper compartilhado (mesmo usado pelos testes):
    // separa flag ReadOnly do campo cache e exige campo == InhibitCache.
    // Rejeita RO+Copyback, RO+WriteCombine, RO+Default e qualquer
    // cache != Inhibit; rejeita Inhibit sem RO e RO sem Inhibit.
    // Bits ortogonais fora de (ReadOnly|CacheMask) continuam permitidos.
    if (!GA106LabMapOptionsValidROInhibit((uint32_t)mapOpts)) {
        map->release();
        map = NULL;
        out->status = kGA106LabBarMapStatus_Failed;
        return kIOReturnNotPermitted;
    }
    // Defesa em profundidade: o helper acima já cobre, mas as duas
    // condições separadas documentam a separação flag vs. campo exigida.
    if (((mapOpts & kIOMapReadOnly) == 0) ||
        ((mapOpts & kIOMapCacheMask) != kIOMapInhibitCache)) {
        map->release();
        map = NULL;
        out->status = kGA106LabBarMapStatus_Failed;
        return kIOReturnNotPermitted;
    }
    out->status = kGA106LabBarMapStatus_MapValidated;

    // Fill: só metadata segura (SEM VA/ponteiro kernel).
    out->size = (uint32_t) sizeof(*out);
    out->version = GA106LAB_UC_PROTOCOL_VERSION;
    out->semanticBar = kGA106LabBarMapSemantic_Bar0;
    out->resourceIndex = (uint32_t) matchIndex;
    out->physicalBase = (uint64_t) resPhys; // canônico = resource.
    out->resourceLength = (uint64_t) resLen;
    out->mappedLength = (uint64_t) mapLen;
    out->resourceFlags = 0; // sem fonte não-ambígua: zero, sem VALID bit.
    out->mapOptions = (uint32_t) (mapOpts & 0xFFFFFFFFu);
    out->mapState = 1;
    out->reserved0 = 0;
    out->validMask = kGA106LabBarMapValid_Resource |
                     kGA106LabBarMapValid_PhysicalBase |
                     kGA106LabBarMapValid_ResourceLength |
                     kGA106LabBarMapValid_MappedLength |
                     kGA106LabBarMapValid_MapOptions;

    // Release: mapa nunca sobrevive ao selector (single cleanup path).
    map->release();
    map = NULL;
    out->status = kGA106LabBarMapStatus_MapReleased;

    // Command after: qualquer mudança = failure (sem restaurar/escrever).
    commandAfter = pci->configRead16(kIOPCIConfigCommand);
    if (commandAfter != commandBefore) {
        out->status = kGA106LabBarMapStatus_Failed;
        out->validMask &=
            (uint32_t) ~kGA106LabBarMapValid_ReleaseConfirmed;
        return kIOReturnError;
    }
    out->validMask |= kGA106LabBarMapValid_ReleaseConfirmed;
    return kIOReturnSuccess;
}

// TG-KEXT4 primeira leitura MMIO — wrapper FINO (ONE_SHARED_MMIO_FLOW).
// Todo o fluxo decisório vive em GA106LabMmioFlow.hpp::RunFirstMmioReadFlow
// (mesmo template instanciado pelos testes com MockOps). Aqui só:
// zero-check, cast do provider, RealOps, chamada ao shared flow.
// Nenhuma cópia duplicada de discovery/ambiguidade/map/read/release.
// Semântica preservada: NV_PMC_BOOT_0, BAR0+0x0, 32-bit, 1 load, 0 stores.
IOReturn GA106Lab::readPmcBoot0(GA106LabMmioReadV1 * out)
{
    IOPCIDevice *pci = NULL;
    if (!out) {
        return kIOReturnBadArgument;
    }
    pci = OSDynamicCast(IOPCIDevice, getProvider());
    {
        GA106LabRealOps ops(pci);
        return RunFirstMmioReadFlow(ops, *out);
    }
}

// (corpo duplicado removido; fluxo único em GA106LabMmioFlow.hpp)

// TG-KEXT4-B identidade estática — wrapper FINO
// (ONE_SHARED_STATIC_IDENTITY_FLOW). Todo o fluxo vive em
// GA106LabStaticIdentityFlow.hpp::RunStaticIdentityReadsFlow (mesmo
// template dos testes com MockOps). Aqui só: null-check, cast, RealOps,
// chamada ao shared flow. NÃO relê BOOT_0. Semântica: BOOT_42 @0xA00
// (single-read; BOOT_2 removido), 1 load, 0 stores.
IOReturn GA106Lab::readStaticIdentity(GA106LabStaticIdentityV1 * out)
{
    IOPCIDevice *pci = NULL;
    if (!out) {
        return kIOReturnBadArgument;
    }
    pci = OSDynamicCast(IOPCIDevice, getProvider());
    {
        GA106LabRealOps ops(pci);
        return RunStaticIdentityReadsFlow(ops, *out);
    }
}

void GA106Lab::clientClosed(GA106LabUserClient * client)
{
    // Sem retain (evita ciclo): só limpa se for o ativo, sob trava.
    // Direção fail-safe: se algo escapar, novos opens são negados, nunca
    // duplicados. Idempotente (comparação + NULL), sem double-clear.
    if (!client || !fSlotLock) {
        return;
    }
    IOSimpleLockLock(fSlotLock);
    if (fActiveClient == client) {
        fActiveClient = NULL;
    }
    IOSimpleLockUnlock(fSlotLock);
}

IOReturn GA106Lab::newUserClient(task_t owningTask, void * securityID,
                                 UInt32 type, OSDictionary * properties,
                                 IOUserClient ** handler)
{
    if (!handler) {
        return kIOReturnBadArgument;
    }
    *handler = NULL;

    // Superfície mínima: protocolo v1 só define type 0 (ga106ctl usa 0).
    if (type != 0) {
        return kIOReturnUnsupported;
    }
    (void) properties;

    // ROOT-ONLY via API pública (kIOClientPrivilegeAdministrator = "root").
    // Sem entitlement privado. Checagens stateless ANTES da trava (hold curto).
    if (IOUserClient::clientHasPrivilege(securityID,
                                         kIOClientPrivilegeAdministrator)
        != kIOReturnSuccess) {
        return kIOReturnNotPrivileged;
    }

    // Reserva atômica do slot: check + assignment sob a mesma trava
    // (IOSimpleLock: spin curto, sem workloop/command gate).
    if (!fSlotLock) {
        return kIOReturnNoResources;
    }
    IOSimpleLockLock(fSlotLock);
    if (fActiveClient) {
        IOSimpleLockUnlock(fSlotLock);
        return kIOReturnExclusiveAccess;
    }
    fActiveClient = (GA106LabUserClient *) 0x1; // reserva (não é ponteiro válido)
    IOSimpleLockUnlock(fSlotLock);

    GA106LabUserClient * client = OSTypeAlloc(GA106LabUserClient);
    if (!client) {
        goto failRelease;
    }
    if (!client->initWithTask(owningTask, securityID, type)) {
        client->release();
        goto failRelease;
    }
    if (!client->attach(this)) {
        client->release();
        goto failRelease;
    }
    if (!client->start(this)) {
        client->detach(this);
        client->release();
        goto failRelease;
    }
    IOSimpleLockLock(fSlotLock);
    fActiveClient = client; // não retido; limpo em clientClosed().
    IOSimpleLockUnlock(fSlotLock);
    *handler = client;      // ownership transferida à conexão.
    return kIOReturnSuccess;

failRelease:
    // Falha parcial: libera a reserva; slot nunca fica preso.
    IOSimpleLockLock(fSlotLock);
    if (fActiveClient == (GA106LabUserClient *) 0x1) {
        fActiveClient = NULL;
    }
    IOSimpleLockUnlock(fSlotLock);
    return kIOReturnBadArgument;
}

// ---------------------------------------------------------------------------
// Lifecycle de MÓDULO do kernel — distinto de GA106Lab::start(provider).
//
// GA106Lab::start() acima é lifecycle do SERVIÇO IOKit (roda por provider casado).
// Abaixo vai só a infraestrutura normal de registro do KEXT: o kernel exige
// rotinas start/stop de módulo apontadas por _kmod_info. São triviais por
// desenho — NENHUMA lógica da RTX vive aqui; IOKit personalities dirigem o resto.
// ---------------------------------------------------------------------------

kern_return_t GA106Lab_module_start(kmod_info_t * ki, void * d)
{
    (void) ki;
    (void) d;
    return KERN_SUCCESS;
}

kern_return_t GA106Lab_module_stop(kmod_info_t * ki, void * d)
{
    (void) ki;
    (void) d;
    return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL(org_nvidia_macos_GA106Lab, "1.4.1",
                   GA106Lab_module_start, GA106Lab_module_stop);
