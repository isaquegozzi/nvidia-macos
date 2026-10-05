> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# P80 — Hardware Blocks (fail-closed determinístico)

Author: Muse, 2026-09-09 -0300. Reflete `GA106LabHardwareBlocks.h` (1.8.0, offline).

## Os 13 blocos (§14)

Cada stub: incrementa seu contador, retorna `kIOReturnNotPermitted`, zero side effect.

```text
0 MMIO write | 1 PCI config write | 2 BME enable | 3 DMA real | 4 GSP live
5 firmware upload | 6 VRAM write | 7 interrupt enable | 8 reset
9 real GPFIFO kick | 10 real PUT write | 11 real doorbell | 12 real command submission
```

`blockRealOperation(i)` no serviço: despacha o stub + latch `LiveBlocked`
(fase ≥ Attached). Índice fora de 0..12 → `BadArgument` sem latch.

## Garantias negativas (prova por auditoria + teste)

- Source: `GA106LabHardwareBlocks.h` + phase/contracts headers contêm zero símbolos
  de HW (`HARDWARE_BLOCKS_ZERO_HW_SOURCE` no build).
- Gate B: produção + camada P80 sem stores/BME/DMA/Falcon (`GATE_B_SOURCE_PATH = NO`;
  stubs `GA106LabBlock*` isentos nominalmente — são a barreira, não o caminho).
- Binário: `nm -u` sem `OSWrite`/`configWrite`/`setBusLeadEnable|setBusMasterEnable`
  (`BINARY_ZERO_STORES_PCI_BME = YES`); filetype kext bundle x86_64 + `_kmod_info`.
- Runtime: `test-production-blocks` (3 grupos) — 13/13 NotPermitted, latch por
  contador, NULL-safe.

## Por que latch em vez de só retornar erro

Um erro retornado pode ser ignorado pelo chamador; o latch de fase torna a
tentativa observável (`blockLog()` + `productionPhase()`) e trava o caminho de
produção até re-init — fail-closed em duas camadas.
