> Status: Fase 0 — compilado de fontes upstream via websearch/webfetch em 2026-09-04. Sem código copiado, apenas referências. Seções "Não verificado" exigem confirmação no código-fonte local.

# Ampere — Architecture overview (GA100 / GA102 / GA103 / GA104 / GA106 / GA107)

## 1. Dies

| Die | Processo | Área | Transistores | Produtos típicos |
|---|---|---|---|---|
| GA100 | TSMC 7 nm | 826 mm² | 54.2 B | A100 / A30 |
| GA102 | Samsung 8 nm | 628 mm² | 28.3 B | 3090 / 3080 / 3070 Ti |
| GA103 | — | 496 mm² | 22 B | 3060 Ti (`2414`) |
| GA104 | — | 392 mm² | 17.4 B | 3070 / 3060 Ti / 3060 (`2487`) |
| GA106 | Samsung 8 N | 276 mm² | 12 B | 3060 12 GB (`2501` / `2503` / `2504` / `2544`), 3050, A2000, mobile (`2520` / `2521`) |
| GA107 | — | 200 mm² | 8.7 B | 3050 |

Fontes:

- GA102 whitepaper: <https://www.nvidia.com/content/PDF/nvidia-ampere-ga-102-gpu-architecture-whitepaper-v2.1.pdf>
- GA106: <https://www.techpowerup.com/gpu-specs/nvidia-ga106.g966>

## 2. Blocos

- GPC / TPC / SM, ROPs, GDDR6/X, PCIe4 x16, NVDEC5 (AV1), NVENC7, DP 1.4a / HDMI 2.1.
- Manuais:
  - <https://nvidia.github.io/open-gpu-doc/manuals/ampere/ga102/>
  - <https://github.com/NVIDIA/open-gpu-doc>

## 3. RTX 3060 12 GB (GA106 de referência)

- 3584 CUDA / 112 TMU / 48 ROP / 12 GB 192-bit / 170 W / CUDA 8.6 / Vulkan 1.4 / OGL 4.6:
  - <https://www.techpowerup.com/gpu-specs/geforce-rtx-3060-12-gb.c3682>

## 4. Como o Nouveau modela Ampere

- `drivers/gpu/drm/nouveau/nvkm/engine/device/base.c`: `nv170_chipset` (GA100) / `nv172` (GA102) / `nv173` (GA103) / `nv174` (GA104) / `nv176` (GA106, `case 0x176`) / `nv177` (GA107) → construtores `ga102_*`/`ga100_*`/`tu102_*` (lista exata na §7). Origem GA106: `46741e4f` (2021-11-18); observado `v6.6` + `master@986c24e0` (2026-09-04).
- `drivers/gpu/drm/nouveau/include/nvkm/core/device.h`: identidade / BARs.
- GSP `ga102_gsp_new` / `tu102_gsp_*` / `r535` (`r535_rm_ga102`=libos3 para GA10x incl. GA106 vs `r535_rm_tu102`=libos2; booters `535.113.01` + `570.144`, GA10x libos3). Observado `v6.6` + `master@986c24e0`.
- `bar` tu102 / `fb` ga102 / `mmu` tu102 / `vmm` (+ `acr` ga102, `gpio` ga102, `mc` ga100, `ltc` ga102, `ce`/`fifo`/`gr`/`disp`/`nvdec`/`sec2` ga102, `devinit` ga100, `dma` gv100 — ver §7).
- Engines `fifo` / `gr` / `ce` / `disp` ga102 + shims `r535_*`.
- KMS `dispnv50`.
- Userspace `nvk_physical_device` / `queue` / `device_info` (Vulkan 1.4, kernel ≥ 6.6).

## 5. Firmwares GA106

Inventário: <https://gitlab.com/kernel-firmware/linux-firmware/-/tree/main/nvidia/ga106?ref_type=heads>

- `gr/{fecs_bl,fecs_sig,gpccs_bl,gpccs_sig,NET_img}`
- `sec2/{desc,image,sig}`
- `nvdec/scrubber`
- `acr/{ucode_ahesasc,ucode_asb,ucode_unload}`
- `gsp/{booter_load,booter_unload,bootloader,gsp}-<versões>`

## 6. Não verificado

Exige confirmação no código-fonte local:

- GPC / TPC / SM e L2 exatos da GA106 (só GA102 tem whitepaper: 7 GPC / 42 TPC / 84 SM).
- `bigpage` / `dma` / `bar2` / `kind` de `nv176_chipset` (antes grafado `ga106_chipset`).
- FRTS / WPR2 GA106 vs GA100.
- `nova-core` GA106 = `0x176` é driver Rust separado — só corroboração (sem link/commit pinado; segue não verificado).

## 7. Correções Fase 0.5 (2026-09-04, somente leitura + webfetch upstream)

Símbolo conclusivo: **`nv176_chipset = { .name = "GA106" }`** em `drivers/gpu/drm/nouveau/nvkm/engine/device/base.c`, `case 0x176`. Não existe `ga10*_chipset`/`ga106_chipset`. Origem [`46741e4f593ff1bd0e4a140ab7e566701946484b`](https://github.com/torvalds/linux/commit/46741e4f593ff1bd0e4a140ab7e566701946484b) (2021-11-18); [`v6.6`](https://raw.githubusercontent.com/torvalds/linux/v6.6/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c); [`master@986c24e0`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c) (2026-09-04).

Erros corrigidos (antes → depois):

1. §4: `base.c ga10*_chipset → ga102_*` → `nv170/nv172/nv173/nv174/nv176/nv177_chipset` (GA106=`nv176`).
2. §4: `device.h` → caminho completo `drivers/gpu/drm/nouveau/include/nvkm/core/device.h`.
3. §4: `GSP ga102 / tu102 / r535 (…, GA10x libos3)` → `r535_rm_ga102`=libos3 (GA10x incl. GA106) vs `r535_rm_tu102`=libos2; booters `535.113.01`+`570.144`.
4. §4: `bar tu102 / fb ga102 / mmu tu102 / vmm` → lista completa (acr/gpio/mc/ltc/ce/fifo/gr/disp/nvdec/sec2/devinit/dma — idem tabela do `nouveau/ampere-discovery.md` §8).
5. §6: `ga106_chipset` → `nv176_chipset`.
6. NVDEC5/NVENC7/DP1.4a/HDMI2.1 e números de SM/GPC/TPC da GA106 seguem com fonte TechPowerUp/NVIDIA apenas onde linkado; sem whitepaper GA106 — sem alteração, apenas commit/tag do lado Nouveau adicionados.
