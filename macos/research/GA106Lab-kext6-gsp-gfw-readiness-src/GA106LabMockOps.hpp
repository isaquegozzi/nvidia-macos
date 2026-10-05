// GA106LabMockOps.hpp — MockOps para testes offline (mesmos shared flows).
//
// Simula hardware via fixtures. Contadores incrementados SOMENTE dentro
// das operações mock equivalentes às reais:
//   read32() -> read32Count++ (+ readOffsets[] log em ordem)
//   createMap() -> mapCreateCount++
//   releaseMap() -> mapReleaseCount++
// Testes NUNCA incrementam manualmente; medem pontos efetivos dos flows
// (selector 5 RunFirstMmioReadFlow, selector 6 RunStaticIdentityReadsFlow
// e selector 7 RunPrepareGspSysmemFlow).
//
// Static dispatch (sem virtual), mesma assinatura das RealOps.

#ifndef GA106LAB_MOCK_OPS_HPP
#define GA106LAB_MOCK_OPS_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"
#include "GA106LabGspSysmemFlow.hpp"

#include <IOKit/IOReturn.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>

#define GA106LAB_MOCK_MAX_RES 8

struct GA106LabMockRes {
    uint64_t base;
    uint64_t len;
    bool present; // false = getDeviceMemoryWithIndex retorna NULL
};

struct GA106LabMockOps {
    // Fixtures
    bool providerIsPCI;
    uint16_t vendor;
    uint16_t device;
    uint16_t cmdBefore;
    uint16_t cmdAfter;
    uint32_t bar0Raw;
    GA106LabMockRes res[GA106LAB_MOCK_MAX_RES];
    uint32_t nRes;
    bool mapNull;
    uint64_t mapLen;
    uint32_t mapOpts;
    bool vaZero;
    uint32_t mmioRaw; // selector 5 (BOOT_0)
    // Selector 6 fixture (BOOT_42 @0xA00; BOOT_2 removido).
    uint32_t boot42Raw;
    // Offset/width: default fixo; testes podem injetar para alignment/bounds.
    bool useCustomOffset;
    uint32_t customOffset;
    uint32_t customWidth; // 0 = usar fixo 4
    // Overrides do selector 6 (test-only; produção sempre fixa).
    bool useCustomBoot42Offset;
    uint32_t customBoot42Offset;

    // Contadores reais (dentro das ops) + estado do mapa mockado.
    int read32Count;
    int mapCreateCount;
    int mapReleaseCount;
    int configReadCount;
    bool liveMap;
    // Ordem das leituras (para READ_ORDER_FIXED): registra offset de cada read32.
    uint32_t readOffsets[8];
    int readOffsetsCount;

    void resetCounts()
    {
        read32Count = 0;
        mapCreateCount = 0;
        mapReleaseCount = 0;
        configReadCount = 0;
        liveMap = false;
        readOffsetsCount = 0;
    }

    bool isProviderPCI() { return providerIsPCI; }

    uint16_t readVendorId()
    {
        configReadCount++;
        return vendor;
    }
    uint16_t readDeviceId()
    {
        configReadCount++;
        return device;
    }
    uint16_t readCommandBefore()
    {
        configReadCount++;
        return cmdBefore;
    }
    uint16_t readCommandAfter()
    {
        configReadCount++;
        return cmdAfter;
    }
    uint32_t readBar0Raw()
    {
        configReadCount++;
        return bar0Raw;
    }

    uint32_t getResourceCount() { return nRes; }

    bool getResource(uint32_t idx, uint64_t &base, uint64_t &len)
    {
        if (idx >= nRes || idx >= GA106LAB_MOCK_MAX_RES) {
            return false;
        }
        if (!res[idx].present) {
            return false;
        }
        base = res[idx].base;
        len = res[idx].len;
        return true;
    }

