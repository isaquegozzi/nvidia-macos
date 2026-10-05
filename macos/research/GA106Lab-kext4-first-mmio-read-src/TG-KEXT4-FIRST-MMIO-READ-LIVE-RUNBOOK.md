# TG-KEXT4-FIRST-MMIO-READ-LIVE-RUNBOOK (NÃO EXECUTADO — sem autorização live)

Baseline: KEXT3 1.3.0 live PASS (`MAC_GPU_BAR_MAP_0`, BAR0 0xFB000000/16MiB).
Artefato futuro: GA106Lab 1.4.0 `TG-KEXT4-FIRST-MMIO-READ` (ainda SEM staging).

## Pré-requisitos (fase separada futura, nesta ordem)
1. `ASTRA-TG-KEXT4-FIRST-MMIO-READ-AUDIT` PASS (externo, read-only).
2. Stage DESABILITADO 1.4.0 na EFI (bundle + staged config, ativo 1.3.0
   inalterado; NDRV gate revalidado).
3. Activate + reboot manual + primeiro boot 1.3.0→1.4.0 (sem selector 5).

## Live (1 selector 5 call, 1 MMIO load, 0 retries)
```bash
sudo ./ga106ctl first-mmio-read   # binário 1.4.0 auditado, 1x
```
PASS (`MAC_GPU_MMIO_READ_0`) somente se: provider 10de:2504; BAR0 único;
map RO+Inhibit; exatamente 1 leitura no offset fixo 0 width 4; raw
`0x176xxxxx` (chip-id 0x176; `ffffffff`/`00000000` = FAIL); MMIO write NONE;
configWrite NONE; Command/MSE/BME unchanged; map released; sem DMA/IRQ/
reset/GSP; AMD/WS healthy; panic NO; GPU restart NO. Qualquer falha = STOP.

(End of runbook — NÃO EXECUTADO)
