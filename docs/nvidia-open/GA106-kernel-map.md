> Status: Fase 0 — pesquisa documental via websearch + leitura de cache em `/tmp/opencode/nvidia-ref-kernel` em 2026-09-04. Nenhum código copiado; só referências (arquivo→símbolo→papel→link+tag) e trechos de 1–3 linhas quando essencial. Clones usados só como cache fora do repo, nunca dentro dele. Driver NVIDIA não modificado/compilado.

# GA106 — mapa da implementação no kernel open (Ampere)

## 0. Driver instalado vs source estudado

| Item | Valor |
|---|---|
| Driver instalado (máquina) | `610.57.04`, flavor **NVIDIA Open Kernel Module** (Turing+ / Ampere por GSP; open é default desde 560) |
| Source estudado | tag **`610.57.04`** em `NVIDIA/open-gpu-kernel-modules` — **match exato, não "mais próxima"** |
| Tag | `610.57.04` — <https://github.com/NVIDIA/open-gpu-kernel-modules/releases> · tarball <https://github.com/NVIDIA/open-gpu-kernel-modules/archive/refs/tags/610.57.04.tar.gz> |
| SHA (commit da tag) | `e4a5faa2567f28c8eabe0ebb6422b6d0abcf37eb` — <https://github.com/NVIDIA/open-gpu-kernel-modules/commit/e4a5faa2567f28c8eabe0ebb6422b6d0abcf37eb> (`2026-08-03`, autor `mmaneetsingh`, +202626/−201532 em 114 arquivos) |
| README no source | declara `This is the source release of the NVIDIA Linux open GPU kernel modules, version 610.57.04.` — <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/README.md> |
| Cache local (somente leitura) | `/tmp/opencode/nvidia-ref-kernel` (`git clone --depth 1 --branch 610.57.04 …`; `rev-parse HEAD` = `e4a5faa…`; `describe --tags` = `610.57.04`) |

### Justificativa da tag/SHA escolhida

A task pedia "a tag/release mais próxima (ex. 610.x)". A pesquisa (websearch `open-gpu-kernel-modules 610.57.04 release tag`) mostrou que **existe release/tag exata `610.57.04`** e que `main` já declara essa versão. Não há motivo para aproximar: estudar qualquer 610.x anterior introduziria delta artificial. SHA pinado acima (`e4a5faa…`) é o commit da tag; todos os links deste documento usam `blob/610.57.04/…` para estabilidade.

## 1. Como ler este mapa

- Base de links kernel: `https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/<path>`.
- "Papel" = por que o arquivo/função importa para trazer GA106 (Ampere, `ARCH=0x17/GA100 IMPL=6`) para cima: probe PCI → identidade HAL → classes RM → engines (GR/FIFO/MMU/CE/GSP/SEC2/Falcon/BIF/DISP/video) → memória/BAR.
- Símbolos GA106/Ampere verificados por `grep` no cache da tag (linhas citadas abaixo batem com a tag; números de linha podem variar ±poucas linhas entre visualizadores).

## 2. Identidade do chip (PCI → HAL → nome)

