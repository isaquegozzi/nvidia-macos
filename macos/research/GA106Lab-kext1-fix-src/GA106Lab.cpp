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

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>
#include <mach/kmod.h>

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
    GA106LabPublishString(this, "GA106LabRevision", "TG-KEXT1");

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

KMOD_EXPLICIT_DECL(org_nvidia_macos_GA106Lab, "1.1.0",
                   GA106Lab_module_start, GA106Lab_module_stop);
