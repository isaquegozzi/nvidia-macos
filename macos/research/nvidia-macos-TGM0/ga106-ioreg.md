# GA106 ioreg — Tahoe 26.6.2 (25G83), MacPro7,1 — 2026-09-05

> Fase TGM0, SOMENTE LEITURA. Nenhum driver instalado, nenhum MMIO, nenhum NVRAM alterado,
> nenhum dext ativado, nenhum detach. Boot-args intacto `-wegnoegpu`.
> Repo em `/Volumes/Downloads` = NTFS read-only → snapshots crus em `/tmp/macos-tgm0/`
> + cópias sanitizadas em `~/Documents/nvidia-macos-TGM0/artifacts-macos/` (fora do repo).
> Quando o repo estiver gravável (Linux ou NTFS rw), copiar para `artifacts/macos/`
> (gitignored por `.gitignore:7`) com os mesmos nomes datados abaixo.

## 0. Pré-condição: desktop na AMD — PASS

- `system_profiler SPDisplaysDataType`: `AMD Radeon RX Renoir Graphics (1002:1638 rev c9)`,
  `Main Display: Yes`, `ED320QR S3 1920x1080@180`, `Framebuffer 30-Bit`, `Metal 3`, `VRAM 2 GB`.
- `SPPCIDataType`: só AMD `ATY_GPU built-in, Driver Installed: Yes, Link x16 8.0 GT/s up`.
  Nenhuma NVIDIA listada.
- Display físico na saída da placa-mãe. Nenhum experimento além de leitura foi feito.

## 1. Build / modelo (para datar o snapshot)

- `sw_vers`: `ProductName macOS, ProductVersion 26.6.2, BuildVersion 25G83`
- `uname -a`: `Darwin ... 25.6.0 ... RELEASE_X86_64 x86_64`
- `SPHardwareDataType`: `MacPro7,1, Ryzen 5 5600G, 16 GB`
  (Serial/UUID sanitizados — ver snapshots sanitizados, nunca colar cru em docs)
- `nvram boot-args`: `-v debug=0x100 keepsyms=1 -amfipassbeta -wegnoegpu`
  (literal, com `e` extra — NÃO é `-wegnogpu` do HANDOFF §6; corrigir o HANDOFF quando gravável)
- `kextstat`: `Lilu 1.7.3` loaded, **sem** `WhateverGreen`, sem `NVDA/GeForce`,
  AMD `AMDSupport 7.0.1 + AMDRadeonX6000Framebuffer 7.0.1 + AMDRadeonX5000 7.0.1`,
  `IOGraphicsFamily 600, IOAcceleratorFamily2 487.4.3`.
- `systemextensionsctl list`: `0 extension(s)` — nenhuma TinyGPU instalada (RISK respeitado).

## 2. RTX `10de:2504` — HIDDEN (não enumerada como IOPCIDevice)

| Campo (runbook §2) | Valor observado | Fonte |
|---|---|---|
| `vendor-id` / `device-id` | `0x10DE` / `0x2504` **só no log de boot**, ausente no `ioreg -l -w0 -c IOPCIDevice` vivo (0 nós `10de`; `grep vendor-id` só `1022/1002/10ec/2646`) | `log show --last boot` + `ioreg` |
| `class-code` | `0x030000` no log (`Found ... class-code 0x030000 ... 0x10de:0x2504`); sem nó vivo para confirmar | log |
| `subsys` / `revision` | Sem nó vivo → UNKNOWN no macOS; referência Linux: `subsys 10de:2504 rev a1` (baseline Fase 0) | Linux docs |
| `built-in` | **UNKNOWN (HIDDEN)** — sem nó, nada a ler. Não é ABSENT (ABSENT exigiria nó presente sem a chave) | `ioreg` 0 nós |
| `IOPCITunnelled` | **UNKNOWN (HIDDEN)** — global `grep -c IOPCITunnelled = 0`; sem nó, sem parents para busca recursiva | `ioreg` |
| `IOServiceDEXTEntitlements` | Ausente (sem nó) | `ioreg` |
| `AAPL,slot-name` | Ausente (sem nó) | `ioreg` |
| `IORegistry path` | Ausente. Pai esperado `GPP0@1,1 (0:1:1)` existe mas com filho vazio `D004@ff` (só `IORegistryEntry`, sem `IOPCIDevice`) | `ioreg -p IODeviceTree`: `GPP0@10001` + `D004@ff` |
| `provider` / driver anexado | Nenhum (`current provider` = removida). `log` prova remoção kernel antes de matching | log abaixo |
| `IOPowerManagement` / `_STA` / `disable-gpu` | Sem nó → não observáveis. `_STA` dos pais: `GPP0` sem `_STA` dedicado no dump; `D004@ff` só `acpi-path`, sem `_STA` | `ioreg -p IODeviceTree` |

