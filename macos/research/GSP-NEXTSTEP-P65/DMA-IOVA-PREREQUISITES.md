# P65 DMA/IOVA PREREQUISITES (selector7 suficiente? NAO)

Foundation validada (selector7): 1 pagina sysmem 4096 alinhada, 1 segmento, IOVA oculta de
userspace, `MAPPING_LEFT_PREPARED`, sem GPU DMA (`DMA_ENDPOINT_VALIDATED=NO`, `GPU_DMA_EXECUTED=NO`).

## Resposta

```text
CURRENT_SELECTOR7_MAPPING_SUFFICIENT_FOR_FIRST_GSP_DMA = NO
MISSING_DMA_PREREQUISITES =
  - BME ON (setBusLeadEnable) + dma mask/coerente configurados (dma_set_mask_and_coherent)
  - Sysmem flush page registrada (HI+LO) e valida durante todo o boot (Drop so no unload)
  - Multiplas alocacoes DMA coerentes vitalicias, nao 1 pagina generica:
    GSP firmware Radix3 (fw SGTable ToDevice + L2/L1 SGTables + L0 Coherent[4K/u64] com L0[0]=L1 DMA addr LE),
    signatures Coherent, bootloader RiscvFirmware ucode Coherent (+code/data/manifest offsets),
    WPR meta Coherent (GspFwWprMeta com magic/revision + 3 DMA addrs + ranges),
    libos Coherent<[LibosMemoryRegionInitArgument]> (PA=dma_address, CONTIGUOUS/SYSMEM),
    cmdq TX/RX queues (MSGQ pages + sharedMemPhysAddr), flush page PAGE_SIZE
  - Direcao DMA ToDevice (fw/tabelas) + Coherent; enderecos via dma_address() (nunca userspace)
  - Restricoes: alinhamento 256B p/ blocos Falcon (MEM_BLOCK_ALIGNMENT, Booter DMA 256),
    PAGE_SIZE p/ flush, 128K/1M p/ ranges FB (FRTS 128K-align, WPR2/heap 1M-align); descritores
    FBDMA em blocos de 256B (Size256B), poll idle 2s por bloco
  - Largura: LO=(addr>>8) (39:8), HI=(addr>>40) (63:40); ate 64-bit (HI obrigatorio em GA106)
  - Coerencia: FBIF_TRANSCFG[0] = {CoherentSysmem, Physical} antes de cada dma_load; dma_reset
    (FBIF_CTL.allow_phys_no_ctx + DMACTL=0) antes
  - Destino firmware: IMEM secure + DMEM (dma_wr por target_mem), BOOTVEC=brom/boot_addr,
    program_brom() antes de start
  - Flush/sysmembar ativo + WPR2/FRTS/FbRanges calculados + VBIOS presente + mailboxes/sequencer
```

## Por que selector7 nao basta (prova)

- `firmware/gsp.rs:GspFirmware::new`: exige `request_tlv(gsp)` + `FILE` => `nvidia/<chip>/gsp/<file>`
  + `SIGN` + 3 niveis de page table + bootloader `gsp_bootloader` + `RiscvFirmware::new`. Uma pagina
  de 4096 nao comporta Radix3 ELF + tabelas + signatures + bootloader.
- `gsp/fw.rs:GspFwWprMeta::from_ranges`: consome `radix3_dma_address() + bootloader dma + signatures dma`
  + 10+ ranges (non_wpr_heap, wpr2, wpr2_heap, elf, boot, frts, vga, pmu...). Sem estes, Booter nao tem
  o que carregar.
- `gsp/hal/tu102.rs:boot`: cria `wpr_meta` Coherent + `FbRanges::new` + `Vbios::new` + libos (de `Gsp::new`)
  + cmdq — tudo DMA-mapped antes do primeiro FBDMA.
- `falcon.rs:dma_load`: aloca `CoherentBox` temporario do tamanho do fw arredondado p/ 256B, copia fw,
  `dma_reset + TRANSCFG(CoherentSysmem/Physical)`, depois `dma_wr(IMEM_SEC)+dma_wr(DMEM)` em loop
  256B com poll 2s. Requer TRANSCFG + BME + flush, nao apenas "uma pagina mapeada".
- Tamanho/direcao: `SGTable::new(dev, vvec, ToDevice)`; libos `CONTIGUOUS/SYSMEM`; cmdq usa
  `sharedMemPhysAddr`. Nada disto existe no selector7.

## O que falta exatamente (sem implementar)

1. Defesa de lifetime: todas as alocacoes vivas do register-flush ate unload (PinnedDrop); selector7
   deixa 1 mapping preparado sem dono claro p/ boot.
2. Descoberta de tamanhos: `LibosParams::from_chipset(GA106)=LIBOS3` + `wpr_heap_size(fb_size)` +
   `FbRanges` a partir de `usable_fb_size` + VGA workspace; sem FB size real nao ha WPR2.
3. Firmware staging real (TLV parse + signature patch via fuse version) — proibido baixar/executar aqui.
4. BME + flush + TRANSCFG + descritores FBDMA — primeiro DMA real (FWSEC) depende de todos.
