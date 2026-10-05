// GA106LabR3ProbeFlow.hpp — R3 READ-ONLY probe flows (revF, shared templates).
//
// Três fluxos zero-input, somente-leitura, sem writes em qualquer caminho:
//   RunR3FalconStateFlow<Ops>      (selector 12)
//   RunR3MappingContractFlow<Ops>   (selector 13)
//   RunR3QuiescenceFlow<Ops>        (selector 14)
//
// Produção (R3RealOps) e testes (R3MockOps) instanciam OS MESMOS templates.
// Static dispatch, sem virtual.
//
// Regras congeladas:
//   - launchAttempted == 0 em TODOS os caminhos (R3 gera zero IOVAs, zero launches).
//   - PMU: slot VOID (QWEN-F7) — sem probe PMU, sem chosen=PMU.
//   - Hashes (transcfg/mailbox/bootvec/fbif) = double-sample intra-probe;
//     stable = iguais. Sem baseline GateB neste boot (sem replay, por §5).
//   - Qualquer campo ilegível => FAIL_PRELAUNCH (ou NEEDS_REVIEW se hash ausente).
//   - duplicate => REJECT_DUPLICATE; stopping => Aborted; busy => Busy.
//   - Leituras limitadas e bounded (idle spacing ~1ms via ops.waitShort()).
//
// Ops deve prover (somente operações; policy aqui):
//   lock(); unlock(); bool isStopping(); bool isBusy(); void setBusy(bool);
//   bool wasInvoked(int probe); void markInvoked(int probe); // 12/13/14
//   bool acquireProvider(); void releaseProvider(); // pin 1:1 (flow garante)
//   bool isProviderPCI();
//   uint16_t readVendorId(); uint16_t readDeviceId(); uint16_t readCommandBefore();
//   uint16_t readCommandAfter(); uint16_t readStatusReg();
//   uint32_t readBar0Raw(); uint32_t readBarRaw(unsigned idx); // idx 1..5, 0 = ausente-ok? (só 1..5)
//   uint32_t getResourceCount();
//   bool getResource(uint32_t idx, uint64_t &base, uint64_t &len);
//   bool createMap(uint32_t idx, uint64_t &mapLenOut, uint32_t &mapOptsOut); // RO+Inhibit
//   uint64_t getVA(); void releaseMap();
//   uint32_t read32(uint32_t absOffset); // ÚNICA primitiva MMIO (RO); bounds checados no flow
//   bool readAbsent(uint32_t v); // 0xFFFFFFFF/0
//   void waitShort(); // ~1ms idle spacing (bounded)
//   bool checkIdentity(bool &stable); // 10de:2504/classe vs snapshot
//   bool queryMapper(bool &present, bool &useSystem, bool &nullArmed);
//   bool queryVtd(unsigned &present, unsigned &state); // state: 0/1/2/3
//   bool readBarFlags(bool &above4G, bool &bar1Large); // 0/1; (2=unknown só no mock)
//   bool readDriverState(bool &schedZero, bool &ceQuiet, bool &barOursNone, bool &epochOk);

#ifndef GA106LAB_R3_PROBE_FLOW_HPP
#define GA106LAB_R3_PROBE_FLOW_HPP

#include "GA106LabProtocol.h"
#include "GA106LabCoreValidate.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

#define GA106LAB_R3_PROBE_FALCON 12
#define GA106LAB_R3_PROBE_MAPPING 13
#define GA106LAB_R3_PROBE_QUIES 14

// Amostra RO de uma janela Falcon (SEC2 ou GSP). Sem decisão aqui.
struct GA106LabR3FalconSample {
    bool readable;
    uint32_t cpuctl;
    uint32_t trfPre;
    uint32_t trfIdle2;
    uint32_t trfPost;
    uint32_t bcr;
    uint32_t riscv;
    uint32_t tcfgA;
    uint32_t tcfgB;
    uint32_t mb0A;
    uint32_t mb0B;
    uint32_t mb1A;
    uint32_t mb1B;
    uint32_t bvA;
    uint32_t bvB;
    uint32_t fbifA;
    uint32_t fbifB;
};

