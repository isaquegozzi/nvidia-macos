//
// GA106LabUserClient.cpp — user client read-only KEXT1.
//
// Lê SOMENTE o cache imutável do GA106Lab via tabela dispatch estática.
// Sem IOPCIDevice*, sem map/config/DMA/interrupts, sem shm. Entrada zero;
// saídas zeradas por atribuição explícita, tamanho fixo, cópia padrão.
//

#include "GA106LabUserClient.hpp"
#include "GA106Lab.hpp"
#include "GA106LabUserClientOwnerFlow.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(GA106LabUserClient, IOUserClient);

// --- GA106LabUserClientOwnerRealOps: SOMENTE operações (policy no template) ---
// Seções críticas curtas (flag/ponteiro/refcount); nunca provider/MMIO/wait
// sob fOwnerLock; nunca aninhada com fSysmemLock (ordem global documentada
// no header do flow). retain/release = refcount atômico, seguro sob spinlock.

void GA106LabUserClientOwnerRealOps::lock()
{
    if (fSelf && fSelf->fOwnerLock) {
        IOSimpleLockLock(fSelf->fOwnerLock);
    }
}

void GA106LabUserClientOwnerRealOps::unlock()
{
    if (fSelf && fSelf->fOwnerLock) {
        IOSimpleLockUnlock(fSelf->fOwnerLock);
    }
}

bool GA106LabUserClientOwnerRealOps::isClosing()
{
    return fSelf ? fSelf->fClosing : true;
}

bool GA106LabUserClientOwnerRealOps::isOpen()
{
    return fSelf ? fSelf->fOpen : false;
}

void *GA106LabUserClientOwnerRealOps::getOwnerRaw()
{
    return fSelf ? (void *)fSelf->fOwner : NULL;
}

void GA106LabUserClientOwnerRealOps::markClosing()
{
    if (fSelf) {
        fSelf->fClosing = true;
    }
}

void GA106LabUserClientOwnerRealOps::setOpen(bool b)
{
    if (fSelf) {
        fSelf->fOpen = b;
    }
}

void GA106LabUserClientOwnerRealOps::retainOwner(void *o)
{
    if (o) {
        ((GA106Lab *)o)->retain();
    }
}

void GA106LabUserClientOwnerRealOps::releaseOwner(void *o)
{
    if (o) {
        ((GA106Lab *)o)->release();
    }
}

int GA106LabUserClientOwnerRealOps::activeCount()
{
    return fSelf ? fSelf->fActiveCalls : 0;
}

void GA106LabUserClientOwnerRealOps::setActiveCount(int n)
{
    if (fSelf) {
        fSelf->fActiveCalls = n;
    }
}

void GA106LabUserClientOwnerRealOps::waitShort()
{
    // Fora de qualquer lock (disciplina do template); quantum do drain.
    IODelay(1000u);
}

void GA106LabUserClientOwnerRealOps::onPinEntry()
{
    // no-op na produção.
}

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

static IOReturn GA106LabUCBarMapProbe(OSObject * target, void *,
                                       IOExternalMethodArguments * args)
{
    GA106LabUserClient * self =
        OSDynamicCast(GA106LabUserClient, target);
    GA106LabBarMapInfoV1 info;
    GA106Lab * owner;
    unsigned int i;
    if (!self || !args) {
        return kIOReturnBadArgument;
    }
    // Zero-input: nenhum scalar/structure input permitido.
    if (args->scalarInputCount != 0 || args->structureInputSize != 0) {
        return kIOReturnBadArgument;
    }
    if (!args->structureOutput ||
        args->structureOutputSize < sizeof(info)) {
        return kIOReturnBadArgument;
    }
    owner = self->getOwnerService();
    if (!owner) {
        return kIOReturnNotOpen;
    }
    // Zero explícito (o owner também zera; defesa em profundidade).
    {
        uint8_t * b = (uint8_t *) &info;
        for (i = 0; i < sizeof(info); i++) {
            b[i] = 0;
        }
    }
    // Owner executa discovery→map→validate→release; erro propaga direto.
    {
        IOReturn kr = owner->probeBar0Map(&info);
        if (kr != kIOReturnSuccess) {
            return kr;
        }
    }
    *((GA106LabBarMapInfoV1 *) args->structureOutput) = info;
    args->structureOutputSize = (uint32_t) sizeof(info);
    return kIOReturnSuccess;
}

static IOReturn GA106LabUCReadFirstMmio(OSObject * target, void *,
                                        IOExternalMethodArguments * args)
{
    GA106LabUserClient * self =
        OSDynamicCast(GA106LabUserClient, target);
    GA106LabMmioReadV1 info;
    GA106Lab * owner;
    unsigned int i;
    if (!self || !args) {
        return kIOReturnBadArgument;
    }
    // Zero-input: nenhum scalar/structure input permitido (offset fixo).
    if (args->scalarInputCount != 0 || args->structureInputSize != 0) {
        return kIOReturnBadArgument;
    }
    if (!args->structureOutput ||
        args->structureOutputSize < sizeof(info)) {
        return kIOReturnBadArgument;
    }
    owner = self->getOwnerService();
    if (!owner) {
        return kIOReturnNotOpen;
    }
    {
        uint8_t * b = (uint8_t *) &info;
        for (i = 0; i < sizeof(info); i++) {
            b[i] = 0;
        }
    }
    // Owner executa map→1 leitura→valida→unmap; erro propaga direto.
    {
        IOReturn kr = owner->readPmcBoot0(&info);
        if (kr != kIOReturnSuccess) {
            return kr;
        }
    }
    *((GA106LabMmioReadV1 *) args->structureOutput) = info;
    args->structureOutputSize = (uint32_t) sizeof(info);
    return kIOReturnSuccess;
}

