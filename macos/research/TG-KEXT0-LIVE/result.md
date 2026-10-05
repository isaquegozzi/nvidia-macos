# TG-KEXT0-LIVE result — boot 06531E7E (2026-09-06 16:45:13)

## Veredito

```text
MAC_COMPUTE_KEXT_ATTACH_0 = PASS (com 1 ressalva de telemetria)
```

Todos os 15 critérios atendidos; ressalva: telemetria IOLog própria ausente
(START_SUCCESS provado por estado de registry, não por log).

## Prova por critério

- boot SUCCESS / desktop normal / sem panic-reboot (observação humana + causa 5 prévia).
- loaded YES: kextstat idx 86 + kmutil + nó registry + `GA106Lab=1` em IOKitDiagnostics.
- START_ENTER/PROVIDER_CONFIRMED: sem linhas IOLog (anomalia registrada) — inferidos.
- START_SUCCESS YES: nó `active` com flags idênticas às de Lilu/NootedRed/
  RestrictEvents/SMCSuperIO (drivers indiscutivelmente rodando); sem panic; 3+ min uptime.
- provider = RTX 10de:2504 (filho direto do nó `pci10de,2504`/1:0:0/rev a1,
  id pai correlacionado; entryID serviço `0x100000274`).
- AMD_MATCHED NO / HDAU_MATCHED NO (1 único nó GA106Lab no registro).
- RTX: PRESENT/030000/built-in<00>/ignore-ndrv<01>/SEM fb/accel (.Display_boot 0).
- AMD HEALTHY (Main+Online, accel+fbs); WindowServer HEALTHY; displaypolicyd 0 exits.
- panic NO (neste boot; panic 00:38:47 é do boot NDRV0, sem strings GPU/nossas).

## Tabela NDRV0 vs TG-KEXT0

boot/loginwindow/DidReconfigure idênticos (+4s/+69s); TG-KEXT0 tem +1 reconfigure
(+79s) e verbose mais longo — BOOT_DELAY_ASSOCIATED_WITH_GA106LAB = UNLIKELY.

## Follow-up obrigatório (1 item)

Migrar telemetria de `IOLog` para `os_log` (ou provar coleta de IOLog de KEXTs
injetados no boot) antes de qualquer KEXT1 — sem telemetria própria, próximos
milestones ficam cegos.