    bool createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut)
    {
        (void)idx;
        mapCreateCount++;
        if (mapNull) {
            return false;
        }
        mapLenOut = mapLen;
        mapOptsOut = mapOpts;
        liveMap = true;
        return true;
    }

    uint64_t getVA()
    {
        if (!liveMap) {
            return 0;
        }
        if (vaZero) {
            return 0;
        }
        return 0x1000u; // VA fake não-zero (nunca dereferenciado; read usa mmioRaw)
    }

    uint32_t mmioOffset()
    {
        if (useCustomOffset) {
            return customOffset;
        }
        return kGA106LabMmio_PmcBoot0_Offset;
    }
    uint32_t mmioWidth()
    {
        if (customWidth != 0) {
            return customWidth;
        }
        return kGA106LabMmio_PmcBoot0_Width;
    }

    uint32_t read32(uint32_t offset)
    {
        if (readOffsetsCount >= 0 &&
            readOffsetsCount < (int)(sizeof(readOffsets) /
                                     sizeof(readOffsets[0]))) {
            readOffsets[readOffsetsCount] = offset;
        }
        readOffsetsCount++;
        read32Count++;
        // Despacha pelo offset efetivo: BOOT_42 ou BOOT_0.
        if (offset == boot42Offset()) {
            return boot42Raw;
        }
        return mmioRaw;
    }

    uint32_t boot42Offset()
    {
        if (useCustomBoot42Offset) {
            return customBoot42Offset;
        }
        return kGA106LabStaticIdentity_Boot42_Offset;
    }
    uint32_t boot42Width() { return kGA106LabStaticIdentity_Boot42_Width; }

    void releaseMap()
    {
        mapReleaseCount++;
        liveMap = false;
    }
};

// Fixture live real: BAR0 16M + BAR1 256M + BAR3 32M + ROM 512K + I/O + sub-range.
inline void GA106LabMockMakeLive(GA106LabMockOps &o)
{
    unsigned i = 0;
    for (i = 0; i < GA106LAB_MOCK_MAX_RES; i++) {
        o.res[i].base = 0;
        o.res[i].len = 0;
        o.res[i].present = false;
    }
    o.providerIsPCI = true;
    o.vendor = 0x10DEu;
    o.device = 0x2504u;
    o.cmdBefore = 0x0003u;
    o.cmdAfter = 0x0003u;
    o.bar0Raw = 0xFB000000u;
    o.nRes = 6;
    o.res[0].present = true;
    o.res[0].base = 0xFB000000ULL;
    o.res[0].len = 0x1000000ULL;
    o.res[1].present = true;
    o.res[1].base = 0x440000000ULL;
    o.res[1].len = 0x10000000ULL;
    o.res[2].present = true;
    o.res[2].base = 0x450000000ULL;
    o.res[2].len = 0x2000000ULL;
    o.res[3].present = true;
    o.res[3].base = 0xFC000000ULL;
    o.res[3].len = 0x80000ULL;
    o.res[4].present = true;
    o.res[4].base = 0x1001ULL;
    o.res[4].len = 0x100ULL;
    o.res[5].present = true;
    o.res[5].base = 0x0ULL;
    o.res[5].len = 0x1000ULL;
    o.mapNull = false;
    o.mapLen = 0x1000000ULL;
    o.mapOpts = 0x1100u;
    o.vaZero = false;
    o.mmioRaw = 0x176000A1u;
    o.boot42Raw = 0x176000A1u; // CHIP_ID 10-bit 0x176 (máscara 0x3FF00000)
    o.useCustomOffset = false;
    o.customOffset = 0;
    o.customWidth = 0;
    o.useCustomBoot42Offset = false;
    o.customBoot42Offset = 0;
    o.resetCounts();
}

// GA106LabSysmemMockOps — MockOps do selector 7 (mesmo shared flow da
// produção, FIX-ASTRA-C). Fixtures por ponto de falha + counters dentro
// das ops + opLog de ordem + gate p/ testes de concorrência (pthreads).
// mmio/config/BME/DMA counters existem e devem permanecer 0 (assert).
// lock = pthread mutex (análogo funcional do IOSimpleLock p/ disciplina;
// TSan valida quando disponível).
struct GA106LabSysmemMockOps {
    // Fixtures de falha (false = sucesso).
    bool allocFail;
    bool bytesNull;
    bool cmdFail;
    bool bindFail;
    // prepareMode: 0=ok, 1=falha antes de ativar (sem fActive),
    // 2=falha COM estado ativo interno (exige complete — modelo XNU real).
    int prepareMode;
    // Segmentos: 0=ok(1x4096 alinhado), 1=zero, 2=multi, 3=curto,
    // 4=misaligned, 5=width-overflow.
    int segMode;
    // clearFail: clearDma retorna erro (sem release nesse caso).
    bool clearFail;