**Log prova (sanitizado, essencial):**

```text
kernel: (IOPCIFamily) Found type 0 device class-code 0x030000 cmd 0x0003 at [i16]1:0:0(0x10de:0x2504)
kernel: (IOPCIFamily) Found type 0 device class-code 0x040300 cmd 0x0000 at [i17]1:0:1(0x10de:0x228e)
kernel: (IOPCIFamily) Probing type 0 device ... 0x10de:0x2504
kernel: (IOPCIFamily) Probing type 0 device ... 0x10de:0x228e
kernel: (IOPCIFamily) bridge [i5]0:1:1(1:1) removing child <private> [i16]1:0:0(0x10de:0x2504)
kernel: (IOPCIFamily) bridge [i5]0:1:1(1:1) removing child <private> [i17]1:0:1(0x10de:0x228e)
```

Interpretação: PCI enumeration viu a RTX + áudio (`228e` = GA106 HDA), depois o bridge
`0:1:1` removeu ambos — comportamento clássico `-wegnoegpu` (WhateverGreen disable-external-GPU).
Sem `WhateverGreen` em `kextstat` (só `Lilu 1.7.3`), mas a remoção ocorreu mesmo assim
(patch via Lilu/OpenCore ou SSDT; `WEG` pode estar injetado sem aparecer em `kextstat` ou
a remoção é via `SSDT-GPU-DISABLE`; sem `config.plist` lido — EFI não montada nesta sessão —
não afirmar o mecanismo além de “flag presente + remoção observada”).

**Classificação (§6 HANDOFF): `HIDDEN` (não `BARE`, não `POWER_GATED`).**

- `BARE` exigiria `IOPCIDevice` presente sem driver — NÃO observado (0 nós).
- `POWER_GATED` exigiria `_OFF`/power-state evidência — NÃO observada; o log mostra
  remoção lógica pelo bridge, não power-gate ACPI.
- `HIDDEN` = presente eletricamente no boot, removida por software antes do matching.
  **Tensão central confirmada:** sem remover `-wegnoegpu` (recria o problema de boot original),
  nenhum teste TinyGPU/NVDev/MMIO é possível; com a flag, a RTX é invisível para DriverKit.

## 3. Controle AMD `1002:1638` — presente, display, `built-in=YES`

| Campo | Valor literal | Fonte |
|---|---|---|
| `vendor-id` / `device-id` | `0x1002` (`<02100000>` LE) / `0x1638` (`<38160000>` LE) | `ioreg -l -w0 -c IOPCIDevice` linha ~5292 |
| `class-code` | `0x030000` (`<00000300>`) | idem |
| `subsys` | `1458:d000` (`<58140000>:<00d00000>`) | idem + `sysprof` |
| `revision-id` | `0xc9` (`<c9000000>`) | `ioreg` + `sysprof Revision 0x00c9` |
| `built-in` | `YES` (presente `<00>`) | `ioreg`: `"built-in" = <00>` |
| `IOPCITunnelled` | `ABSENT` (0 ocorrências globais) | `grep -c = 0` |
| `IOPCITunnelCompatible` | `Yes` nas personalities do sistema (11 ocorrências, ex. `AMDSupport`); **não** é propriedade do nó GPU | `ioreg` |
| `AAPL,slot-name` | `"built-in"` | `ioreg` |
| `IOServiceDEXTEntitlements` | `(("com.apple.developer.driverkit.transport.pci"))` | `ioreg` |
| `acpi-path` / `pcidebug` | `IOACPIPlane:/_SB/PCI0@0/GP17@80001/VGA@0` / `8:0:0` | `ioreg` |
| `model` / `compatible` | `AMD Radeon RX Renoir Graphics` / `pci1458,d000,pci1002,1638,pciclass,030000,VGA,IGPU` | `ioreg` |
| `IOPowerManagement` | `Children=2 Current=2 Caps=258 Max=3` | `ioreg` |
| `current provider` (sem detach) | Filho `AMDSupport (IOProbeScore 65050, IOPCIMatch 0x00001002&0x0000FFFF)` → `AMDRadeonX6000_AmdGpuWrangler (probe 60000)` → framebuffer/controller; `systemextensionsctl` sem dext; `log --predicate tinygpu` vazio | `ioreg` + `kextstat` + `log` |