| # | Arquivo | Símbolo | Papel | Link (tag `610.57.04`) |
|---|---|---|---|---|
| 1 | `kernel-open/nvidia/nv-pci.c` | `nv_pci_probe` (~L1903), `nv_pci_driver` (`.id_table = nv_pci_table`, `.probe = nv_pci_probe`, ~L2974) | Entry PCI de `nvidia.ko`: probe/match, detecta GPU perdida, registra `pci_register_driver` via `nv_pci_register_driver` (~L2997) | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/kernel-open/nvidia/nv-pci.c> |
| 2 | `kernel-open/nvidia/nv-pci-table.c` | `nv_pci_table[]` (match genérico `PCI_VENDOR_ID_NVIDIA` + `PCI_CLASS_DISPLAY_VGA/3D`, não por device-ID) | Tabela PCI do RM open: aceita qualquer VGA/3D NVIDIA (Turing+ efetivo via GSP); contraste com Nouveau que também usa `PCI_ANY_ID` (ver `docs/ga106/device-ids.md`) | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/kernel-open/nvidia/nv-pci-table.c> |
| 3 | `src/nvidia/generated/g_hal_archimpl.h` | `chipID[]`: `{ NV_PMC_BOOT_42_ARCHITECTURE_GA100, NV_PMC_BOOT_42_IMPLEMENTATION_6, 0x0 }, // GA106` (~L76) | Mapeamento BOOT42 → chip: GA106 = arch `GA100 (0x17)` + impl `6`; lista ainda TU10x/GA10x/AD10x/GH10x/GB10x/GB20x/GR10x/T23xD/T26xD | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/generated/g_hal_archimpl.h> |
| 4 | `src/common/inc/swref/published/nv_ref.h` | `NV_PMC_BOOT_42_ARCHITECTURE_GA100 (0x17)` (~L180), `NV_PMC_BOOT_42_IMPLEMENTATION_6 (0x06)` (~L161) | Definições publicadas dos valores usados no item 3 | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/common/inc/swref/published/nv_ref.h> |
| 5 | `src/nvidia/generated/g_hal.h` | `HAL_IMPL_GA106` (~L116), `{ HAL_IMPL_GA106, "GA106" }` (~L172) | Enum/nome HAL do chip; chave de dispatch de todos os HALs GA106 | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/generated/g_hal.h> |
| 6 | `src/nvidia/generated/g_chips2halspec_nvoc.c` | `arch == 0x17 && impl == 0x6 → __nvoc_HalVarIdx = 46 // GA106` (~L64) | Liga BOOT42 ao índice HAL GA106 (vizinhos: GA104 `impl 0x4 → 45`, GA107 `impl 0x7 → 47`) | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/generated/g_chips2halspec_nvoc.c> |
| 7 | `src/nvidia/src/kernel/gpu/gpu.c` | `case HAL_IMPL_GA106: *gpuArch = GPU_ARCHITECTURE_AMPERE; *gpuImpl = GPU_IMPLEMENTATION_GA106;` (~L4809) | Classificação arquitetural: GA106 é **Ampere** (não Turing/Ada) | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/src/kernel/gpu/gpu.c> |
| 8 | `src/nvidia/generated/g_nv_name_released.h` | `sChipsReleased[]`: `{ 0x2503, … "NVIDIA GeForce RTX 3060" }` (~L641), `{ 0x2504, … }` (~L642); `{ 0x2487, … "NVIDIA GeForce RTX 3060" }` (~L615, mas é **GA104**, armadilha — ver `docs/ga106/device-ids.md`) | Nome comercial por device-ID (strings de `nvidia.ko`); confirma `2503/2504` = RTX 3060 GA106 | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/generated/g_nv_name_released.h> |

Trecho essencial (1–3 linhas, identidade GA106):

```c
{ NV_PMC_BOOT_42_ARCHITECTURE_GA100, NV_PMC_BOOT_42_IMPLEMENTATION_6,   0x0 } , // GA106
```

```c
case HAL_IMPL_GA106: { *gpuArch = GPU_ARCHITECTURE_AMPERE; *gpuImpl = GPU_IMPLEMENTATION_GA106; break; }
```

## 3. Classes RM da GA106 (contrato userspace ↔ kernel)

| # | Arquivo | Símbolo | Papel | Link (tag `610.57.04`) |
|---|---|---|---|---|
| 9 | `src/nvidia/generated/g_gpu_class_list.c` | `gpuGetEngClassDescriptorList_GA106` (~L1133), `gpuGetNoEngClassList_GA106` (~L1080) | **Lista master de classes da GA106**: ~60 descritores com engine (`ENG_GR/FIFO/CE/DISPLAY/…`); é o que `NV0080_CTRL_GPU_CLASSLIST` expõe | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/generated/g_gpu_class_list.c> |
| 10 | `src/nvidia/generated/g_allclasses.h` | `AMPERE_B (0xc797)` (~L1115), `AMPERE_COMPUTE_B (0xc7c0)` (~L1119), `AMPERE_DMA_COPY_B (0xc7b5)` (~L1063), `AMPERE_CHANNEL_GPFIFO_A (0xc56f)` (~L556), `AMPERE_USERMODE_A (0xc561)` (~L591) | IDs numéricos das classes Ampere; `g_allclasses.h` inclui `class/clc797.h`, `clc7c0.h`, `clc7b5.h`, `clc56f.h`, `clc561.h` | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/generated/g_allclasses.h> |
| 11 | `src/common/sdk/nvidia/inc/class/clc797.h` | `NVC797_*` (`NVC797_SET_OBJECT 0x0000`, `NO_OPERATION 0x0100`, …; 4481 linhas) | Métodos da classe 3D **AMPERE_B** no SDK do driver; espelha `open-gpu-doc/classes/3d/clc797.h` (4827 linhas — delta de versão a investigar, não copiar) | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/common/sdk/nvidia/inc/class/clc797.h> |
| 12 | `src/common/sdk/nvidia/inc/class/` (família) | `clc7b5.h` (DMA_COPY_B), `clc7c0.h` (COMPUTE_B), `clc670.h/clc671.h/clc673.h/clc67a–e.h` (display NVC670), `clc7b0.h` (NVDEC), `clc7b7.h` (NVENC), `clc7fa.h` (OFA) | Headers das demais classes que aparecem na lista GA106 do item 9 | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/common/sdk/nvidia/inc/class> |

