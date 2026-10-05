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
#include "GA106LabGspSysmemFlow.hpp"
#include "GA106LabGfwReadinessFlow.hpp"
#include "GA106LabFlushPrewriteFlow.hpp"
#include "GA106LabInitFreeLockFlow.hpp"
#include "GA106LabRealOps.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOMapTypes.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>
#include <mach/kmod.h>
#include <mach/mach_time.h> // mach_absolute_time (relógio monotônico GFW).
#include <kern/clock.h>     // absolutetime_to_nanoseconds (conversão SDK).

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

// TEST-PATH-FIX: sem policy duplicada aqui. Toda decisão de init/free/lock
// vive em GA106LabInitFreeLockFlow.hpp (ONE_SHARED_INIT_FREE_LOCK_FLOW).
// GA106Lab::init/free são wrappers finos que delegam ao shared flow.
// DUPLICATED_INIT_FREE_TEST_LOGIC = NO por construção.

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
    fSysmemDesc = NULL;
    fSysmemCmd = NULL;
    fSysmemDmaAddr = 0;
    fSysmemSegLen = 0;
    fSysmemState = kSysmemPhase_Empty;
    fSysmemBusy = false;
    fSysmemStopping = false;
    fGfwBusy = false;
    fGfwStopping = false;
    fFlushDesc = NULL;
    fFlushCmd = NULL;
    fFlushDmaAddr = 0;
    fFlushSegLen = 0;
    fFlushState = kGA106LabFlushPrewritePhase_Unprepared;
    fFlushBusy = false;
    fFlushStopping = false;
    fProductionPhase = kProdPhase_Detached;
    GA106LabBlockLogInit(&fBlockLog);
    fChannelContractOk = false;
    fVmContractOk = false;
    // Delega ao shared production path (mesmo usado pelos testes offline).
    // LOCK_INIT_SHARED_PATH = YES
    {
        GA106LabOwnerRealOps ops(this);
        return InitOwnerLocksFlow(ops);
    }
}

void GA106Lab::free(void)
{
    // Flush page Gate A (defensivo; stop() já drenou e desmontou).
    // Usa o MESMO helper failure-safe testado (sem KPI direto aqui).
    // CleanupFailed => objetos retidos de propósito (ainda referenciados).
    if (fFlushDesc || fFlushCmd ||
        (fFlushState != kGA106LabFlushPrewritePhase_Unprepared)) {
        GA106LabFlushRealOps flushFreeOps(this);
        bool canTouch = false;
        flushFreeOps.lock();
        canTouch = !flushFreeOps.isBusy();
        if (canTouch) {
            flushFreeOps.setBusy(true);
        }
        flushFreeOps.unlock();
        if (canTouch) {
            IOReturn tdRc = TeardownFlushPrewritePageFlowSafe(flushFreeOps);
            if (tdRc != kIOReturnSuccess) {
                IOLog("[GA106Lab] FLUSH_FREE_RETAINED_ON_CLEAR_FAIL\n");
            }
            flushFreeOps.lock();
            flushFreeOps.setBusy(false);
            flushFreeOps.unlock();
        } else {
            IOLog("[GA106Lab] FLUSH_UNEXPECTED_BUSY_IN_FREE\n");
        }
    }
    // Delega ao shared production path (mesmo usado pelos testes offline).
    // FREE_CLEANUP_FAILED_SHARED_PATH = YES
    // CLEAR_FAILURE_RELEASE_POLICY = LEAK_SAFE_NO_RELEASE (no shared flow).
    {
        GA106LabOwnerRealOps ops(this);
        FreeOwnerResourcesFlow(ops);
    }
    super::free();
}

