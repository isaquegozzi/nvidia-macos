# P81.2 Test Matrix (S01–S20 + mutations)

```text
HARNESS_IS_NOT_LIVE_EVIDENCE = YES
GPU_STATE = UNKNOWN
LIVE_EXECUTION_AUTHORIZED = NO
```

| ID | Inject | Expect | Asserts em |
|---|---|---|---|
| S01 | nominal | PASS | test_verdicts |
| S02 | remove EVENT-2 | FAIL missing-required | test_verdicts |
| S03 | INDEX sem header | FAIL malformed-index | test_verdicts |
| S04 | IDENT-1 pós-INDEX | FAIL hash-mismatch | test_verdicts |
| S05 | IDENT-2 fora allowlist | FAIL ident2-diff | test_verdicts |
| S06 | timeout 75s | ABORT timeout | test_verdicts |
| S07 | timeout 90s + retry=1 | ABORT retry-exhausted | test_verdicts |
| S08 | no-video | ABORT | test_verdicts |
| S09 | panic | ABORT | test_verdicts |
| S10 | no-signal | ABORT | test_verdicts |
| S11 | extra driver line | FAIL-3 | test_verdicts |
| S12 | skew 120s + jumps | FAIL clock-gate | test_verdicts |
| S13 | destino 1.2GB | FAIL space | test_verdicts |
| S14 | save_ok=False | FAIL save | test_verdicts |
| S15 | CLOSE unsigned | FAIL close-signature | test_verdicts |
| S16 | operator_abort | ABORT operator | test_verdicts |
| S17 | catalog entry as action | ABORT forbidden | test_verdicts |
| S18 | allowlist v0-stale | FAIL stale-allowlist | test_verdicts |
| S19 | fragment `VERDICT: APP` | BLOCKED | test_verdicts |
| S20 | fake secret | FAIL + redacted | test_verdicts |
| HASH | correct/truncated/modified/missing/duplicated | ok/FAIL | test_index_hash |
| M1–M9 | status flip, removal, truncation, driver inject, timeout, drop CLOSE, forbidden, dup/partial verdict | killed, rate 100% | test_mutations |
| GREP | forbidden/exec-sink scan de run_rehearsal.py | zero hits | test_prohibition_grep |

Critério: PASS só S01; `MUTATION_KILL_RATE = 100%` críticas; senão `P81_2 = REJECT`.

| M10 | tamper+reindex auto-consistente | FAIL ground-truth-mismatch | test_regressions_f1_f6 + test_mutations |
| M11 | inventario vazio | FAIL empty-inventory | test_regressions_f1_f6 + test_mutations |
| M12 | forbidden UPPER (SUDO) | ABORT forbidden | test_regressions_f1_f6 + test_mutations |
| M13 | forbidden com args | ABORT forbidden | test_regressions_f1_f6 + test_mutations |
| M14 | forbidden path-qualified | ABORT forbidden | test_regressions_f1_f6 + test_mutations |
| M15 | forbidden whitespace-padded | ABORT forbidden | test_regressions_f1_f6 + test_mutations |
| M16 | forbidden multi-action | ABORT forbidden | test_regressions_f1_f6 + test_mutations |
| M17 | segredo ghp_ | FAIL redacted | test_regressions_f1_f6 + test_mutations |
| M18 | segredo Bearer | FAIL redacted | test_regressions_f1_f6 + test_mutations |
| M19 | segredo password generico | FAIL redacted | test_regressions_f1_f6 + test_mutations |
| M20 | segredo aws_secret_access_key | FAIL redacted | test_regressions_f1_f6 + test_mutations |
| M21 | CLOSE prefix injection | FAIL shape-mismatch | test_regressions_f1_f6 + test_mutations |
| M22 | CLOSE suffix injection | FAIL shape-mismatch | test_regressions_f1_f6 + test_mutations |
| M23 | timeout ausente | FAIL missing-field | test_regressions_f1_f6 + test_mutations |
| M24 | clock ausente | FAIL missing-field | test_regressions_f1_f6 + test_mutations |
| M25 | space ausente | FAIL missing-field | test_regressions_f1_f6 + test_mutations |

V2: 42 testes verdes; `MUTATION_KILL_RATE = 100%` (25/25 criticas); senao `P81_2 = REJECT`.
