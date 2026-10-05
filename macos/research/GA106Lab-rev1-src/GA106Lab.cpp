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

#include <IOKit/IOLib.h>
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

bool GA106Lab::start(IOService * provider)
{
    IOLog("[GA106Lab] START_ENTER\n");
    GA106LabPublishString(this, "GA106LabState", "START_ENTER");
    GA106LabPublishString(this, "GA106LabRevision", "TG-KEXT0-REV1");

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

    IOLog("[GA106Lab] registryEntryID=%llu\n",
          (unsigned long long) pci->getRegistryEntryID());
    OSNumber * eid = OSNumber::withNumber(pci->getRegistryEntryID(), 64);
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
    return true;
}

void GA106Lab::stop(IOService * provider)
{
    IOLog("[GA106Lab] STOP\n");
    super::stop(provider);
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

KMOD_EXPLICIT_DECL(org_nvidia_macos_GA106Lab, "1.0.0",
                   GA106Lab_module_start, GA106Lab_module_stop);
