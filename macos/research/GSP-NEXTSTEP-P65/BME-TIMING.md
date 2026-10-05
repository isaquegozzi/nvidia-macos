# P65 BME TIMING

## Respostas

```text
BME_REQUIRED_BEFORE_CPU_MMIO_WRITE = NO
BME_REQUIRED_BEFORE_FIRST_GPU_DMA = YES
BME_ENABLE_POINT_IN_SEQUENCE = no probe PCI, antes de GFW wait / flush / qualquer Falcon reset ou FBDMA (Nova: driver.rs probe faz enable_device_mem + set_master antes de Gpu::new); no macOS equivale a setBusLeadEnable(true) com MSE ja ON. BME permanece ON durante todo o boot GSP.
```

## Prova primaria

- `nova-core-driver.rs:probe`: `pdev.enable_device_mem()?; pdev.set_master();` antes de
  `Gpu::new(pdev, bar)`. `enable_device_mem` = MSE/BAR (MMIO CPU); `set_master` = BME (bus mastering
  p/ DMA iniciado pelo device). Ordem prova que BME nao e pos-GFW nem pos-flush no Nova — e
  pre-requisito de probe.
- `nova-core-gpu.rs:Gpu::new`: `dma_set_mask_and_coherent()` antes de `wait_gfw_boot_completion`,
  confirmando que a capacidade DMA e configurada antes mesmo de GFW_READY.
- `nova-core-falcon.rs:dma_load/dma_wr`: programa `FBIF_TRANSCFG[target=CoherentSysmem,mem_type=Physical]`
  + `DMATRFBASE/BASE1/MOFFS/FBOFFS/CMD` e da poll `DMATRFCMD idle` 2s. Este e o primeiro DMA iniciado
  pela GPU (FBDMA lendo a sysmem). Sem BME, este DMA nao completa. Nenhum comentario no Nova sugere
  que CPU MMIO store precise de BME.
- Estado live atual: Command `0003` (MSE ON, BME OFF). CPU MMIO reads (BOOT0/BOOT42/GFW) ja passam;
  logo BME=OFF nao bloqueia CPU MMIO. O inverso (GPU DMA com BME OFF) e que e invalido.

## Distincao pedida

- CPU MMIO store (ex. flush HI/LO, MAILBOX, BOOTVEC): requer MSE/BAR0 mapeado; BME irrelevante.
- GPU-initiated DMA / Falcon FBDMA / GSP DMA (FWSEC load, Booter load, Radix3 via WPR meta, libos,
  cmdq): requer BME ON + dma mask + pagina/DMA mapping valido + flush page registrada.
- No macOS: `setBusLeadEnable(true)` (IOPCIDevice.h:1095; `setBusMasterEnable` :574 deprecated).
  Esperado Command antes `0003` -> depois `0007` (MSE permanece, BME acende; IOspace continua 0).
  Rollback: `setBusLeadEnable(false)` retorna a `0003`. Tudo ainda PROIBIDO neste prompt (sem live).

## Evidencia parcial vs total

- Total para "BME antes do primeiro GPU DMA" (prova por construcao + semantica PCI).
- Total para "BME desnecessario p/ CPU MMIO" (prova por live atual MSE-only + semantica PCI).
- O ponto exato "probe vs pos-GFW" no macOS e adaptacao (no Nova e probe); marcar como INFERENCIA
  de port, nao como numero magico: trazer BME para ON antes do primeiro FBDMA, nunca depois.
