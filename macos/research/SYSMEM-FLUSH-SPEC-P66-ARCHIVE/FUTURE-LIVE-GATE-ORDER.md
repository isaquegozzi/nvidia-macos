# P66 FUTURE-LIVE-GATE-ORDER — ordem estrita (§23 + §24)

## 1. Ordem (§23)

```text
FUTURE_LIVE_GATE_ORDER =
  1. future selector8 GFW readiness live PASS
  2. separate authorization (humana, explícita, para Gate A)
  3. read-only PREWRITE GATE A (selector9 candidato VERIFY_SYSMEM_FLUSH_PRECONDITIONS)
  4. review results ( Gate A PASS + diffs + logs)
  5. Astra review (obrigatória; hoje indisponível)
  6. separate authorization (humana, explícita, para Gate B)
  7. FIRST-WRITE GATE B (PROGRAM_SYSMEM_FLUSH_PAGE, exatamente 2 stores + opcional 2 loads)
  8. stop (sem prosseguir para BME/DMA/firmware; preservar página pinned)
```

Proibido:
```text
selector8 + first write na mesma call
prewrite + write automáticos (sem autorização separada + review)
write retry
BME enable no mesmo gate
DMA no mesmo gate
Falcon reset / firmware / doorbell / interrupt no mesmo gate
```

## 2. Astra (§24)

```text
ASTRA_REVIEW_REQUIRED_BEFORE_IMPLEMENTATION = YES
IMPLEMENTATION_AUTHORIZED = NO
LIVE_WRITE_AUTHORIZED = NO
BME_ENABLE_AUTHORIZED = NO
DMA_AUTHORIZED = NO
GSP_AUTHORIZED = NO
```

Este P66 deixa pacote completo para futura auditoria Astra: 10 docs + prova reconfirmada + encoding + lifetime + Gates A/B + atomicidade + falhas + testes + mutações + ordem live. Nenhum código, nenhum live, nenhum write executado neste prompt.

## 3. Estado deste prompt

```text
PROJECT_SOURCE_CHANGED = NO
EFI_CHANGED = NO
REBOOT_EXECUTED = NO
LIVE_HARDWARE_INTERACTION = NO
NEXT_PROMPT_NUMBER = 67
```
