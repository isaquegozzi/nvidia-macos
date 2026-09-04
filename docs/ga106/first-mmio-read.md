> Status: Fase 0 — documento de candidatura, somente leitura. NENHUMA leitura MMIO foi
> executada, implementada ou proposta como código neste commit. Sem código copiado:
> apenas nomes, offsets, campos e links + commit/tag. Qualquer leitura futura exige
> `docs/SAFETY.md` + `docs/SAFETY_MMIO_PREREQUISITES.md` desbloqueado + revisão explícita.
> Data: 2026-09-04.

# GA106 — first MMIO read candidate: `NV_PMC_BOOT_0` via BAR0

Candidato único para a eventual primeira leitura MMIO (quando — e se — a Fase 0
deixar de ser somente leitura): ler 32 bits em `BAR0 + 0x00000000`
(`NV_PMC_BOOT_0`) e validar o padrão `0x176xxxxx` da GA106.

Este documento consolida: (1) veredito de reuse de `common/` no macOS,
(2) confirmação upstream de `NV_PMC_BOOT_0` + valor esperado GA106,
(3) riscos e (4) pré-requisitos (boot sem NVIDIA + SSH + desktop independente).
**NÃO implementa leitura.**

## 0. Escopo / não-escopo

- Escopo: documentar *qual* registrador ler primeiro, *por que* ele é o mais
  seguro, *o que* esperar e *o que* precisa estar pronto antes.
- Não-escopo (proibido neste commit, ver `README.md` raiz + `macos/README.md`):
  - nenhum `.cpp`/`.hpp` novo ou alterado em `common/`, `linux/` ou `macos/`;
  - nenhum `mmap`, `O_RDWR`, `/dev/mem`, `ioremap`, `nvkm_rd32`, `IOMemoryMap`;
  - nenhum unbind/bind, reset, clock, firmware, power-state;
  - nenhum código DriverKit/IOKit/PCIDriverKit.

## 1. Veredito reuse de `common/` no macOS

**Veredito: LIMPO de código — reutilizável sem dependências Linux.**

| Tipo | Arquivo | Reutilizável macOS? | Notas |
|---|---|---|---|
| `NvidiaPciBar` | `common/include/nvidia/pci_bar.hpp` | SIM | Só `<cstdint>`; `constexpr is_valid()/is_memory()`; sem OS. |
| `NvidiaPciDevice` | `common/include/nvidia/pci_device.hpp` | SIM | Só `<cstdint> <optional> <string> <vector>` + `pci_bar.hpp`; `is_nvidia()`, `bar(i)` puros. |
| `NvidiaDeviceInfo` | `common/include/nvidia/device_info.hpp` | SIM, com ressalva de tabela (§1.1) | Agregado + `looks_like_ga106()` puro; sem OS. |
| `pci_types.hpp` | `common/include/nvidia/pci_types.hpp` | SIM | Umbrella, só re-exporta. |
| helpers | `common/src/common.cpp` | SIM | Só headers acima + `<cstdint> <optional> <string> <vector>`; `phase0_version()`, `format_vendor_device()`, `count_valid_bars()` sem IO/estado global. |
| build | `common/CMakeLists.txt` | SIM | `cxx_std_20`, `-Wall -Wextra -Wpedantic` sob `GNU|Clang`; sem lib de OS; `add_library(nvidia_common STATIC)`. |

Detalhe completo (DriverKit C++ vs IOKit C++ vs helper userspace):
`docs/macos/common-reuse.md`.

### 1.1 Acoplamento sysfs/Linux — só comentários, zero código

`grep` em `common/` (`sysfs|linux|unistd|fcntl|mmap|/sys|/proc|/dev|libpci|IOKit|DriverKit|sys/`)
retorna **4 matches, todos inócuos**:

- `common/include/nvidia/pci_device.hpp:3` — comentário:
  `// Dispositivo PCI identificado passivamente (lspci / sysfs, O_RDONLY).`
- `common/include/nvidia/pci_bar.hpp:11` — comentário de campo:
  `// bus address lido via sysfs (somente leitura)`
- `common/src/common.cpp:5`, `common/include/nvidia/pci_types.hpp:6` — falsos
  positivos (`#include "nvidia/device_info.hpp"` contém `device_info` ≈ `dev`+`info`,
  casado pelo padrão `/dev/`).

