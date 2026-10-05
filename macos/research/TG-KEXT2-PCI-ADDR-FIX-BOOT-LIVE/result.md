# TG-KEXT2-PCI-ADDR-FIX-BOOT-LIVE result — boot 2844E679 (2026-09-07 19:34:30)

## Veredito

```text
TG_KEXT2_PCI_ADDR_FIX_FIRST_BOOT = PASS
KEXT2_FIX_BOOT_BASELINE_STABLE = YES
MAC_GPU_PCI_READ_0_RETRY_READY_FOR_SEPARATE_AUTHORIZATION = YES
LIVE_PCI_SNAPSHOT_EXECUTED = NO (ga106ctl pci NÃO executado)
```

## Prova

boot SUCCESS / FIX 1.2.1 YES / KEXT2-1.2.0/KEXT1/REV1/PASS NO /
nó PRESENT+ACTIVE / State START_SUCCESS + Revision TG-KEXT2-PCI-ADDR-FIX +
vendor 10de + device 2504 + providerEntryID 4294967865 = pai GFX0@0 (0x100000239, BDF 01:00.0) /
ENTRY_ID_MATCH YES / FIX_PROVIDER_CONFIRMED YES /
AMD NO / HDAU NO (1 único nó) / ignore-ndrv PRESENT / IONDRVFramebuffer 0 /
RTX IOAccelerator NO + IOFramebuffer NO /
AMD 1002:1638 HEALTHY + Metal 3 + WS HEALTHY + displaypolicyd HEALTHY /
version+identity+status PASS (driver 0x00010201) /
ga106ctl pci NOT_RUN / panic NO (só stale 2026-09-06-003847, DumpPanic no panic, shutdown cause 5) / sem regressões.

## Arquivos

boot-info.txt / loaded-kexts.txt / ioreg-ga106lab.txt / ioreg-provider.txt /
graphics-health.txt / legacy-selectors.txt / regression-check.txt / result.md
```

(End of file)
