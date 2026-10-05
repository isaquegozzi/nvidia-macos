# P81.2 Limitations (leia antes de citar este harness)

```text
HARNESS_IS_NOT_LIVE_EVIDENCE = YES
GPU_STATE = UNKNOWN
LIVE_EXECUTION_AUTHORIZED = NO
```

1. **Não substitui evidência live.** Este harness valida SOMENTE o pipeline
   (runbook/ordem/bounds/SAVE/INDEX/hashes/CLOSE/FAIL/ABORT/recovery) contra
   fixtures sintéticas MOCK. Ele NÃO prova macOS live, RTX/GA106Lab state,
   KEXT safety, panic-rate, MMIO/BME/DMA/GSP/VRAM/command/Metal.
2. **Option 0 non-target real ainda pendente de humano** (HUMAN_BLOCK: sem VM
   macOS nem máquina non-target neste milestone). Nada aqui autoriza execução.
3. Fixtures marcadas MOCK; sem serial/UUID/MAC real, sem dump do target.
4. Clock é fixture sintética do método G-M0244-01-R1 (parsing/gates apenas).
5. Foto é metadata EXIF-only sintética.
6. `live=0` em todos os artefatos gerados. Qualquer citação fora do rehearsal
   de pipeline é inválida.

## Garantias P81.2-V2 (fix Smoke P81-2-SMOKE-01, F1-F6)

```text
HARNESS_IS_NOT_LIVE_EVIDENCE = YES
```

- F1: digests nominais pinados a `GROUND_TRUTH_DIGESTS` (run_rehearsal.py);
  tamper+reindex auto-consistente agora FAIL (`fail:ground-truth-mismatch`).
- F2: inventario vazio FAIL (`fail:empty-inventory`, exige `len>=1`).
- F3: forbidden normalizado (lower/strip/collapse/substring/path/args,
  multi-action) via `is_forbidden_action`; variantes SUDO/args/path/case/
  whitespace/multi-action ABORT.
- F4: redacao expandida (FAKE_SECRET/AKIA/PRIVATE KEY/xox + ghp-ghu-ghs-ghr/
  Bearer/password generico/aws_secret_access_key/secret generico deny-unknown).
- F5: CLOSE exact-shape por `build_close_text(scenario,verdict)` (igualdade
  total => hash-pin implicito); prefixo/sufixo FAIL (`fail:close-shape-mismatch`).
- F6: presenca explicita exigida de `timeout_s`/`retries_used`/`clock_skew_s`/
  `clock_jumps`/`space_gb`; ausente = FAIL (`fail:missing-field-*`), sem default-safe.
- Mutations M10-M25 criticas, `MUTATION_KILL_RATE = 100%` senao `P81_2 = REJECT`.
- Regressoes em `tests/test_regressions_f1_f6.py` (F1-F6).

## Residuais restantes (nao-mitigiados)

- `forbidden-catalog.txt` sem hash-pin proprio; edicao da fixture redefine S17/M7.
- `MOCK-SIGNATURE-FULL` estatica, replayavel; CLOSE e exato mas nao criptografico.
- Redacao e por lista (deny-unknown parcial via `secret=` generico); shapes novos
  exigem revisao.
- BLOCKED-vs-FAIL: fragmento checado antes de order/required/hashes (mascara causa).
- Fixture clock/space/IDENT sem vinculo com host real; citacao live = contaminacao.
- `HARNESS_IS_NOT_LIVE_EVIDENCE = YES` permanente; nada aqui e evidencia live.