Nenhum `#include` de sistema além do portável (`<cstdint> <optional> <string> <vector>`).
Nenhum `open/mmap/ioctl/sysfs/config/resource*` no código. **Declaração: sem
acoplamento de código; apenas menções em comentários.**

### 1.2 Ressalva de tabela (não bloqueia reuse, exige alinhamento)

`device_info.hpp:20-31` (`looks_like_ga106()`) lista:

```cpp
case 0x2487:  // GA106M (laptop)
case 0x2503:
case 0x2504:
case 0x24AA:
```

Divergência vs `docs/ga106/device-ids.md` (fonte `pci-ids.ucw.cz` + mapa open-RM):

- `0x2487` está em `device-ids.md` §2 como **armadilha NÃO-GA106 (die real GA104,
  nome comercial RTX 3060)**. O comentário `GA106M` no header conflita com o doc.
- `0x24AA` não consta na lista verificada (`2501/2503/2504/2505/2507/2508/2509/
  2520/2521/2523/252f/2531/2544/2571` + HDA `228e`); origem UNKNOWN.
- Faltam no `switch` IDs verificados (`2501/2505/2507/2508/2509/2520/2521/2523/
  252f/2531/2544/2571`); hoje caem no fallback `chip_name == "GA106"`.

Impacto: heurística pode classificar GA104 como GA106 e vice-versa. Como o
fallback por `chip_name` existe e o código é puro/OS-agnóstico, o reuse continua
válido — mas **antes de qualquer uso da heurística para gating de MMIO**, alinhar
a tabela com `device-ids.md` em commit de docs separado. Não feito aqui (doc only).

## 2. `NV_PMC_BOOT_0` — confirmado

| Item | Valor confirmado |
|---|---|
| Nome exato | `NV_PMC_BOOT_0` |
| Offset | `0x00000000` (base BAR0/PRI) |
| Largura | 32-bit (`R--4R` = read-only, 4 bytes) |
| Semântica | Chip/device ID: arquitetura + implementação + revisão do chip |
| Efeitos colaterais de leitura | **Nenhum conhecido**: registrador `R--` (não `clear-on-read`, não dispara engine, não altera estado); é a primeira leitura que Nouveau e RM fazem após `ioremap` BAR0, sem lock/sequência especial |
| Valor esperado GA106 | `(boot0 & 0x1ff00000) >> 20 == 0x176` → padrão `0x176xxxxx` (ex. rev `a1` → `0x176000A1` aprox.; byte baixo = stepping, varia por placa) |

### 2.1 Campos (tag `610.57.04`, `nv_ref.h`)

Fonte: `src/common/inc/swref/published/nv_ref.h` @ tag `610.57.04`
(SHA `e4a5faa2567f28c8eabe0ebb6422b6d0abcf37eb`):

```c
#define NV_PMC_BOOT_0                0x00000000 /* R--4R */
#define NV_PMC_BOOT_0_MINOR_REVISION        3:0 /* R--VF */
#define NV_PMC_BOOT_0_MAJOR_REVISION        7:4 /* R--VF */
#define NV_PMC_BOOT_0_ARCHITECTURE_1        8:8 /* R--VF */
#define NV_PMC_BOOT_0_IMPLEMENTATION      23:20 /* R--VF */
#define NV_PMC_BOOT_0_ARCHITECTURE_0      28:24 /* R--VF */
#define NV_PMC_BOOT_0_ARCHITECTURE_GA100 0x17   /* R---V */
```

Helpers no mesmo arquivo (`decodePmcBoot0Architecture()` = `ARCH_1<<5 | ARCH_0`)
confirmam que arquitetura é dividida em dois campos; `IMPLEMENTATION_6 (0x06)`
é o valor GA106 (ver §2.3). `NV_PMC_BOOT_1` = `0x00000004`, `NV_PMC_BOOT_42` =
`0x00000A00` — **não** são o candidato (BOOT_1 tem escrita de endianness no
Nouveau; BOOT_42 é o caminho novo do RM, redundante para Ampere).

Mesma definição em `open-gpu-doc` commit `9fdf5c4…`:
`manuals/ampere/ga100/dev_boot.ref.txt` (`NV_PMC_BOOT_0 0x00000000 R--4R`,
mesmos campos `3:0/7:4/23:20/28:24`) e `manuals/ampere/ga102/` como delta.
`R--4R` nas duas fontes = leitura sem escrita associada.

### 2.2 Precedente Nouveau (leitura é o primeiro uso de MMIO)