bool GA106Lab::start(IOService * provider)
{
    IOLog("[GA106Lab] START_ENTER\n");
    GA106LabPublishString(this, "GA106LabState", "START_ENTER");
    GA106LabPublishString(this, "GA106LabRevision", "TG-KEXT8-PRODUCTION-ARCHITECTURE");

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

    noteProductionMilestone(kProdPhase_Attached);
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
    // Teardown sysmem pelo MESMO template dos testes (WaitForSysmemIdle
    // AndTeardownFlow): flag terminal + espera !BUSY sem lock retido +
    // reserva + teardown central. Nunca toca objetos de operação alheia;
    // nunca bloqueia sob spinlock.
    if (fSysmemLock) {
        GA106LabSysmemRealOps stopOps(this);
        WaitForSysmemIdleAndTeardownFlow(stopOps);
    }
    // Espera o poll GFW (selector 8) terminar pelo MESMO template dos testes
    // (WaitForGfwIdleFlow): flag terminal + espera !BUSY sem lock retido.
    // O mapa do poll é local por chamada — stop nunca libera recurso em uso.
    // WaitForGfwIdleFlow só usa lock/flags/wait (nunca provider/MMIO).
    if (fSysmemLock) {
        GA106LabGfwRealOps gfwStopOps(this);
        WaitForGfwIdleFlow(gfwStopOps);
    }
    // Gate A flush page teardown (TG-KEXT7): flag terminal + espera !BUSY +
    // teardown central via mesmo template sysmem (complete/clear/releases).
    // Zero MMIO writes (Gate A nunca programou hardware; sem unregister).
    if (fSysmemLock) {
        GA106LabFlushRealOps flushStopOps(this);
        // Marca stopping sob trava curta.
        flushStopOps.lock();
        flushStopOps.setStopping(true);
        flushStopOps.unlock();
        // Espera !busy sem lock retido.
        for (int i = 0; i < 4000; i++) {
            bool b = false;
            flushStopOps.lock();
            b = flushStopOps.isBusy();
            flushStopOps.unlock();
            if (!b) break;
            flushStopOps.waitMs(1);
        }
        // Reserva e teardown se ainda houver pagina.
        {
            bool doTeardown = false;
            flushStopOps.lock();
            if (!flushStopOps.isBusy() && flushStopOps.getState() != kGA106LabFlushPrewritePhase_Unprepared) {
                flushStopOps.setBusy(true);
                doTeardown = true;
            }
            flushStopOps.unlock();
            if (doTeardown) {
                // Teardown via helper failure-safe compartilhado (zero MMIO,
                // sem KPI direto em stop(); contrato STOP_USES_SHARED_FLOW).
                // clear != Success => CleanupFailed com objetos retidos
                // (controlled retention); nunca releases inseguros.
                IOReturn tdRc = TeardownFlushPrewritePageFlowSafe(flushStopOps);
                if (tdRc != kIOReturnSuccess) {
                    IOLog("[GA106Lab] FLUSH_TEARDOWN_RETAINED_ON_CLEAR_FAIL\n");
                }
                flushStopOps.lock();
                flushStopOps.setBusy(false);
                flushStopOps.unlock();
            }
        }
    }
    // Limpa o slot sob trava: teardown do provider não deixa vaga presa.
    if (fSlotLock) {
        IOSimpleLockLock(fSlotLock);
        fActiveClient = NULL;
        IOSimpleLockUnlock(fSlotLock);
    }
    // P80: teardown determinístico da fase (qualquer estado drenável volta a
    // Attached; LiveBlocked exige re-init). Contratos de modelo descartados.
    resetProductionPhaseForTeardown();
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
    noteProductionMilestone(kProdPhase_Bar0Mapped);
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
        IOReturn kr = RunStaticIdentityReadsFlow(ops, *out);
        if (kr == kIOReturnSuccess) {
            noteProductionMilestone(kProdPhase_BootIdentityVerified);
        }
        return kr;
    }
}

// --- GA106LabSysmemRealOps: SOMENTE operacoes (policy no template) ---
// Disciplina FIX-ASTRA-C: nenhum método bloqueia sob spinlock (o template
// chama lock()/unlock() só em secoes curtas); complete/clear retornam
// status (teardown decide); releases s\u00e3o null-safe.

void GA106LabSysmemRealOps::lock()
{
    if (fOwner && fOwner->fSysmemLock) {
        IOSimpleLockLock(fOwner->fSysmemLock);
    }
}

void GA106LabSysmemRealOps::unlock()
{
    if (fOwner && fOwner->fSysmemLock) {
        IOSimpleLockUnlock(fOwner->fSysmemLock);
    }
}

uint32_t GA106LabSysmemRealOps::getState()
{
    return fOwner ? fOwner->fSysmemState : kSysmemPhase_Empty;
}

void GA106LabSysmemRealOps::setState(uint32_t s)
{
    if (fOwner) {
        fOwner->fSysmemState = s;
    }
}

bool GA106LabSysmemRealOps::isStopping()
{
    return fOwner ? fOwner->fSysmemStopping : false;
}

bool GA106LabSysmemRealOps::isBusy()
{
    return fOwner ? fOwner->fSysmemBusy : false;
}

void GA106LabSysmemRealOps::setBusy(bool b)
{
    if (fOwner) {
        fOwner->fSysmemBusy = b;
    }
}

void GA106LabSysmemRealOps::setStopping(bool b)
{
    if (fOwner) {
        fOwner->fSysmemStopping = b;
    }
}

void GA106LabSysmemRealOps::waitMs(unsigned ms)
{
    // Fora de qualquer spinlock (disciplina do template); IODelay gira
    // sem dormir sob lock. Usado SÓ pelo poll de stop().
    IODelay(ms * 1000u);
}

bool GA106LabSysmemRealOps::allocBuffer()
{
    IOBufferMemoryDescriptor * desc;
    if (!fOwner || fOwner->fSysmemDesc) {
        return false;
    }
    desc = IOBufferMemoryDescriptor::withOptions(
        kIODirectionOut,
        (vm_size_t)kGA106LabGspSysmem_Size,
        (vm_offset_t)kGA106LabGspSysmem_Alignment);
    if (!desc) {
        return false;
    }
    if (desc->getCapacity() < (vm_size_t)kGA106LabGspSysmem_Size) {
        desc->release();
        return false;
    }
    fOwner->fSysmemDesc = desc; // owned.
    return true;
}

bool GA106LabSysmemRealOps::zeroBuffer()
{
    UInt8 * bytes;
    uint32_t i;
    if (!fOwner || !fOwner->fSysmemDesc) {
        return false;
    }
    bytes = (UInt8 *)fOwner->fSysmemDesc->getBytesNoCopy();
    if (!bytes) {
        return false;
    }
    for (i = 0; i < kGA106LabGspSysmem_Size; i++) {
        bytes[i] = 0; // exatamente 4096; pointer nunca logado/exposto.
    }
    return true;
}

