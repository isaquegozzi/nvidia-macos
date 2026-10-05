> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# PROJECT-HANDOFF — nvidia-macos (canônico, 2026-09-08)

## Objetivo final

```text
RTX 3060 GA106 → bring-up → IOGPU / IOAccelerator → MTLDevice → Metal acceleration
NOT compute-only; display/DP later
```

## Hardware

- Ryzen 5 5600G; iGPU AMD Cezanne `1002:1638` (display) + áudio
- RTX 3060 LHR `10de:2504` rev a1 (`01:00.0`, GA106) + áudio `10de:228e` (`01:00.1`)
- BARs: BAR0 16M (MMIO), BAR1 256M (<< VRAM → sysmem-path obrigatório), BAR3 32M

## macOS / OpenCore / toolchain

- Tahoe 26.6.2 (25G83), x86_64, SMBIOS MacPro7,1 (Hackintosh)
- EFI = pendrive `MACOS PENDR` (device varia: disk6s1/disk8s1 — resolver pelo volume),
  `EFI/OC`, OpenCore REL-108; `config.plist` ativo conforme CURRENT-STATE.md
- Xcode 26.5 + CLT; sem Apple Developer Program

## Paths principais

- Repo (read-only, NTFS): `<PROJECTS>/Nvidia/nvidia-macos/`
- Docs/dumps/builds: `~/Documents/nvidia-macos-BARE0/`
- Handoff desta sessão: `MUSE-SESSION-HANDOFF-2026-09-08.md` (relatórios 27→38)
- Prompts de fase: `~/Downloads/*.md` (TG-*.md)
- EFI: `/Volumes/MACOS PENDR/EFI/OC/` (`Kexts/`, `config*.plist`)

## NDRV0 (baseline gráfico — NUNCA remover sem fase própria)

`AAPL,iokit-ignore-ndrv` PRESENT na RTX → SEM `IONDRVFramebuffer`; desktop AMD
saudável. Sem ele: freeze userspace; com class-spoof: também freeze.
VoodooHDA fora; AMD/NootedRed/Lilu intocados; uma revisão GA106Lab por vez.

## Architecture milestones

Fase 0 (lab Linux) → TGM0/H0 → BARE-NDRV0 SUCCESS → KEXT0/REV1 attach →
KEXT1 UserClient → KEXT2 PCI (bug 00:00.0 → fix 1.2.1 wrappers 1-arg + gate
10de:2504) → KEXT3 BAR0 map probe → KEXT4 BOOT_0 live PASS (0xb76000a1/0x176) →
KEXT4B BOOT_42 live PASS (0x176a1000/0x176, mask 10-bit 0x3FF00000; BOOT_2 removido)
→ bring-up model C (pre-GSP mínimo + GSP steady state) → GSP deep dive →
sysmem/DMA arch → alloc SPEC → 1.5.0 (prototype) → Astra C → XNU reassessment →
**1.5.1 FIX-ASTRA-C** (lifecycle/serialização/testes corrigidos, 12/12 mutações).

## Major rejected paths (permanentes)

- `LEGACY_WRITE_0x140 = REJECTED` — 0x140 é R--/must-not-write; RM usa
  INTR_EN_SET/CLEAR; Nouveau legado não prova Ampere. NUNCA ressuscitar.
- `FIRST_WRITE_CANDIDATE = NONE` — nenhum write simples tem segurança+progresso;
  INTR_EN_CLEAR tem semântica mas máscara RM-owned GA106 UNKNOWN + progresso NONE.
- BOOT_2 @0x8 removido (evidência primária insuficiente; zero==absent infundado).
- `setBusMasterEnable` deprecated — futuro BME usa `setBusLeadEnable`.

## Current implementation trees