`drivers/gpu/drm/nouveau/nvkm/engine/device/base.c` (pin §3):

- `boot0 = nvkm_rd32(device, 0x000000);` (≈L3010 em `v6.6`; ≈L3190 em `master`)
- `device->chipset = (boot0 & 0x1ff00000) >> 20;` + `device->chiprev = boot0 & 0xff;`
- `case 0x170: device->card_type = GA100;` (GA106 herda `card_type GA100`)
- `case 0x176: device->chip = &nv176_chipset;` (`.name = "GA106"`)
- Endianness toca `0x04` (`PMC_BOOT_1`), **nunca** `0x00` — BOOT_0 é só lido.

`nv176_chipset` existe desde `46741e4f` ("drm/nouveau: recognise GA106",
2021-11-18); verificado em `v6.6` e `master@986c24e0` (2026-09-04).
Ver `docs/ga106/implementation-map.md` §1 e `initialization-map.md` Etapa 1.

### 2.3 Valor esperado GA106 — derivação (sem bancada, por construção)

- RM open, tag `610.57.04`: `src/nvidia/generated/g_hal_archimpl.h` →
  `{ NV_PMC_BOOT_42_ARCHITECTURE_GA100, NV_PMC_BOOT_42_IMPLEMENTATION_6, 0x0 }, // GA106`;
  `src/nvidia/generated/g_chips2halspec_nvoc.c` → `arch == 0x17 && impl == 0x6 → idx 46 // GA106`;
  `src/nvidia/src/kernel/gpu/gpu.c` → `HAL_IMPL_GA106 → GPU_ARCHITECTURE_AMPERE`.
  Ou seja: GA106 = arquitetura `0x17` + implementação `0x6`.
- Em BOOT_0 os mesmos `arch/impl` ocupam bits `28:20`:
  `(0x17 << 4) | 0x6 = 0x176`. Máscara Nouveau `0x1ff00000>>20` captura exatamente
  `28:20` (9 bits, inclui `ARCH_1` bit 8 via `0x100` extra — zero em Ampere).
- Bits restantes: `31:29` reservados (0), `19:9` reservados (0), `8` = `ARCH_1` (0
  p/ `0x17`), `7:0` = revisão (`MAJOR:MINOR`; bancada = rev `a1` ⇒ `0xA1` —
  ver `SAFETY_MMIO_PREREQUISITES.md` item 1: `01:00.0 [10de:2504] rev a1`).
- Composição: `(0x176 << 20) | rev` = `0x17600000 | rev`.
  **Esperado aprox.: `0x176000A1` para rev `a1`; padrão geral `0x176xxxxx`.**
  Steppings/revisões mudam só o byte baixo; os 12 bits altos `0x176` são fixos.
  Qualquer valor fora de `0x176xxxxx` (ex. `0xFFFFFFFF` = GPU caída do barramento,
  `0x00000000` = BAR não mapeada) = abortar, não retry cego.

Nota RM moderno: o RM open `610.57.04` roteia HAL por BOOT_42 (`arch 0x17/impl 6`),
mas BOOT_0 decodifica o mesmo `arch/impl` e segue válido em Ampere. O esvaziamento
de BOOT_0 (`boot42` futuro) vale para GPUs **pós-Blackwell** (série Nova, patches
Out/2025) — GA106 não é afetada. Ver §3.

## 3. Referências upstream pinadas (tag/commit, sem código copiado)

