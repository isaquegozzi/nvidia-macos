# P66 HILO-COMMIT-ATOMICITY — partial write, commit point, rollback (§§16–19 + §4–§5 detalhe)

## 1. Write order (§4)

```text
FLUSH_WRITE_ORDER = HI then LO
WRITE_ORDER_STRENGTH = IMPLEMENTATION_ORDERING (não provado como mandatory)
```

- Upstream (`write_sysmem_flush_page_ga100`) escreve HI primeiro, LO depois. Nenhum comentário diz "must", "latch", "commit on LO".
- Read compõe `(LO<<8)|(HI<<40)` — ordem de leitura irrelevante para valor final.
- NÃO afirmar que "LO latches/commits o endereço" sem fonte. Não inventar latch semantics.
- Estratégia: preservar ordem HI→LO exatamente como upstream (menor desvio), mas documentar como `IMPLEMENTATION_ORDERING` até prova contrária. Mutação futura deve detectar reversão (ver MUTATION-PLAN), mas sem alegar que reversão quebra HW com certeza — apenas que desvia de upstream provado.

## 2. Readback (§5)

```text
FLUSH_READBACK_SUPPORTED = YES
READBACK_SEMANTICS = register-programming confirmation (read HI+LO, recompor, comparar com IOVA)
READBACK_COMPARISON_SAFE = YES para confirmar programação; NO para provar que sysmembar funciona
```

- Suporte provado por `read_sysmem_flush_page_ga100` + uso no `Drop` (`if read()==dma { unregister }`).
- Após HI+LO, `read HI; read LO` é válido e útil; mismatch → FAIL (EIO).
- Distinguir: readback==dma prova que os 2 stores aterrissaram; NÃO prova que sysmembar drena writes GPU→sysmem (isso só seria provado por handshake Falcon futuro, fora de escopo).

## 3. Partial HI-LO (§17)

```text
PARTIAL_HILO_SEMANTICS = UNKNOWN (upstream não documenta estado com só HI novo + LO velho)
```

Estratégia preventiva (sem esconder `stopping` para sempre):
- Prevalidar TUDO antes do primeiro store (Gate A + recheck imediato).
- Dois stores back-to-back, sem sleep/wait/schedule point entre HI e LO.
- Sem janela de cancelamento entre HI e LO (mas sem lock bloqueante em torno de MMIO; ver §4).
- Se `stop()` chegar entre HI e LO: não abortar no meio do par a menos que fonte/HW exija; completar o par (nanossegundos) e então drenar. Justificativa: deixar registrador em estado meio-novo/meio-velho é pior que completar par já validado. `stop()` espera o drain do COMMIT crítico, mas COMMIT é minúsculo (2 stores + opcional 2 loads), sem espera longa.

## 4. Commit point (§18)

```text
COMMIT_POINT_MODEL = PREPARE (sem writes) → COMMIT (HI, LO, [readback]) → CLEANUP (reter página, retornar semântica)
```

- PREPARE: validação, alloc, mapping, encoding, roundtrip, retain provider, reserve operação. Abortável livremente (só cleanup de SW, sem MMIO).
- COMMIT: inicia no store de HI. A partir daqui, `do not abort between the two stores unless hardware/source requires it`. Sem alocação, sem falha esperada (tudo prevalidado).
- CLEANUP: manter página pinned, preencher ABI semântica, release temporários (não a página), permitir stop/close drenar.
- `stop()` fora de COMMIT: rejeita nova operação, espera ativas drenarem. `stop()` dentro de COMMIT: aguarda os poucos stores/loads terminarem (spin curtíssimo, sem sleep), depois impede novas. Nunca `write after stop returned`.

## 5. Rollback (§19)

```text
ROLLBACK_MMIO_WRITE = NOT_AUTHORIZED
PARTIAL_PROGRAMMING_POLICY = em falha/stop após HI mas antes de LO (ou readback mismatch): NÃO restaurar valores antigos com write cego; marcar FAIL, não prosseguir, preservar página, exigir reboot para known-good driver/config para qualquer nova tentativa
```

- Nova `Drop` só reescreve `(0,0)` se `read()==minha_pagina`; se outro valor assumiu, apenas warn. Copiar isso: nunca "desfazer" com HI/LO velhos sem prova de que reprogramação durante bring-up é segura.
- Rollback futuro = `do not proceed + reboot to known-good`, não sequência de MMIO corretivo.
- Sem prova de que flush pode ser reprogramado com segurança durante bring-up → qualquer re-write além do par inicial é novo gate, não rollback implícito.

## 6. Stop/lifetime model (resumo §15)

- Pin atômico do owner sob trava compartilhada (lição 1.6.2), retain provider, own map/mapping/descriptor/command.
- `close()` só marca closing; `free` após drain. `stop()` espera drain completo.
- Adversariais exigidos antes de build/live (ver OFFLINE-TEST-PLAN): stop-before-commit, stop-at-commit, close-during-commit.