| Árvore | Versão | Estado |
|---|---|---|
| GA106Lab-kext4-first-mmio-read-src | 1.4.0 | superada (BOOT_0 live PASS) |
| GA106Lab-kext4-static-identity-src | 1.4.1 (KEXT4B) | superada (BOOT_42 live PASS; substituída pela 1.5.3 no L0) |
| GA106Lab-kext5-gsp-sysmem-alloc-src | 1.5.0 | OFFLINE PROTOTYPE ONLY (Astra C) |
| GA106Lab-kext5-gsp-sysmem-alloc-fix-src | 1.5.1 | superada (FIX-ASTRA-C READY, sem staging) |
| GA106Lab-kext5-gsp-sysmem-alloc-fix2-src | 1.5.2 | superada (FIX-ASTRA-B2 READY 14/14, sem staging) |
| GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src | 1.5.3 | **ATIVA/LIVE (L0+L1)** (FIRST_BOOT PASS + SYSMEM_PREPARE_LIVE_0 PASS 1×4096 ADDRESS_READY; DMA endpoint NOT validated) |
| GA106Lab-kext6-gsp-gfw-readiness-src | 1.6.0 | superada (offline PASS; Astra R54 = B, sem staging) |
| GA106Lab-kext6-gsp-gfw-readiness-astra-b-fix-src | 1.6.1 | superada (offline PASS; reauditoria interrompida, sem staging) |
| GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src | 1.6.2 | **CANDIDATA OFFLINE** (R57 offline PASS; Gemini R58 PASS; Astra final pendente; pacote disabled-stage pronto em PRESTAGE-P60-1.6.2/, NÃO stageado) |
| GA106Lab-kext7-sysmem-flush-prewrite-src | 1.7.0 | **FALHOU Astra (D — FAIL) / DO NOT STAGE** (blockers P74: selector9 self-busy + teardown-on-clear-fail; pacote P70 INVALID/inválido pós-áudio) |
| GA106Lab-kext7-sysmem-flush-prewrite-fix-src | 1.7.1 | **CANDIDATA OFFLINE ATUAL (fix, NÃO aprovada)** (P74: single-owner template + failure-safe teardown; real-path tests 11/11; mutações 6/6 novas + 16/16 + 12/12 + 2/2; ASAN/TSAN; reproduzível; NADA stageado/live; aguarda reaudit Astra) |

Selectors: 0 version, 1 identity, 2 status, 3 PCI snapshot, 4 BAR probe,
5 BOOT_0 (1 read/0 stores), 6 BOOT_42 (1 read/0 stores),
7 PREPARE_GSP_SYSMEM (1× live ADDRESS_READY em 1.5.3),
8 READ_GFW_BOOT_READINESS (1.6.x, offline PASS, nunca executado live),
9 VERIFY_SYSMEM_FLUSH_PRECONDITIONS (1.7.0 Gate A read-only, offline PASS, nunca executado live).

## Current live baseline

