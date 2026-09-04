> Status: Fase 0 — mapa verificado contra código-fonte upstream (somente leitura).
> Sem código copiado: apenas símbolos, arquivos e links.
> Pin upstream: `torvalds/linux@986c24e0fe44f844b44d365b71ce831947f50298`
> (`master` em 2026-09-04; clone/cache somente em `/tmp/nvidia-macos-research/linux`).

# GA106 — Implementation map (Nouveau `nvkm`)

Tabela mestra: qual implementação cada subsistema da GA106 usa no Nouveau,
de onde ela foi herdada, e o que acontece quando o GSP-RM está ativo.

Base: `nv176_chipset` (`name = "GA106"`, chipset `0x176`, `card_type = GA100`).

Link base (mesmo commit para todas as linhas):
`https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/`

## 1. Dispatch raiz (chipset)

| Subsystem | GA106 implementation | Origem | Símbolo | Arquivo | Link + commit/tag |
|---|---|---|---|---|---|
| device/chipset | `nv176_chipset`, `chipset 0x176`, `card_type GA100` | GA106-specific (tabela) | `nv176_chipset` | `nvkm/engine/device/base.c` | [base.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c) |
| boot0 → chipset | `boot0 & 0x1ff00000) >> 20 == 0x176` → `&nv176_chipset`; `0x170 & 0x1f0` → `card_type GA100` | lógica genérica NVKM | `nvkm_device_ctor` | `nvkm/engine/device/base.c` | [base.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c) |
| PCI probe | `PCI_ANY_ID` + classe display; quirk por subsystem | genérico | `nouveau_drm_pci_table` / `nouveau_drm_probe` / `nvkm_device_pci_new` | `nouveau_drm.c`, `nvkm/engine/device/pci.c` | [nouveau_drm.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nouveau_drm.c) |

## 2. Pedidos na tarefa (BAR, BIOS, devinit, FB, GSP, MMU, PCI, CE, display, FIFO, GR, NVDEC, SEC2)

| Subsystem | GA106 implementation | Origem | Símbolo | Arquivo | Link + commit/tag |
|---|---|---|---|---|---|
| BAR | `tu102_bar_new` (regs `0xb80f48/50`; sob GSP → `r535_bar_new_`) | **reuse TU102** | `tu102_bar_new` | `nvkm/subdev/bar/tu102.c` | [tu102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/bar/tu102.c) |
| BIOS/VBIOS | `nvkm_bios_new` (genérico; sem ramo GSP) | **genérico** (todas as gerações) | `nvkm_bios_new` | `nvkm/subdev/bios/base.c` | [base.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/bios/base.c) |
| devinit | `ga100_devinit_new` (sob GSP → `r535_devinit_new`) | **reuse GA100** (⚠️ hipótese inicial dizia TU102 — corrigido) | `ga100_devinit_new` | `nvkm/subdev/devinit/ga100.c` | [ga100.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/devinit/ga100.c) |
| FB | `ga102_fb_new` (`vidmem_size` = `rd32(0x1183a4) << 20`; sob GSP → `r535_fb_new`) | **reuse GA102** | `ga102_fb_new` | `nvkm/subdev/fb/ga102.c` | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/fb/ga102.c) |
| GSP | `ga102_gsp_new` (booter `ga106` em `535.113.01` **e** `570.144`; preferência `570.144`) | **reuse GA102** | `ga102_gsp_new` | `nvkm/subdev/gsp/ga102.c` | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/gsp/ga102.c) |
| MMU | `tu102_mmu_new` (`dma_bits = 47`; sob GSP → `r535_mmu_new`) | **reuse TU102** | `tu102_mmu_new` | `nvkm/subdev/mmu/tu102.c` | [tu102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/mmu/tu102.c) |
| PCI | `gp100_pci_new` (sem ramo GSP) | **reuse GP100 (Pascal)** | `gp100_pci_new` | `nvkm/subdev/pci/gp100.c` | [gp100.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/pci/gp100.c) |
| CE | `ga102_ce_new`, máscara `0x1f` (5 CEs); sob GSP → **`-ENODEV`** (sem engine nvkm; RM assume via RPC) | **reuse GA102** | `ga102_ce_new` | `nvkm/engine/ce/ga102.c` (+ `nvkm/subdev/gsp/rm/r535/ce.c`) | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/ce/ga102.c) |
| display | `ga102_disp_new` (sob GSP → `r535_disp_new`; KMS em `dispnv50`) | **reuse GA102** | `ga102_disp_new` | `nvkm/engine/disp/ga102.c` | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/disp/ga102.c) |
| FIFO | `ga102_fifo_new` (sob GSP → `r535_fifo_new`) | **reuse GA102** | `ga102_fifo_new` | `nvkm/engine/fifo/ga102.c` | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/fifo/ga102.c) |
| GR | `ga102_gr_new` (sob GSP → **`-ENODEV`**; contexto golden via `r535_gr.c` no RM) | **reuse GA102** | `ga102_gr_new` | `nvkm/engine/gr/ga102.c` (+ `nvkm/subdev/gsp/rm/r535/gr.c`) | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/gr/ga102.c) |
| NVDEC | `ga102_nvdec_new`, máscara `0x03`; sob GSP → **`-ENODEV`** | **reuse GA102** | `ga102_nvdec_new` | `nvkm/engine/nvdec/ga102.c` | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/nvdec/ga102.c) |
| SEC2 | `ga102_sec2_new` (HS falcon, `addr 0x840000`; sob GSP → `r535_sec2_new`; falcon carrega o booter GSP) | **reuse GA102** | `ga102_sec2_new` | `nvkm/engine/sec2/ga102.c` | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/sec2/ga102.c) |

