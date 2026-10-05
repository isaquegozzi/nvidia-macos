// test-production-blocks.cpp — P80 §14: stubs fail-closed determinísticos + latch.
#include <cassert>
#include <cstdio>
#include <IOKit/IOReturn.h>
#include "GA106LabHardwareBlocks.h"

static int g = 0;
#define PASS(m) do { printf("%s\n", m); g++; } while (0)

int main(void) {
    GA106LabBlockLog log;
    GA106LabBlockLogInit(&log);
    assert(GA106LabBlockLogTotal(&log) == 0);
    PASS("BLOCK_LOG_ZERO_INIT");

    assert(GA106LabBlockMmioWrite(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockPciConfigWrite(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockBmeEnable(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockRealDma(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockGspLive(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockFirmwareUpload(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockVramWrite(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockInterruptEnable(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockReset(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockRealGpfifoKick(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockRealPutWrite(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockRealDoorbell(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockRealCommandSubmission(&log) == kIOReturnNotPermitted);
    assert(GA106LabBlockLogTotal(&log) == 13);
    PASS("BLOCK_ALL_13_DETERMINISTIC_NOT_PERMITTED");

    // Repetição acumula (latch observável); NULL log ainda falha fechado.
    assert(GA106LabBlockRealDma(&log) == kIOReturnNotPermitted);
    assert(log.realDma == 2);
    assert(GA106LabBlockRealDma(NULL) == kIOReturnNotPermitted);
    assert(GA106LabBlockLogTotal(NULL) == 0);
    PASS("BLOCK_COUNTER_LATCH_AND_NULL_SAFE");

    printf("PRODUCTION_BLOCK_TESTS = PASS (%d groups)\n", g);
    return 0;
}
