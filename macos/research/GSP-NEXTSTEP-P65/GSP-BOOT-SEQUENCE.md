# P65 GSP BOOT SEQUENCE — GA106 (GFW_READY => RPC_READY)

Fonte normativa: Nova-core `gsp/boot.rs` (Gsp::boot), `gsp/hal/tu102.rs` (Tu102::boot),
`gsp/hal/ga102.rs` (GA102_HAL sem bootloader), `firmware/*`, `falcon.rs`, `fb.rs`, `gpu.rs`,
`driver.rs`. GA106 = Ampere nao-GA100 => `GA102_HAL` + `Ga102` Falcon HAL (DMA) + `ga100/ga102`
FB HAL (flush HI+LO). Corroboracao OpenRM kernel_gsp.c (WPR2->FWSEC->Booter->SetSystemInfo/SetRegistry).

## Fase 0 — CPU-side / PCI (antes e imediatamente apos GFW_READY)

1. `driver.rs:probe`: `pdev.enable_device_mem()` (MSE/BAR0) + `pdev.set_master()` (BME).
   No macOS: MSE ja ON (Command 0003), BME OFF; equivalente e `setBusLeadEnable(true)`
   (IOPCIDevice.h:1095; `setBusMasterEnable` :574 deprecated). No Linux isto ocorre ANTES de
   `Gpu::new`, ou seja, antes de GFW wait. BME NAO e pos-GFW no Nova; e pre-requisito de probe.
2. `gpu.rs:Gpu::new`: `dma_set_mask_and_coherent(hal.dma_mask())` -> `wait_gfw_boot_completion(bar)`
   (GFW_BOOT 0x118234 == 0xff apos gate 0x118128 bit0). Estado GFW_READY = scratch completed,
   NAO significa firmware executando.
3. `SysmemFlush::register`: alloc PAGE_SIZE Coherent + `write_sysmem_flush_page` (HI 0x100c40,
   LO 0x100c10). PRIMEIROS MMIO writes pos-GFW. Sem isto, nenhum reset Falcon e valido.
4. `Falcon::new(gsp)` + `clear_swgen0_intr()` (write NV_PFALCON_FALCON_IRQSCLR.swgen0=1);
   `Falcon::new(sec2)`. Ainda sem DMA GPU.

## Fase 1 — WPR2 via FWSEC-FRTS (cria regiao protegida em VRAM)

5. `FbRanges::new`: calcula fb / vga_workspace / frts (1M, tu102) / boot / elf / wpr2_heap /
   wpr2 (ate frts.end) / non_wpr_heap. `wpr_meta = Coherent(GspFwWprMeta::from_ranges(...))`
   com magic/revision + sysmemAddrOfRadix3Elf/Bootloader/Signature (DMA addrs) + ranges.
6. `Vbios::new` (extrai FWSEC do ROM).
7. `build_unload_bundle` (prepara FWSEC-SB + booter_unload p/ unload futuro; nao executa).
8. Se `!frts.is_empty()` (GA106: sim, 1M; GA100: vazio => pula):
   `run_fwsec_frts`: exige `wpr2_range(bar).is_none()` (senao EBUSY => precisa reset GPU);
   `FwsecFirmware::new(dev, falcon=GSP, bios, Frts{frts_addr,frts_size})`; como GA106
   `needs_fwsec_bootloader=false`, `fw.run(dev, gsp_falcon)` direto (reset+load+boot mbox0=Some(0));
   checa `NV_PBUS_SW_SCRATCH_0E_FRTS_ERR == 0` e `wpr2_range() == frts` (start confere, senao erro).
   Primeiro(s) DMA(s) GPU reais ocorrem AQUI dentro (FWSEC dma_load via FBDMA).

## Fase 2 — GSP reset + boot via mailbox + Booter_load via SEC2

