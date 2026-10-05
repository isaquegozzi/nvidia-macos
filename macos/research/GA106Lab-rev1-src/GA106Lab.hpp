//
// GA106Lab.hpp — KEXT0 mínimo de observação GA106 (spec TG-KEXT0).
// ATTACH_ONLY: anexa ao IOPCIDevice, lê registry, nunca toca hardware.
// Sem IOUserClient, mappings, DMA, interrupts, timers, workloops, gates.
//

#ifndef GA106LAB_HPP
#define GA106LAB_HPP

#include <IOKit/IOService.h>

class GA106Lab : public IOService
{
    OSDeclareDefaultStructors(GA106Lab);

public:
    bool start(IOService * provider) override;
    void stop(IOService * provider) override;
};

#endif /* GA106LAB_HPP */
