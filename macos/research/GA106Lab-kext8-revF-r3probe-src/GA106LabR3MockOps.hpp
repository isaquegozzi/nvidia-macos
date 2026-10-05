// GA106LabR3MockOps.hpp — MockOps dos 3 flows R3 (host-only, sem IOKit/HW).
// Fixtures por ponto de falha + contadores SOMENTE dentro das ops.
// Nenhum store em qualquer caminho: este struct NÃO possui write32/configWrite.
#ifndef GA106LAB_R3_MOCK_OPS_HPP
#define GA106LAB_R3_MOCK_OPS_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"
#include "GA106LabR3ProbeFlow.hpp"

#include <IOKit/IOReturn.h>
#include <stdint.h>
#include <pthread.h>

#define GA106LAB_R3_MOCK_MAX_RES 8

struct GA106LabR3MockRes {
    uint64_t base;
    uint64_t len;
    bool present;
};

struct GA106LabR3MockOps {
    // Discovery fixtures (mesmo layout live 16MiB).
    bool providerIsPCI;
    uint16_t vendor;
    uint16_t device;
    uint16_t cmdBefore;
    uint16_t cmdAfter;
    uint16_t statusReg;
    uint32_t bar0Raw;
    GA106LabR3MockRes res[GA106LAB_R3_MOCK_MAX_RES];
    uint32_t nRes;
    bool mapNull;
    uint64_t mapLen;
    uint32_t mapOpts;
    bool vaZero;
    // Falcon fixtures por instância (0=SEC2, 1=GSP).
    bool instReadable[2];
    uint32_t cpuctl[2];
    uint32_t trfPre[2];
    uint32_t trfIdle2[2];
    uint32_t trfPost[2];
    uint32_t bcr[2];
    uint32_t riscv[2];
    uint32_t tcfgA[2];
    uint32_t tcfgB[2];
    uint32_t mb0A[2];
    uint32_t mb0B[2];
    uint32_t mb1A[2];
    uint32_t mb1B[2];
    uint32_t bvA[2];
    uint32_t bvB[2];
    uint32_t fbifA[2];
    uint32_t fbifB[2];
    bool boundsFail; // R3ReadFalconWindow retorna false (base fora do mapa)
    // Mapper fixtures.
    bool identityStable;
    bool mapperQueryFail;
    bool mapperPresent;
    bool vtdPresent;
    unsigned vtdState; // 0..3
    bool barFlagsOk;
    bool above4G;
    bool bar1Large;
    // Driver-state fixtures.
    bool driverStateFail;
    bool schedZero;
    bool ceQuiet;
    bool barOursNone;
    bool epochOk;
    // Latch/busy/stopping.
    bool stopping;
    bool busy;
    bool invoked[3]; // idx 0/1/2 = probes 12/13/14
    bool providerAcquired;
    bool liveMap;
    pthread_mutex_t mutex;
    // Contadores (só nas ops). Devem permanecer 0: escritas não existem aqui.
    int read32Count;
    int acquireCount;
    int releaseCount;
    int mapCreateCount;
    int mapReleaseCount;
    int configReadCount;
    int waitShortCount;
    int mmioWriteCount;
    int configWriteCount;
    int dmaTriggerCount;

