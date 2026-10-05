# README-P68 — TG-KEXT7 SYSMEM FLUSH PREWRITE GATE A (1.7.0, offline only)

Candidate: `GA106Lab-kext7-sysmem-flush-prewrite-src/`
Version 1.7.0, revision `TG-KEXT7-SYSMEM-FLUSH-PREWRITE-READINESS`.
Baseline 1.6.2 intacta (nao modificada); esta arvore e copia + selector9.

Escopo P68: implementar SOMENTE Gate A read-only
(`selector9 = VERIFY_SYSMEM_FLUSH_PRECONDITIONS`), offline, sem staging/live,
sem Gate B. MMIO writes = 0, PCI writes = 0, BME = 0, DMA trigger = 0.

Documentos: SELECTOR9-ABI.md, GATE-A-IMPLEMENTATION.md, ZERO-WRITE-PROOF.md,
LIFETIME.md, OFFLINE-TEST-REPORT.md, REPRODUCIBILITY.md, FUTURE-ASTRA-AUDIT.md.
