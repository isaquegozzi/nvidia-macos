# P67 GATE-B-HARDENED-SPEC — PROGRAM_SYSMEM_FLUSH_PAGE (futura, nao implementar)

## Semântica exata (determinística)

```text
GATE_B_NAME = PROGRAM_SYSMEM_FLUSH_PAGE
GATE_B_INPUT = ZERO
GATE_B_EXPECTED_MMIO_WRITES = 2
GATE_B_EXPECTED_MMIO_READS = 2
GATE_B_BME_CHANGE = NO
GATE_B_GPU_DMA = NO
GATE_B_FALCON_ACTION = NO
GATE_B_FIRMWARE_ACTION = NO
```

Sequência:

1. pin/reserve owner/provider (lições 1.6.2, sem janela load→retain)
2. revalidar invariantes equivalentes a Gate A internamente (GFW, MSE, BME==OFF, BAR0, pagina/mapping/segmento/alinhamento/representabilidade/roundtrip, stop nao pendente)
3. usar dedicated coherent mapping já owned (nunca alocar entre HI e LO)
4. derivar IOVA internamente (nunca userspace)
5. computar HI/LO internamente + roundtrip
6. entrar em COMMIT
7. write HI @0x100c40 exatamente 1×
8. write LO @0x100c10 exatamente 1× (back-to-back, sem sleep/alloc)
9. read HI 1×, read LO 1×, recompor `(LO<<8)|(HI<<40)`, comparar com IOVA
10. retornar ABI semantica; mismatch → FAIL (sem mais writes)
11. manter pagina/mapping alive (`CAN_RELEASE_AFTER_GATE_B=NO`); teardown MMIO = 0

Userspace fornece ZERO: offset, value, address, BDF, BAR, width, IOVA, timeout.

## BME/DMA/Falcon/firmware

- `FIRST_WRITE_WITH_BME_OFF=YES`; BME permanece OFF; Gate B nunca liga BME.
- 0 Falcon reset, 0 TRANSCFG/DMATRFBASE/BASE1/MOFFS/FBOFFS/CMD, 0 boot, 0 mailbox, 0 doorbell, 0 IRQ, 0 firmware load.
- Auditoria: `write_reg`==2 fixos; ausência de `setBusLeadEnable`, `dma_load`, `Falcon::`, `gsp.boot`, doorbell.

## ABI (endurecida, 48B)

Campos semânticos apenas: abiVersion, abiSize, status, phase(=B), gfwReady, mseEnabled, bmeEnabled, pageReady, segmentCount, alignmentReady, encodingReady, programmed(bool), readbackMatch(bool), writeCount(=2), readCount(=2), reserved zero. Nada de IOVA/HI/LO/addrs/ponteiros/físico. Tamanho/layout exato a confirmar no prompt de implementação; até lá, travar como spec.

## Pós-condições

`read()==IOVA`; só 0x100c10/0x100c40 alterados (diff 0x100c00–0x100c80 + WPR2 + Command); sem espera de firmware; sem retry.