bool GA106LabSysmemRealOps::createCommand()
{
    IODMACommand * cmd;
    if (!fOwner || fOwner->fSysmemCmd || !fOwner->fSysmemDesc) {
        return false;
    }
    cmd = IODMACommand::withSpecification(
        kIODMACommandOutputHost64,
        64,
        (UInt64)kGA106LabGspSysmem_Size,
        IODMACommand::kMapped,
        (UInt64)kGA106LabGspSysmem_Size,
        (UInt32)kGA106LabGspSysmem_Alignment,
        NULL,
        NULL);
    if (!cmd) {
        return false;
    }
    fOwner->fSysmemCmd = cmd; // owned.
    return true;
}

bool GA106LabSysmemRealOps::bindDescriptor()
{
    IOReturn kr;
    if (!fOwner || !fOwner->fSysmemDesc || !fOwner->fSysmemCmd) {
        return false;
    }
    // autoPrepare=false OBRIGATÓRIO: prepare explícito vem depois.
    kr = fOwner->fSysmemCmd->setMemoryDescriptor(fOwner->fSysmemDesc,
                                                 false);
    return (kr == kIOReturnSuccess) ? true : false;
}

bool GA106LabSysmemRealOps::prepareDma()
{
    IOReturn kr;
    if (!fOwner || !fOwner->fSysmemCmd) {
        return false;
    }
    // Prepare explícito ÚNICO (sem duplicar via autoPrepare).
    kr = fOwner->fSysmemCmd->prepare((UInt64)0,
                                     (UInt64)kGA106LabGspSysmem_Size,
                                     true, true);
    return (kr == kIOReturnSuccess) ? true : false;
}

bool GA106LabSysmemRealOps::genSegments(uint32_t &countOut,
                                        uint64_t &addrOut,
                                        uint64_t &lenOut)
{
    UInt64 off;
    UInt32 n;
    IODMACommand::Segment64 segs[1];
    IOReturn kr;
    countOut = 0;
    addrOut = 0;
    lenOut = 0;
    if (!fOwner || !fOwner->fSysmemCmd) {
        return false;
    }
    off = 0;
    n = 1;
    kr = fOwner->fSysmemCmd->gen64IOVMSegments(&off, segs, &n);
    if (kr != kIOReturnSuccess) {
        return false;
    }
    countOut = n;
    if (n == 1) {
        addrOut = (uint64_t)segs[0].fIOVMAddr;
        lenOut = (uint64_t)segs[0].fLength;
    }
    return true;
}

void GA106LabSysmemRealOps::markAddressReady(uint64_t addr, uint32_t len)
{
    if (!fOwner) {
        return;
    }
    fOwner->fSysmemDmaAddr = addr; // kernel-only; nunca na ABI.
    fOwner->fSysmemSegLen = len;
}

void GA106LabSysmemRealOps::clearAddressRecord()
{
    if (!fOwner) {
        return;
    }
    fOwner->fSysmemDmaAddr = 0;
    fOwner->fSysmemSegLen = 0;
}

IOReturn GA106LabSysmemRealOps::completeDma()
{
    if (!fOwner || !fOwner->fSysmemCmd) {
        return kIOReturnNotReady;
    }
    return fOwner->fSysmemCmd->complete(true, true);
}

IOReturn GA106LabSysmemRealOps::clearDma()
{
    if (!fOwner || !fOwner->fSysmemCmd) {
        return kIOReturnNotReady;
    }
    // autoComplete=false OBRIGATÓRIO: complete explícito já ocorreu
    // (ou nunca houve prepare — clear aqui só desbinda).
    return fOwner->fSysmemCmd->clearMemoryDescriptor(false);
}

void GA106LabSysmemRealOps::releaseCommand()
{
    if (!fOwner || !fOwner->fSysmemCmd) {
        return;
    }
    fOwner->fSysmemCmd->release();
    fOwner->fSysmemCmd = NULL;
}

void GA106LabSysmemRealOps::releaseDescriptor()
{
    if (!fOwner || !fOwner->fSysmemDesc) {
        return;
    }
    fOwner->fSysmemDesc->release();
    fOwner->fSysmemDesc = NULL;
}

// --- GA106LabOwnerRealOps: SOMENTE operações brutas (policy no shared flow) ---
// Disciplina TEST-PATH-FIX: nenhum método contém branching de policy;
// guards/ordem/rollback/CLEANUP_FAILED vivem em InitOwnerLocksFlow /
// ReleaseOwnerLocksFlow / FreeOwnerResourcesFlow (mesmos templates dos testes).

void GA106LabOwnerRealOps::initNullLocks()
{
    if (!fOwner) {
        return;
    }
    fOwner->fSysmemLock = NULL;
    fOwner->fSlotLock = NULL;
}

bool GA106LabOwnerRealOps::allocFirstLockRaw()
{
    if (!fOwner) {
        return false;
    }
    fOwner->fSysmemLock = IOSimpleLockAlloc();
    return (fOwner->fSysmemLock != NULL) ? true : false;
}

