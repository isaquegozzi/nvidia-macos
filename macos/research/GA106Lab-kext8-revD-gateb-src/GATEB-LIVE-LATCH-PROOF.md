# GATEB-LIVE-LATCH-PROOF — one-way latch (R2, offline)

## Diagrama de estados

```text
NeverTouched --HI--> HIWritten --LO--> HIAndLOWritten
NeverTouched --stop/latch-set--> AlreadyProgrammed / RecoveryRequired (sem stores)
HIWritten    --stop--> UnknownPartial (+reboot)
*            --qualquer tentativa com latch != NeverTouched--> recusado, zero stores
```

## Ownership

Service-owned: `GA106Lab::fGateBLatch` (+ `fGateBBusy`), sob `fSysmemLock` em
seções curtas (mesma disciplina FIX-ASTRA-C). Inicializado em `init()` =
NeverTouched. Pin do UserClient (retain do owner) mantém o serviço vivo
durante a chamada; o latch sobrevive ao lifecycle necessário.

## Lifetime

- `stop()` / `free()` / `clientClose()` NUNCA limpam (verificado por leitura;
  auditoria de build conta exatamente 1 atribuição `fGateBLatch = `, no init).
- Somente reboot (novo objeto serviço) restabelece NeverTouched.
- `setLatch` só é chamado pelo flow: HIWritten pós-HI, HIAndLOWritten pós-LO,
  UnknownPartial em incerteza pós-store. Nenhum outro writer (grep).

## Transições proibidas (testadas)

- HIWritten/HIAndLOWritten/UnknownPartial -> NeverTouched: impossível no live
  path (mutações CLEAR_LATCH_* provam detecção).
- Segunda invocação pós-sucesso -> AlreadyProgrammed, zero stores (L4 + L8
  concorrente: exatamente-um-prossegue).
- Latch parcial + nova chamada -> RecoveryRequired.

## Testes que provam não haver reset por software

- L4 (latch preset), L5 (duplicate), L6 (close-after-success persiste), L8
  (concorrente), mutações CLEAR_LATCH_TEARDOWN/CLOSE + SECOND_INVOCATION:
  21/21 CAUGHT no harness.
