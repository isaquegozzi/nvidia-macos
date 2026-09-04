> Status: Fase 0 — compilado de fontes upstream via websearch/webfetch em 2026-09-04. Sem código copiado, apenas referências. Seções "Não verificado" exigem confirmação no código-fonte local.

# Nouveau (kernel drm/nouveau) — Ampere discovery (GA10x / GA106)

Referências arquivo → função → papel (sem código copiado).

## 1. Identificação / probe PCI

| Arquivo (caminho completo no kernel) | Função / símbolo | Papel | Commit/tag observado |
|---|---|---|---|
| `drivers/gpu/drm/nouveau/nouveau_drm.c` | `nouveau_drm_pci_table` / `nouveau_drm_probe` | Tabela genérica `PCI_ANY_ID` + probe via `nvkm_device_pci_new` | `v6.6` + `master@986c24e0` (2026-09-04) |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/pci.c` | `nvkm_device_pci_10de*[]`, helpers BAR0/BAR1/BAR2 | `resource_idx` / `addr` / `size` / `irq` | `v6.6` + `master@986c24e0` (2026-09-04) |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/base.c` | `nv170_chipset` (GA100) / `nv172_chipset` (GA102) / `nv173_chipset` (GA103) / `nv174_chipset` (GA104) / `nv176_chipset` (GA106) / `nv177_chipset` (GA107) — dispatch `case 0x176` | Dispatch real; GA106 (`nv176`, `.name="GA106"`) reusa construtores `ga102_*`/`ga100_*`/`tu102_*` (ver lista na §8) | origem `46741e4f` (2021-11-18) · `v6.6` · `master@986c24e0` (2026-09-04) |
| `drivers/gpu/drm/nouveau/include/nvkm/core/device.h` | `struct nvkm_device` / `chip`, `NVKM_BAR0_PRI` / `BAR1_FB` / `BAR2_INST`, `nvkm_rd32` / `wr32` | Identidade, BARs, acessores MMIO | `v6.6` + `master@986c24e0` (2026-09-04) |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/ga102.c` | `ga102_gsp_new`, `NVKM_GSP_FIRMWARE_BOOTER(ga106, 535.113.01)` + `NVKM_GSP_FIRMWARE_BOOTER(ga106, 570.144)` (duas linhas distintas) | GSP-RM em GA106 (r535 + r570) | `v6.6` + `master@986c24e0` (2026-09-04) |

Fontes:

- <https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nouveau_drm.c>
- <https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/drm/nouveau/nvkm/engine/device/pci.c>
- <https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c>
- <https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/drm/nouveau/include/nvkm/core/device.h>
- <https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/drm/nouveau/nvkm/subdev/gsp/ga102.c>

## 2. Structs / construtores GA10x (engines)

| Arquivo (caminho completo no kernel) | Símbolo | Papel | Commit/tag observado |
|---|---|---|---|
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ga102.c` | `ga102_gr_new` | `AMPERE_B` / `AMPERE_COMPUTE_B` | `v6.6` + `master@986c24e0` (2026-09-04) |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/ga102.c` | `ga102_fifo_new` | `AMPERE_CHANNEL_GPFIFO_A`; desvia `r535_fifo` sob GSP | `v6.6` + `master@986c24e0` (2026-09-04) |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/ga102.c` | `ga102_ce_new` | `AMPERE_DMA_COPY_A` / `B`; `-ENODEV` sob GSP | `v6.6` + `master@986c24e0` (2026-09-04) |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/ga102.c` | `ga102_disp_new` | `GA102_DISP`; desvia `r535_disp` | `v6.6` + `master@986c24e0` (2026-09-04) |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ga102.c` | `ga102_fb_new` / `vidmem_size` | Reg `0x1183a4`; scrubber `nvidia/ga106/nvdec/scrubber.bin` | `v6.6` + `master@986c24e0` (2026-09-04) |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec2/ga102.c` | `ga102_sec2_new` | Secure Engine 2 (HS falcon) | `v6.6` + `master@986c24e0` (2026-09-04) |

## 3. Init / GSP-RM

- `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/tu102.c` :: `tu102_gsp_load_rm` / `oneinit` / `init` / `fini` / `tu102_gsp_booter_load` / `tu102_gsp_booter_unload`
- `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/rm.c` :: `r535_rm_ga102` (libos3) vs `r535_rm_tu102` (libos2)
  - <https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/rm.c>
- `r535.c` — núcleo RPC `cmdq` / `msgq`
  - <https://docs.kernel.org/gpu/nouveau.html>
- Série GSP-RM 535.54.04:
  - <https://lists.freedesktop.org/archives/nouveau/2023-September/043193.html>
- Firmwares `nvidia/ga106/{gr,sec2,nvdec,acr,gsp}`:
  - <https://gitlab.com/kernel-firmware/linux-firmware/-/tree/main/nvidia/ga106?ref_type=heads>

## 4. Memória / BAR / MMIO / VM

- `drivers/gpu/drm/nouveau/nvkm/subdev/bar/base.c` :: `nvkm_bar_ctor` / `bar1` / `bar2_vmm`
- `drivers/gpu/drm/nouveau/nvkm/subdev/bar/tu102.c` :: `tu102_bar_new` (reg confirmado `0xb80f40`; `48`/`50` não confirmados — ver §8; usado por GA10x)
- `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/tu102.c` :: `tu102_mmu_new` (`dma_bits=47`)
- `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmm.c` :: `nvkm_vmm_new_` / `get` / `put` / `map` / `boot` / `join`

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
- ~~Valor numérico do chipset GA106 (esperado `0x176`; só nomes vistos).~~ Verificado na Fase 0.5: `case 0x176 → nv176_chipset` (`.name="GA106"`) em `base.c` — ver §8.
- `instmem` / `devinit` / `vmm` exatos de `nv176_chipset` (antes grafado incorretamente `ga106_chipset`; ver §8 para a lista completa).
- `drm_sched` no FIFO Ampere.
- KMS `dispnv50` sob GSP.
- GSP default 535 vs 570.

## 8. Correções Fase 0.5 (2026-09-04, somente leitura + webfetch upstream)

Conclusão GA106: o símbolo exato é **`nv176_chipset = { .name = "GA106" }`**
em **`drivers/gpu/drm/nouveau/nvkm/engine/device/base.c`**, com dispatch
`case 0x176: device->chip = &nv176_chipset;`. **Não existe `ga106_chipset`.**

- Origem: commit [`46741e4f593ff1bd0e4a140ab7e566701946484b`](https://github.com/torvalds/linux/commit/46741e4f593ff1bd0e4a140ab7e566701946484b) — "drm/nouveau: recognise GA106" (Ben Skeggs, 2021-11-18).
- Tag estável verificada: [`v6.6`](https://raw.githubusercontent.com/torvalds/linux/v6.6/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c) (`nv176_chipset` na linha ~2687, `case 0x176` na linha ~3164).
- Master observado: [`base.c @ master`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c) em `master@986c24e0` (2026-09-04) — `nv176_chipset` (~linha 2627), `case 0x176` (~linha 3349). Contexto descrito (sem copiar o arquivo): bloco `static const struct nvkm_device_chip nv176_chipset` seguido de ~25 campos `.subdev = { mask, *_new }`, depois o `switch (boot0)` que mapeia `0x170/172/173/174/176/177` para `nv170/172/173/174/176/177_chipset`.
- GA103 também segue o padrão numérico: `nv173_chipset` (`.name="GA103"`) — não `ga103_chipset`.

`nv176_chipset` (GA106) liga, em `v6.6` e `master@986c24e0` (master com `nvdec` máscara `0x03` vs `0x01` na v6.6; demais iguais):

| Campo | `*_new` ligado |
|---|---|
| `acr` | `ga102_acr_new` |
| `bar` | `tu102_bar_new` |
| `bios` | `nvkm_bios_new` |
| `devinit` | `ga100_devinit_new` |
| `fault` | `tu102_fault_new` |
| `fb` | `ga102_fb_new` |
| `gpio` | `ga102_gpio_new` |
| `gsp` | `ga102_gsp_new` |
| `i2c` | `gm200_i2c_new` |
| `imem` | `nv50_instmem_new` |
| `ltc` | `ga102_ltc_new` |
| `mc` | `ga100_mc_new` |
| `mmu` | `tu102_mmu_new` |
| `pci` | `gp100_pci_new` |
| `privring` | `gm200_privring_new` |
| `timer` | `gk20a_timer_new` |
| `top` | `ga100_top_new` |
| `vfn` | `ga100_vfn_new` |
| `ce` (`0x1f`) | `ga102_ce_new` |
| `disp` | `ga102_disp_new` |
| `dma` | `gv100_dma_new` |
| `fifo` | `ga102_fifo_new` |
| `gr` | `ga102_gr_new` |
| `nvdec` | `ga102_nvdec_new` |
| `sec2` | `ga102_sec2_new` |

Erros corrigidos (antes → depois), sem apagar o histórico:

1. §1: `ga100 / ga102 / ga103 / ga104 / ga106 / ga107_chipset` → `nv170 / nv172 / nv173 / nv174 / nv176 / nv177_chipset` (nomes reais verificados; GA106 = `nv176`, `.name="GA106"`, `case 0x176`).
2. §1: `include/nvkm/core/device.h` → `drivers/gpu/drm/nouveau/include/nvkm/core/device.h` (prefixo kernel-root restituído; idem `nvkm/...` → `drivers/gpu/drm/nouveau/nvkm/...`).
3. §1: `NVKM_GSP_FIRMWARE_BOOTER(ga106, 535.113.01 / 570.144)` → duas linhas distintas `BOOTER(ga106, 535.113.01)` + `BOOTER(ga106, 570.144)` (ga102.c ~linhas 196/202) mais entradas `{…, &r535_rm_ga102, "535.113.01"}` / `{…, &r570_rm_ga102, "570.144"}` (~linhas 180-181).
4. §2: `AMPERE_B` / `COMPUTE_B` (gr) → `AMPERE_B` / `AMPERE_COMPUTE_B` (nome exato em `gr/ga102.c`).
5. §3: `tu102_gsp_load` / `booter_load` / `unload` → `tu102_gsp_load_rm` / `tu102_gsp_booter_load` / `tu102_gsp_booter_unload` (nomes exatos em `gsp/tu102.c`); `vs tu102 (libos2)` → `vs r535_rm_tu102 (libos2)`.
6. §4: regs `0xb80f40 / 48 / 50` → só `0xb80f40` confirmado em `bar/tu102.c`; `48`/`50` seguem não verificados.
7. §7: `ga106_chipset` → `nv176_chipset`; valor `0x176` promovido a verificado.
8. Referências `master` sem SHA: adicionada coluna "Commit/tag observado" (`v6.6` + `master@986c24e0`, origem `46741e4f`).