bool GA106LabOwnerRealOps::allocSecondLockRaw()
{
    if (!fOwner) {
        return false;
    }
    fOwner->fSlotLock = IOSimpleLockAlloc();
    return (fOwner->fSlotLock != NULL) ? true : false;
}

void *GA106LabOwnerRealOps::getFirstLock()
{
    return fOwner ? (void *)fOwner->fSysmemLock : (void *)NULL;
}

void *GA106LabOwnerRealOps::getSecondLock()
{
    return fOwner ? (void *)fOwner->fSlotLock : (void *)NULL;
}

void GA106LabOwnerRealOps::freeLockRaw(void *lock)
{
    // Operação bruta: assume non-null (guard vive no shared flow).
    // Mutação PROD_NULL_LOCK_FREE remove o guard e chega aqui com NULL.
    IOSimpleLock *l = (IOSimpleLock *)lock;
    IOSimpleLockFree(l);
}

void GA106LabOwnerRealOps::clearFirstLock()
{
    if (fOwner) {
        fOwner->fSysmemLock = NULL;
    }
}

void GA106LabOwnerRealOps::clearSecondLock()
{
    if (fOwner) {
        fOwner->fSlotLock = NULL;
    }
}

uint32_t GA106LabOwnerRealOps::getState()
{
    return fOwner ? fOwner->fSysmemState : kSysmemPhase_Empty;
}

bool GA106LabOwnerRealOps::hasCommand()
{
    return (fOwner && fOwner->fSysmemCmd) ? true : false;
}

bool GA106LabOwnerRealOps::hasDescriptor()
{
    return (fOwner && fOwner->fSysmemDesc) ? true : false;
}

void GA106LabOwnerRealOps::releaseCommandRaw()
{
    if (!fOwner || !fOwner->fSysmemCmd) {
        return;
    }
    fOwner->fSysmemCmd->release();
    fOwner->fSysmemCmd = NULL;
}

void GA106LabOwnerRealOps::releaseDescriptorRaw()
{
    if (!fOwner || !fOwner->fSysmemDesc) {
        return;
    }
    fOwner->fSysmemDesc->release();
    fOwner->fSysmemDesc = NULL;
}

void GA106LabOwnerRealOps::logCleanupFailedLeakSafe()
{
    IOLog("[GA106Lab] SYSMEM_CLEANUP_FAILED_IN_FREE_LEAK_SAFE\n");
}

void GA106LabOwnerRealOps::logUnexpectedStateInFree()
{
    IOLog("[GA106Lab] SYSMEM_UNEXPECTED_STATE_IN_FREE\n");
}

// TG-KEXT5 GSP-DMA1 — wrapper FINO (ONE_SHARED_SYSMEM_FLOW).
IOReturn GA106Lab::prepareGspSysmem(GA106LabGspSysmemV1 * out)
{
    if (!out) {
        return kIOReturnBadArgument;
    }
    {
        GA106LabSysmemRealOps ops(this);
        IOReturn kr = RunPrepareGspSysmemFlow(ops, *out);
        if (kr == kIOReturnSuccess) {
            noteProductionMilestone(kProdPhase_SysmemPrepared);
        }
        return kr;
    }
}

// --- GA106LabGfwRealOps: SOMENTE operacoes (policy no template) ---
// Disciplina KEXT6: nenhum método bloqueia sob spinlock (o template chama
// lock()/unlock() só em seções curtas); waitMs roda destravado; MMIO via
// membro fMmio (mesma descoberta/mapa RO+Inhibit dos selectors 5/6).
// Sem selector 7, sem DMA, sem firmware, sem stores.

void GA106LabGfwRealOps::lock()
{
    if (fOwner && fOwner->fSysmemLock) {
        IOSimpleLockLock(fOwner->fSysmemLock);
    }
}

void GA106LabGfwRealOps::unlock()
{
    if (fOwner && fOwner->fSysmemLock) {
        IOSimpleLockUnlock(fOwner->fSysmemLock);
    }
}

bool GA106LabGfwRealOps::isStopping()
{
    return fOwner ? fOwner->fGfwStopping : false;
}

void GA106LabGfwRealOps::setStopping(bool b)
{
    if (fOwner) {
        fOwner->fGfwStopping = b;
    }
}

bool GA106LabGfwRealOps::isBusy()
{
    return fOwner ? fOwner->fGfwBusy : false;
}

void GA106LabGfwRealOps::setBusy(bool b)
{
    if (fOwner) {
        fOwner->fGfwBusy = b;
    }
}

void GA106LabGfwRealOps::waitMs(unsigned ms)
{
    // Fora de qualquer spinlock (disciplina do template); IODelay gira
    // sem dormir sob lock. Só usado pelo stop-flow (poll usa waitBudgetedNs).
    IODelay(ms * 1000u);
}

uint64_t GA106LabGfwRealOps::nowNs()
{
    // Relógio monotônico real (SDK: mach/mach_time.h + kern/clock.h).
    // mach_absolute_time nunca é ajustável (não é wall clock); conversão
    // absoluta documentada, sem timebase manual (sem risco de shift errado).
    uint64_t abstime = mach_absolute_time();
    uint64_t ns = 0;
    absolutetime_to_nanoseconds(abstime, &ns);
    return ns;
}

