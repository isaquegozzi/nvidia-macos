# P81.2 PASS / FAIL / ABORT (fail-closed)

```text
HARNESS_IS_NOT_LIVE_EVIDENCE = YES
GPU_STATE = UNKNOWN
LIVE_EXECUTION_AUTHORIZED = NO
```

- **PASS**: SOMENTE nominal S01 (ordem+bounds+SAVE+INDEX+hashes+allowlist+clock+
  espaço+CLOSE assinado, sem abort/panic/segredo/fragmento).
- **FAIL**: artefato required faltante; INDEX malformado/duplicado; hash
  errado/truncado/modificado/ausente; IDENT-2 fora allowlist; clock gate
  inválido; destino < 5GB; SAVE failure; CLOSE sem assinatura/veredicto;
  allowlist stale; segredo detectado (redigido).
- **FAIL-3**: linha inesperada no inventário de drivers (KEXT diff) — subcódigo
  de FAIL, conta como FAIL no `expected/`.
- **ABORT**: forbidden action; timeout persistente (>60s) ou retry esgotado (1);
  panic / no-signal / no-video simulados; operator abort.
- **BLOCKED**: veredicto parcial/fragmentado do extrator (S19) — nunca APPROVE.
- **NUNCA**: PASS parcial, FAIL→PASS, ABORT→PASS, BLOCKED→PASS no mesmo run.
- **Mutations**: `MUTATION_KILL_RATE = 100%` nas críticas, senão `P81_2 = REJECT`.

## V2 (fix F1-F6)

- **FAIL tambem**: `fail:ground-truth-mismatch` (tamper+reindex); `fail:empty-inventory`;
  `fail:missing-field-timeout|clock|space`; `fail:close-shape-mismatch` (prefixo/sufixo);
  segredos ghp/gho/Bearer/password/aws_secret/secret-generico (redigido).
- **ABORT tambem**: forbidden normalizado — exact, UPPER/case, com-args,
  path-qualified, whitespace-padded, multi-action (`abort:forbidden-action`).
- **CLOSE**: exact-shape `build_close_text(scenario,verdict)`; qualquer byte extra
  = FAIL; declarado non-PASS propaga (`close-declared-nonpass:*`).
- **Mutations**: M1-M25 criticas, kill 100% senao `P81_2 = REJECT`.
