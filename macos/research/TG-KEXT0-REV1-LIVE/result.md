# TG-KEXT0-REV1-LIVE result — boot A38682C7 (2026-09-07 00:24:48)

## Veredito

```text
TG_KEXT0_REV1_LIVE = PASS
KEXT0_OBSERVABILITY_COMPLETE = YES
```

Telemetria registry presente e correlacionada; IOLog segue ausente (telemetria
oficial = IORegistry, conforme fase REV1).

## Evidência (células do critério)

- REV1 1.0.1 loaded YES (kextstat) / PASS 1.0.0 loaded NO (ausente).
- Nó `GA106Lab` id `0x10000027f`, active, 1 instância no registro.
- `GA106LabState=START_SUCCESS`, `Revision=TG-KEXT0-REV1`, `Vendor=10de`,
  `Device=2504`, `ProviderEntryID=4294967865` = pai `GFX0@0` (`0x100000239`).
- `ENTRY_ID_MATCH = YES`. Provider: pci10de,2504, 1:0:0, 030000.
- AMD/HDAU: zero filhos GA106Lab. NDRV runtime CONFIRMED (gate `<01>` no pai).
- SEM IONDRVFramebuffer (0 instâncias) / SEM .Display_boot / SEM accel RTX.
- AMD HEALTHY (Main+Online), WindowServer HEALTHY, displaypolicyd 0 exits.
- TEXT_LOG_AVAILABLE = NO (IOLog segue invisível; não invalida o registry).
- PANIC/restart/GPU-restart/WS-crash/loop = NO (só o `.panic` stale de 00:38,
  era NDRV0, sem strings do nosso trabalho).

## PASS 1.0.0 vs REV1 1.0.1

Idênticos em boot/provider/AMD/NDRV/desktop/panic. Única diferença funcional:
telemetria `GA106Lab*` no serviço (observabilidade adicional pura).

## Autorizações futuras: tudo continua NÃO

`IOUSERCLIENT/M MIO/PCI_CONFIG/GSP/COMPUTE_AUTHORIZED = NO`.