void GA106LabGfwRealOps::waitBudgetedNs(uint64_t ns)
{
    // Espera ATÉ ns microssegundos-arredondados, fora de qualquer lock.
    // ns==0 => retorno imediato (sem IODelay(0)).
    unsigned us;
    if (ns == 0u) {
        return;
    }
    us = (unsigned)(ns / 1000u);
    if (us == 0u) {
        us = 1u;
    }
    if (us > 1000u) {
        us = 1000u;
    }
    IODelay(us);
}

bool GA106LabGfwRealOps::acquireProvider()
{
    // Pin de lifetime (ASTRA-B-FIX): retém o provider entre acquire e
    // release. Chamado SOMENTE sob reserva fGfwBusy já detida, logo stop()
    // não pode estar liberando o provider concorrentemente (ele espera
    // !busy). Balanceado 1:1 com releaseProvider; sem leak/double-release.
    // Base SDK: getProvider() estável com cliente attached; retain() é
    // refcount atômico (OSObject) — seguro fora de trava.
    IOPCIDevice *pci;
    if (!fOwner) {
        return false;
    }
    pci = OSDynamicCast(IOPCIDevice, fOwner->getProvider());
    if (!pci) {
        return false;
    }
    pci->retain();
    fRetPci = pci;
    fMmio = GA106LabRealOps(pci);
    return true;
}

void GA106LabGfwRealOps::releaseProvider()
{
    if (fRetPci) {
        fRetPci->release();
        fRetPci = NULL;
    }
    fMmio = GA106LabRealOps(NULL);
}

uint16_t GA106LabGfwRealOps::readVendorId()
{
    return fMmio.readVendorId();
}

uint16_t GA106LabGfwRealOps::readDeviceId()
{
    return fMmio.readDeviceId();
}

uint16_t GA106LabGfwRealOps::readCommandBefore()
{
    return fMmio.readCommandBefore();
}

uint32_t GA106LabGfwRealOps::readBar0Raw()
{
    return fMmio.readBar0Raw();
}

uint32_t GA106LabGfwRealOps::getResourceCount()
{
    return fMmio.getResourceCount();
}

bool GA106LabGfwRealOps::getResource(uint32_t idx, uint64_t &base,
                                     uint64_t &len)
{
    return fMmio.getResource(idx, base, len);
}

bool GA106LabGfwRealOps::createMap(uint32_t idx, uint64_t &mapLenOut,
                                   uint32_t &mapOptsOut)
{
    return fMmio.createMap(idx, mapLenOut, mapOptsOut);
}

uint64_t GA106LabGfwRealOps::getVA()
{
    return fMmio.getVA();
}

uint32_t GA106LabGfwRealOps::read32(uint32_t offset)
{
    return fMmio.read32(offset);
}

void GA106LabGfwRealOps::releaseMap()
{
    fMmio.releaseMap();
}

// TG-KEXT6 GSP-GFW-BOOT-READINESS — wrapper FINO (ONE_SHARED_GFW_READINESS_FLOW).
// Reserva + acquire + retain vivem no shared flow (via GfwRealOps); aqui só
// validação de argumento e delegação. Nenhum acesso a provider antes da reserva.
IOReturn GA106Lab::readGfwBootReadiness(GA106LabGfwReadinessV1 * out)
{
    if (!out) {
        return kIOReturnBadArgument;
    }
    {
        GA106LabGfwRealOps ops(this);
        IOReturn kr = RunGfwBootReadinessFlow(ops, *out);
        if (kr == kIOReturnSuccess) {
            noteProductionMilestone(kProdPhase_GfwReady);
        }
        return kr;
    }
}