Trecho essencial (lista GA106, item 9):

```c
{ AMPERE_B, ENG_GR(0) }, { AMPERE_COMPUTE_B, ENG_GR(0) },
{ AMPERE_DMA_COPY_B, ENG_CE(0..4) }, { AMPERE_CHANNEL_GPFIFO_A, ENG_KERNEL_FIFO },
```

> Confirmação **AMPERE_B**: `gpuGetEngClassDescriptorList_GA106` contém `{ AMPERE_B, ENG_GR(0) }` como classe 3D do `ENG_GR(0)`. Detalhe completo (header `clc797.h` + GPUs cobertas + commit) em `docs/nvidia-open-gpu-doc/ampere-ga106-index.md` §3.

## 4. Engines (implementação por bloco)

| # | Arquivo(s) | Símbolo / papel | Link (tag `610.57.04`) |
|---|---|---|---|
| 13 | `src/nvidia/src/kernel/gpu/gr/arch/ampere/kgraphics_ga100.c`, `kgrmgr_ga100.c` | GR (graphics/SM): init/gerência compartilhado GA10x; backend da classe `AMPERE_B`/`AMPERE_COMPUTE_B` | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/gr/arch/ampere> |
| 14 | `src/nvidia/src/kernel/gpu/fifo/arch/ampere/kernel_fifo_ga100.c`, `kernel_fifo_ga102.c`, `kernel_channel_ga100.c`, `kernel_channel_ga10b.c` | FIFO/scheduler/canals: `AMPERE_CHANNEL_GPFIFO_A` + `TURING/VOLTA_CHANNEL_GPFIFO_A` (compat listada no item 9); runlist/PBDMA | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/fifo/arch/ampere> |
| 15 | `src/nvidia/src/kernel/gpu/mmu/arch/ampere/kern_gmmu_ga100.c`, `kern_gmmu_fmt_ga10x.c`, (`kern_gmmu_ga10b.c`) + `src/nvidia/src/kernel/gpu/mmu/{kern_gmmu.c,mmu_fault_buffer.c,fault_buffer_ctrl.c,bar2_walk.c}` | MMU/GMMU GA10x + formatos + fault buffer (`MMU_FAULT_BUFFER` na lista GA106); BAR2 walk; base de `NV50_MEMORY_VIRTUAL`/`FERMI_VASPACE_A` | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/mmu/arch/ampere> |
| 16 | `src/nvidia/src/kernel/gpu/ce/arch/ampere/kernel_ce_ga100.c`, `kernel_ce_ga102.c` (+ `src/nvidia/src/kernel/gpu/ce/kernel_ce*.c`) | CE (copy engines 0–4): backend de `AMPERE_DMA_COPY_B` ×5 na lista GA106 | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/ce/arch/ampere> |
| 17 | `src/nvidia/src/kernel/gpu/gsp/kernel_gsp.c` (+ `kernel_gsp_booter.c`, `kernel_gsp_fwsec.c`) + `arch/ampere/kernel_gsp_ga100.c`, `kernel_gsp_ga102.c`, `kernel_gsp_falcon_ga102.c` | GSP-RM: RPC/infra, booter, FWSEC; **GA106 usa o caminho `ga102`** (não há `kernel_gsp_ga106.c` — só `ga100`/`ga102` no dir `ampere/`); obrigatório em Turing+ e base do open flavor | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/gsp/arch/ampere> · <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/src/kernel/gpu/gsp/kernel_gsp.c> |
| 18 | `src/nvidia/src/kernel/gpu/sec2/arch/ampere/kernel_sec2_ga102.c`, `kernel_sec2_ga100.c` + `falcon/arch/ampere/kernel_falcon_ga100.c`, `kernel_falcon_ga102.c` | SEC2 + Falcon: segurança/boot (cadeia ACR/FWSEC); GA106 cai no par `ga100/ga102` (sem `ga106` dedicado) | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/sec2/arch/ampere> · <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/falcon/arch/ampere> |
| 19 | `src/nvidia/src/kernel/gpu/bif/arch/ampere/kernel_bif_ga100.c`, `kernel_bif_ga102.c` | BIF/PCIe: link/configuração PCIe4 x16 da GA106 (via `ga100/ga102`) | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/bif/arch/ampere> |
| 20 | `src/nvidia/src/kernel/gpu/disp/kern_disp.c` (+ `disp_objs.c`, `disp_channel.c`, `head/`, `inst_mem/`) | Display core (KMD): serve `NV04_DISPLAY_COMMON` + `NVC670_DISPLAY` + `NVC67A/B/D/E` + `NVC372_DISPLAY_SW` da lista GA106 | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/src/kernel/gpu/disp/kern_disp.c> |
| 21 | `src/nvidia/src/kernel/gpu/{nvdec,nvenc,ofa}/` (`kernel_nvdec_ctx.c`, `kernel_nvenc_ctx.c`, `kernel_ofa_ctx.c` + engdesc) | Vídeo/OFA: `NVC7B0_VIDEO_DECODER` (NVDEC ×2), `NVC7B7_VIDEO_ENCODER` (NVENC), `NVC7FA_VIDEO_OFA` — batem com a lista GA106 | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/nvdec> · <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/nvenc> · <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/ofa> |
| 22 | `src/nvidia/src/kernel/gpu/mem_mgr/arch/ampere/mem_mgr_ga100.c`, `mem_mgr_ga102.c` (+ `mem_mgr.c`, `mem_mgr_gsp_client.c`, `heap.c`, `dma.c`) | Memória local/BAR/heap + cliente GSP do mem-mgr; `GF100_ZBC_CLEAR`, `GP100_UVM_SW` da lista passam por aqui | <https://github.com/NVIDIA/open-gpu-kernel-modules/tree/610.57.04/src/nvidia/src/kernel/gpu/mem_mgr/arch/ampere> |
| 23 | `kernel-open/nvidia/nv.c`, `nv-mmap.c`, `nv-dma.c`, `nv-vm.c` | `nvidia.ko` core + mmap/DMA/VM da interface kernel (BAR0/1, system memory, UVM bridge) | <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/kernel-open/nvidia/nv.c> · <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/kernel-open/nvidia/nv-mmap.c> |

