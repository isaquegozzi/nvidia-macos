> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# GA106_MACOS_PCI_VISIBLE_STABLE — baseline do boot NDRV0 2EB44592 (2026-09-06)

> Primeiro macOS estável com RTX 3060 visível como PCI. Boot 00:38:43, UUID
> `<UUID-REDACTED>`, config viva SHA `a2e3d4d1…` (= NDRV0).
> boot-args sem `-wegnoegpu`. Dumps: `ioreg-{full,IOPCIDevice,IOFramebuffer}.txt`
> (5,3 MB cada) + `log-filtered.txt` (1191 linhas) nesta pasta. SOMENTE LEITURA.
> SHAs (`baseline-sha256.txt`): full `3e5c4577…`, IOPCIDevice `5b9508c4…`,
> IOFramebuffer `9ac186f9…`, log `bdec499a…`. Config NDRV0 `a2e3d4d1…` (EFI/OC).
> Boot-args runtime reconfirmado nesta sessão: `-v debug=0x100 keepsyms=1 -amfipassbeta`.
> Snapshot re-medido p/ match (2026-09-06): `snapshot-rtx-NDRV0-live.json`,
> `snapshot-amd-NDRV0-live.json`, saídas `match-{rtx,amd}-live.txt` (port Node fiel).

## RTX 3060 — PRESENTE

`GFX0@0`, id `0x100000239`, `pcidebug 1:0:0`, sob bridge `GPP0 0:1:1` via `IOPP`.
vendor `0x10DE`, device `0x2504`, subsys `10DE:2504`, class `0x030000` (original),
`compatible pci10de,2504/pciclass,030000/VGA/GFX0`, `IOName display`.
BARs (`IODeviceMemory`): BAR0 16 MiB @4211081216, janela BAR1 256 MiB @18253611008,
BAR3 32 MiB (+ sub-descriptor). `built-in = <00>` PRESENTE. Entitlements vivos:
`transport.pci` (single). Power `CurrentPowerState 2`. `hda-gfx onboard-2`.
Filhos: NENHUM. Áudio `HDAU@0,1` (1:0:1) presente, quieto.

## Gate runtime — CONFIRMED

`"AAPL,iokit-ignore-ndrv" = <01>` PRESENTE no provider vivo (Data 1 byte; no plist era
Boolean `true` — OC normalizou; presença = o que o gate testa).
`NDRV_GATE_RUNTIME = CONFIRMED` (observado no runtime, não só no config).

## Framebuffer — REMOVIDO

`IONDRVFramebuffer_ON_RTX = NO` (zero filhos; zero instâncias no sistema —
`IOKitDiagnostics IONDRVFramebuffer=0`). `DISPLAY_BOOT_ON_RTX = NO`
(zero `.Display_boot`). `NDRV_FRAMEBUFFER_REMOVED = CONFIRMED`
(contrafactual CLASS0 tinha os três).

## Estado gráfico

- `GPUWrangler_SEES_RTX = YES` (1 registro `(DG,quiet)` 00:38:50) mas nunca
  `published/pubSched/pubArmed` — contraste com BARE/CLASS0. `IOGraphics_SEES_RTX = NO`
  (sem GFX0-rename além do nome ACPI, sem fb). `RTX_ACCELERATOR = NO`.
  `RTX_FRAMEBUFFER = NO`. Serviços na RTX: nenhum.
- AMD: `AMD_ACCELERATOR = HEALTHY` (accel + fbs; ruído benigno ROM/TTL/cursor como
  desde TGM0). `AMD_FRAMEBUFFERS = 3` (só fb@0 com display). `AMD_ACTIVE_DISPLAYS = 1`
  (WSK R2401 1080p@60, Main). `WINDOWSERVER_RENDER_GPU = AMD` (único accel/fb do
  sistema; `DidReconfigure` + `MTLCompiler SUCCESS`). `BOOT_VGA/MAIN_GPU = AMD`.
- `WindowServer = HEALTHY`. `displaypolicyd = HEALTHY` (`displaypolicyd_exit1_count = 0`;
  sem loop). `WindowServer_start_timeout = NO`. NootedRed carregado, silencioso.

## Monitor verbose

Um AppleDisplay no registro (AMD fb@0). `VERBOSE_MONITOR_OWNER = NVIDIA` (inferência:
fb@1/fb@2 sem display → sem HPD em segunda cabeça AMD; saída sem driver retém últimos
pixels; histórico splash-nunca-assumido). `VERBOSE_MONITOR_MACOS_ACTIVE = NO`.
Confiança MEDIUM — cabo não inspecionado.

## Comparação

```text
               BARE        CLASS0        BARE-NDRV0
RTX PCI        YES         YES           YES
class-code     030000      030000+ff00?  030000
ignore-ndrv    ABSENT      ABSENT        PRESENT(runtime)
IONDRVFB       YES         YES           NO
.Display_boot  (n/capt)    YES           NO
GPUWrangler    DG publ.    DG publ.loop  DG quiet 1×
AMD accel      HEALTHY     HEALTHY       HEALTHY
loginwindow    running     running       desktop
WindowServer   re-init     reconfigure   HEALTHY+Metal OK
displaypolicyd exit×1      exit loop ×4  HEALTHY(0)
desktop        NO          NO            YES
panic          NO          NO            NO
```

## Causalidade

`NDRV_PATH_CAUSALITY = STRONGLY_SUPPORTED` (6/6 condições ideais: RTX YES, runtime
PRESENT, fb NO, display_boot NO, AMD HEALTHY, boot SUCCESS). Não CONFIRMED absoluto
por princípio (hang é ausência de evento), mas é o teto da escala permitida.

## Próximo passo (UM, não executar)

Re-rodar `tinygpu-match-check` contra snapshot VIVO da RTX (10de:2504/030000,
built-in PRESENTE `<00>`, tunnelled ABSENT, BARs acima) — 100% read-only, risco zero.
Dado novo que muda ADR-0002: `built-in` vivo é YES, o que torna a perna C
(LIKELY_NO_MATCH p/ dext de produção sem isenção) o bloqueio documentado nº 1 do
transporte — recalibrar A/B/C e MAC-COMPUTE-0 antes de qualquer proposta de dext.
