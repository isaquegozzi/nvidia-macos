//
// GA106LabUserClient.hpp — user client read-only KEXT1 (sem hardware access).
//
// Lê SOMENTE o cache imutável do GA106Lab. Sem IOPCIDevice*, sem map/config/DMA.
//

#ifndef GA106LAB_USER_CLIENT_HPP
#define GA106LAB_USER_CLIENT_HPP

#include <IOKit/IOUserClient.h>

class GA106Lab;

class GA106LabUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(GA106LabUserClient);

private:
    GA106Lab * fOwner; // retido; NUNCA IOPCIDevice* aqui.
    bool       fOpen;

public:
    virtual bool initWithTask(task_t owningTask, void * securityToken,
                              UInt32 type) override;
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

#endif /* GA106LAB_USER_CLIENT_HPP */