    // Estado owner-held espelhado (fases internas do flow header).
    uint32_t state;
    bool busy;
    bool stopping;
    bool descExists;
    bool cmdExists;
    bool cmdActive; // espelha fActive>0 do XNU após prepare tentado.
    uint64_t dmaAddr;
    uint32_t segLen;

    // Gate p/ concorrência: prepareDma bloqueia até gateOpen.
    bool prepareBlocks;
    bool gateOpen;
    pthread_mutex_t gateMutex;
    pthread_cond_t gateCond;

    // Exclusão (análogo do IOSimpleLock; TSan observa).
    pthread_mutex_t mutex;

    // Counters efetivos (só dentro das ops; complete/clear contam
    // TODA invocação p/ detectar double; releases só se objeto existia).
    // Atômicos: são instrumentação de teste lida por outras threads
    // (polling); o estado do owner continua sob mutex (TSan valida).
    std::atomic<int> allocCount;
    std::atomic<int> zeroCount;
    std::atomic<int> commandCreateCount;
    std::atomic<int> setDescriptorCount;
    std::atomic<int> prepareCount;
    std::atomic<int> segmentGenCount;
    std::atomic<int> completeCount;
    std::atomic<int> clearCount;
    std::atomic<int> descriptorReleaseCount;
    std::atomic<int> commandReleaseCount;
    // Devem permanecer 0 nesta microfase:
    std::atomic<int> mmioReadCount;
    std::atomic<int> mmioWriteCount;
    std::atomic<int> configWriteCount;
    std::atomic<int> bmeEnableCount;
    std::atomic<int> dmaTriggerCount;

    // Log de ordem das ops de teardown/cleanup (códigos abaixo).
    uint8_t opLog[64];
    int opLogCount;

    void logOp(uint8_t code)
    {
        if (opLogCount >= 0 &&
            opLogCount < (int)(sizeof(opLog) / sizeof(opLog[0]))) {
            opLog[opLogCount] = code;
        }
        opLogCount++;
    }

    void resetCounts()
    {
        allocCount = 0;
        zeroCount = 0;
        commandCreateCount = 0;
        setDescriptorCount = 0;
        prepareCount = 0;
        segmentGenCount = 0;
        completeCount = 0;
        clearCount = 0;
        descriptorReleaseCount = 0;
        commandReleaseCount = 0;
        mmioReadCount = 0;
        mmioWriteCount = 0;
        configWriteCount = 0;
        bmeEnableCount = 0;
        dmaTriggerCount = 0;
        opLogCount = 0;
    }

    void lock() { pthread_mutex_lock(&mutex); }
    void unlock() { pthread_mutex_unlock(&mutex); }

    uint32_t getState() { return state; }
    void setState(uint32_t s) { state = s; }
    bool isStopping() { return stopping; }
    void setStopping(bool b) { stopping = b; }
    bool isBusy() { return busy; }
    void setBusy(bool b) { busy = b; }
    void waitMs(unsigned ms) { usleep((useconds_t)(ms * 1000u)); }