    void resetCounts()
    {
        read32Count = 0; acquireCount = 0; releaseCount = 0; mapCreateCount = 0; mapReleaseCount = 0;
        configReadCount = 0; waitShortCount = 0;
        mmioWriteCount = 0; configWriteCount = 0; dmaTriggerCount = 0;
    }
    void lock() { pthread_mutex_lock(&mutex); }
    void unlock() { pthread_mutex_unlock(&mutex); }
    bool isStopping() { return stopping; }
    bool isBusy() { return busy; }
    void setBusy(bool b) { busy = b; }
    bool acquireProvider()
    {
        acquireCount++;
        if (!providerIsPCI) return false;
        providerAcquired = true;
        return true;
    }
    void releaseProvider()
    {
        if (!providerAcquired) return;
        releaseCount++;
        providerAcquired = false;
    }
    bool wasInvoked(int probe)
    {
        if (probe < 12 || probe > 14) return true;
        return invoked[probe - 12];
    }
    void markInvoked(int probe)
    {
        if (probe >= 12 && probe <= 14) invoked[probe - 12] = true;
    }
    bool isProviderPCI() { return providerIsPCI; }
    uint16_t readVendorId() { configReadCount++; return vendor; }
    uint16_t readDeviceId() { configReadCount++; return device; }
    uint16_t readCommandBefore() { configReadCount++; return cmdBefore; }
    uint16_t readCommandAfter() { configReadCount++; return cmdAfter; }
    uint16_t readStatusReg() { configReadCount++; return statusReg; }
    uint32_t readBar0Raw() { configReadCount++; return bar0Raw; }
    uint32_t readBarRaw(unsigned idx)
    {
        (void)idx; configReadCount++; return 0;
    }
    uint32_t getResourceCount() { return nRes; }
    bool getResource(uint32_t idx, uint64_t &base, uint64_t &len)
    {
        if (idx >= nRes || idx >= GA106LAB_R3_MOCK_MAX_RES) return false;
        if (!res[idx].present) return false;
        base = res[idx].base; len = res[idx].len;
        return true;
    }
    bool createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut)
    {
        (void)idx; mapCreateCount++;
        if (mapNull) return false;
        mapLenOut = mapLen; mapOptsOut = mapOpts; liveMap = true;
        return true;
    }
    uint64_t getVA()
    {
        if (!liveMap || vaZero) return 0;
        return 0x1000u;
    }
    void releaseMap() { mapReleaseCount++; liveMap = false; }
    uint32_t read32(uint32_t absOffset)
    {
        read32Count++;
        // Janelas: SEC2 PFalcon 0x840000 / PF2 0x841000; GSP 0x110000 / 0x111000.
        if (absOffset >= 0x00841000u && absOffset < 0x00842000u) {
            uint32_t r2 = absOffset - 0x00841000u;
            if (!instReadable[0] || boundsFail) return 0xFFFFFFFFu;
            if (r2 == GA106LAB_R3_PF2_BCR_CTRL_REL) return bcr[0];
            if (r2 == GA106LAB_R3_PF2_RISCV_CPUCTL_REL) return riscv[0];
            return 0;
        }
        if (absOffset >= 0x00111000u && absOffset < 0x00112000u) {
            uint32_t r2 = absOffset - 0x00111000u;
            if (!instReadable[1] || boundsFail) return 0xFFFFFFFFu;
            if (r2 == GA106LAB_R3_PF2_BCR_CTRL_REL) return bcr[1];
            if (r2 == GA106LAB_R3_PF2_RISCV_CPUCTL_REL) return riscv[1];
            return 0;
        }
        int inst = -1;
        uint32_t rel = 0;
        if (absOffset >= 0x00840000u && absOffset < 0x00841000u) { inst = 0; rel = absOffset - 0x00840000u; }
        else if (absOffset >= 0x00110000u && absOffset < 0x00111000u) { inst = 1; rel = absOffset - 0x00110000u; }
        else return 0;
        if (inst < 0 || !instReadable[inst]) return 0xFFFFFFFFu;
        if (boundsFail) return 0xFFFFFFFFu;
        if (rel == GA106LAB_R3_FALCON_CPUCTL_REL) return cpuctl[inst];
        if (rel == GA106LAB_R3_FALCON_TRFCMD_REL) return trfNext(inst);
        if (rel == GA106LAB_R3_FBIF_TRANSCFG0_REL) return pairNext(inst, 0);
        if (rel == GA106LAB_R3_FALCON_MAILBOX0_REL) return pairNext(inst, 1);
        if (rel == GA106LAB_R3_FALCON_MAILBOX1_REL) return pairNext(inst, 2);
        if (rel == GA106LAB_R3_FALCON_BOOTVEC_REL) return pairNext(inst, 3);
        if (rel == GA106LAB_R3_FBIF_CTL_REL) return pairNext(inst, 4);
        return 0;
    }
    // Sequenciadores de leitura (estado de fixture, reset por MakeLive).
    int trfCalls[2];
    int pairCalls[2][5]; // 0=tcfg 1=mb0 2=mb1 3=bv 4=fbif; par=A ímpar=B
    uint32_t trfNext(int inst)
    {
        int c = trfCalls[inst]++;
        if (c == 0) return trfPre[inst];
        if (c == 1) return trfIdle2[inst];
        return trfPost[inst];
    }
    uint32_t pairNext(int inst, int which)
    {
        int c = pairCalls[inst][which]++;
        uint32_t a = 0;
        uint32_t b = 0;
        if (which == 0) { a = tcfgA[inst]; b = tcfgB[inst]; }
        else if (which == 1) { a = mb0A[inst]; b = mb0B[inst]; }
        else if (which == 2) { a = mb1A[inst]; b = mb1B[inst]; }
        else if (which == 3) { a = bvA[inst]; b = bvB[inst]; }
        else { a = fbifA[inst]; b = fbifB[inst]; }
        return ((c % 2) == 0) ? a : b;
    }
    bool readAbsent(uint32_t v) { return (v == 0xFFFFFFFFu || v == 0u) ? true : false; }
    void waitShort() { waitShortCount++; }
    bool checkIdentity(bool &stable) { stable = identityStable; return true; }
    bool queryMapper(bool &present, bool &useSystem, bool &nullArmed)
    {
        if (mapperQueryFail) return false;
        present = mapperPresent;
        useSystem = !mapperPresent;
        nullArmed = !mapperPresent;
        return true;
    }
    bool queryVtd(unsigned &present, unsigned &state)
    {
        present = vtdPresent ? 1u : 0u; state = vtdState; return true;
    }
    bool readBarFlags(bool &a4g, bool &b1l)
    {
        if (!barFlagsOk) return false;
        a4g = above4G; b1l = bar1Large; return true;
    }
    bool readDriverState(bool &sz, bool &ce, bool &bar, bool &ep)
    {
        if (driverStateFail) return false;
        sz = schedZero; ce = ceQuiet; bar = barOursNone; ep = epochOk;
        return true;
    }
};

