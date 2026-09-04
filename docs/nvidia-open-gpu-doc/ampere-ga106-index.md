> Status: Fase 0 — pesquisa documental via websearch + leitura de cache em `/tmp/opencode/nvidia-ref-doc` em 2026-09-04. Nenhum código copiado; só referências (doc→cobertura→link+commit) e trechos de 1–3 linhas quando essencial. Cache fora do repo, nunca dentro dele.

# Ampere / GA106 — índice da documentação pública (`NVIDIA/open-gpu-doc`)

## 0. Source estudado

| Item | Valor |
|---|---|
| Repo | `NVIDIA/open-gpu-doc` — <https://github.com/NVIDIA/open-gpu-doc> · view HTML <https://nvidia.github.io/open-gpu-doc/> |
| HEAD estudado | `9fdf5c4062007929d9f4e6cbad9c9771fe61b880` (`2026-06-09`, `John Hubbard`, `video classes: add CEB6 (VIC) class and type headers`) |
| Link do HEAD | <https://github.com/NVIDIA/open-gpu-doc/commit/9fdf5c4062007929d9f4e6cbad9c9771fe61b880> |
| Commit histórico-chave (3D) | `6fa752daf81f1789d1abff0b43768b211ef1fd01` (`3D class header files for Fermi through Ampere`) — <https://github.com/NVIDIA/open-gpu-doc/commit/6fa752daf81f1789d1abff0b43768b211ef1fd01> |
| Cache local (somente leitura) | `/tmp/opencode/nvidia-ref-doc` (`git clone --depth 1 …`; `rev-parse HEAD` = `9fdf5c4…`) |
| Relação com driver `610.57.04` | **Independente**: open-gpu-doc não tem tag por driver; é documentação de interface de hardware versionada por commit. O mapa do driver está em `docs/nvidia-open/GA106-kernel-map.md` (tag `610.57.04`, SHA `e4a5faa…`). Usar os dois juntos: este índice = "o que o HW garante", o outro = "o que o RM open implementa". |

Base de links deste documento: `https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/<path>` (estável por SHA; `master` muda).

## 1. Mapa geral do repo (o que existe)

```
ampere/host/          interrupt map Ampere
classes/3d/           headers 3D por geração (cl9097…clce97) + README.txt  ← AMPERE_B aqui
classes/compute/      headers compute (clc7c0 = AMPERE_COMPUTE_B …)
classes/dma-copy/     headers CE copy (clc7b5 = AMPERE_DMA_COPY_B …)
classes/host/         channels/GPFIFO/usermode (clc56f = AMPERE_CHANNEL_GPFIFO_A …)
classes/display/      display (clc57x Turing + clc67x Ampere parcial)
classes/video/        NVDEC/NVENC/OFA (clc7b0/clc7b7 …)
classes/{twod,inline-to-memory,memory-to-memory-format}/
manuals/ampere/{ga100,ga102}/  manuais de referência de registradores por chip
{BIOS-Information-Table,DCB,Devinit,Falcon-Security,Shader-Program-Header,
 MME-MacroMethodExpander,MemoryClockTable,MemoryTweakTable,Display-CRC,…}/
```

README do repo: <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/README.md>

## 2. Índice aplicável a Ampere / GA106