static IOReturn GA106LabUCReadStaticIdentity(OSObject * target, void *,
                                             IOExternalMethodArguments * args)
{
    GA106LabUserClient * self =
        OSDynamicCast(GA106LabUserClient, target);
    GA106LabStaticIdentityV1 info;
    GA106Lab * owner;
    unsigned int i;
    if (!self || !args) {
        return kIOReturnBadArgument;
    }
    // Zero-input: nenhum scalar/structure input permitido (allowlist fixa).
    if (args->scalarInputCount != 0 || args->structureInputSize != 0) {
        return kIOReturnBadArgument;
    }
    if (!args->structureOutput ||
        args->structureOutputSize < sizeof(info)) {
        return kIOReturnBadArgument;
    }
    owner = self->getOwnerService();
    if (!owner) {
        return kIOReturnNotOpen;
    }
    {
        uint8_t * b = (uint8_t *) &info;
        for (i = 0; i < sizeof(info); i++) {
            b[i] = 0;
        }
    }
    // Owner executa map→2 leituras fixas→valida→unmap; erro propaga direto.
    {
        IOReturn kr = owner->readStaticIdentity(&info);
        if (kr != kIOReturnSuccess) {
            return kr;
        }
    }
    *((GA106LabStaticIdentityV1 *) args->structureOutput) = info;
    args->structureOutputSize = (uint32_t) sizeof(info);
    return kIOReturnSuccess;
}

static IOReturn GA106LabUCPrepareGspSysmem(OSObject * target, void *,
                                           IOExternalMethodArguments * args)
{
    GA106LabUserClient * self =
        OSDynamicCast(GA106LabUserClient, target);
    GA106LabGspSysmemV1 info;
    GA106Lab * owner;
    unsigned int i;
    if (!self || !args) {
        return kIOReturnBadArgument;
    }
    // Zero-input: comando semântico fixo, sem size/address/flags de fora.
    if (args->scalarInputCount != 0 || args->structureInputSize != 0) {
        return kIOReturnBadArgument;
    }
    if (!args->structureOutput ||
        args->structureOutputSize < sizeof(info)) {
        return kIOReturnBadArgument;
    }
    owner = self->getOwnerService();
    if (!owner) {
        return kIOReturnNotOpen;
    }
    {
        uint8_t * b = (uint8_t *) &info;
        for (i = 0; i < sizeof(info); i++) {
            b[i] = 0;
        }
    }
    // Owner executa alloc→mapping (sem HW); erro propaga direto.
    // Nenhum objeto DMA guardado aqui: tudo no owner.
    {
        IOReturn kr = owner->prepareGspSysmem(&info);
        if (kr != kIOReturnSuccess) {
            return kr;
        }
    }
    *((GA106LabGspSysmemV1 *) args->structureOutput) = info;
    args->structureOutputSize = (uint32_t) sizeof(info);
    return kIOReturnSuccess;
}