// Lê a janela completa com bounds-check por leitura. RO.
template <typename Ops>
inline bool R3ReadFalconWindow(Ops &ops, uint32_t base, uint32_t pf2base,
                               uint64_t mapLen, GA106LabR3FalconSample &s)
{
    uint32_t offs[17];
    uint32_t *dsts[17];
    unsigned n = 0;
    unsigned k = 0;
    s.readable = false;
    s.cpuctl = 0; s.trfPre = 0; s.trfIdle2 = 0; s.trfPost = 0;
    s.bcr = 0; s.riscv = 0;
    s.tcfgA = 0; s.tcfgB = 0; s.mb0A = 0; s.mb0B = 0;
    s.mb1A = 0; s.mb1B = 0; s.bvA = 0; s.bvB = 0; s.fbifA = 0; s.fbifB = 0;

    offs[0] = base + GA106LAB_R3_FALCON_CPUCTL_REL;
    offs[1] = base + GA106LAB_R3_FALCON_TRFCMD_REL;
    // idle2/fullPost lidos após spacing (abaixo).
    offs[2] = pf2base + GA106LAB_R3_PF2_BCR_CTRL_REL;
    offs[3] = pf2base + GA106LAB_R3_PF2_RISCV_CPUCTL_REL;
    offs[4] = base + GA106LAB_R3_FBIF_TRANSCFG0_REL;
    offs[5] = base + GA106LAB_R3_FALCON_MAILBOX0_REL;
    offs[6] = base + GA106LAB_R3_FALCON_MAILBOX1_REL;
    offs[7] = base + GA106LAB_R3_FALCON_BOOTVEC_REL;
    offs[8] = base + GA106LAB_R3_FBIF_CTL_REL;
    for (k = 0; k < 9; k++) {
        uint64_t end = (uint64_t)offs[k] + 4u;
        if (end < (uint64_t)offs[k] || end > mapLen) {
            return false;
        }
    }
    s.cpuctl = ops.read32(offs[0]);
    s.trfPre = ops.read32(offs[1]);
    ops.waitShort();
    s.trfIdle2 = ops.read32(offs[1]);
    s.trfPost = ops.read32(offs[1]);
    s.bcr = ops.read32(offs[2]);
    s.riscv = ops.read32(offs[3]);
    s.tcfgA = ops.read32(offs[4]);
    s.mb0A = ops.read32(offs[5]);
    s.mb1A = ops.read32(offs[6]);
    s.bvA = ops.read32(offs[7]);
    s.fbifA = ops.read32(offs[8]);
    ops.waitShort();
    s.tcfgB = ops.read32(offs[4]);
    s.mb0B = ops.read32(offs[5]);
    s.mb1B = ops.read32(offs[6]);
    s.bvB = ops.read32(offs[7]);
    s.fbifB = ops.read32(offs[8]);
    (void)dsts; (void)n;
    s.readable = true;
    return true;
}