| # | Doc | Cobertura para GA106 | Link (commit `9fdf5c4`) |
|---|---|---|---|
| 1 | `classes/3d/README.txt` | **Tabela classe→GPU**: `AMPERE_A = clc697.h (ga100)` vs `AMPERE_B = clc797.h (ga102/103/104/106/107/t234/t239)` | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/README.txt> |
| 2 | `classes/3d/clc797.h` (+ `clc797tex.h`, `clc797sph.h`) | **Métodos 3D AMPERE_B** (`NVC797_*`, 4827 linhas) + layout de textura/sampler + shader-program-header da classe | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clc797.h> |
| 3 | `classes/3d/clc697.h` | Contraste: `AMPERE_A` exclusivo `ga100` — **não** usar para GA106 (evita confusão GA100 vs GA10x) | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clc697.h> |
| 4 | `classes/compute/clc7c0.h` (+ `clc7c0qmd.h`) | Compute `AMPERE_COMPUTE_B` (QMD): backend de compute da GA106 (par de `AMPERE_B` no `ENG_GR`) | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/compute/clc7c0.h> |
| 5 | `classes/dma-copy/clc7b5.h` (família `clc*b5.h`) | CE copy `AMPERE_DMA_COPY_B` (×5 CEs na GA106 segundo `g_gpu_class_list.c`) | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/dma-copy/clc7b5.h> |
| 6 | `classes/host/clc56f.h` (+ `clc561.h` **só no SDK do driver**, ausente neste repo — ver lacunas) | `AMPERE_CHANNEL_GPFIFO_A (0xc56f)` confirmado aqui; `AMPERE_USERMODE_A (0xc561)` existe no SDK (`src/common/sdk/nvidia/inc/class/clc561.h`, tag `610.57.04`) mas sem correspondente em `classes/host/` | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/host/clc56f.h> |
| 7 | `classes/display/` (`clc571/573/57a/57b/57d/57e.h` + `clc671/673/67a/67b/67d/67e.h`) | Display: série `57x` (Turing, ainda na lista GA106 por compat) + série `67x` (Ampere). **Atenção**: `clc670.h` (NVC670 allocation) existe no SDK do driver mas **não** neste dir | <https://github.com/NVIDIA/open-gpu-doc/tree/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/display> |
| 8 | `classes/video/clc7b0.h`, `clc7b7.h` (+ `nvdec_drv.h`, `nvenc_drv.h`) | NVDEC (`NVC7B0`) / NVENC (`NVC7B7`) usados pela GA106 (×2 NVDEC + NVENC na class-list) | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/video/clc7b0.h> |
| 9 | `manuals/ampere/ga100/` (`dev_boot/bus/ctrl/ctxsw/func/pbdma/ram/runlist/timer/vm.ref.txt`, `pri_*`) | Referência de registradores/engines da base Ampere (GA100): BOOT, BUS, CTRL, CTXSW, PBDMA, RAM/FC, RUNLIST, VM, FE/MME/SKED — leitura de fundo para GA106 (que reusa `ga100` nos HALs) | <https://github.com/NVIDIA/open-gpu-doc/tree/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/manuals/ampere/ga100> · HTML <https://nvidia.github.io/open-gpu-doc/manuals/ampere/ga100/> |
| 10 | `manuals/ampere/ga102/` (`dev_ce/ctxsw/display_withoffset/hdacodec/trim/vm/nv_xve…`) | Delta GA102 (o mais próximo da GA106 publicamente): CE, display com offset, trim, VM, XVE PCI — **é o manual a priorizar para GA106** junto com `ga100` | <https://github.com/NVIDIA/open-gpu-doc/tree/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/manuals/ampere/ga102> · HTML <https://nvidia.github.io/open-gpu-doc/manuals/ampere/ga102/> |
| 11 | `ampere/host/ampere_interrupt_map.csv` | Mapa de interrupções Ampere (host): útil para `nv.c`/`nv-pci.c`/ISR antes do GSP assumir | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/ampere/host/ampere_interrupt_map.csv> |
| 12 | `Shader-Program-Header/Shader-Program-Header.html` (+ `classes/3d/clc797sph.h`) | Header de shader program (carga de shaders 3D/compute, par do `clc797sph.h`) | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/Shader-Program-Header/Shader-Program-Header.html> |
| 13 | `BIOS-Information-Table/BIOS-Information-Table.html` | BIT: parsing de VBIOS (tabelas de boot/clock); base para `kernel_gsp_vbios`/`devinit` | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/BIOS-Information-Table/BIOS-Information-Table.html> |
| 14 | `DCB/DCB-4.x-Specification.html` | DCB 4.x (display configuration block no VBIOS): conectores/heads para `kern_disp` | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/DCB/DCB-4.x-Specification.html> |
| 15 | `Devinit/` | Devinit scripts do VBIOS (sequências de init de HW) | <https://github.com/NVIDIA/open-gpu-doc/tree/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/Devinit> |
| 16 | `Falcon-Security/Falcon-Security.html` | Modelo de segurança Falcon/SEC2 (cadeia de boot, FWSEC): fundo de `kernel_sec2/kernel_falcon/kernel_gsp_fwsec` | <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/Falcon-Security/Falcon-Security.html> |
| 17 | `MME-MacroMethodExpander/` | MME (macro method expander do FE): expande macros 3D (`NVC797_LOAD_MME_*`) | <https://github.com/NVIDIA/open-gpu-doc/tree/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/MME-MacroMethodExpander> |
| 18 | `MemoryClockTable/`, `MemoryTweakTable/`, `virtual-p-state-table/` | Clocks de memória/tweaks/p-states (GDDR6 da GA106 192-bit): contexto para `mem_mgr`/`mem_ctrl` | <https://github.com/NVIDIA/open-gpu-doc/tree/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/MemoryClockTable> |
| 19 | `classes/{twod,inline-to-memory,memory-to-memory-format}/` (`cl902d.h`, `cla040/cla140.h`, `cl5039.h`…) | 2D/IMEM/M2MF (`FERMI_TWOD_A`, `KEPLER_INLINE_TO_MEMORY_B` na class-list GA106): blits/clears auxiliares | <https://github.com/NVIDIA/open-gpu-doc/tree/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/twod> |
| 20 | `Display-CRC/`, `pascal/`, `volta/`, `turing/` | CRC de display + dirs históricos: só como contraste geracional (não aplicar registrador Pascal/Turing direto à GA106) | <https://github.com/NVIDIA/open-gpu-doc/tree/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/Display-CRC> |

## 3. Confirmação: GA106 usa a classe AMPERE_B — SIM

Três evidências independentes, todas pinadas:

