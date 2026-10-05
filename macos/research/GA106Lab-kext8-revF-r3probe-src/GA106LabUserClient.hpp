//
// GA106LabUserClient.hpp — user client read-only KEXT1 (sem hardware access).
//
// Lê SOMENTE o cache imutável do GA106Lab. Sem IOPCIDevice*, sem map/config/DMA.
//

#ifndef GA106LAB_USER_CLIENT_HPP
#define GA106LAB_USER_CLIENT_HPP

#include <IOKit/IOUserClient.h>
#include <IOKit/IOLocks.h>

class GA106Lab;

struct GA106LabUserClientOwnerRealOps;

class GA106LabUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(GA106LabUserClient);
    friend struct GA106LabUserClientOwnerRealOps;

private:
    GA106Lab * fOwner; // retido; NUNCA IOPCIDevice* aqui.
    bool       fOpen;
    // Estado de lifetime do owner (PROMPT 57): protege fOwner/fOpen contra
    // stop()/close() concorrente com pin em externalMethod. Trava de ordem
    // mais externa: nunca retida durante provider/MMIO/wait; nunca aninhada
    // com fSysmemLock (sempre solta antes). Seções curtas (flag/ptr/refcount).
    IOSimpleLock * fOwnerLock;
    bool           fClosing;     // terminal: impede novos pins.
    int            fActiveCalls; // pins em andamento (drenado em stop()).

public:
    virtual bool initWithTask(task_t owningTask, void * securityToken,
                              UInt32 type) override;
    virtual void free(void) override;
    virtual bool start(IOService * provider) override;
    virtual IOReturn clientClose(void) override;
    virtual IOReturn clientDied(void) override;
    virtual void stop(IOService * provider) override;

    virtual IOReturn externalMethod(uint32_t selector,
                                    IOExternalMethodArguments * arguments,
                                    IOExternalMethodDispatch * dispatch,
                                    OSObject * target,
                                    void * reference) override;

    GA106Lab * getOwnerService(void) const;
};

// GA106LabUserClientOwnerRealOps — adaptador de PRODUÇÃO para o shared
// owner-lifetime flow (PROMPT 57). SOMENTE operações brutas sobre o estado
// fOwnerLock/fClosing/fOpen/fOwner/fActiveCalls; TODA policy (pin atômico,
// closing, drain) vive em GA106LabUserClientOwnerFlow.hpp.
// retain()/release() são refcount atômico único (nunca dorme), seguro sob
// IOSimpleLock. Métodos implementados em GA106LabUserClient.cpp.
struct GA106LabUserClientOwnerRealOps {
    GA106LabUserClient *fSelf;

    explicit GA106LabUserClientOwnerRealOps(GA106LabUserClient *s) : fSelf(s) {}

    void lock();
    void unlock();
    bool isClosing();
    bool isOpen();
    void *getOwnerRaw();
    void markClosing();
    void setOpen(bool b);
    void retainOwner(void *o);
    void releaseOwner(void *o);
    int activeCount();
    void setActiveCount(int n);
    void waitShort();   // IODelay ~1ms; fora de qualquer lock (drain).
    void onPinEntry();  // no-op na produção (só teste sincroniza aqui).
};

#endif /* GA106LAB_USER_CLIENT_HPP */