// Predicados de instância (policy). hashesStable exige 2ª amostra válida.
template <typename Ops>
inline bool R3FalconInstancePass(Ops &ops, const GA106LabR3FalconSample &s,
                                 bool &hashUnknown)
{
    bool ok = false;
    bool idle2Ok = false;
    bool fullPostOk = false;
    bool hashOk = false;
    bool coreOk = false;
    bool riscvOk = false;
    hashUnknown = false;
    if (!s.readable) {
        return false;
    }
    if (ops.readAbsent(s.cpuctl) || ops.readAbsent(s.trfPre)) {
        return false;
    }
    if (!GA106LabR3Halted(s.cpuctl)) {
        return false;
    }
    if (GA106LabR3Full(s.trfPre)) {
        return false;
    }
    if (!GA106LabR3Idle(s.trfPre)) {
        return false;
    }
    idle2Ok = GA106LabR3Idle(s.trfIdle2);
#ifdef R3MUT_SKIP_SECOND_IDLE
    idle2Ok = true; // MUTAÇÃO TEST-ONLY: ignora 2ª amostra idle.
#endif
    if (!idle2Ok) {
        return false;
    }
    fullPostOk = !GA106LabR3Full(s.trfPost);
#ifdef R3MUT_SKIP_FULL_POST
    fullPostOk = true; // MUTAÇÃO TEST-ONLY: ignora full pós.
#endif
    if (!fullPostOk) {
        return false;
    }
    coreOk = GA106LabR3CoreSelectFalcon(s.bcr);
#ifdef R3MUT_SKIP_CORESELECT
    coreOk = true; // MUTAÇÃO TEST-ONLY: ignora core-select.
#endif
    if (!coreOk) {
        return false;
    }
    riscvOk = GA106LabR3RiscvQuiet(s.riscv);
#ifdef R3MUT_SKIP_RISCV
    riscvOk = true; // MUTAÇÃO TEST-ONLY: ignora RISCV active.
#endif
    if (!riscvOk) {
        return false;
    }
    // Hashes: só 0xFFFFFFFF é ilegível (mailbox zero é baseline legítima).
    if (s.tcfgA == 0xFFFFFFFFu || s.mb0A == 0xFFFFFFFFu ||
        s.mb1A == 0xFFFFFFFFu || s.bvA == 0xFFFFFFFFu ||
        s.fbifA == 0xFFFFFFFFu) {
        hashUnknown = true;
        return false;
    }
    hashOk = (s.tcfgA == s.tcfgB) && (s.mb0A == s.mb0B) &&
             (s.mb1A == s.mb1B) && (s.bvA == s.bvB) && (s.fbifA == s.fbifB);
#ifdef R3MUT_SKIP_HASH
    hashOk = true; // MUTAÇÃO TEST-ONLY: ignora drift de baseline.
#endif
    ok = hashOk;
    return ok;
}

// Preâmbulo RO comum: provider→identidade→MSE→BAR0→discovery→map→cache→VA.
// Retorna IOReturn; em Success, matchIndex/mapLen/va preenchidos (mapa ABERTO).
template <typename Ops>
inline IOReturn R3RoMapOpen(Ops &ops, uint32_t &matchIndexOut, uint64_t &mapLenOut,
                            uint64_t &vaOut)
{
    uint16_t vendor = 0;
    uint16_t device = 0;
    uint16_t commandBefore = 0;
    uint32_t bar0Raw = 0;
    uint64_t bar0Base = 0;
    uint32_t count = 0;
    uint32_t i = 0;
    int matchIndex = -1;
    int matchCount = 0;
    uint64_t resLen = 0;
    uint64_t resPhys = 0;
    uint64_t mapLen = 0;
    uint32_t mapOpts = 0;
    uint64_t va = 0;

    matchIndexOut = 0; mapLenOut = 0; vaOut = 0;
    if (!ops.isProviderPCI()) {
        return kIOReturnNotReady;
    }
    vendor = ops.readVendorId();
    if (vendor == 0xFFFFu) {
        return kIOReturnNotReady;
    }
    device = ops.readDeviceId();
    if (vendor != GA106LAB_VENDOR_NVIDIA || device != GA106LAB_DEVICE_GA106) {
        return kIOReturnNoDevice;
    }
    commandBefore = ops.readCommandBefore();
    if (!GA106LabCommandMseOn(commandBefore)) {
        return kIOReturnNotReady;
    }
    bar0Raw = ops.readBar0Raw();
    if (!GA106LabBar0RawValid(bar0Raw)) {
        return kIOReturnNoDevice;
    }
    bar0Base = GA106LabBar0BaseFromRaw(bar0Raw);
    count = ops.getResourceCount();
    for (i = 0; i < count; i++) {
        uint64_t base = 0;
        uint64_t len = 0;
        if (!ops.getResource(i, base, len)) {
            continue;
        }
        if (GA106LabBarResourceMatches(len, base, bar0Base)) {
            matchIndex = (int)i;
            matchCount++;
            resLen = len;
            resPhys = base;
        }
    }
    if (matchCount != 1 || matchIndex < 0) {
        return kIOReturnNotFound;
    }
    if (resLen != GA106LAB_BAR0_EXPECTED_LENGTH || resPhys != bar0Base) {
        return kIOReturnNoDevice;
    }
    if (!ops.createMap((uint32_t)matchIndex, mapLen, mapOpts)) {
        return kIOReturnNoResources;
    }
    if (mapLen != resLen) {
        ops.releaseMap();
        return kIOReturnNoMemory;
    }
    if (!GA106LabMapOptionsValidROInhibit(mapOpts)) {
        ops.releaseMap();
        return kIOReturnNotPermitted;
    }
    va = ops.getVA();
    if (va == 0) {
        ops.releaseMap();
        return kIOReturnNoResources;
    }
    matchIndexOut = (uint32_t)matchIndex; mapLenOut = mapLen; vaOut = va;
    return kIOReturnSuccess;
}