Notas de leitura:

- Não existe `*ga106.c` na maioria dos engines: o padrão Ampere no open-RM é implementar `ga100` (base) + `ga102` (delta) e reutilizar para GA103/104/**106**/107. Evidência: `gsp/arch/ampere/` tem só `kernel_gsp_ga100.c`/`kernel_gsp_ga102.c`/`kernel_gsp_falcon_ga102.c`; `fifo/arch/ampere/` só `ga100/ga102/ga10b`; `gr/arch/ampere/` só `ga100`. **GA106 = configuração/tabela + HAL index, não arquivo dedicado.**
- Display: a lista GA106 mistura `NVC57x` (Turing display, compat) + `NVC670` (Ampere display). Os headers NVC670 vivem no SDK (`clc670.h` etc.) — ver lacunas §5 sobre `clc670.h` ausente no open-gpu-doc.

## 5. Lacunas (UNKNOWN)

- [UNKNOWN] `2505`/`2544` e stepping GA106-300 vs 302 ↔ `2503`/`2504`: `g_nv_name_released.h` na tag só dá nome comercial, sem stepping/subsystem; exige bancada/subsystem IDs.
- [UNKNOWN] GPC/TPC/SM/L2 exatos da GA106-300-A1 via open-RM: `kgraphics/kgrmgr_ga100.c` são compartilhados GA10x; contagens por SKU provavelmente vêm de VBIOS/GSP, não de `#define` GA106 neste repo.
- [UNKNOWN] `clc797.h` SDK (4481 linhas) vs `open-gpu-doc` (4827 linhas): delta de geração/versão não explicado; qual usar como referência de método deve ser decidido em `research/` antes de qualquer tabela local.
- [UNKNOWN] Display `clc670.h` (NVC670 allocation params) existe no SDK mas **não** em `open-gpu-doc/classes/display/` (só `clc671/673/67a–e` + série `clc57x`); fluxo exato de modeset GA106 no open-RM ainda por mapear.
- [UNKNOWN] GSP firmware blobs: open repo tem `g_bindata_kgsp*` gerados, mas os binários GSP/SEC2/GR (booter, bootloader, NET_img, fecs/gpccs, scrubber, ACR ucode) distribuídos via `linux-firmware/nvidia/ga106` não estão versionados aqui; pinar versão GSP usada com `610.57.04` exige inventário separado (ver `docs/ampere/architecture-overview.md` §5).
- [UNKNOWN] `NV_CHIP_CTRL` (citado na task como exemplo): não há símbolo com esse nome exato no tree da tag (busca retorna só `osapi.c` incidental); equivalente provável é `NV0080_CTRL_GPU_*` / `ctrl0080gpu.h` + `NV_PMC_BOOT_42_*` — confirmar antes de referenciar em código.