| # | Repo | Tag/commit | Arquivo/símbolo | Link estável |
|---|---|---|---|---|
| 1 | `NVIDIA/open-gpu-kernel-modules` | tag `610.57.04`, SHA `e4a5faa2567f28c8eabe0ebb6422b6d0abcf37eb` (2026-08-03) | `src/common/inc/swref/published/nv_ref.h`: `NV_PMC_BOOT_0 0x00000000 R--4R` + campos + `ARCHITECTURE_GA100 0x17` + `IMPLEMENTATION_6` + helpers `decodePmcBoot0*` | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/common/inc/swref/published/nv_ref.h> · commit <https://github.com/NVIDIA/open-gpu-kernel-modules/commit/e4a5faa2567f28c8eabe0ebb6422b6d0abcf37eb> · release <https://github.com/NVIDIA/open-gpu-kernel-modules/releases> |
| 2 | idem | idem | `src/nvidia/generated/g_hal_archimpl.h`: `{ …GA100, …_6, 0x0 }, // GA106` · `src/nvidia/generated/g_chips2halspec_nvoc.c`: `arch 0x17 && impl 0x6 → 46 // GA106` · `src/nvidia/src/kernel/gpu/gpu.c`: `HAL_IMPL_GA106 → AMPERE` | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/generated/g_hal_archimpl.h> |
| 3 | `NVIDIA/open-gpu-doc` | commit `9fdf5c4062007929d9f4e6cbad9c9771fe61b880` (2026-06-09, `John Hubbard`, `video classes: add CEB6…`) | `manuals/ampere/ga100/dev_boot.ref.txt`: `NV_PMC_BOOT_0 0x00000000 R--4R` + mesmos campos; `classes/3d/README.txt`: `AMPERE_B = clc797.h (ga102/103/104/106/107/…)` | <https://github.com/NVIDIA/open-gpu-doc/commit/9fdf5c4062007929d9f4e6cbad9c9771fe61b880> · <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/manuals/ampere/ga100/dev_boot.ref.txt> · <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/README.txt> |
| 4 | `torvalds/linux` (Nouveau) | commit origem `46741e4f593ff1bd0e4a140ab7e566701946484b` ("drm/nouveau: recognise GA106", 2021-11-18) · `v6.6` · `master@986c24e0` (2026-09-04) | `drivers/gpu/drm/nouveau/nvkm/engine/device/base.c`: `nv176_chipset (.name="GA106")`, `boot0=rd32(0x00)`, `(boot0&0x1ff00000)>>20`, `case 0x176`, `case 0x170→GA100` | <https://github.com/torvalds/linux/commit/46741e4f593ff1bd0e4a140ab7e566701946484b> · <https://raw.githubusercontent.com/torvalds/linux/v6.6/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c> · patch <https://lists.freedesktop.org/archives/nouveau/2021-November/039578.html> |
| 5 | Nova (informativo, pós-Blackwell) | Out/2025: `[PATCH 0/2] gpu: nova: add boot42 support` + `[PATCH v2 07/21] fix layout of NV_PMC_BOOT_0` | `NV_PMC_BOOT_0 @ 0x00000000` (`3:0 minor, 7:4 major, 8:8 arch_1, 23:20 impl, 28:24 arch_0`); BOOT_42 futuro só p/ "next-gen" (BOOT_0 zerado) — Ampere segue em BOOT_0 | <https://lists.freedesktop.org/archives/nouveau/2025-October/049873.html> · <https://lists.freedesktop.org/archives/nouveau/2025-May/047335.html> |
| 6 | NvWiki (contexto) | — | `NV3 PMC`: `PMC_BOOT_0` offset `0x0`, desde NV1+ | <https://nvwiki.org/index.php/NV3_PMC> |

Cache local (se usado): fora do repo (`/tmp/opencode/nvidia-ref-kernel`,
`/tmp/opencode/nvidia-ref-doc`); nunca dentro do repo.

## 4. Riscos (mesmo para leitura)

Leitura de 32 bits parece inócua — **não é risco zero**:

