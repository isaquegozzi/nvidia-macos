//
// GA106LabModelContracts.h — P80 §7: contratos internos de produção que
// refletem os modelos aprovados P77/P78/P79, sem executar hardware.
// Kernel + host-test compatível: C++ puro, sem IOKit, sem alocação.
// Os simuladores (chsim/vm78/sub79) servem como ORACLE nos testes; aqui vive
// o contrato que a produção exige. Não copiar simulador cegamente: cada campo
// tem bound documentado com origem (P77/P78/P79).
//

#ifndef GA106LAB_MODEL_CONTRACTS_H
#define GA106LAB_MODEL_CONTRACTS_H

#include <stdint.h>

// P77: channel/FIFO (class 0xc56f, 2048 ch, runq<2, entries=len/8 pot.2).
struct GA106LabChannelContract {
    uint32_t chid;          // < 2048
    uint32_t runq;          // < 2
    uint32_t engineKnown;   // 0/1 (token validado; numérico opaco P77)
    uint32_t vasHandle;     // != 0
    uint32_t gpfifoLength;  // múltiplo de 8, > 0
    uint32_t scheduled;     // 0/1 (BIND+SCHEDULE completos)
};

inline bool GA106LabChannelContractValid(const GA106LabChannelContract * c) {
#ifdef PROD_MUT_CONTRACT_SKIP
    (void)c; return true; // MUTATION TEST-ONLY
#endif
    if (!c) return false;
    if (c->chid >= 2048u) return false;
    if (c->runq >= 2u) return false;
    if (c->engineKnown != 0u && c->engineKnown != 1u) return false;
    if (c->vasHandle == 0u) return false;
    if (c->gpfifoLength == 0u || (c->gpfifoLength % 8u) != 0u) return false;
    uint32_t entries = c->gpfifoLength / 8u;
    if ((entries & (entries - 1u)) != 0u) return false; // potência de 2 (ramfc limit2)
    if (c->scheduled != 0u && c->scheduled != 1u) return false;
    return true;
}

inline uint32_t GA106LabChannelContractEntries(const GA106LabChannelContract * c) {
    return c->gpfifoLength / 8u;
}

// P78: VM/MMU (VA 47 bits, shifts leaf, aperture PTE 0/2/3, flush token).
struct GA106LabVmContract {
    uint64_t va;        // < 2^47, alinhado a (1<<shift)
    uint64_t size;      // múltiplo de (1<<shift), > 0, sem wrap, < 2^47
    uint32_t shift;     // 12/16/21/29
    uint32_t aperture;  // 0 VRAM / 2 HOST / 3 NCOH (nunca 1)
    uint32_t flushed;   // 0/1 (token de flush pós-map)
};

inline bool GA106LabVmShiftLeaf(uint32_t shift) {
    return shift == 12u || shift == 16u || shift == 21u || shift == 29u;
}

inline bool GA106LabVmContractValid(const GA106LabVmContract * m) {
    if (!m) return false;
    if (!GA106LabVmShiftLeaf(m->shift)) return false;
    uint64_t ps = 1ULL << m->shift;
    if (m->size == 0u || (m->size & (ps - 1ULL)) != 0ULL) return false;
    if ((m->va & (ps - 1ULL)) != 0ULL) return false;
    const uint64_t kVaLimit = (1ULL << 47);
    if (m->va >= kVaLimit) return false;
    if (m->va + m->size < m->va) return false; // wrap
    if (m->va + m->size > kVaLimit) return false;
    if (m->aperture != 0u && m->aperture != 2u && m->aperture != 3u) return false;
    if (m->flushed != 0u && m->flushed != 1u) return false;
    return true;
}

// P79: submission (anel pot.2, PUT/GET, token, fence).
struct GA106LabSubmissionContract {
    uint32_t entries;   // pot.2, >= 2, == channel entries (cross-check)
    uint32_t put;       // < entries (índice)
    uint32_t get;       // < entries (índice)
    uint32_t tokenLive; // 0/1 (work-submit token obtido)
    uint32_t fenceLive; // 0/1 (há fence pendente)
};

inline bool GA106LabSubmissionContractValid(const GA106LabSubmissionContract * s) {
    if (!s) return false;
    if (s->entries < 2u || (s->entries & (s->entries - 1u)) != 0u) return false;
    if (s->put >= s->entries) return false;
    if (s->get >= s->entries) return false;
    if (s->tokenLive != 0u && s->tokenLive != 1u) return false;
    if (s->fenceLive != 0u && s->fenceLive != 1u) return false;
    return true;
}

// Cross-model (P77+P78+P79): anel == channel entries; PB dentro do mapping;
// submit só com channel scheduled + VM flushed + token vivo.
inline bool GA106LabCrossContractValid(const GA106LabChannelContract * c,
                                       const GA106LabVmContract * m,
                                       const GA106LabSubmissionContract * s,
                                       uint64_t pbBase, uint64_t pbLength) {
    if (!GA106LabChannelContractValid(c)) return false;
    if (!GA106LabVmContractValid(m)) return false;
    if (!GA106LabSubmissionContractValid(s)) return false;
    if (GA106LabChannelContractEntries(c) != s->entries) return false;
    if (c->scheduled != 1u) return false;
    if (m->flushed != 1u) return false;
    if (s->tokenLive != 1u) return false;
    if (pbLength == 0u) return false;
    if (pbBase + pbLength < pbBase) return false; // wrap
    if (pbBase < m->va || pbBase + pbLength > m->va + m->size) return false;
    return true;
}

#endif /* GA106LAB_MODEL_CONTRACTS_H */
