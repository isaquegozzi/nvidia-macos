# TG-KEXT3-BAR-MAP-PROBE-LIVE-RUNBOOK (NÃO EXECUTADO)

Baseline: KEXT2 FIX 1.2.1 live PASS (provider 10de:2504, Command 0x0003).
Artefato futuro: GA106Lab 1.3.0 `TG-KEXT3-BAR-MAP-PROBE` (staged, Enabled=false).

## Stage (fase separada, sem reboot automático)
1. Copiar `build/obj/GA106Lab.kext` → `EFI/OC/Kexts/GA106Lab-KEXT3-BAR-MAP-PROBE.kext`.
2. Backup `config.plist` com timestamp; verificar SHA.
3. Adicionar entry Enabled=false (staged); validar `plutil -lint`, TOTAL por
   categoria; NDRV gate (`AAPL,iokit-ignore-ndrv` PRESENT, sem wegnoegpu/
   disable-gpu/class-spoof, VoodooHDA ausente).
4. Ativar em config EXPERIMENT separado; só então `cp` → `config.plist`;
   confirmar SHA + `ACTIVE_GA106LAB=KEXT3_1_3_0`; PARAR ANTES DO REBOOT.
5. Rollback pronto (config 1.2.1 live + config 1.2.0).

## Primeiro boot 1.3.0 (sem probe ainda)
- `kextstat/kmutil`: só 1.3.0; 1.2.x/1.1.0/1.0.x NO.
- `GA106LabRevision=TG-KEXT3-BAR-MAP-PROBE`, State START_SUCCESS, nó+PRESENT+ACTIVE.
- Provider GFX0@0 01:00.0 10de:2504; AMD/WS/dp HEALTHY; panic NO.
- Legacy selectors 0-3 ainda PASS (version 0x00010300).

## Live probe (1 call only, sem retry automático)
```bash
sudo ./ga106ctl bar-map-probe   # binário 1.3.0 auditado, 1x
```
PASS (`MAC_GPU_BAR_MAP_0`) somente se: provider 10de:2504; BAR0 unicamente
identificado; base/len sane (16 MiB); Command/MSE/BME before==after; map
created RO+Inhibit confirmados; metadata válida; map released; MMIO deref
NONE; MMIO write NONE; configWrite NONE; DMA/IRQ/reset/GSP NONE; AMD/WS
healthy; panic NO; GPU restart NO. Qualquer falha = STOP, sem 2ª chamada.

(End of runbook — NÃO EXECUTADO)
