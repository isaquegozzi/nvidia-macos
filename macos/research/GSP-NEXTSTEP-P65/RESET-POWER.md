# P65 RESET / POWER PRECONDITIONS

## Respostas

```text
RESET_REQUIRED_BEFORE_FIRST_GSP_DMA = YES (multiplos resets Falcon ordenados + WPR2 ausente + flush antes)
POWER_CLOCK_PRECONDITIONS = BAR0/MSE habilitados e mapeados (enable_device_mem) + GFW_BOOT==0xff (ownership/pronto) + BME/DMA mask + flush page ativa; sem evidencia de clock-gate/power-gating extra a programar no bring-up Nova (clocks tratados fora do caminho GSP boot aqui); fotografie BOOT_0 como RM.
```

## Prova (Nova-core)

- `Falcon::reset()`: `hal.reset_eng()` + `hal.select_core()` + `hal.reset_wait_mem_scrubbing()` +
  `RM = PMC_BOOT_0`. Ga102 (GA106): reset_engine + scrub wait; Tu102: engine reset + scrub 10ms.
  `dma_reset()`: `FBIF_CTL.allow_phys_no_ctx=true` + `DMACTL=0` antes de cada `dma_load`.
- `BooterFirmware::run`: `sec2.reset()` antes de `load`+`boot`. `FwsecFirmware::run` (GA106 direto):
  reset+load+boot. `Tu102::boot`: `gsp.reset()` antes de `gsp.boot(libos addr)`. Sequencer `CoreReset` =
  `gsp.reset + dma_reset`; `CoreResume` = `gsp.reset` + mailboxes + sec2 start.
- `run_fwsec_frts` exige `wpr2_range().is_none()` senao EBUSY ("GPU needs to be reset"); apos FRTS exige
  `wpr2_range().start == frts.start`. Ou seja, estado de reset/power inclui "WPR2 limpo".
- `Gpu::new` exige `wait_gfw_boot_completion` antes de tudo (GFW ownership). `driver.rs:probe` exige
  `enable_device_mem` (BAR/MSE) antes. `priv_target_mask_released()` (HWCFG2 != 0xbadf41xx) e
  `riscv_branch_privilege_lockdown()` sao checagens de acesso, nao resets extras.
- Sem fonte que exija engine reset alem de Falcon, power-gating, clock enable ou "privileged access"
  manual antes do primeiro DMA — nao assumir; firmware pre-boot + GFW + resets Falcon bastam no modelo Nova.