9. `gsp_falcon.reset()` (reset_eng + select_core + scrub wait + RM=BOOT_0).
10. `libos_dma = gsp.libos.dma_address()`; `gsp_falcon.boot(Some(lo), Some(hi))`
    (write MAILBOX0/1 + start + wait_till_halted 2s; retorna mbox0/mbox1).
11. `BooterFirmware::new(dev, Loader, chipset, sec2)` (parse booter_load.tlv, patch signature via
    fuse version) + `.run(dev, sec2, &wpr_meta)`:
    `sec2.reset(); sec2.load(booter)` (dma_load FBDMA 256B blocos); `sec2.boot(wpr_dma lo/hi)`
    (mailboxes = endereco da WPR meta); exige `mbox0 == 0` (senao ENODEV "Booter-load failed").
    Booter carrega/verifica GSP-RM em WPR2 a partir de Radix3 + bootloader + signatures.

## Fase 3 — RISC-V ativo + RPCs iniciais + sequencer + INIT_DONE

12. `gsp_falcon.write_os_version(bootloader.app_version)` (NV_PFALCON_FALCON_OS).
13. Poll `is_riscv_active()` 10ms / 5s (`read_poll_timeout`). `dev_dbg! RISC-V active?`.
14. `cmdq.send_command_no_wait(SetSystemInfo::new(pdev, chipset))` (host->GSP).
15. `cmdq.send_command_no_wait(SetRegistry::new(vgpu_state))` (host->GSP; entradas fixas
    RMSecBusResetEnable=1, RMForcePcieConfigSave=1, RMDevidCheckIgnore=1 [+RMSetSriovMode se vGPU]).
16. `hal.post_boot` (TU102/GA102): `GspSequencer::run(&cmdq, ctx, libos, bootloader_app_version)`:
    recebe `GspSequence` via `cmdq.receive_msg(RECEIVE_TIMEOUT 5s)` (ERANGE=>continua);
    executa comandos DIRIGIDOS POR FIRMWARE: RegWrite/RegModify/RegPoll/DelayUs/RegStore/
    CoreReset/CoreStart/CoreWaitForHalt/CoreResume. `CoreResume` = gsp reset + libos addr nas
    mailboxes + sec2 start + `check_reload_completed` (BSI_SCRATCH_14.boot_stage_3_handoff, 2s) +
    sec2 mbox0==0 + write_os_version + is_riscv_active. Ou seja, parte do "boot" e decidida pelo
    firmware em runtime, nao e sequencia fixa de writes do host.
17. `wait_gsp_init_done(&cmdq)` (loop `receive_msg::<GspInitDone>` 5s; ERANGE=>continua) =>
    RPC_READY/INIT_DONE. So entao `get_static_info`, unload_bundle retornado.

## Separacao pedida no prompt

- CPU-side preparation: 1-4 (PCI, DMA mask, GFW poll, flush page alloc, falcon objs).
- PCI configuration: enable_device_mem + set_master (probe); nada mais antes do boot.
- MMIO writes: flush HI/LO, IRQSCLR swgen0, depois todos os writes de falcon (FBIF_CTL/TRANSCFG,
  DMATRFBASE/BASE1/MOFFS/FBOFFS/CMD, BOOTVEC, BROM, MAILBOX, OS, CPUCTL) + sequencer RegWrite/Modify.
- DMA mapping: todas as allocs Coherent/SGTable (flush page, fw radix3 L0/L1/L2, signatures,
  bootloader, wpr_meta, libos, cmdq) com dma_address() como fonte de verdade.
- Falcon/FBDMA: FWSEC load, Booter load, (PIO indisponivel em GA106: load_method()=Dma).
- Firmware staging: FWSEC (do VBIOS) -> Booter_load (SEC2, via WPR meta) -> bootloader+GSP-RM (WPR2).
- GSP boot: mailbox boot + RISC-V poll + sequencer resume.
- RPC transport: cmdq TX/RX + SetSystemInfo/SetRegistry + sequencer msgs + GspInitDone.