static IOReturn GA106LabUCReadGfwReadiness(OSObject * target, void *,
                                           IOExternalMethodArguments * args)
{
    GA106LabUserClient * self =
        OSDynamicCast(GA106LabUserClient, target);
    GA106LabGfwReadinessV1 info;
    unsigned int i;
    if (!self || !args) {
        return kIOReturnBadArgument;
    }
    // Zero-input: comando semântico fixo (gate + progresso fixos, polling
    // bounded interno). Sem offset/mask/timeout de fora.
    if (args->scalarInputCount != 0 || args->structureInputSize != 0) {
        return kIOReturnBadArgument;
    }
    if (!args->structureOutput ||
        args->structureOutputSize < sizeof(info)) {
        return kIOReturnBadArgument;
    }
    // Pin de lifetime ATÔMICO (PROMPT 57 §3): load + retain sob a mesma
    // trava que stop()/close() usam para invalidar. Sem janela
    // read-ptr -> race -> retain. Sem ciclo (owner nunca retém o client).
    // Balanceamento 1:1 — soltura do pin em todos os exits abaixo.
    void *pinned = NULL;
    GA106Lab * owner = NULL;
    {
        GA106LabUserClientOwnerRealOps pinOps(self);
        if (!PinOwnerForExternalMethod(pinOps, pinned)) {
            return kIOReturnNotOpen;
        }
        owner = (GA106Lab *)pinned;
    }
    {
        uint8_t * b = (uint8_t *) &info;
        for (i = 0; i < sizeof(info); i++) {
            b[i] = 0;
        }
    }
    // Owner executa gate→poll→fill em mapa local (só leituras); erro propaga.
    // Nenhum objeto guardado aqui: mapa é local por chamada, liberado antes.
    // O pin (retain próprio) mantém o owner válido por toda a chamada,
    // mesmo com stop()/terminate concorrente.
    {
        IOReturn kr = owner->readGfwBootReadiness(&info);
        if (kr != kIOReturnSuccess) {
            GA106LabUserClientOwnerRealOps relOps(self);
            ReleasePinnedOwner(relOps, pinned);
            return kr;
        }
    }
    *((GA106LabGfwReadinessV1 *) args->structureOutput) = info;
    args->structureOutputSize = (uint32_t) sizeof(info);
    {
        GA106LabUserClientOwnerRealOps relOps(self);
        ReleasePinnedOwner(relOps, pinned);
    }
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
    /* 4 BAR_MAP_PROBE        */ { (IOExternalMethodAction) GA106LabUCBarMapProbe,
        0, 0, 0, (uint32_t) sizeof(GA106LabBarMapInfoV1) },
    /* 5 READ_FIRST_MMIO      */ { (IOExternalMethodAction) GA106LabUCReadFirstMmio,
        0, 0, 0, (uint32_t) sizeof(GA106LabMmioReadV1) },
    /* 6 READ_STATIC_IDENTITY */ { (IOExternalMethodAction) GA106LabUCReadStaticIdentity,
        0, 0, 0, (uint32_t) sizeof(GA106LabStaticIdentityV1) },
    /* 7 PREPARE_GSP_SYSMEM   */ { (IOExternalMethodAction) GA106LabUCPrepareGspSysmem,
        0, 0, 0, (uint32_t) sizeof(GA106LabGspSysmemV1) },
    /* 8 READ_GFW_READINESS  */ { (IOExternalMethodAction) GA106LabUCReadGfwReadiness,
        0, 0, 0, (uint32_t) sizeof(GA106LabGfwReadinessV1) },
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
    fClosing = false;
    fActiveCalls = 0;
    fOwnerLock = IOSimpleLockAlloc();
    if (!fOwnerLock) {
        return false;
    }
    return true;
}

void GA106LabUserClient::free(void)
{
    // stop() já drenou active==0; a trap de externalMethod retém o próprio
    // UserClient, logo nenhum pin pode estar em progresso aqui. Assert
    // documenta; soltura defensiva null-safe (semântica leak-do-XNU nunca
    // necessária neste caminho, mas sem risco).
    if (fActiveCalls != 0) {
        IOLog("[GA106LabUserClient] ACTIVE_CALLS_NONZERO_IN_FREE\n");
    }
    if (fOwnerLock) {
        IOSimpleLockFree(fOwnerLock);
        fOwnerLock = NULL;
    }
    super::free();
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
    // Publica fOwner sob proteção (contrato §4): a partir daqui pins
    // concorrentes com stop()/close() são ordenados pela trava.
    if (fOwnerLock) {
        IOSimpleLockLock(fOwnerLock);
    }
    fOwner = owner;
    fOpen = true;
    fClosing = false;
    fActiveCalls = 0;
    if (fOwnerLock) {
        IOSimpleLockUnlock(fOwnerLock);
    }
    return true;
}

IOReturn GA106LabUserClient::clientClose(void)
{
    // Teardown real (padrão XNU auditado): super::clientClose() retorna
    // kIOReturnUnsupported e NÃO termina. Marca closing de forma sincronizada
    // (shared flow — pins já obtidos seguem válidos pelo retain próprio).
    // Limpamos o slot e iniciamos terminate() — idempotente no framework —
    // e stop() drena + libera o owner.
    {
        GA106LabUserClientOwnerRealOps closeOps(this);
        MarkUserClientClosingFlow(closeOps);
    }
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
    // Marca closing de forma sincronizada (mesmo shared flow do clientClose).
    {
        GA106LabUserClientOwnerRealOps diedOps(this);
        MarkUserClientClosingFlow(diedOps);
    }
    return super::clientDied();
}

void GA106LabUserClient::stop(IOService * provider)
{
    // Impede novos pins e drena chamadas pinadas (shared flow) ANTES de
    // soltar a referência base do owner: stop só conclui após a política.
    // In-flight bounded (selector 8: contagem E deadline), logo o drain é
    // bounded. Sem espera sob trava; sem aninhamento com a trava do owner.
    {
        GA106LabUserClientOwnerRealOps stopOps(this);
        BeginUserClientStopAndDrainFlow(stopOps);
    }
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
    if (!arguments || !fOwnerLock) {
        return kIOReturnNotOpen;
    }
    // Gate rápido sob trava (o pin autoritativo no handler re-checa).
    {
        bool ok = false;
        IOSimpleLockLock(fOwnerLock);
        ok = (fOpen && !fClosing && fOwner != NULL) ? true : false;
        IOSimpleLockUnlock(fOwnerLock);
        if (!ok) {
            return kIOReturnNotOpen;
        }
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