template <typename Ops>
inline void R3ZeroFalcon(GA106LabR3FalconStateV1 &out)
{
    unsigned k = 0;
    out.size = 0; out.version = 0; out.status = kGA106LabR3Status_NotRun;
    out.phase = 0; out.resolved = 0; out.halted = 0; out.fullPre = 0;
    out.idle1 = 0; out.idle2 = 0; out.fullPost = 0; out.coreSelectFalcon = 0;
    out.riscvQuiet = 0; out.transcfgStable = 0; out.mailboxStable = 0;
    out.bootvecStable = 0; out.fbifStable = 0; out.chosen = kGA106LabR3Chosen_None;
    out.launchAttempted = 0; out.reserved0 = 0; out.reserved1 = 0;
    for (k = 0; k < 8; k++) out.reserved[k] = 0;
    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
}

template <typename Ops>
inline IOReturn RunR3FalconStateFlow(Ops &ops, GA106LabR3FalconStateV1 &out)
{
    uint32_t matchIndex = 0;
    uint64_t mapLen = 0;
    uint64_t va = 0;
    uint16_t commandBefore = 0;
    uint16_t commandAfter = 0;
    IOReturn kr = kIOReturnError;
    GA106LabR3FalconSample sec2;
    GA106LabR3FalconSample gsp;
    bool secHashUnk = false;
    bool gspHashUnk = false;
    bool secPass = false;
    bool gspPass = false;

    R3ZeroFalcon<Ops>(out);
    ops.lock();
    if (ops.isStopping()) { ops.unlock(); out.status = kGA106LabR3Status_Aborted; return kIOReturnAborted; }
    if (ops.isBusy()) { ops.unlock(); out.status = kGA106LabR3Status_Aborted; return kIOReturnBusy; }
    ops.setBusy(true);
    ops.unlock();
#ifdef R3MUT_SKIP_DUPLICATE
    (void)0; // MUTAÇÃO TEST-ONLY: ignora latch duplicate.
#else
    if (ops.wasInvoked(GA106LAB_R3_PROBE_FALCON)) {
        out.status = kGA106LabR3Status_RejectDuplicate;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
#endif
    if (!ops.acquireProvider()) {
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    commandBefore = ops.readCommandBefore();
    kr = R3RoMapOpen(ops, matchIndex, mapLen, va);
    (void)matchIndex; (void)va;
    if (kr != kIOReturnSuccess) {
        ops.releaseProvider();
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    out.resolved = 1;
    if (!R3ReadFalconWindow(ops, GA106LAB_R3_SEC2_PFALCON_BASE,
                            GA106LAB_R3_SEC2_PFALCON2_BASE, mapLen, sec2)) {
        ops.releaseMap();
        ops.releaseProvider();
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    if (!R3ReadFalconWindow(ops, GA106LAB_R3_GSP_PFALCON_BASE,
                            GA106LAB_R3_GSP_PFALCON2_BASE, mapLen, gsp)) {
        ops.releaseMap();
        ops.releaseProvider();
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    secPass = R3FalconInstancePass(ops, sec2, secHashUnk);
    gspPass = R3FalconInstancePass(ops, gsp, gspHashUnk);
    out.halted = GA106LabR3Halted(sec2.cpuctl) ? 1 : 0;
    out.fullPre = GA106LabR3Full(sec2.trfPre) ? 0 : 1;
    out.idle1 = GA106LabR3Idle(sec2.trfPre) ? 1 : 0;
    out.idle2 = GA106LabR3Idle(sec2.trfIdle2) ? 1 : 0;
    out.fullPost = GA106LabR3Full(sec2.trfPost) ? 0 : 1;
    out.coreSelectFalcon = GA106LabR3CoreSelectFalcon(sec2.bcr) ? 1 : 0;
    out.riscvQuiet = GA106LabR3RiscvQuiet(sec2.riscv) ? 1 : 0;
    out.transcfgStable = (sec2.tcfgA == sec2.tcfgB) ? 1 : 0;
    out.mailboxStable = ((sec2.mb0A == sec2.mb0B) && (sec2.mb1A == sec2.mb1B)) ? 1 : 0;
    out.bootvecStable = (sec2.bvA == sec2.bvB) ? 1 : 0;
    out.fbifStable = (sec2.fbifA == sec2.fbifB) ? 1 : 0;
    if (secHashUnk || gspHashUnk) {
        ops.releaseMap();
        ops.releaseProvider();
#ifdef R3MUT_UNKNOWN_AS_PASS
        out.chosen = kGA106LabR3Chosen_Sec2; // MUTAÇÃO: infere PASS no UNKNOWN.
        out.status = kGA106LabR3Status_Pass;
#else
        out.status = kGA106LabR3Status_NeedsReview;
#endif
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
#ifdef R3MUT_CHOOSE_DIRTY
    out.chosen = kGA106LabR3Chosen_Sec2; // MUTAÇÃO: escolhe SEC2 mesmo sujo.
    out.status = kGA106LabR3Status_Pass;
#else
    if (secPass) {
        out.chosen = kGA106LabR3Chosen_Sec2;
        out.status = kGA106LabR3Status_Pass;
    } else if (gspPass) {
        out.chosen = kGA106LabR3Chosen_Gsp;
        out.status = kGA106LabR3Status_Pass;
    } else {
        out.chosen = kGA106LabR3Chosen_None;
        out.status = kGA106LabR3Status_FailPrelaunch;
    }
#endif
    ops.releaseMap();
    commandAfter = ops.readCommandAfter();
    ops.releaseProvider();
    if (commandAfter != commandBefore) {
        out.status = kGA106LabR3Status_FailHealth;
        ops.markInvoked(GA106LAB_R3_PROBE_FALCON);
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
#ifdef R3MUT_EXPOSE_ADDR
    out.reserved[0] = 0xDEADBEEFu; // MUTAÇÃO TEST-ONLY: vaza valor no reserved.
#endif
#ifdef R3MUT_ALLOW_LAUNCH
    out.launchAttempted = 1; // MUTAÇÃO TEST-ONLY: finge launch em probe RO.
#endif
    ops.markInvoked(GA106LAB_R3_PROBE_FALCON);
    ops.lock(); ops.setBusy(false); ops.unlock();
    return kIOReturnSuccess;
}

template <typename Ops>
inline IOReturn RunR3MappingContractFlow(Ops &ops, GA106LabR3MappingContractV1 &out)
{
    unsigned k = 0;
    bool stable = false;
    bool present = false;
    bool useSystem = false;
    bool nullArmed = false;
    unsigned vtdPresent = 0;
    unsigned vtdState = 3u;
    bool above4G = false;
    bool bar1Large = false;

    out.size = 0; out.version = 0; out.status = kGA106LabR3Status_NotRun;
    out.phase = 0; out.providerMapperPresent = 0; out.systemMapperUsed = 0;
    out.mapperNullArmed = 0; out.dmarPresent = 2u; out.appleVtdState = 3u;
    out.providerIdentityStable = 0; out.above4GDecoding = 2u; out.rebarBar1Large = 0;
    out.gen64NotRunInR3 = 1; out.launchAttempted = 0;
    out.reserved0 = 0; out.reserved1 = 0;
    for (k = 0; k < 9; k++) out.reserved[k] = 0;
    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
    ops.lock();
    if (ops.isStopping()) { ops.unlock(); out.status = kGA106LabR3Status_Aborted; return kIOReturnAborted; }
    if (ops.isBusy()) { ops.unlock(); out.status = kGA106LabR3Status_Aborted; return kIOReturnBusy; }
    ops.setBusy(true);
    ops.unlock();
#ifdef R3MUT_SKIP_DUPLICATE
    (void)0; // MUTAÇÃO TEST-ONLY.
#else
    if (ops.wasInvoked(GA106LAB_R3_PROBE_MAPPING)) {
        out.status = kGA106LabR3Status_RejectDuplicate;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
#endif
    if (!ops.acquireProvider()) {
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    if (!ops.checkIdentity(stable) || !stable) {
        ops.releaseProvider();
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    out.providerIdentityStable = 1;
    if (!ops.queryMapper(present, useSystem, nullArmed)) {
        ops.releaseProvider();
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    out.providerMapperPresent = present ? 1 : 0;
    out.systemMapperUsed = useSystem ? 1 : 0;
    out.mapperNullArmed = nullArmed ? 1 : 0;
    if (!present && !useSystem && !nullArmed) {
        ops.releaseProvider();
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    (void)ops.queryVtd(vtdPresent, vtdState);
    out.dmarPresent = (vtdPresent > 2u) ? 2u : (uint8_t)vtdPresent;
    out.appleVtdState = (vtdState > 3u) ? 3u : (uint8_t)vtdState;
    if (ops.readBarFlags(above4G, bar1Large)) {
        out.above4GDecoding = above4G ? 1u : 0u;
        out.rebarBar1Large = bar1Large ? 1u : 0u;
    }
    out.gen64NotRunInR3 = 1;
#ifdef R3MUT_RUN_GEN64
    out.gen64NotRunInR3 = 0; // MUTAÇÃO TEST-ONLY: finge IOVA gerada em R3.
#endif
    out.launchAttempted = 0;
#ifdef R3MUT_ALLOW_LAUNCH
    out.launchAttempted = 1; // MUTAÇÃO TEST-ONLY.
#endif
    out.status = kGA106LabR3Status_Pass;
    ops.releaseProvider();
    ops.markInvoked(GA106LAB_R3_PROBE_MAPPING);
    ops.lock(); ops.setBusy(false); ops.unlock();
    return kIOReturnSuccess;
}

template <typename Ops>
inline IOReturn RunR3QuiescenceFlow(Ops &ops, GA106LabR3QuiescenceV1 &out)
{
    unsigned k = 0;
    uint32_t matchIndex = 0;
    uint64_t mapLen = 0;
    uint64_t va = 0;
    uint16_t commandBefore = 0;
    uint16_t commandAfter = 0;
    uint16_t statusReg = 0;
    IOReturn kr = kIOReturnError;
    GA106LabR3FalconSample sec2;
    bool hashUnk = false;
    bool engPass = false;
    bool schedZero = false;
    bool ceQuiet = false;
    bool barNone = false;
    bool epochOk = false;

    out.size = 0; out.version = 0; out.status = kGA106LabR3Status_NotRun;
    out.phase = 0; out.quiescencePass = 0; out.baselineStable = 0;
    out.pendingProjectDmaState = 0; out.halted = 0; out.fullPre = 0;
    out.idle1 = 0; out.idle2 = 0; out.fullPost = 0; out.transcfgStable = 0;
    out.mailboxStable = 0; out.bootvecStable = 0; out.fbifStable = 0;
    out.schedZero = 0; out.ceNoPending = 0; out.aerClean = 0; out.iommuClean = 0;
    out.providerEpochOk = 0; out.launchAttempted = 0; out.recoveryRequired = 0;
    out.reserved0 = 0;
    for (k = 0; k < 7; k++) out.reserved[k] = 0;
    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
    ops.lock();
    if (ops.isStopping()) { ops.unlock(); out.status = kGA106LabR3Status_Aborted; return kIOReturnAborted; }
    if (ops.isBusy()) { ops.unlock(); out.status = kGA106LabR3Status_Aborted; return kIOReturnBusy; }
    ops.setBusy(true);
    ops.unlock();
#ifdef R3MUT_SKIP_DUPLICATE
    (void)0; // MUTAÇÃO TEST-ONLY.
#else
    if (ops.wasInvoked(GA106LAB_R3_PROBE_QUIES)) {
        out.status = kGA106LabR3Status_RejectDuplicate;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
#endif
    if (!ops.acquireProvider()) {
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    commandBefore = ops.readCommandBefore();
    statusReg = ops.readStatusReg();
    out.aerClean = GA106LabR3AerClean(statusReg) ? 1 : 0;
    kr = R3RoMapOpen(ops, matchIndex, mapLen, va);
    (void)matchIndex; (void)va;
    if (kr != kIOReturnSuccess) {
        ops.releaseProvider();
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    if (!R3ReadFalconWindow(ops, GA106LAB_R3_SEC2_PFALCON_BASE,
                            GA106LAB_R3_SEC2_PFALCON2_BASE, mapLen, sec2)) {
        ops.releaseMap();
        ops.releaseProvider();
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    engPass = R3FalconInstancePass(ops, sec2, hashUnk);
    out.halted = GA106LabR3Halted(sec2.cpuctl) ? 1 : 0;
    out.fullPre = GA106LabR3Full(sec2.trfPre) ? 0 : 1;
    out.idle1 = GA106LabR3Idle(sec2.trfPre) ? 1 : 0;
    out.idle2 = GA106LabR3Idle(sec2.trfIdle2) ? 1 : 0;
    out.fullPost = GA106LabR3Full(sec2.trfPost) ? 0 : 1;
    out.transcfgStable = (sec2.tcfgA == sec2.tcfgB) ? 1 : 0;
    out.mailboxStable = ((sec2.mb0A == sec2.mb0B) && (sec2.mb1A == sec2.mb1B)) ? 1 : 0;
    out.bootvecStable = (sec2.bvA == sec2.bvB) ? 1 : 0;
    out.fbifStable = (sec2.fbifA == sec2.fbifB) ? 1 : 0;
    out.baselineStable = (out.transcfgStable && out.mailboxStable &&
                          out.bootvecStable && out.fbifStable) ? 1 : 0;
    if (!ops.readDriverState(schedZero, ceQuiet, barNone, epochOk)) {
        ops.releaseMap();
        ops.releaseProvider();
        out.status = kGA106LabR3Status_FailPrelaunch;
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    out.schedZero = schedZero ? 1 : 0;
    out.ceNoPending = ceQuiet ? 1 : 0;
    out.providerEpochOk = epochOk ? 1 : 0;
    out.iommuClean = 1; // Fraco e documentado: nenhum DMA do nosso driver neste boot,
                        // logo nenhuma falha surfada; R4 re-checa pós-DMA.
    (void)barNone;
    ops.releaseMap();
    commandAfter = ops.readCommandAfter();
    ops.releaseProvider();
    if (commandAfter != commandBefore) {
        out.status = kGA106LabR3Status_FailHealth;
        out.recoveryRequired = 1;
        ops.markInvoked(GA106LAB_R3_PROBE_QUIES);
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    if (hashUnk) {
        out.status = kGA106LabR3Status_NeedsReview;
        ops.markInvoked(GA106LAB_R3_PROBE_QUIES);
        ops.lock(); ops.setBusy(false); ops.unlock();
        return kIOReturnSuccess;
    }
    if (engPass && out.aerClean && schedZero && ceQuiet && epochOk && out.baselineStable) {
        out.status = kGA106LabR3Status_Pass;
        out.quiescencePass = 1;
        out.pendingProjectDmaState = 0;
        out.recoveryRequired = 0;
    } else {
        out.status = kGA106LabR3Status_FailPrelaunch;
        out.recoveryRequired = 0;
    }
    ops.markInvoked(GA106LAB_R3_PROBE_QUIES);
    ops.lock(); ops.setBusy(false); ops.unlock();
    return kIOReturnSuccess;
}

#endif /* GA106LAB_R3_PROBE_FLOW_HPP */