    bool allocBuffer()
    {
        allocCount++;
        logOp(1);
        if (allocFail) {
            return false;
        }
        descExists = true;
        return true;
    }
    bool zeroBuffer()
    {
        zeroCount++;
        logOp(2);
        if (!descExists || bytesNull) {
            return false;
        }
        return true;
    }
    bool createCommand()
    {
        commandCreateCount++;
        logOp(3);
        if (cmdFail || !descExists) {
            return false;
        }
        cmdExists = true;
        return true;
    }
    bool bindDescriptor()
    {
        setDescriptorCount++;
        logOp(4);
        if (bindFail || !descExists || !cmdExists) {
            return false;
        }
        return true;
    }
    bool prepareDma()
    {
        prepareCount++;
        logOp(5);
        if (prepareBlocks) {
            pthread_mutex_lock(&gateMutex);
            while (!gateOpen) {
                pthread_cond_wait(&gateCond, &gateMutex);
            }
            pthread_mutex_unlock(&gateMutex);
        }
        if (prepareMode == 1) {
            return false; // erro antes de ativar: sem fActive.
        }
        cmdActive = true; // XNU incrementou fActive (sucesso OU erro).
        if (prepareMode == 2) {
            return false; // erro COM estado ativo: exige complete.
        }
        return true;
    }
    bool genSegments(uint32_t &countOut, uint64_t &addrOut,
                     uint64_t &lenOut)
    {
        segmentGenCount++;
        logOp(6);
        countOut = 0;
        addrOut = 0;
        lenOut = 0;
        if (!cmdActive) {
            return false;
        }
        switch (segMode) {
        case 0:
            countOut = 1;
            addrOut = 0x1000u;
            lenOut = 4096u;
            return true;
        case 1:
            countOut = 0;
            return true;
        case 2:
            countOut = 2;
            addrOut = 0x1000u;
            lenOut = 4096u;
            return true;
        case 3:
            countOut = 1;
            addrOut = 0x1000u;
            lenOut = 2048u;
            return true;
        case 4:
            countOut = 1;
            addrOut = 0x1001u;
            lenOut = 4096u;
            return true;
        case 5:
        default:
            countOut = 1;
            addrOut = 0xFFFFFFFFFFFFF000ULL; // +4096 faz wrap.
            lenOut = 4096u;
            return true;
        }
    }
    bool validateAddressWidth(uint64_t addr)
    {
        return GA106LabSysmemAddressValid(addr) ? true : false;
    }
    void markAddressReady(uint64_t addr, uint32_t len)
    {
        dmaAddr = addr;
        segLen = len;
    }
    int completeDma()
    {
        completeCount++;
        logOp(7);
        if (!cmdActive) {
            return kIOReturnNotReady; // XNU: fActive<1.
        }
        cmdActive = false; // decremento ocorre antes do trabalho.
        return kIOReturnSuccess;
    }
    int clearDma()
    {
        clearCount++;
        logOp(8);
        if (clearFail) {
            return kIOReturnError; // objeto ainda referencia: sem release.
        }
        return kIOReturnSuccess;
    }
    void releaseCommand()
    {
        logOp(9);
        if (!cmdExists) {
            return;
        }
        commandReleaseCount++;
        cmdExists = false;
    }
    void releaseDescriptor()
    {
        logOp(10);
        if (!descExists) {
            return;
        }
        descriptorReleaseCount++;
        descExists = false;
    }
    void clearAddressRecord()
    {
        dmaAddr = 0;
        segLen = 0;
    }
};

inline void GA106LabSysmemMockMakeLive(GA106LabSysmemMockOps &o)
{
    o.allocFail = false;
    o.bytesNull = false;
    o.cmdFail = false;
    o.bindFail = false;
    o.prepareMode = 0;
    o.segMode = 0;
    o.clearFail = false;
    o.state = kSysmemPhase_Empty;
    o.busy = false;
    o.stopping = false;
    o.descExists = false;
    o.cmdExists = false;
    o.cmdActive = false;
    o.dmaAddr = 0;
    o.segLen = 0;
    o.prepareBlocks = false;
    o.gateOpen = false;
    pthread_mutex_init(&o.gateMutex, NULL);
    pthread_cond_init(&o.gateCond, NULL);
    pthread_mutex_init(&o.mutex, NULL);
    o.resetCounts();
}

// GA106LabOwnerMockOps — MockOps para o MESMO shared init/free/lock flow
// da produção (TEST-PATH-FIX). Simula KPIs + conta operações; policy vive
// SOMENTE em GA106LabInitFreeLockFlow.hpp. POLICY_IN_OPS = NO.
//
// FakeLock* como void* opaco (análogo funcional de IOSimpleLock).
// state/desc/cmd espelham o owner kernel (fases internas do flow header).
struct GA106LabOwnerMockOps {
    struct FakeLock {
        int id;
    };

    FakeLock *firstLock;   // fSysmemLock análogo
    FakeLock *secondLock;  // fSlotLock análogo
    uint32_t state;
    bool cmdExists;
    bool descExists;

    // Fixtures de falha de alocação.
    bool failFirst;
    bool failSecond;

    // Counters (operações brutas; freeNull deve permanecer 0).
    int allocCount;
    int freeCount;
    int freeNullCount;
    int cmdReleaseCount;
    int descReleaseCount;