1. **Tabela pública classe→GPU** (`classes/3d/README.txt`, introduzida em `6fa752d`, vigente em `9fdf5c4`):

   ```text
   Class Name:                 AMPERE_B
   3D Class Header:            clc797.h
   3D Class Texture Header:    clc797tex.h
   GPUs:                       ga102, ga103, ga104, ga106, ga107, t234, t239
   ```

   - Link vigente: <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/README.txt>
   - Link do commit que introduziu: <https://github.com/NVIDIA/open-gpu-doc/blob/6fa752daf81f1789d1abff0b43768b211ef1fd01/classes/3d/README.txt>
   - Header: <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clc797.h> · textura: <https://github.com/NVIDIA/open-gpu-doc/blob/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/classes/3d/clc797tex.h>
2. **Driver open, tag `610.57.04`**: `gpuGetEngClassDescriptorList_GA106` lista `{ AMPERE_B, ENG_GR(0) }` — <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/generated/g_gpu_class_list.c> — e `g_allclasses.h` define `AMPERE_B (0xc797)` — <https://github.com/NVIDIA/open-gpu-kernel-modules/blob/610.57.04/src/nvidia/generated/g_allclasses.h>. Ver `docs/nvidia-open/GA106-kernel-map.md` §3.
3. **Negativa de confusão**: `AMPERE_A` (`clc697.h`) é só `ga100` no mesmo README — GA106 **não** é `AMPERE_A`.

## 4. PCI / FIFO / MMU / display — onde ler no open-gpu-doc

- **PCI**: sem doc de PCI dedicado neste repo para GA106; o canônico público é `manuals/ampere/ga102/dev_nv_xve3g_fn*.ref.txt` (XVE = PCI Express virtual): <https://github.com/NVIDIA/open-gpu-doc/tree/9fdf5c4062007929d9f4e6cbad9c9771fe61b880/manuals/ampere/ga102> (complementa `kernel_bif_ga10x.c` + `nv-pci.c` do outro mapa).
- **FIFO**: `manuals/ampere/ga100/` (`dev_pbdma/dev_ram/dev_runlist/dev_ctxsw.ref.txt`, `pri_sk…/pri_eng`) + delta `ga102/dev_ctxsw.ref.txt`. Cobre PBDMA, RAMFC/RAMRL, runlists — base de `AMPERE_CHANNEL_GPFIFO_A`.
- **Memory/MMU**: `manuals/ampere/ga100/dev_vm.ref.txt` + `ga102/dev_vm.ref.txt` + `ga100/dev_ram.ref.txt`. Não há `dev_vm` de GA106 dedicado — usar `ga102` como proxy (mesma geração GA10x não-GA100).
- **Display/3D**: display em `manuals/ampere/ga102/dev_display_withoffset.ref.txt` + `classes/display/` acima; 3D em `classes/3d/clc797.h` + `Shader-Program-Header`.
- **Shader headers**: `Shader-Program-Header.html` + `clc797sph.h` (repo) + `clc797tex.h` (textura/sampler).
- **BIOS/DCB**: `BIOS-Information-Table.html` + `DCB-4.x-Specification.html` (genéricos, sem seção GA106 dedicada — ver lacunas).
- **Falcon-GSP**: `Falcon-Security.html` (modelo) + `Devinit/` (scripts). Não há doc GSP-RM protocol neste repo — GSP é inferido pelo driver (`kernel_gsp*`), não por spec pública aqui.

## 5. Lacunas (UNKNOWN)

- [UNKNOWN] **Sem manual `ga106/`**: só existem `manuals/ampere/{ga100,ga102}/`. Todo registrador GA106 é por proxy (`ga102` preferido, `ga100` base). Diferenças GA106 (mobile `2520/2521`, A2000 `2531/2571`, LHR `2504`) não têm spec aqui.
- [UNKNOWN] `clc670.h` (NVC670 display allocation) ausente em `classes/display/` (só `671/673/67a–e` + `57x`); fluxo modeset completo da GA106 não fecha só com este repo.
- [UNKNOWN] `AMPERE_USERMODE_A` (`0xc561`): header `clc561.h` existe no SDK do driver mas **não** em `classes/host/` (só `clc56f.h` + família `cl*6f.h`); fluxo usermode fecha pelo SDK, não por este repo.
- [UNKNOWN] Video: `classes/video/` tem `clc7b0/clc7b7` (NVDEC/NVENC GA106) mas OFA (`NVC7FA`) não tem header correspondente óbvio neste repo — verificar se OFA é documentado ou só SDK.
- [UNKNOWN] GSP-RM protocol, FRTS/WPR2 GA106, `bigpage/kind/bar2` por SKU: fora do escopo deste repo; dependem de `linux-firmware/nvidia/ga106` + VBIOS de bancada.
- [UNKNOWN] `pascal/` e `volta/` contêm docs que às vezes são reutilizados por herança (ex. `NV50_MEMORY_VIRTUAL`, `VOLTA_CHANNEL_GPFIFO_A` ainda na class-list GA106) — quais seções ainda valem para GA106 exige checagem caso a caso, não assumida aqui.