// --- GA106LabFlushRealOps: pagina DEDICADA Gate A (estado fFlush*) ---
void GA106LabFlushRealOps::lock()
{
    if (fOwner && fOwner->fSysmemLock) {
        IOSimpleLockLock(fOwner->fSysmemLock);
    }
}
void GA106LabFlushRealOps::unlock()
{
    if (fOwner && fOwner->fSysmemLock) {
        IOSimpleLockUnlock(fOwner->fSysmemLock);
    }
}
uint32_t GA106LabFlushRealOps::getState()
{
    // Passthrough no domínio flush (0/1/2/20). O template sysmem NÃO é mais
    // usado para a página Gate A (single-owner em RunVerifyFlushPrewriteFlow).
    return fOwner ? fOwner->fFlushState : kGA106LabFlushPrewritePhase_Unprepared;
}
void GA106LabFlushRealOps::setState(uint32_t s)
{
    if (fOwner) {
        fOwner->fFlushState = s;
    }
}
bool GA106LabFlushRealOps::hasPageObjects()
{
    return (fOwner && (fOwner->fFlushDesc || fOwner->fFlushCmd)) ? true : false;
}
bool GA106LabFlushRealOps::getStoredPage(uint64_t &addrOut, uint32_t &lenOut)
{
    if (!fOwner) {
        return false;
    }
    addrOut = fOwner->fFlushDmaAddr;
    lenOut = fOwner->fFlushSegLen;
    return true;
}
bool GA106LabFlushRealOps::isStopping()
{
    return fOwner ? fOwner->fFlushStopping : false;
}
bool GA106LabFlushRealOps::isBusy()
{
    return fOwner ? fOwner->fFlushBusy : false;
}
void GA106LabFlushRealOps::setBusy(bool b)
{
    if (fOwner) fOwner->fFlushBusy = b;
}
void GA106LabFlushRealOps::setStopping(bool b)
{
    if (fOwner) fOwner->fFlushStopping = b;
}
void GA106LabFlushRealOps::waitMs(unsigned ms)
{
    IODelay(ms * 1000u);
}
bool GA106LabFlushRealOps::allocBuffer()
{
    IOBufferMemoryDescriptor *desc;
    if (!fOwner || fOwner->fFlushDesc) return false;
    desc = IOBufferMemoryDescriptor::withOptions(kIODirectionOut,
        (vm_size_t)kGA106LabFlushPrewrite_Size, (vm_offset_t)kGA106LabFlushPrewrite_Alignment);
    if (!desc) return false;
    if (desc->getCapacity() < (vm_size_t)kGA106LabFlushPrewrite_Size) {
        desc->release();
        return false;
    }
    fOwner->fFlushDesc = desc;
    return true;
}
bool GA106LabFlushRealOps::zeroBuffer()
{
    UInt8 *bytes;
    uint32_t i;
    if (!fOwner || !fOwner->fFlushDesc) return false;
    bytes = (UInt8 *)fOwner->fFlushDesc->getBytesNoCopy();
    if (!bytes) return false;
    for (i = 0; i < kGA106LabFlushPrewrite_Size; i++) bytes[i] = 0;
    return true;
}
bool GA106LabFlushRealOps::createCommand()
{
    IODMACommand *cmd;
    if (!fOwner || fOwner->fFlushCmd || !fOwner->fFlushDesc) return false;
    cmd = IODMACommand::withSpecification(kIODMACommandOutputHost64, 64,
        (UInt64)kGA106LabFlushPrewrite_Size, IODMACommand::kMapped,
        (UInt64)kGA106LabFlushPrewrite_Size, (UInt32)kGA106LabFlushPrewrite_Alignment, NULL, NULL);
    if (!cmd) return false;
    fOwner->fFlushCmd = cmd;
    return true;
}
bool GA106LabFlushRealOps::bindDescriptor()
{
    IOReturn kr;
    if (!fOwner || !fOwner->fFlushDesc || !fOwner->fFlushCmd) return false;
    kr = fOwner->fFlushCmd->setMemoryDescriptor(fOwner->fFlushDesc, false);
    return (kr == kIOReturnSuccess) ? true : false;
}
bool GA106LabFlushRealOps::prepareDma()
{
    IOReturn kr;
    if (!fOwner || !fOwner->fFlushCmd) return false;
    kr = fOwner->fFlushCmd->prepare((UInt64)0, (UInt64)kGA106LabFlushPrewrite_Size, true, true);
    return (kr == kIOReturnSuccess) ? true : false;
}
bool GA106LabFlushRealOps::genSegments(uint32_t &countOut, uint64_t &addrOut, uint64_t &lenOut)
{
    UInt64 off;
    UInt32 n;
    IODMACommand::Segment64 segs[1];
    IOReturn kr;
    countOut = 0; addrOut = 0; lenOut = 0;
    if (!fOwner || !fOwner->fFlushCmd) return false;
    off = 0; n = 1;
    kr = fOwner->fFlushCmd->gen64IOVMSegments(&off, segs, &n);
    if (kr != kIOReturnSuccess) return false;
    countOut = n;
    if (n == 1) {
        addrOut = (uint64_t)segs[0].fIOVMAddr;
        lenOut = (uint64_t)segs[0].fLength;
    }
    return true;
}
void GA106LabFlushRealOps::markAddressReady(uint64_t addr, uint32_t len)
{
    if (!fOwner) return;
    fOwner->fFlushDmaAddr = addr;
    fOwner->fFlushSegLen = len;
}
void GA106LabFlushRealOps::clearAddressRecord()
{
    if (!fOwner) return;
    fOwner->fFlushDmaAddr = 0;
    fOwner->fFlushSegLen = 0;
}
IOReturn GA106LabFlushRealOps::completeDma()
{
    if (!fOwner || !fOwner->fFlushCmd) return kIOReturnNotReady;
    return fOwner->fFlushCmd->complete(true, true);
}
IOReturn GA106LabFlushRealOps::clearDma()
{
    if (!fOwner || !fOwner->fFlushCmd) return kIOReturnNotReady;
    return fOwner->fFlushCmd->clearMemoryDescriptor(false);
}
void GA106LabFlushRealOps::releaseCommand()
{
    if (!fOwner || !fOwner->fFlushCmd) return;
    fOwner->fFlushCmd->release();
    fOwner->fFlushCmd = NULL;
}
void GA106LabFlushRealOps::releaseDescriptor()
{
    if (!fOwner || !fOwner->fFlushDesc) return;
    fOwner->fFlushDesc->release();
    fOwner->fFlushDesc = NULL;
}

