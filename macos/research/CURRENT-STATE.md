> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# CURRENT-STATE — nvidia-macos (2026-09-09, PROMPT 72 sync; base P61 P2 preservada abaixo)

```text
ACTIVE LIVE STATE =
  1.5.3 TG-KEXT5-GSP-SYSMEM-ALLOC-TEST-PATH-FIX
  provider 10de:2504 / class 030000
  START_SUCCESS, IONDRVFramebuffer RTX = 0
  selector 7 EXECUTED EXACTLY ONCE live (ADDRESS_READY, 1x4096, mapping LEFT PREPARED)

EFI CURRENT (verificado read-only em P61) =
  config SHA = 41e2a921f399c45b25186b574b5e03bc9d3846863fbf1c3411ad1fe2b002b63d
  1.4.1 Enabled = false
  1.5.3 Enabled = true
  enabled GA106Lab count = 1

LATEST OFFLINE CANDIDATE =
  1.7.1 TG-KEXT7-SYSMEM-FLUSH-PREWRITE-ASTRA-FIX (fix P74, NÃO aprovada; reaudit pendente)
  KEXT_BINARY_SHA = 59d949cfeba94697ccf2f50bfe6ddc11b171ce16cd749f5da6d05a9020074f47
  MANIFEST_SHA = 66051ad9f9db4b744930bfa5088d6180efe2e513b6e8bda5c813e915fcd79e5a
  (prior: 1.7.0 Astra FAIL D DO NOT STAGE; P70 INVALID pós-áudio)
  selector 9 VERIFY_SYSMEM_FLUSH_PRECONDITIONS (Gate A read-only, offline PASS, nunca executado live)
  KEXT_BINARY_SHA = 7938e9a3a92b12717c0787b9562576152f25f819d679cc56bd0096e6fd3fd3dd
  MANIFEST_SHA = 405b1d00c66ed31a7aa38fb9618c6de77671d9d7a4321e4a382fea8b9a7f4505
  prelive package READY em LIVE-PREFLIGHT-P70-1.7.0/ (S0 7b549ac4..., L0 b6065f8c...)
  STAGING_1_7_0_AUTHORIZED = NO; LIVE_SELECTOR8_AUTHORIZED = NO; LIVE_SELECTOR9_AUTHORIZED = NO
  (prior: 1.6.2 selector8 GFW readiness offline PASS, nunca live; pacote em PRESTAGE-P60-1.6.2/)

WHERE WE ARE NOW =
  PCI visibility                  COMPLETE
  PCI config read                 COMPLETE
  BAR0 map                        COMPLETE
  BOOT0                           COMPLETE
  BOOT42                          COMPLETE
  selector7 sysmem foundation     LIVE COMPLETE
  selector8 GFW readiness         OFFLINE candidate / NOT live
  selector9 Gate A                OFFLINE candidate / NOT live
  Gate B first MMIO write         SPEC ONLY / NOT implemented
  BME                             NOT authorized
  first GPU DMA                   NOT authorized
  GSP/FW                          NOT authorized
  Metal                           future

AUDIO =
  AppleALC 1.9.8 + Lilu 1.7.3 present/live; layout-id = 1; alcid ausente
  AppleHDA onboard NÃO attached; IOHDACodecDevice = 0
  ONBOARD_CODEC = UNKNOWN (UNPROVEN); próximo = leitura read-only via Linux

PENDING =
  Astra re-review 1.7.1 = PENDING (input pronto em ASTRA-REVIEW-P74-1.7.1-INPUT/, manifesto 2102aa73...;
  1.7.0 recebeu PRIMARY_VERDICT D FAIL)
  input pronto em ASTRA-PRIMARY-REVIEW-P71-INPUT/ (manifesto 0b1e54bb...)
  P66 archivado byte-identical em SYSMEM-FLUSH-SPEC-P66-ARCHIVE/

WHAT IS NOT AUTHORIZED =
  FIRST_MMIO_WRITE, PCI_CONFIG_WRITE, BME_ENABLE, DMA, INTERRUPTS, RESET, GSP,
  FIRMWARE, VRAM_WRITE, COMMAND_SUBMISSION,
  STAGING_1_7_0, LIVE_BOOT_1_7_0, LIVE_SELECTOR8, LIVE_SELECTOR9,
  EFI edit, reboot, KEXT load novo, ga106ctl/selector live,
  MMIO/BAR access, DISPLAY_INIT, audio EFI/reboot/layout
  (tudo NO; verificado P72 §8)

EXACT NEXT STEP =
  PROMPT 75 (a definir) — sem stage/boot de 1.7.x sem autorização explícita;
  ordem futura: S0 -> L0 -> L1A selector8 1x -> review -> L1B selector9 1x -> STOP (Astra antes de Gate B)

ROLLBACK (vivo, pronto, não executado) =
  EFI-BACKUP-PROMPT46-20260908-143529 + EFI-BACKUP-PROMPT50-L0-20260908-154333
  rollback = 1.5.3 Enabled -> false, 1.4.1 Enabled -> true
```

Handoff P2: `MUSE-SESSION-HANDOFF-2026-09-08-P2.md` (autocontido).
Handoff P1: `MUSE-SESSION-HANDOFF-2026-09-08.md` (sessão 27→38).