inline void GA106LabR3MockMakeLive(GA106LabR3MockOps &o)
{
    unsigned i = 0;
    for (i = 0; i < GA106LAB_R3_MOCK_MAX_RES; i++) {
        o.res[i].base = 0; o.res[i].len = 0; o.res[i].present = false;
    }
    o.providerIsPCI = true;
    o.vendor = 0x10DEu; o.device = 0x2504u;
    o.cmdBefore = 0x0002u; o.cmdAfter = 0x0002u; // MSE ON, BME OFF (R3 prefere)
    o.statusReg = 0x0010u; // capability-list, sem error bits
    o.bar0Raw = 0xFB000000u;
    o.nRes = 3;
    o.res[0].present = true; o.res[0].base = 0xFB000000ULL; o.res[0].len = 0x1000000ULL;
    o.res[1].present = true; o.res[1].base = 0x440000000ULL; o.res[1].len = 0x10000000ULL;
    o.res[2].present = true; o.res[2].base = 0x450000000ULL; o.res[2].len = 0x2000000ULL;
    o.mapNull = false; o.mapLen = 0x1000000ULL; o.mapOpts = 0x1100u; o.vaZero = false;
    for (i = 0; i < 2; i++) {
        o.instReadable[i] = true;
        o.cpuctl[i] = 0x10u; // halted bit4
        o.trfPre[i] = 0x02u; o.trfIdle2[i] = 0x02u; o.trfPost[i] = 0x02u; // idle, sem full
        o.bcr[i] = 0x01u; // valid, core_select=Falcon(0)
        o.riscv[i] = 0x00u; // active==0
        o.tcfgA[i] = 0x05u; o.tcfgB[i] = 0x05u;
        o.mb0A[i] = 0; o.mb0B[i] = 0; o.mb1A[i] = 0; o.mb1B[i] = 0;
        o.bvA[i] = 0x12340000u; o.bvB[i] = 0x12340000u;
        o.fbifA[i] = 0x80u; o.fbifB[i] = 0x80u;
        o.trfCalls[i] = 0;
        o.pairCalls[i][0] = 0; o.pairCalls[i][1] = 0; o.pairCalls[i][2] = 0;
        o.pairCalls[i][3] = 0; o.pairCalls[i][4] = 0;
    }
    o.boundsFail = false;
    o.identityStable = true;
    o.mapperQueryFail = false; o.mapperPresent = true;
    o.vtdPresent = true; o.vtdState = 2u;
    o.barFlagsOk = true; o.above4G = true; o.bar1Large = true;
    o.driverStateFail = false;
    o.schedZero = true; o.ceQuiet = true; o.barOursNone = true; o.epochOk = true;
    o.stopping = false; o.busy = false;
    o.invoked[0] = false; o.invoked[1] = false; o.invoked[2] = false;
    o.providerAcquired = false;
    o.liveMap = false;
    pthread_mutex_init(&o.mutex, NULL);
    o.resetCounts();
}

#endif /* GA106LAB_R3_MOCK_OPS_HPP */