// --- Gate A verify (TG-KEXT7.1): wrapper FINO sobre o shared template ---
// RunVerifyFlushPrewriteFlow<Ops> é O caminho de produção (mesmo template dos
// testes host). Reserva ÚNICA dentro do template (single-owner, fix P74
// Blocker A). Contrato transporte-vs-semantica documentado no header.
// Zero stores por construção (só leituras + DMA-KPI de preparo, sem Gate B).
struct VerifyFlushRealOps {
    GA106Lab *o;
    GA106LabFlushRealOps page;
    explicit VerifyFlushRealOps(GA106Lab *owner) : o(owner), page(owner) {}
    void lock() { page.lock(); }
    void unlock() { page.unlock(); }
    bool isStopping() { return page.isStopping(); }
    bool isBusy() { return page.isBusy(); }
    void setBusy(bool b) { page.setBusy(b); }
    uint32_t getState() { return page.getState(); }
    void setState(uint32_t s) { page.setState(s); }
    bool hasPageObjects() { return page.hasPageObjects(); }
    bool getStoredPage(uint64_t &a, uint32_t &l) { return page.getStoredPage(a, l); }
    IOReturn checkTarget(bool &prov, bool &exact, bool &mse, bool &bme)
    {
        GA106LabPciSnapshotV1 pci;
        IOReturn kr;
        unsigned k = 0;
        uint8_t *b = (uint8_t *)&pci;
        prov = false; exact = false; mse = false; bme = false;
        for (k = 0; k < sizeof(pci); k++) b[k] = 0;
        if (!o) return kIOReturnNotReady;
        kr = o->copyPciSnapshot(&pci);
        if (kr != kIOReturnSuccess) return kr;
        prov = (pci.validMask & kGA106LabPciValid_Identity) ? true : false;
        exact = (pci.vendorId == 0x10de && pci.deviceId == 0x2504) ? true : false;
        mse = ((pci.command & 0x0002u) != 0) ? true : false;
        bme = ((pci.command & 0x0004u) != 0) ? true : false;
        return kIOReturnSuccess;
    }
    IOReturn checkBar0(bool &ready)
    {
        GA106LabBarMapInfoV1 bar;
        IOReturn kr;
        unsigned k = 0;
        uint8_t *b = (uint8_t *)&bar;
        ready = false;
        for (k = 0; k < sizeof(bar); k++) b[k] = 0;
        if (!o) return kIOReturnNotReady;
        kr = o->probeBar0Map(&bar);
        if (kr != kIOReturnSuccess) return kr;
        ready = (bar.status == kGA106LabBarMapStatus_MapReleased) ? true : false;
        return kIOReturnSuccess;
    }
    IOReturn checkGfw(bool &ready)
    {
        GA106LabGfwReadinessV1 gfw;
        IOReturn kr;
        unsigned k = 0;
        uint8_t *b = (uint8_t *)&gfw;
        ready = false;
        for (k = 0; k < sizeof(gfw); k++) b[k] = 0;
        if (!o) return kIOReturnNotReady;
        // Chamada direta ao método provider (shared flow); NUNCA via
        // UserClient/selector (P68 §7). Mapa do poll é local por chamada.
        kr = o->readGfwBootReadiness(&gfw);
        if (kr != kIOReturnSuccess) return kr;
        ready = (gfw.status == kGA106LabGfwStatus_Completed &&
                 gfw.progress == 0xffu && gfw.gateReady) ? true : false;
        return kIOReturnSuccess;
    }
    bool allocBuffer() { return page.allocBuffer(); }
    bool zeroBuffer() { return page.zeroBuffer(); }
    bool createCommand() { return page.createCommand(); }
    bool bindDescriptor() { return page.bindDescriptor(); }
    bool prepareDma() { return page.prepareDma(); }
    bool readCommandAfter(uint16_t &cmd)
    {
        GA106LabPciSnapshotV1 pci;
        IOReturn kr;
        unsigned k = 0;
        uint8_t *b = (uint8_t *)&pci;
        cmd = 0;
        for (k = 0; k < sizeof(pci); k++) b[k] = 0;
        if (!o) return false;
        kr = o->copyPciSnapshot(&pci);
        if (kr != kIOReturnSuccess) return false;
        if ((pci.validMask & kGA106LabPciValid_Identity) == 0) return false;
        cmd = pci.command;
        return true;
    }
    bool genSegments(uint32_t &n, uint64_t &a, uint64_t &l)
    { return page.genSegments(n, a, l); }
    bool validateAddressWidth(uint64_t a) { return page.validateAddressWidth(a); }
    void markAddressReady(uint64_t a, uint32_t l) { page.markAddressReady(a, l); }
    IOReturn completeDma() { return page.completeDma(); }
    IOReturn clearDma() { return page.clearDma(); }
    void releaseCommand() { page.releaseCommand(); }
    void releaseDescriptor() { page.releaseDescriptor(); }
    void clearAddressRecord() { page.clearAddressRecord(); }
};