KEXT4B 1.4.1 `TG-KEXT4-STATIC-IDENTITY-READS`; provider 10de:2504/01:00.0;
BOOT_0+BOOT_42 confirmados live; AMD/WindowServer/displaypolicyd healthy.
EFI staged DISABLED: 1.5.3 `TG-KEXT5-GSP-SYSMEM-ALLOC-TEST-PATH-FIX`
(`GA106Lab-1.5.3.kext`, Enabled=false; enabled count = 1).
REL47 disabled-stage secondary audit PASS, live side effects NONE.
Astra final reaudit PENDING. Prompt48 preflight prepared
(`LIVE-PREFLIGHT-P48/`, live candidate offline only). NO live authorization.
PROMPT 50 L0 EXECUTED 2026-09-08: config 41e2a921... (1.4.1 false / 1.5.3 true),
reboot once, 1.5.3 loaded START_SUCCESS, NDRV intacto, AMD/WindowServer/displaypolicyd
healthy, selector 7 count 0. `KEXT5_1_5_3_FIRST_BOOT_BASELINE = PASS`. L1 NÃO autorizado.
PROMPT 51 L1 EXECUTED 2026-09-08: selector 7 1× via `ga106ctl prepare-gsp-sysmem`
(exit 0, 48B v1, status/state ADDRESS_READY, 1×4096, reserved zero por source evidence,
sem IOVA exposta). Pós-call + 5min healthy, mapping LEFT PREPARED.
`MAC_GPU_GSP_SYSMEM_PREPARE_LIVE_0 = PASS`, `DMA_ENDPOINT_VALIDATED = NO`.
PROMPT 52/53: spec + implementação 1.6.0 selector 8 GFW readiness (offline PASS).
PROMPT 54: Astra audit 1.6.0 = B (3 blockers reais, sem stage).
PROMPT 55: fixes 1.6.1 + AppleALC preflight (codec onboard segue UNKNOWN).
PROMPT 56: reauditoria Astra interrompida por quota (sem relatório final).
PROMPT 57: 1.6.2 UserClient-lifetime/mutation fix (offline PASS).
PROMPT 58: Gemini secundária 1.6.2 = PASS READY_FOR_ASTRA_FINAL_REAUDIT.
PROMPT 60: pacote disabled-stage 1.6.2 pronto OFFLINE (`PRESTAGE-P60-1.6.2/`,
candidate `a6b6c98d...`, diff mínimo, plutil PASS); EFI intacta
(`41e2a921...`, 1.5.3 enabled); áudio: codec segue UNKNOWN, próximo passo
é leitura read-only via Linux. Astra indisponível (quota 0%).
Live atual: 1.5.3 `TG-KEXT5-GSP-SYSMEM-ALLOC-TEST-PATH-FIX`, config
`41e2a921...`, 1.5.3 Enabled = true, count = 1.

## Safety invariants

Sem MMIO/BAR-write (exceto reads autorizados 5/6), sem DMA/GSP/firmware/
interrupts/reset; BME OFF; sem EFI sem fase; sem reboot automático; NDRV0
intacto; repo NTFS read-only. Autorizações atuais em CURRENT-STATE.md
(tudo NO exceto o já executado).

## Roadmap

PCI config → BAR → MMIO identity → sysmem/DMA foundation → GSP bootstrap →
VRAM/MMU → queues/runlists → engines → command submission → offscreen
rendering → IOGPU/IOAccelerator → MTLDevice → Metal.

## NEW SESSION BOOTSTRAP

1. Ler `MUSE-SESSION-HANDOFF-2026-09-09-P3.md` (handoff P3, autocontido, verdade atual).
2. Ler `CURRENT-STATE.md`.
3. `MUSE-SESSION-HANDOFF-2026-09-08-P2.md` (histórico P2) só se necessário.
3. Consultar documentos específicos SOMENTE quando necessário.
4. NÃO reconstruir a conversa.
5. Revalidar hashes/estado vivo antes de qualquer escrita.
6. Astra volta SOMENTE com "Astra voltou" explícito do usuário; sem isso,
   próxima etapa é PROMPT 62 (a definir), sem stage/boot de 1.6.2.

## Canonical sync P72 (2026-09-09, PROMPT 71/72 — P3 é source de recuperação)

```text
LIVE = GA106Lab 1.5.3 (inalterado; sole enabled; config 41e2a921...)

CURRENT OFFLINE CANDIDATE =
GA106Lab 1.7.0
TG-KEXT7-SYSMEM-FLUSH-PREWRITE-READINESS

selector8 =
GFW readiness candidate, NOT live validated

selector9 =
VERIFY_SYSMEM_FLUSH_PRECONDITIONS
Gate A read-only/offline candidate (zero-input, ABI 48B semantica,
dedicated 4096B flush page mapping, BME must be OFF, mapping persists,
no userspace IOVA exposure; MMIO writes = 0, PCI writes = 0,
BME changes = 0, GPU DMA = 0, Falcon/GSP/firmware actions = 0)
NOT live validated

Gate B =
SYSMEM_FLUSH first-write spec only (HI 0x100c40 then LO 0x100c10)
NOT implemented
NOT authorized

KEXT 7938e9a3a92b12717c0787b9562576152f25f819d679cc56bd0096e6fd3fd3dd
plist 598be9e088332445afb95132ec1d483701ef904c4b92122899eed2cb58d6b364
CLI 9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840
manifest 405b1d00c66ed31a7aa38fb9618c6de77671d9d7a4321e4a382fea8b9a7f4505

PRELIVE_PACKAGE_READY = YES (LIVE-PREFLIGHT-P70-1.7.0/, tudo NO)
S0 SHA = 7b549ac4d13e177dab31ef5f9ede7f70fef76a7a5c27e7ac10dae2c7cffefb32 (1.7.0 disabled)
L0 SHA = b6065f8c1b7691485c246e765b77fc456df5030438257bdee146ba53ec0dd529 (só dois flips p/ 1.7.0-only)
plutil S0/L0 PASS; ocvalidate exact REL-108/1.0.8 UNAVAILABLE
```