1. **Mapear BAR0 errado = hang PCIe.** BAR0 precisa vir de `resource*` já
   mapeado no doc (`SAFETY_MMIO_PREREQUISITES.md` item 2: BAR0 16M/BAR1 16G/BAR3
   32M). Offset fora da BAR ou largura errada (8/16 vs 32) pode gerar `MCA`,
   `Xid 79`, `pmc_boot_0 = 0xffffffff` (GPU caída — precedente RTX 3060 em
   <https://forums.developer.nvidia.com/t/nvrm-gpu-000000-0-gpu-has-fallen-off-the-bus-and-now-pmc-boot-0-0xffffffff/334741>).
2. **Dono do recurso.** Com driver `nvidia` bound + `nvidia-drmdrmfb` como primary
   (estado atual `lab-status`), qualquer `mmap` — mesmo `PROT_READ` — concorre com
   o RM/GSP. Leitura só faz sentido com **display isolado** (§5) e sem Xorg/Wayland
   na target.
3. **Armadilha de endianness.** O precedente Nouveau escreve `0x04` (BOOT_1) se
   CPU/GPU divergirem — em x86_64 LE é no-op, mas copiar o padrão como "leitura"
   e incluir a escrita acidentalmente transforma o candidato em escrita. O
   candidato aqui é **só `0x00`, só leitura, sem RMW**.
4. **Falso positivo de identificação.** `0x176xxxxx` confirma família GA106, não
   SKU (`2503` vs `2504` LHR vs `2509` Rev.2) nem stepping; `0x2487` (GA104) nunca
   deve passar no gate — reforça §1.2.
5. **Irreversibilidade operacional.** Sem SSH + sem display independente, um hang
   (mesmo improvável em leitura) = power-cycle cego. Por isso §5 é gate, não
   sugestão.

Critério de abort (para a futura proposta, não para este doc): valor lido
`!= 0x176xxxxx` (incl. `0x00000000`/`0xFFFFFFFF`), `lspci` sem resposta, `dmesg`
com `Xid`/`fallen off the bus` → parar, anexar log, reverter para estado
somente-PCI/config-space. Sem retry com escrita.

## 5. Pré-requisitos (gate — todos BLOCKED hoje, ver `SAFETY_MMIO_PREREQUISITES.md`)

Estado em 2026-09-04: **`BLOCKED` — 3/11 pronto (itens 1, 2, 6). Fase 0 segue
somente leitura.** A primeira leitura MMIO — mesmo read-only — só pode ser
*proposta* quando os itens abaixo saírem de `BLOCKED` com evidência datada:

| Gate | Item SAFETY | Estado hoje | Evidência / falta |
|---|---|---|---|
| Boot sem NVIDIA | lab `nvidia-free-boot-test` (plano REVERSÍVEL DE UM ÚNICO BOOT, NÃO APLICADO) | `BLOCKED` | Bootloader Limine 12.7.0 + `module_blacklist=` (não só `modprobe.blacklist`); edição `E` no menu, sem persistir. Itens SAFETY 3/4/5/8/9 seguem sem proposta. Ver `docs/lab/nvidia-free-boot-test.md`. |
| SSH | item 10 — acesso remoto B→A testado | `BLOCKED` | `sshd` instalado mas `inactive+disabled`, sem `:22`, sem `authorized_keys`, `ufw` UNKNOWN, alvo `192.168.5.12`. Teste real não executado. Ver `docs/lab/ssh-readiness.md`. |
| Desktop independente | item 11 — iGPU = display, RTX = target | `BLOCKED` (`PARTIALLY_READY`) | boot_vga AMD=1/NVIDIA=0, fb só `amdgpudrmfb`, RTX 0 conectores, `glxinfo` AMD — MAS `gnome-shell` ainda com fds na RTX (`renderD128`+`card1`+`/dev/nvidia*`). "Dependência zero" não provada. Ver `docs/lab/dual-gpu-setup.md` + `docs/lab/igpu-readiness.md`. |
| Docs de proposta | itens 3/4/5/8/9 + `SAFETY.md` por-proposta | `BLOCKED` | Este doc preenche *candidato + fonte + valor esperado*, mas NÃO preenche BDF/BAR físico final, dono final, reversão, revisor. |
| Recuperação | item 7 — `recovery-plan.md` exercitado | `BLOCKED` | Plano criado, nunca exercitado. Ver `docs/lab/recovery-plan.md`. |

Regra de desbloqueio (reafirmação): cada caixa só marca `[x]` com link p/ evidência
datada; marcar sem evidência = violação de revisão.

## 6. Critérios de aceite da futura leitura (quando desbloqueada)

1. BDF + rev + VBIOS + BAR0 físico de `resource*` anexados (só leitura).
2. Uma única leitura 32-bit em `BAR0+0x00` via caminho `PROT_READ` documentado;
   logar valor, `(boot0&0x1ff00000)>>20`, `boot0&0xff`, `lspci -nnv` antes/depois,
   `dmesg` antes/depois.
3. Aceite: valor em `0x176xxxxx` consistente com rev da bancada (`a1` ⇒ `…A1`);
   `lspci` responde; sem `Xid`/hang.
4. Falha: qualquer desvio ⇒ abort + issue em `docs/research/` (beco sem saída
   declarado), sem segunda tentativa com escrita.

## 7. Proibições reafirmadas

Nada neste documento autoriza: `mmap(PROT_WRITE)`, `O_RDWR`, `/dev/mem`,
`nvkm_wr32`/`*volatile*=`, unbind/bind, `vfio-pci` bind, reset/FLR, clocks,
firmware, power-state, command buffer, modeset. Ver `README.md` raiz §"ga106-lab
é SOMENTE LEITURA" + `docs/SAFETY.md` + `macos/README.md` (sem `.kext`/`.dext`
nesta fase).