## 3. Extras relevantes (LTC/MC/TOP/GPIO/ACRS + FAULT/DMA/infra antiga)

| Subsystem | GA106 implementation | Origem | Símbolo | Arquivo | Link + commit/tag |
|---|---|---|---|---|---|
| LTC | `ga102_ltc_new` (sob GSP → `-ENODEV`) | **reuse GA102** | `ga102_ltc_new` | `nvkm/subdev/ltc/ga102.c` | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/ltc/ga102.c) |
| MC | `ga100_mc_new` (sob GSP → `-ENODEV`) | **reuse GA100** | `ga100_mc_new` | `nvkm/subdev/mc/ga100.c` | [ga100.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/mc/ga100.c) |
| TOP | `ga100_top_new` (sob GSP → `-ENODEV`; `nvkm_top_parse` no preinit) | **reuse GA100** | `ga100_top_new` | `nvkm/subdev/top/ga100.c` | [ga100.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/top/ga100.c) |
| GPIO | `ga102_gpio_new` (sob GSP → `-ENODEV`) | **reuse GA102** | `ga102_gpio_new` | `nvkm/subdev/gpio/ga102.c` | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/gpio/ga102.c) |
| ACR | `ga102_acr_new` (sob GSP → `-ENODEV`; autenticação assume o RM) | **reuse GA102** (⚠️ hipótese inicial dizia TU102 — corrigido) | `ga102_acr_new` | `nvkm/subdev/acr/ga102.c` | [ga102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/acr/ga102.c) |
| FAULT | `tu102_fault_new` (sob GSP → `-ENODEV`) | **reuse TU102** | `tu102_fault_new` | `nvkm/subdev/fault/tu102.c` | [tu102.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/fault/tu102.c) |
| DMA | `gv100_dma_new` | **reuse GV100 (Volta)** | `gv100_dma_new` | `nvkm/engine/dma/gv100.c` | [gv100.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/dma/gv100.c) |
| VFN | `ga100_vfn_new` | **reuse GA100** | `ga100_vfn_new` | `nvkm/subdev/vfn/ga100.c` | [ga100.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/vfn/ga100.c) |
| I2C | `gm200_i2c_new` | **reuse GM200 (Maxwell)** | `gm200_i2c_new` | `nvkm/subdev/i2c/gm200.c` | [gm200.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/i2c/gm200.c) |
| PRIVRING | `gm200_privring_new` | **reuse GM200 (Maxwell)** | `gm200_privring_new` | `nvkm/subdev/privring/gm200.c` | [gm200.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/privring/gm200.c) |
| TIMER | `gk20a_timer_new` | **reuse GK20A (Kepler mobile)** | `gk20a_timer_new` | `nvkm/subdev/timer/gk20a.c` | [gk20a.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/timer/gk20a.c) |
| INSTMEM | `nv50_instmem_new` | **reuse NV50 (Tesla, 2006)** | `nv50_instmem_new` | `nvkm/subdev/instmem/nv50.c` | [nv50.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/subdev/instmem/nv50.c) |
| NVENC | **ausente** em `nv176_chipset` (sem entrada; ≠ TU102 que tem `tu102_nvenc_new`) | UNKNOWN (provável via GSP-RM) | — | — | UNKNOWN |
| NVJPG / OFA | **ausente** em `nv176_chipset` | UNKNOWN | — | — | UNKNOWN |
| FUSE / MXM / BUS / CLK / THERM / PMU / VOLT / ICCSENSE | **ausente** em `nv176_chipset` (sem entrada) | N/A (não instanciado na GA106) | — | `include/nvkm/core/layout.h` (ordem de init) | [layout.h@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/include/nvkm/core/layout.h) |