    void resetCounts()
    {
        allocCount = 0;
        freeCount = 0;
        freeNullCount = 0;
        cmdReleaseCount = 0;
        descReleaseCount = 0;
    }

    // --- Locks (raw, sem policy) ---
    void initNullLocks()
    {
        firstLock = nullptr;
        secondLock = nullptr;
    }
    bool allocFirstLockRaw()
    {
        allocCount++;
        if (failFirst) {
            return false;
        }
        firstLock = new FakeLock();
        firstLock->id = 1;
        return true;
    }
    bool allocSecondLockRaw()
    {
        allocCount++;
        if (failSecond) {
            return false;
        }
        secondLock = new FakeLock();
        secondLock->id = 2;
        return true;
    }
    void *getFirstLock() { return (void *)firstLock; }
    void *getSecondLock() { return (void *)secondLock; }
    void freeLockRaw(void *lock)
    {
        // Operação bruta: assume non-null (guard vive no shared flow).
        // Se shared flow falhar (mutação), lock pode ser NULL => conta.
        if (lock == nullptr) {
            freeNullCount++;
            return;
        }
        FakeLock *f = (FakeLock *)lock;
        delete f;
        freeCount++;
    }
    void clearFirstLock() { firstLock = nullptr; }
    void clearSecondLock() { secondLock = nullptr; }

    // --- Free resources (raw) ---
    uint32_t getState() { return state; }
    bool hasCommand() { return cmdExists; }
    bool hasDescriptor() { return descExists; }
    void releaseCommandRaw()
    {
        // Assume existência (shared flow checou hasCommand, exceto mutação
        // de fase terminal que libera mesmo sem deveria — conta aqui).
        if (!cmdExists) {
            // Mutação liberando sem objeto: ainda conta como release indevido.
            cmdReleaseCount++;
            return;
        }
        cmdReleaseCount++;
        cmdExists = false;
    }
    void releaseDescriptorRaw()
    {
        if (!descExists) {
            descReleaseCount++;
            return;
        }
        descReleaseCount++;
        descExists = false;
    }
    void logCleanupFailedLeakSafe() {}
    void logUnexpectedStateInFree() {}
};

inline void GA106LabOwnerMockMakeLive(GA106LabOwnerMockOps &o)
{
    o.firstLock = nullptr;
    o.secondLock = nullptr;
    o.state = kSysmemPhase_Empty;
    o.cmdExists = false;
    o.descExists = false;
    o.failFirst = false;
    o.failSecond = false;
    o.resetCounts();
}

// GA106LabGfwMockOps — MockOps do selector 8 (mesmo shared GFW readiness
// flow da produção, KEXT6). Fixtures por ponto de falha + counters SOMENTE
// dentro das ops + gate p/ concorrência (pthreads). waitMs NÃO dorme (só
// conta), exceto com waitBlocks (gate determinístico p/ T9/T10).
// storeCount/sysmemTouchCount existem e devem permanecer 0 (asserts);
// write32()/noteSelector7Call() só são chamados sob mutação test-only.
// lock = pthread mutex (análogo funcional do IOSimpleLock; TSan observa).
// POLICY_IN_OPS = NO (sem branching de gate/progress/poll aqui).
struct GA106LabGfwMockOps {
    // Fixtures de provider/recurso (mesmo layout live 16MiB da casa).
    bool providerIsPCI;
    uint16_t vendor;
    uint16_t device;
    uint16_t cmdBefore;
    uint32_t bar0Raw;
    GA106LabMockRes res[GA106LAB_MOCK_MAX_RES];
    uint32_t nRes;
    bool mapNull;
    uint64_t mapLen;
    uint32_t mapOpts;
    bool vaZero;

    // Fixtures de progresso: gateMode 0=sempre falso, 1=sempre verdadeiro,
    // 2=falso nas primeiras gateFalseCount leituras e depois verdadeiro.
    int gateMode;
    int gateFalseCount;
    // progMode 0=0xff imediato, 1=nunca (progValue), 2=sequência progSeq[].
    int progMode;
    uint32_t progValue;
    uint32_t progSeq[16];
    int progSeqLen;
    int progSeqIdx;

    // stopAtWait: waitMsCount ao atingir dispara stopping (T8); -1 = nunca.
    int stopAtWait;

