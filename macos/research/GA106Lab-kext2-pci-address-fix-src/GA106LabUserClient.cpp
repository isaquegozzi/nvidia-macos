//
// GA106LabUserClient.cpp — user client read-only KEXT1.
//
// Lê SOMENTE o cache imutável do GA106Lab via tabela dispatch estática.
// Sem IOPCIDevice*, sem map/config/DMA/interrupts, sem shm. Entrada zero;
// saídas zeradas por atribuição explícita, tamanho fixo, cópia padrão.
//

#include "GA106LabUserClient.hpp"
#include "GA106Lab.hpp"

#include <IOKit/IOLib.h>

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(GA106LabUserClient, IOUserClient);

// --- handlers (só cache; sem hardware) ---

static IOReturn GA106LabUCGetVersion(OSObject * target, void *,
                                     IOExternalMethodArguments * args)
{
    GA106LabUserClient * self =
        OSDynamicCast(GA106LabUserClient, target);
    if (!self || !args) {
        return kIOReturnBadArgument;
    }
    if (args->scalarOutputCount < 2) {
        return kIOReturnBadArgument;
    }
    args->scalarOutput[0] = GA106LAB_UC_PROTOCOL_VERSION;
    args->scalarOutput[1] = GA106LAB_DRIVER_VERSION_U32;
    return kIOReturnSuccess;
}

static IOReturn GA106LabUCGetIdentity(OSObject * target, void *,
                                      IOExternalMethodArguments * args)
{
    GA106LabUserClient * self =
        OSDynamicCast(GA106LabUserClient, target);
    GA106LabCachedState cached;
    GA106LabIdentityV1 * out;
    if (!self || !args) {
        return kIOReturnBadArgument;
    }
    if (!args->structureOutput ||
        args->structureOutputSize < sizeof(GA106LabIdentityV1)) {
        return kIOReturnBadArgument;
    }
    out = (GA106LabIdentityV1 *) args->structureOutput;
    // Zero explícito campo a campo (sem memset): sem padding vazado.
    out->size = 0;
    out->version = 0;
    out->vendor = 0;
    out->device = 0;
    out->providerEntryID = 0;
    out->state = kGA106LabState_Unknown;
    out->reserved = 0;
    if (!self->getOwnerService() ||
        !self->getOwnerService()->copyCachedState(&cached)) {
        return kIOReturnNotReady;
    }
    out->size = (uint32_t) sizeof(GA106LabIdentityV1);
    out->version = GA106LAB_UC_PROTOCOL_VERSION;
    out->vendor = cached.vendor;
    out->device = cached.device;
    out->providerEntryID = cached.providerEntryID;
    out->state = cached.state;
    out->reserved = 0;
    args->structureOutputSize = (uint32_t) sizeof(GA106LabIdentityV1);
    return kIOReturnSuccess;
}

static IOReturn GA106LabUCGetStatus(OSObject * target, void *,
                                    IOExternalMethodArguments * args)
{
    GA106LabUserClient * self =
        OSDynamicCast(GA106LabUserClient, target);
    GA106LabCachedState cached;
    GA106LabStatusV1 * out;
    if (!self || !args) {
        return kIOReturnBadArgument;
    }
    if (!args->structureOutput ||
        args->structureOutputSize < sizeof(GA106LabStatusV1)) {
        return kIOReturnBadArgument;
    }
    out = (GA106LabStatusV1 *) args->structureOutput;
    out->size = 0;
    out->version = 0;
    out->state = kGA106LabState_Unknown;
    out->providerConfirmed = 0;
    out->revision = 0;
    out->reserved0 = 0;
    out->reserved1 = 0;
    out->reserved2 = 0;
    if (!self->getOwnerService() ||
        !self->getOwnerService()->copyCachedState(&cached)) {
        return kIOReturnNotReady;
    }
    out->size = (uint32_t) sizeof(GA106LabStatusV1);
    out->version = GA106LAB_UC_PROTOCOL_VERSION;
    out->state = cached.state;
    out->providerConfirmed = cached.providerConfirmed;
    out->revision = GA106LAB_DRIVER_VERSION_U32;
    out->reserved0 = 0;
    out->reserved1 = 0;
    out->reserved2 = 0;
    args->structureOutputSize = (uint32_t) sizeof(GA106LabStatusV1);
    return kIOReturnSuccess;
}