## 4. Firmwares GA106 (linux-firmware, nomes — conteúdo binário fora de escopo)

| Subsistema | Arquivos (`nvidia/ga106/...`) | Fonte |
|---|---|---|
| ACR | `acr/ucode_ahesasc.bin`, `acr/ucode_asb.bin`, `acr/ucode_unload.bin` | [file list linux-firmware-nvidia (Arch, 2026-08)](https://archlinux.org/packages/core/any/linux-firmware-nvidia/files) |
| GR | `gr/NET_img.bin`, `gr/fecs_bl.bin`, `gr/fecs_sig.bin`, `gr/gpccs_bl.bin`, `gr/gpccs_sig.bin` | idem |
| GSP | `gsp/{booter_load,booter_unload,bootloader,gsp}-{535.113.01,570.144}.bin` | idem + `NVKM_GSP_FIRMWARE_BOOTER(ga106, …)` em `nvkm/subdev/gsp/ga102.c` |
| NVDEC | `nvdec/scrubber.bin` (VPR scrub; `ga102_fb_oneinit` usa `nvdec[0]->falcon` no caminho nativo) | idem + `MODULE_FIRMWARE("nvidia/ga106/nvdec/scrubber.bin")` em `nvkm/subdev/fb/ga102.c` |
| SEC2 | `sec2/{desc,hs_bl_sig,image,sig}.bin` | idem |

## 5. Correções à hipótese inicial (`docs/nouveau/ampere-discovery.md`)

1. **Chipset numérico CONFIRMADO:** GA106 = `0x176` (`case 0x176: device->chip = &nv176_chipset`), `card_type GA100`.
   Era "não verificado"; agora CONFIRMED em `nvkm/engine/device/base.c`.
2. **`devinit` é `ga100_devinit_new`, não `tu102`** — corrigido (com shim `r535_devinit_new` sob GSP).
3. **`acr` é `ga102_acr_new`, não `tu102`** — corrigido (retorna `-ENODEV` sob GSP).
4. **`mc`/`top`/`vfn` são GA100; `pci` é GP100; `dma` é GV100; `i2c`/`privring` são GM200;
   `timer` é GK20A; `imem` é NV50** — a GA106 reaproveita código com até ~15 anos.
5. **GSP:** duas versões de booter (`535.113.01` fallback, **`570.144` preferida** —
   primeira entrada da tabela `ga102_gsps`). Era "não verificado"; agora CONFIRMED.
6. **GA106 não tem `nvenc`/`nvjpg`/`ofa` no `nv176_chipset`** — TU102 tem; GA10x não.
   Via GSP-RM esses motores são geridos pelo RM (helpers `r535_*`), não por engines nvkm.
7. **Regra GSP (verificada em cada `*_new`):** sob GSP-RM ativo
   (`nvkm_gsp_rm()` = FW RM presente), `fb/fifo/disp/sec2/mmu/bar/devinit` viram shims
   `r535_*`, enquanto `ce/gr/nvdec/acr/gpio/ltc/mc/top/fault` retornam `-ENODEV`
   (sem objeto nvkm — o RM é o dono). `bios/pci/dma/i2c/timer/privring/imem/vfn`
   não têm ramo GSP (operam igual).

## 6. Resumo de reuse (qualitativo)

Contando as 25 entradas instanciadas de `nv176_chipset`:

- **GA102: ~10** (acr, fb, gsp, gpio, ltc, ce, disp, fifo, gr, nvdec, sec2 — 11 com sec2) →
  **~44% do bring-up é GA102 puro**. Todos os motores de computação/mídia/segurança
  e o GSP vêm daqui.
- **GA100: ~5** (devinit, mc, top, vfn + card_type) → ~20%.
- **TU102 (Turing): 3** (bar, mmu, fault) → ~12%, mas são peças críticas
  (janela BAR, page tables, faults).
- **Legado pré-Turing: 6** (pci GP100, dma GV100, i2c/privring GM200, timer GK20A,
  imem NV50) → ~24% — infra estável que atravessa gerações.
- **GA106-specific: ~0** implementações novas; GA106 é 100% composição de
  peças existentes (só a linha da tabela + firmwares assinados por die).

Conclusão: **não existe "driver GA106" no Nouveau — existe o driver GA102
com tabela GA106, fundação Turing (BAR/MMU) e porão pré-Maxwell**.
Qualquer análise de registrador deve começar pelo arquivo da geração de origem,
não por um arquivo `ga106_*` (que não existe no `nvkm`, exceto firmwares).