    // Gate p/ concorrência: waitMs bloqueia até waitGateOpen.
    bool waitBlocks;
    bool waitGateOpen;
    pthread_mutex_t gateMutex;
    pthread_cond_t gateCond;

    // Estado de exclusão espelhado (fGfwBusy/fGfwStopping do owner).
    bool busy;
    bool stopping;
    pthread_mutex_t mutex;
    // Dono atual da trava (para detectar espera sob trava sem data race:
    // todo acesso a held/holder ocorre sob mutex).
    bool lockHeld;
    pthread_t lockHolder;

    // Contadores efetivos (só dentro das ops). Atômicos p/ leitura
    // cross-thread nos testes de concorrência (estado segue sob mutex).
    std::atomic<int> configReadCount;
    std::atomic<int> gateReadCount;
    std::atomic<int> progressReadCount;
    std::atomic<int> mapCreateCount;
    std::atomic<int> mapReleaseCount;
    std::atomic<int> waitMsCount;
    std::atomic<int> read32Count;
    int storeCount;       // deve permanecer 0 (só mutação escreve).
    int sysmemTouchCount; // deve permanecer 0 (só mutação toca sysmem).
    bool liveMap;
    bool waitedUnderLock; // true se waitMs viu lockDepth>0 (só mutação).
    // Ordem das leituras (para READ_ORDER_FIXED).
    uint32_t readOffsets[16];
    int readOffsetsCount;

    void resetCounts()
    {
        configReadCount = 0;
        gateReadCount = 0;
        progressReadCount = 0;
        mapCreateCount = 0;
        mapReleaseCount = 0;
        waitMsCount = 0;
        read32Count = 0;
        storeCount = 0;
        sysmemTouchCount = 0;
        liveMap = false;
        waitedUnderLock = false;
        readOffsetsCount = 0;
        progSeqIdx = 0;
        lockHeld = false;
    }

    void lock()
    {
        pthread_mutex_lock(&mutex);
        lockHeld = true;
        lockHolder = pthread_self();
    }
    void unlock()
    {
        lockHeld = false;
        pthread_mutex_unlock(&mutex);
    }

    bool isStopping() { return stopping; }
    void setStopping(bool b) { stopping = b; }
    bool isBusy() { return busy; }
    void setBusy(bool b) { busy = b; }
    void waitMs(unsigned ms)
    {
        bool under = false;
        (void)ms;
        // Amostra sob mutex: espera sob trava própria do chamador?
        pthread_mutex_lock(&mutex);
        under = lockHeld && pthread_equal(lockHolder, pthread_self());
        pthread_mutex_unlock(&mutex);
        if (under) {
            waitedUnderLock = true;
        }
        waitMsCount++;
        if (waitBlocks) {
            pthread_mutex_lock(&gateMutex);
            while (!waitGateOpen) {
                pthread_cond_wait(&gateCond, &gateMutex);
            }
            pthread_mutex_unlock(&gateMutex);
            return;
        }
        if (stopAtWait >= 0 && waitMsCount >= stopAtWait) {
            stopping = true;
        }
    }

    bool isProviderPCI() { return providerIsPCI; }
    uint16_t readVendorId()
    {
        configReadCount++;
        return vendor;
    }
    uint16_t readDeviceId()
    {
        configReadCount++;
        return device;
    }
    uint16_t readCommandBefore()
    {
        configReadCount++;
        return cmdBefore;
    }
    uint32_t readBar0Raw()
    {
        configReadCount++;
        return bar0Raw;
    }

    uint32_t getResourceCount() { return nRes; }
    bool getResource(uint32_t idx, uint64_t &base, uint64_t &len)
    {
        if (idx >= nRes || idx >= GA106LAB_MOCK_MAX_RES) {
            return false;
        }
        if (!res[idx].present) {
            return false;
        }
        base = res[idx].base;
        len = res[idx].len;
        return true;
    }