Safety invariants (reconfirmados P72): NEVER remove AAPL,iokit-ignore-ndrv;
NEVER reintroduce CLASS0; do not alter AMD properties; VoodooHDA stays absent;
one GA106Lab revision enabled at a time.

```text
FIRST_MMIO_WRITE_AUTHORIZED = NO
PCI_CONFIG_WRITE_AUTHORIZED = NO
BME_ENABLE_AUTHORIZED = NO
DMA_AUTHORIZED = NO
INTERRUPTS_AUTHORIZED = NO
RESET_AUTHORIZED = NO
GSP_AUTHORIZED = NO
FIRMWARE_AUTHORIZED = NO
VRAM_WRITE_AUTHORIZED = NO
COMMAND_SUBMISSION_AUTHORIZED = NO
STAGING_1_7_0_AUTHORIZED = NO
LIVE_BOOT_1_7_0_AUTHORIZED = NO
LIVE_SELECTOR8_AUTHORIZED = NO
LIVE_SELECTOR9_AUTHORIZED = NO
ASTRA_AVAILABLE = NO
ASTRA_PRIMARY_REVIEW_1_7_0_COMPLETE = NO
```

Áudio (P72 §7, inalterado): AppleALC codec identity = UNKNOWN;
next = RUN_LINUX_CODEC_ID_READONLY; sem reboot compartilhado com troca NVIDIA.

## Canonical sync P74 (2026-09-09 — 1.7.0 FAIL, P70 inválido, 1.7.1 fix offline)

```text
LIVE = GA106Lab 1.5.3 (inalterado)
1.7.0 = Astra PRIMARY_VERDICT D FAIL / DO NOT STAGE (2 blockers corrigidos em 1.7.1)
P70 1.7.0 = INVALID_SUPERSEDED_DO_NOT_USE (nota em LIVE-PREFLIGHT-P70-1.7.0/; S0 antigo reverteria áudio layout 20->1)
1.7.1 = offline fix candidate, NOT approved (aguarda reaudit Astra)
  VERSION 1.7.1 / TG-KEXT7-SYSMEM-FLUSH-PREWRITE-ASTRA-FIX
  KEXT 59d949cfeba94697ccf2f50bfe6ddc11b171ce16cd749f5da6d05a9020074f47
  plist cb79a2a7bfb407940d2b94c88c20aff4df9b91d8f6270b0f68f50d13962bf194
  CLI 9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840
  manifest 66051ad9f9db4b744930bfa5088d6180efe2e513b6e8bda5c813e915fcd79e5a
  selector9 single-owner template (REAL path testado); teardown failure-safe;
  Gate B NOT implemented; MMIO/PCI/BME/DMA/Falcon/GSP/firmware = 0
EFI ativa pós-áudio: config 06cf022258d796c942b71786aeff5b9f730d9ebf507d2ca8f85732ab538315ef,
  ALC892 layout-id 20 (Data 14 00 00 00) — preservado, intocado neste prompt
STAGING_1_7_0/1_7_1 = NO; LIVE_BOOT/SELECTOR8/SELECTOR9/FIRST_WRITE/BME/DMA/GSP = NO
```