static IOReturn GA106LabUCGetPciSnapshot(OSObject * target, void *,
                                         IOExternalMethodArguments * args)
{
    GA106LabUserClient * self =
        OSDynamicCast(GA106LabUserClient, target);
    GA106LabPciSnapshotV1 snap;
    GA106Lab * owner;
    if (!self || !args) {
        return kIOReturnBadArgument;
    }
    if (!args->structureOutput ||
        args->structureOutputSize < sizeof(snap)) {
        return kIOReturnBadArgument;
    }
    owner = self->getOwnerService();
    if (!owner) {
        return kIOReturnNotOpen;
    }
    // Snapshot lido na hora (config space pode mudar via firmware/PM).
    // Erro do owner propaga direto: NotReady/BadArgument/NoDevice.
    // NoDevice aqui = wrong-device (ex.: o bug 00:00.0 jamais passa).
    {
        IOReturn kr = owner->copyPciSnapshot(&snap);
        if (kr != kIOReturnSuccess) {
            return kr;
        }
    }
    *((GA106LabPciSnapshotV1 *) args->structureOutput) = snap;
    args->structureOutputSize = (uint32_t) sizeof(snap);
    return kIOReturnSuccess;
}

static const IOExternalMethodDispatch sMethods[] = {
    /* 0 GET_PROTOCOL_VERSION */ { (IOExternalMethodAction) GA106LabUCGetVersion,
        0, 0, 2, 0 },
    /* 1 GET_DEVICE_IDENTITY  */ { (IOExternalMethodAction) GA106LabUCGetIdentity,
        0, 0, 0, (uint32_t) sizeof(GA106LabIdentityV1) },
    /* 2 GET_SERVICE_STATE    */ { (IOExternalMethodAction) GA106LabUCGetStatus,
        0, 0, 0, (uint32_t) sizeof(GA106LabStatusV1) },
    /* 3 GET_PCI_SNAPSHOT     */ { (IOExternalMethodAction) GA106LabUCGetPciSnapshot,
        0, 0, 0, (uint32_t) sizeof(GA106LabPciSnapshotV1) },
};

// --- lifecycle: owner retido, sem ciclo (owner NÃO retém o client) ---

bool GA106LabUserClient::initWithTask(task_t owningTask, void * securityToken,
                                      UInt32 type)
{
    if (!super::initWithTask(owningTask, securityToken, type)) {
        return false;
    }
    fOwner = NULL;
    fOpen = false;
    return true;
}

bool GA106LabUserClient::start(IOService * provider)
{
    GA106Lab * owner = OSDynamicCast(GA106Lab, provider);
    if (!owner) {
        return false;
    }
    if (!super::start(provider)) {
        return false;
    }
    owner->retain(); // pareado com release em stop().
    fOwner = owner;
    fOpen = true;
    return true;
}

IOReturn GA106LabUserClient::clientClose(void)
{
    // Teardown real (padrão XNU auditado): super::clientClose() retorna
    // kIOReturnUnsupported e NÃO termina. Limpamos o slot e iniciamos
    // terminate() — idempotente no framework — e stop() libera o owner.
    fOpen = false;
    if (fOwner) {
        fOwner->clientClosed(this);
    }
    terminate();
    return kIOReturnSuccess;
}

IOReturn GA106LabUserClient::clientDied(void)
{
    // Converge para o mesmo teardown: o XNU protege com CAS em `closed` e
    // chama clientClose() UMA única vez; sem double termination/release.
    fOpen = false;
    return super::clientDied();
}

void GA106LabUserClient::stop(IOService * provider)
{
    fOpen = false;
    if (fOwner) {
        fOwner->clientClosed(this);
        fOwner->release();
        fOwner = NULL;
    }
    super::stop(provider);
}

GA106Lab * GA106LabUserClient::getOwnerService(void) const
{
    return fOwner;
}

IOReturn GA106LabUserClient::externalMethod(uint32_t selector,
        IOExternalMethodArguments * arguments,
        IOExternalMethodDispatch * dispatch,
        OSObject * target, void * reference)
{
    if (!arguments || !fOpen || !fOwner) {
        return kIOReturnNotOpen;
    }
    if (selector >= kGA106LabSelector_Count) {
        return kIOReturnBadArgument;
    }
    (void) dispatch;
    (void) target;
    (void) reference;
    // Tabela estática: validação de bounds/count/size pelo framework + cheques
    // explícitos em cada handler.
    return super::externalMethod(selector, arguments,
                                 (IOExternalMethodDispatch *)
                                 &sMethods[selector],
                                 this, NULL);
}
