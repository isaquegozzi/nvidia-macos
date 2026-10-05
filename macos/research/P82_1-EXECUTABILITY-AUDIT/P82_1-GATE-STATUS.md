# P82.1 — Gate Status (P8 / P81.2 / P9 / P3 — auditoria para C e rotas futuras)

```text
SESSION_ID: P82-1-AUDIT-RELAY-V3
MILESTONE: P82_1_EXECUTABILITY_AUDIT_C (OFFLINE ONLY)
P82_LIVE_EXECUTION_AUTHORIZED = NO (live=0)
```

Nenhum gate é removido silenciosamente. Status herdados de P81/P82-design
(D-minus = ABORT-at-preconditions; D NOT_RUN) e re-auditados aqui contra C
e contra ROUTE_1/ROUTE_2 do VERDICT. Q10 = BLOCKED mantido (ver P82-Q10).

## P8 — Option 0 sem VM (dress-rehearsal do pipeline em non-target)

- Estado: VERMELHO (HUMAN_BLOCK). D-minus abortou aqui; sem non-target real com
  fidelidade publicada G-M0244-13 + segunda conferência, D NÃO abre
  (`P81-CANDIDATE-OPTIONS.md:8-26`; gate duro de D, herdado por C via P-D1).
- `REQUIRED_FOR_C = YES` (P-D1: Option 0 PASS publicado é pré-condição dura de C).
- `CAN_BE_REPLACED_BY_APPROVED_EQUIVALENT = NO` — o harness P81.2 NÃO é
  equivalente (ver P81.2 abaixo). Única alternativa revisável: ensaio em
  non-target físico com fidelidade G-M0244-13 publicada + bounds R6 medidos +
  segunda conferência — isto É o Option 0, não um substituto.
- `BLOCKING = YES` (para C, ROUTE_1 e ROUTE_2 — todas exigem pipeline provado).

## P81.2 — harness pipeline-only (offline rehearsal)

- Estado: VERDE como pipeline, EXPLICITAMENTE NÃO-evidência live
  (`HARNESS_IS_NOT_LIVE_EVIDENCE = YES`, `GPU_STATE = UNKNOWN`;
  S01–S20 20/20 MATCH, mutations 25/25 killed — `P81-BUILD-STATE.md` turno M0252).
- `REQUIRED_FOR_C = NO` (harness nunca foi gate de C; é ensaio do pipeline de
  evidência, não do selector).
- `CAN_BE_REPLACED_BY_APPROVED_EQUIVALENT = N/A` — atua em camada distinta
  (runbook order/bounds/SAVE/INDEX/CLOSE contra fixtures MOCK; "nenhum comando
  de host é executado"). Não substitui Option 0 (P8) nem auditoria P-D4:
  **pipeline PASS ≠ KEXT safety ≠ MMIO safety**.
- `BLOCKING = NO`, com proibição explícita de upgrade: nenhum PASS do harness
  pode ser citado como atenuante de risco de selector live. Se algum dia for
  proposto como substituto de P8, exige milestone próprio + review + prova de
  fidelidade — nunca extensão silenciosa.

## P9 — sem reviewer (reviewer + alternativa nomeados + SLA)

- Estado: VERMELHO. D-minus abortou aqui; P-D2 exige dois nomes pinados em
  F-P82-AUTH-03 (nenhum é o operador) + SLA 7 dias + assinatura; validade 1 uso.
- `REQUIRED_FOR_C = YES` (P-D2; sem reviewer a janela NÃO abre, sem improvisar).
- `CAN_BE_REPLACED_BY_APPROVED_EQUIVALENT = NO` — operador não revisa o próprio
  FAIL; review assíncrono posterior não substitui nomeação prévia (a nomeação é
  o que impede escopo-creep no dia). Alternativa revisável SOMENTE como forma:
  aceitar reviewer remoto + alternativa remota desde que ambos nomeados,
  alcançáveis na janela e com SLA pinado — a exigência (2 humanos ≠ operador)
  não é negociável.
- `BLOCKING = YES` (para C, ROUTE_1, ROUTE_2 e para o próprio D).

## P3 — sem câmera / watchdog (FOTO-0 + observação de hang em headless)

- Estado: VERMELHO (gap bloqueante). D-minus abortou aqui; P-D3 exige caminho
  de foto/evidência + watchdog/serial com ID pinado em F-P82-AUTH-06, capaz de
  observar hang sem depender do alvo; bound userspace 60 s NÃO basta
  (`P82-OPEN-QUESTIONS.md:5,14`; exceção recovery-only só na forma F-P82-REC).
- `REQUIRED_FOR_C = YES` — com agravante específica de C: polling de até 4 s /
  8000 loads com mapa BAR0 retido amplia a janela de hang observável vs um load
  único; sem watchdog externo, um hang durante C seria indistinguível e
  irrecuperável dentro do protocolo.
- `CAN_BE_REPLACED_BY_APPROVED_EQUIVALENT = PARCIAL` — o meio (câmera vs serial
  vs watchdog HW vs segundo dispositivo) é substituível desde que pinado e
  capaz de: (a) registrar tela/LEDs pré/pós com timestamp E02+EXIF, (b) detectar
  hang sem depender do alvo, (c) disparar recovery-only na forma F-P82-REC.
  O que NÃO é substituível: a existência do caminho. "Bound userspace + ssh no
  alvo" não conta (depende do alvo).
- `BLOCKING = YES` (para C e ROUTE_1; para ROUTE_2, Bloqueante com escopo maior:
  stage/load exige ainda rollback ensaiado).

## Resumo

| Gate | Required for C | Replaceable | Blocking |
|---|---|---|---|
| P8 (Option 0) | YES | NO | YES |
| P81.2 (harness) | NO | N/A (camada distinta; sem upgrade silencioso) | NO |
| P9 (reviewer) | YES | NO (só a forma remota, não a exigência) | YES |
| P3 (foto/watchdog) | YES (agravado p/ C) | PARCIAL (meio, não existência) | YES |

Todos os gates de C aplicam-se igualmente a ROUTE_1; ROUTE_2 adiciona gates de
stage/load/rollback (milestone próprio). Nenhum gate foi removido, rebaixado ou
contornado neste turno.
