//
// GA106LabProductionPhase.h — P80: máquina de estados explícita do caminho
// de produção offline (Detached ... SubmissionPrepared, teto LiveBlocked).
// Kernel + host-test compatível: C++ puro, sem IOKit, sem HW, sem alocação.
// Regras: sem pular estados; sem reentrada incompatível; erro -> seguro;
// stop/clientClose -> Attached (teardown determinístico); LiveBlocked é
// terminal (só re-init limpa) e registra tentativa de operação real bloqueada.
//

#ifndef GA106LAB_PRODUCTION_PHASE_H
#define GA106LAB_PRODUCTION_PHASE_H

#include <stdint.h>

enum GA106LabProductionPhase : uint32_t {
    kProdPhase_Detached                   = 0,
    kProdPhase_Attached                   = 1,
    kProdPhase_Bar0Mapped                 = 2,
    kProdPhase_BootIdentityVerified       = 3,
    kProdPhase_SysmemPrepared             = 4,
    kProdPhase_GfwReady                   = 5,
    kProdPhase_FlushPreconditionsVerified = 6,
    kProdPhase_ChannelPrepared            = 7,
    kProdPhase_VmPrepared                 = 8,
    kProdPhase_SubmissionPrepared         = 9,
    kProdPhase_LiveBlocked                = 10,
    kProdPhase_Count                      = 11
};

enum GA106LabPhaseReject : uint32_t {
    kPhaseOk           = 0, // transição válida
    kPhaseSkip         = 1, // pulou estado (só +1 permitido)
    kPhaseReenter      = 2, // mesmo estado ou retrocesso fora de teardown
    kPhaseTerminal     = 3, // LiveBlocked não sai sem re-init
    kPhaseLiveCeiling  = 4, // nada existe além de SubmissionPrepared no P80
};

// Avanço normal: exatamente +1. LiveBlocked: de qualquer estado >= Attached
// quando um hardware-block dispara (fail-closed com latch). Teardown: qualquer
// estado -> Attached (dreno determinístico; ver TeardownTargetOk).
inline GA106LabPhaseReject ProductionPhaseCanAdvance(uint32_t from, uint32_t to) {
    if (from >= kProdPhase_Count || to >= kProdPhase_Count) return kPhaseSkip;
    if (from == kProdPhase_LiveBlocked) return kPhaseTerminal;
    if (to == kProdPhase_LiveBlocked) {
        return (from >= kProdPhase_Attached) ? kPhaseOk : kPhaseReenter;
    }
#ifdef PROD_MUT_ALLOW_SKIP
    if (to == from + 2u) return kPhaseOk; // MUTATION TEST-ONLY
#endif
    if (to == from + 1u) return kPhaseOk;
    if (to == from) return kPhaseReenter;
    if (to == kProdPhase_Attached) return kPhaseOk; // teardown (ver TeardownTargetOk)
    if (to < from) return kPhaseReenter;
    return kPhaseSkip;
}

// Teardown determinístico: qualquer estado (exceto Detached/LiveBlocked) -> Attached.
// Detached não tem o que drenar; LiveBlocked exige re-init (fail-closed conservador).
inline bool ProductionPhaseTeardownOk(uint32_t from) {
    return from > kProdPhase_Detached && from < kProdPhase_LiveBlocked;
}

// Teto live: nenhuma transição de produção sai de SubmissionPrepared no P80.
inline bool ProductionPhaseIsCeiling(uint32_t phase) {
    return phase == kProdPhase_SubmissionPrepared;
}

#endif /* GA106LAB_PRODUCTION_PHASE_H */