    bool createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut)
    {
        (void)idx;
        mapCreateCount++;
        if (mapNull) {
            return false;
        }
        mapLenOut = mapLen;
        mapOptsOut = mapOpts;
        liveMap = true;
        return true;
    }

    uint64_t getVA()
    {
        if (!liveMap) {
            return 0;
        }
        if (vaZero) {
            return 0;
        }
        return 0x1000u; // VA fake não-zero (nunca dereferenciado).
    }

    uint32_t read32(uint32_t offset)
    {
        uint32_t gateHits = (uint32_t)gateReadCount.load();
        if (readOffsetsCount >= 0 &&
            readOffsetsCount < (int)(sizeof(readOffsets) /
                                     sizeof(readOffsets[0]))) {
            readOffsets[readOffsetsCount] = offset;
        }
        readOffsetsCount++;
        read32Count++;
        if (offset == GA106LAB_GFW_GATE_OFFSET) {
            gateReadCount++;
            if (gateMode == 1) {
                return 0x1u;
            }
            if (gateMode == 2 && gateHits >= (uint32_t)gateFalseCount) {
                return 0x1u;
            }
            return 0x0u;
        }
        if (offset == GA106LAB_GFW_PROGRESS_OFFSET) {
            progressReadCount++;
            if (progMode == 1) {
                return progValue;
            }
            if (progMode == 2 && progSeqLen > 0) {
                uint32_t v = progSeq[progSeqIdx % progSeqLen];
                progSeqIdx++;
                return v;
            }
            return 0xFFu;
        }
        return 0x0u; // offset inesperado (mutação de offset cai aqui).
    }

    void write32(uint32_t offset, uint32_t value)
    {
        (void)offset;
        (void)value;
        storeCount++; // SÓ alcançável sob MUT_WRITE_INSTEAD_OF_READ.
    }

    void noteSelector7Call()
    {
        sysmemTouchCount++; // SÓ alcançável sob MUT_CALL_SELECTOR7.
    }

    void releaseMap()
    {
        mapReleaseCount++;
        liveMap = false;
    }
};

inline void GA106LabGfwMockMakeLive(GA106LabGfwMockOps &o)
{
    unsigned i = 0;
    for (i = 0; i < GA106LAB_MOCK_MAX_RES; i++) {
        o.res[i].base = 0;
        o.res[i].len = 0;
        o.res[i].present = false;
    }
    o.providerIsPCI = true;
    o.vendor = 0x10DEu;
    o.device = 0x2504u;
    o.cmdBefore = 0x0003u;
    o.bar0Raw = 0xFB000000u;
    o.nRes = 6;
    o.res[0].present = true;
    o.res[0].base = 0xFB000000ULL;
    o.res[0].len = 0x1000000ULL;
    o.res[1].present = true;
    o.res[1].base = 0x440000000ULL;
    o.res[1].len = 0x10000000ULL;
    o.res[2].present = true;
    o.res[2].base = 0x450000000ULL;
    o.res[2].len = 0x2000000ULL;
    o.res[3].present = true;
    o.res[3].base = 0xFC000000ULL;
    o.res[3].len = 0x80000ULL;
    o.res[4].present = true;
    o.res[4].base = 0x1001ULL;
    o.res[4].len = 0x100ULL;
    o.res[5].present = true;
    o.res[5].base = 0x0ULL;
    o.res[5].len = 0x1000ULL;
    o.mapNull = false;
    o.mapLen = 0x1000000ULL;
    o.mapOpts = 0x1100u;
    o.vaZero = false;
    o.gateMode = 1;
    o.gateFalseCount = 0;
    o.progMode = 0;
    o.progValue = 0x00u;
    o.progSeqLen = 0;
    for (i = 0; i < 16; i++) {
        o.progSeq[i] = 0;
    }
    o.stopAtWait = -1;
    o.waitBlocks = false;
    o.waitGateOpen = false;
    pthread_mutex_init(&o.gateMutex, NULL);
    pthread_cond_init(&o.gateCond, NULL);
    // Trava RECURSIVA de propósito: waitMs amostra lockHeld/lockHolder sob
    // mutex; sob MUT_WAIT_UNDER_LOCK o flow chama waitMs já com a trava
    // retida pela mesma thread — com trava normal isso deadlockaria o
    // harness em vez de deixar o teste detectar a violação via flag.
    // (IOSimpleLock real NÃO é recursivo; a recursividade aqui é só
    // instrumentação de teste, sem efeito na policy auditada.)
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&o.mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    o.busy = false;
    o.stopping = false;
    o.resetCounts();
}

#endif /* GA106LAB_MOCK_OPS_HPP */
