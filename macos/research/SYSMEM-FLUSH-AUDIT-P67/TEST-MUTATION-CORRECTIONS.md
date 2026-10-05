# P67 TEST-MUTATION-CORRECTIONS — significados corrigidos (offline, sem executar)

## Testes (harness futuro deve PROVAR)

- exact-count: Gate B = 2 stores fixos (HI@0x100c40, LO@0x100c10) + 2 loads fixos (HI,LO); qualquer desvio FAIL.
- order-fidelity: HI antes de LO; reverso FAIL como fidelity, não como prova HW.
- LO-only / HI-only / wrong shift-mask (`>>4/12/32`, máscara errada, `as u16`) → roundtrip/count FAIL.
- extra MMIO / PCI write / BME enable / DMA trigger (TRANSCFG/DMATRF/CMD/doorbell) / Falcon reset / firmware action → grep estático + contadores mockados FAIL.
- userspace address/value: fuzz de input não altera HI/LO (derivados internos) senão FAIL.
- IOVA exposure: scan ABI/logs sem IOVA/HI/LO/addrs/ponteiros senão FAIL.
- selector7 internal call: stub registra chamada → FAIL se chamado.
- write-after-stop: stop→drain; qualquer store após stop retornar FAIL.
- mapping released too early: release/complete antes de teardown dedicado FAIL.
- rollback-to-zero: qualquer 3º/4º store de zero/restauração em Gate B FAIL (só 2+2 autorizados).

```text
TEST_PLAN_HARDENED = YES
```

## Mutações (devem ser KILLED)

M1 reversed, M2 LO-only, M3 HI-only, M4 wrong shifts, M5 IOVA exposure, M6 arbitrary API, M7 extra store, M8 PCI write, M9 BME, M10 DMA/doorbell, M11 Falcon/firmware, M12 selector7 call, M13 readback forjado, M14 retry loop, M15 early-release mapping, M16 rollback-to-zero (must FAIL — Gate B permite exatamente 2 stores, nenhum unregister automático).

Requisitos: cada mutante compila e altera comportamento observável (evitar falso-positivos 1.6.1); relatório kill/survived por ID, sem esconder sobreviventes.

```text
MUTATION_PLAN_HARDENED = YES
```

## ABI

```text
ABI_HARDENED = YES (spec travada: 48B semantica; layout exato a confirmar no prompt de implementacao)
Campos: abiVersion, abiSize, status, phase, gfwReady, mseEnabled, bmeEnabled, pageReady, segmentCount, alignmentReady, encodingReady, programmed, readbackMatch, writeCount(=2), readCount(=2), reserved zero. Nada de IOVA/HI/LO/addrs/ponteiros/fisico.
```