## 4. `current provider` — quem detém cada nó (runbook §4, sem detach)

- `10de:2504`: nenhum — removida no bridge, sem `IOPCIDevice`, sem filho `TinyGPUDriver`,
  sem serviço `"tinygpu"`, `log --predicate tinygpu/TinyGPU --last 1h` vazio (só eco do próprio `log`).
- `1002:1638`: `AMDSupport` + `AMDRadeonX6000Framebuffer/X5000` (display ativo).
- `systemextensionsctl list`: `0 extension(s)` — MISS administrativo esperado, não evidência B/C.
- Nenhum `matchPropertyTable ... built-in ... isMatch=false` no log (sem dext para logar).

## 5. `tinygpu-match-check` (port Node — python3 sem CLT neste Tahoe)

`python3` = `/usr/bin/python3` exige CLT ausente (`xcode-select: note: No developer tools were found`).
Port fiel em `/tmp/macos-tgm0/match-check.js` (mesmas constantes do `.py`), `--self-test: ALL PASS`.

- `snapshot-rtx-hidden.json` (observado: `10DE:2504/030000`, `built-in UNKNOWN`, `tunnelled UNKNOWN`) → **`INDETERMINATE`**
  (`built-in UNKNOWN → gate de produção não decidível`; tunnel informativo apenas).
- `snapshot-amd-1638.json` (controle: `1002:1638/030000`, `built-in YES`, `tunnelled ABSENT`) → **`LIKELY_NO_MATCH`**
  (perna `built-in` LIKELY bloqueia sem isenção `...builtin`; sustenta `RISK`, NÃO instalar).
- `snapshot-rtx-hypothetical-bare.json` (hipótese BARE `ABSENT/ABSENT`) → **`LIKELY_MATCH`**
  (se a RTX fosse BARE sem `built-in`, casaria classe+vendor; prova que o bloqueador atual
  é o HIDDEN, não o matching em si).

Snapshots JSON + saídas em `~/Documents/nvidia-macos-TGM0/artifacts-macos/`
(3 JSON + 4 TXT). Formato segue o `--help`/cabeçalho do script.

## 6. Checklist de sessão válida (runbook §5)

1. [x] Desktop AMD confirmado antes de qualquer comando (§0).
2. [x] `1b+1c+1d` executados + campos §2 para **AMD presente** + **RTX ausente+HIDDEN com prova de log** + build `25G83`.
3. [ ] Snapshots em `artifacts/macos/` — **PENDENTE (repo read-only NTFS)**; crus em `/tmp/macos-tgm0/`, sanitizados em `~/Documents/...`. `git status` não executável para `artifacts/` (FS read-only).
4. [x] §4 respondido sem nenhum comando de alteração.
5. [x] Nenhum `TinyGPU.app` instalado/ativado (`systemextensionsctl 0`, sem `developer on/reset/uninstall`).

## 7. Próximo milestone (UM, sem implementar driver)

**UM = tornar a RTX BARE sem perder o boot (pré-requisito para qualquer teste A/B/C).**
Sem isso, `internal-pcie-gap.md` Resultado A/B/C permanece intestável e `MAC-COMPUTE-0` permanece `LOW`
(com viés para `BLOCKED-por-HIDDEN` até o UM fechar). Opções a documentar em `docs/research/`
(uma por vez, sem alterar NVRAM/OpenCore nesta fase): (a) ler `EFI/OC/config.plist` (read-only,
EFI não montada aqui) para achar SSDT/`DeviceProperties`/`boot-args` que removem a RTX;
(b) plano de boot descartável sem `-wegnoegpu` com monitor só na placa-mãe + `IGD primário`
+ log serial; (c) se BARE for atingido, repetir este runbook e só então decidir B/C via
`built-in` vivo. **NÃO remover a flag sem revisão + plano de recovery (`docs/lab/recovery-plan.md`).**
