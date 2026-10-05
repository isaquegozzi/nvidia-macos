# P67 HILO-COMMIT-FAILURE-MODEL — atomicidade honesta + falha pós-commit

## Ordem e partial

```text
HILO_HARDWARE_ORDER_REQUIREMENT = NOT_PROVEN
HILO_IMPLEMENTATION_POLICY = HI then LO (fidelidade a write_sysmem_flush_page_ga100)
REVERSE_ORDER_MUTATION_MEANING = fidelity test
PARTIAL_HILO_SEMANTICS = UNKNOWN
CAN_STOP_ABORT_BETWEEN_HI_LO = NO
```

Upstream não documenta latch em LO; não inventar. Preservar HI→LO sem alegar invalidez HW do reverso.

## Commit

PREPARE (abortável, sem writes) → COMMIT (HI 1×, LO 1×, HI read, LO read; não abortável no meio, sem alloc/sleep/retry/restore) → CLEANUP (reter página, ABI semântica). `stop()` fora de COMMIT rejeita novas + drena; dentro de COMMIT aguarda stores/loads curtíssimos (sem lock bloqueante em MMIO), depois drena. Nunca write-after-stop.

## Falha pós-commit

```text
POST_COMMIT_FAILURE_POLICY = resultado semantico FAIL, sem mais writes; manter pagina pinned; nao prosseguir para BME/DMA/firmware; exigir review + reboot para known-good para nova tentativa
PARTIAL_WRITE_RESIDUAL_RISK = par HI-LO nao e atomico em HW por software comum; excecao CPU/kernel entre HI e LO deixaria estado misto (HI novo + LO velho) com semantica UNKNOWN; mitigado por prevalidacao total + back-to-back + sem janela; residual permanece e deve ser declarado, nao escondido; unica recuperacao confiavel e reboot para known-good (sem restore cego)
```

Readback mismatch (HI+LO OK mas read!=IOVA) → FAIL EIO, sem reescrita, sem retry. Exceção entre HI e LO → mesmo caminho + log de estado misto, sem tentativa de completar/restaurar após exceção fora do COMMIT.
