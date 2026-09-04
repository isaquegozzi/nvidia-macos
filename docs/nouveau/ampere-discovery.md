> Status: Fase 0 — compilado de fontes upstream via websearch/webfetch em 2026-09-04. Sem código copiado, apenas referências. Seções "Não verificado" exigem confirmação no código-fonte local.

# Nouveau (kernel drm/nouveau) — Ampere discovery (GA10x / GA106)

Referências arquivo → função → papel (sem código copiado).

## 1. Identificação / probe PCI

| Arquivo | Função / símbolo | Papel |
|---|---|---|
| `drivers/gpu/drm/nouveau/nouveau_drm.c` | `nouveau_drm_pci_table` / `nouveau_drm_probe` | Tabela genérica `PCI_ANY_ID` + probe via `nvkm_device_pci_new` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/pci.c` | `nvkm_device_pci_10de[]`, helpers BAR0/BAR1/BAR2 | `resource_idx` / `addr` / `size` / `irq` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/base.c` | `ga100` / `ga102` / `ga103` / `ga104` / `ga106` / `ga107_chipset` | Dispatch real; GA106 reusa `ga102_*` |
| `include/nvkm/core/device.h` | `struct nvkm_device` / `chip`, `NVKM_BAR0_PRI` / `BAR1_FB` / `BAR2_INST`, `nvkm_rd32` / `wr32` | Identidade, BARs, acessores MMIO |
| `nvkm/subdev/gsp/ga102.c` | `ga102_gsp_new`, `NVKM_GSP_FIRMWARE_BOOTER(ga106, 535.113.01 / 570.144)` | GSP-RM em GA106 |

Fontes:

- <https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nouveau_drm.c>
- <https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/drm/nouveau/nvkm/engine/device/pci.c>
- <https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c>
- <https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/drm/nouveau/include/nvkm/core/device.h>
- <https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/drm/nouveau/nvkm/subdev/gsp/ga102.c>

## 2. Structs / construtores GA10x (engines)

| Arquivo | Símbolo | Papel |
|---|---|---|
| `nvkm/engine/gr/ga102.c` | `ga102_gr_new` | `AMPERE_B` / `COMPUTE_B` |
| `nvkm/engine/fifo/ga102.c` | `ga102_fifo_new` | `AMPERE_CHANNEL_GPFIFO_A`; desvia `r535_fifo` sob GSP |
| `nvkm/engine/ce/ga102.c` | `ga102_ce_new` | `AMPERE_DMA_COPY_A` / `B`; `-ENODEV` sob GSP |
| `nvkm/engine/disp/ga102.c` | `ga102_disp_new` | `GA102_DISP`; desvia `r535_disp` |
| `nvkm/subdev/fb/ga102.c` | `ga102_fb_new` / `vidmem_size` | Reg `0x1183a4`; scrubber `nvidia/ga106/nvdec/scrubber.bin` |
| `nvkm/engine/sec2/ga102.c` | `ga102_sec2_new` | Secure Engine 2 (HS falcon) |

## 3. Init / GSP-RM

- `nvkm/subdev/gsp/tu102.c` :: `tu102_gsp_load` / `oneinit` / `init` / `fini` / `booter_load` / `unload`
- `nvkm/subdev/gsp/rm/r535/rm.c` :: `r535_rm_ga102` (libos3) vs `tu102` (libos2)
  - <https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/rm.c>
- `r535.c` — núcleo RPC `cmdq` / `msgq`
  - <https://docs.kernel.org/gpu/nouveau.html>
- Série GSP-RM 535.54.04:
  - <https://lists.freedesktop.org/archives/nouveau/2023-September/043193.html>
- Firmwares `nvidia/ga106/{gr,sec2,nvdec,acr,gsp}`:
  - <https://gitlab.com/kernel-firmware/linux-firmware/-/tree/main/nvidia/ga106?ref_type=heads>

## 4. Memória / BAR / MMIO / VM

- `nvkm/subdev/bar/base.c` :: `nvkm_bar_ctor` / `bar1` / `bar2_vmm`
- `nvkm/subdev/bar/tu102.c` :: `tu102_bar_new` (regs `0xb80f40` / `48` / `50`, usado por GA10x)
- `nvkm/subdev/mmu/tu102.c` :: `tu102_mmu_new` (`dma_bits=47`)
- `nvkm/subdev/mmu/vmm.c` :: `nvkm_vmm_new_` / `get` / `put` / `map` / `boot` / `join`

## 5. Submissão / engines / sync

- `fifo` / `gr` / `ce` `ga102 _new` + shims `r535_*` (`ce` / `fifo` / `gr` / `nvdec` / `nvenc` / `nvjpg` / `ofa` / `sec2` / `disp` / `mmu`) — série:
  - <https://lists.freedesktop.org/archives/nouveau/2023-September/043193.html>

## 6. Display

- `nvkm/engine/disp/ga102.c` :: `ga102_disp_new` / `sor_dp_links` / `sor_clock`
- `dispnv50/disp.c` :: `nv50_disp_atomic_*` / `mst` / `sor` (série display-rework):
  - <https://lists.freedesktop.org/archives/nouveau/2023-September/043192.html>
- Wiki:
  - <https://nouveau.freedesktop.org/>

## 7. Não verificado

Exige confirmação no código-fonte local:

- Corpo de `nvkm_device_pci_new` e caminho `boot0` → chipset.
- Valor numérico do chipset GA106 (esperado `0x176`; só nomes vistos).
- `instmem` / `devinit` / `vmm` exatos de `ga106_chipset`.
- `drm_sched` no FIFO Ampere.
- KMS `dispnv50` sob GSP.
- GSP default 535 vs 570.