IOReturn GA106Lab::verifySysmemFlushPreconditions(GA106LabFlushPrewriteV1 *out)
{
    if (!out) {
        return kIOReturnBadArgument;
    }
    {
        VerifyFlushRealOps ops(this);
        IOReturn kr = RunVerifyFlushPrewriteFlow(ops, *out);
        if (kr == kIOReturnSuccess) {
            noteProductionMilestone(kProdPhase_FlushPreconditionsVerified);
        }
        return kr;
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

KMOD_EXPLICIT_DECL(org_nvidia_macos_GA106Lab, "1.8.0",
                   GA106Lab_module_start, GA106Lab_module_stop);

// ---------------- P80 production architecture (offline) ----------------
// Notas de milestone: best-effort sob fSlotLock em seção curta; falha de
// avanço NÃO altera comportamento dos selectors (fail-closed só nos gates
// prepare*/blockRealOperation). Mesma disciplina das flags busy/stopping.
bool GA106Lab::noteProductionMilestone(uint32_t to) {
    bool ok = false;
    if (!fSlotLock) {
        return false;
    }
    IOSimpleLockLock(fSlotLock);
    if (ProductionPhaseCanAdvance(fProductionPhase, to) == kPhaseOk) {
        fProductionPhase = to;
        ok = true;
    }
    IOSimpleLockUnlock(fSlotLock);
    return ok;
}

void GA106Lab::resetProductionPhaseForTeardown(void) {
    if (!fSlotLock) {
        return;
    }
    IOSimpleLockLock(fSlotLock);
    if (ProductionPhaseTeardownOk(fProductionPhase)) {
        fProductionPhase = kProdPhase_Attached;
    }
    fChannelContractOk = false;
    fVmContractOk = false;
    IOSimpleLockUnlock(fSlotLock);
}

uint32_t GA106Lab::productionPhase(void) const {
    return fProductionPhase;
}

const GA106LabBlockLog * GA106Lab::blockLog(void) const {
    return &fBlockLog;
}

// Prepares de modelo: validam contratos P77/P78/P79 e ordem de fase.
// Zero HW: nenhum acesso a provider, PCI, MMIO, DMA, GSP ou firmware.
IOReturn GA106Lab::prepareChannelModel(const GA106LabChannelContract * in) {
    if (!in) {
        return kIOReturnBadArgument;
    }
    if (!GA106LabChannelContractValid(in)) {
        return kIOReturnBadArgument;
    }
    if (!noteProductionMilestone(kProdPhase_ChannelPrepared)) {
        return kIOReturnNotReady;
    }
    fChannelContract = *in;
    fChannelContractOk = true;
    return kIOReturnSuccess;
}

IOReturn GA106Lab::prepareVmModel(const GA106LabVmContract * in) {
    if (!in) {
        return kIOReturnBadArgument;
    }
    if (!GA106LabVmContractValid(in)) {
        return kIOReturnBadArgument;
    }
    if (!noteProductionMilestone(kProdPhase_VmPrepared)) {
        return kIOReturnNotReady;
    }
    fVmContract = *in;
    fVmContractOk = true;
    return kIOReturnSuccess;
}

IOReturn GA106Lab::prepareSubmissionModel(const GA106LabSubmissionContract * in,
                                          uint64_t pbBase, uint64_t pbLength) {
    if (!in) {
        return kIOReturnBadArgument;
    }
    if (!fChannelContractOk || !fVmContractOk) {
        return kIOReturnNotReady;
    }
    if (!GA106LabCrossContractValid(&fChannelContract, &fVmContract, in,
                                    pbBase, pbLength)) {
        return kIOReturnBadArgument;
    }
    if (!noteProductionMilestone(kProdPhase_SubmissionPrepared)) {
        return kIOReturnNotReady;
    }
    return kIOReturnSuccess;
}

// Teto live: qualquer operação real é fail-closed + latch LiveBlocked.
// Terminal até o próximo start() (re-init). Retorno SEMPRE NotPermitted.
IOReturn GA106Lab::blockRealOperation(uint32_t op) {
    IOReturn kr = kIOReturnNotPermitted;
    switch (op) {
        case 0:  kr = GA106LabBlockMmioWrite(&fBlockLog); break;
        case 1:  kr = GA106LabBlockPciConfigWrite(&fBlockLog); break;
        case 2:  kr = GA106LabBlockBmeEnable(&fBlockLog); break;
        case 3:  kr = GA106LabBlockRealDma(&fBlockLog); break;
        case 4:  kr = GA106LabBlockGspLive(&fBlockLog); break;
        case 5:  kr = GA106LabBlockFirmwareUpload(&fBlockLog); break;
        case 6:  kr = GA106LabBlockVramWrite(&fBlockLog); break;
        case 7:  kr = GA106LabBlockInterruptEnable(&fBlockLog); break;
        case 8:  kr = GA106LabBlockReset(&fBlockLog); break;
        case 9:  kr = GA106LabBlockRealGpfifoKick(&fBlockLog); break;
        case 10: kr = GA106LabBlockRealPutWrite(&fBlockLog); break;
        case 11: kr = GA106LabBlockRealDoorbell(&fBlockLog); break;
        case 12: kr = GA106LabBlockRealCommandSubmission(&fBlockLog); break;
        default: kr = kIOReturnBadArgument; break;
    }
    if (kr == kIOReturnNotPermitted) {
        noteProductionMilestone(kProdPhase_LiveBlocked);
    }
    return kr;
}
