# P65 FALCON / FBDMA — primeiro DMA da GPU

## Respostas

```text
FIRST_GPU_DMA_ENGINE = Falcon FBDMA (Framebuffer DMA engine por-falcon; em GA106 via Ga102 HAL, LoadMethod::Dma)
FIRST_GPU_DMA_PURPOSE = carregar FWSEC-FRTS (do VBIOS) no falcon GSP (IMEM secure + DMEM) para criar WPR2; em seguida, carregar Booter_load no SEC2 (IMEM+DMEM) para carregar GSP-RM em WPR2
FIRST_GPU_DMA_COMPLETION_SIGNAL = poll NV_PFALCON_FALCON_DMATRFCMD.idle por bloco 256B (Delta 2s) + wait_till_halted (CPUCTL.halted, 2s) + mailbox0==0 + (FWSEC) FRTS_ERR==0 e wpr2_range criado; (Booter) SEC2 mbox0==0
```

## Quem inicia / fonte / destino / registros

- Quem inicia: CPU programa, GPU executa (FBDMA le sysmem e escreve IMEM/DMEM). Fonte = `CoherentBox`
  temporario com imagem fw (FWSEC ou Booter) em sysmem; destino = `FalconMem::ImemSecure` + `Dmem`
  (e `ImemNonSecure` so em Turing/GA100, nao GA106).
- Registros/descritores (`falcon.rs:dma_wr/dma_load`):
  - `dma_reset()`: `FBIF_CTL.allow_phys_no_ctx=true` + `DMACTL=0`.
  - `FBIF_TRANSCFG[0] = {target=CoherentSysmem, mem_type=Physical}` (GA106).
  - `DMATRFBASE = (dma_addr>>8) as u32` (bits altos vao p/ BASE1), `DMATRFBASE1 = dma_addr>>40`,
    loop por `pos`: `DMATRFMOFFS = dst_start+pos`, `DMATRFFBOFFS = src_start+pos`,
    `DMATRFCMD = {size=Size256B(0x6), falcon_mem=target}`; cada iteracao transfere 256B.
  - Depois: `program_brom(brom_params)` + `BOOTVEC = boot_addr` (GA106: sec load src_start ou ns).
- BME: SIM, ja requerido (probe set_master; sem BME o FBDMA nao le sysmem).
- Condicao de conclusao: `read_poll_timeout(DMATRFCMD, idle, 0, 2s)` por bloco; `wait_till_halted` 2s;
  `boot()` retorna `(mbox0,mbox1)`; FWSEC exige ainda `PBUS_SW_SCRATCH_0E.frts_err_code()==0` e
  `wpr2_range().start == frts.start`; Booter exige `mbox0==0` senao ENODEV.
- Falhas: timeout poll (ETIMEDOUT), halt nao atingido, mbox!=0, FRTS_ERR!=0, WPR2 ausente ou em
  endereco inesperado, fuse/signature mismatch (EINVAL/ERANGE), `wpr2 ja existe` antes (EBUSY).

## Bases Falcon GSP (fixas)

- `falcon/gsp.rs:Gsp`: `PFalconBase = 0x00110000`, `PFalcon2Base = 0x00111000`. Reload-done =
  `NV_PGC6_BSI_SECURE_SCRATCH_14.boot_stage_3_handoff` (`check_reload_completed`, 2s).
