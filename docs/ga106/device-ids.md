> Status: Fase 0 — compilado de fontes upstream via websearch/webfetch em 2026-09-04. Sem código copiado, apenas referências. Seções "Não verificado" exigem confirmação no código-fonte local.

# GA106 / RTX 3060 — Device IDs

Fonte primária: <https://pci-ids.ucw.cz/read/PC/10de>

## 1. GA106 verificados

| Device ID | Die | Nome comercial |
|---|---|---|
| `10de:2501` | GA106 | [RTX 3060] |
| `10de:2503` | GA106 | [RTX 3060] |
| `10de:2504` | GA106 | [RTX 3060 LHR] |
| `10de:2505` | GA106 | — |
| `10de:2507` | GA106 | [3050] |
| `10de:2508` | GA106 | [3050 OEM] |
| `10de:2509` | GA106 | [3060 12GB Rev.2] |
| `10de:2520` | GA106M | [3060 Mobile / Max-Q] |
| `10de:2521` | GA106M | [3060 Laptop] |
| `10de:2523` | GA106M | [3050 Ti Mobile] |
| `10de:252f` | GA106 | [3060 ES] |
| `10de:2531` | GA106 | [RTX A2000] |
| `10de:2544` | GA106 | [RTX 3060] |
| `10de:2571` | GA106 | [A2000 12GB] |

Áudio (HDA):

| Device ID | Die | Função |
|---|---|---|
| `10de:228e` | GA106 | HDA (áudio HDMI/DP) |

## 2. Armadilhas — NÃO-GA106 (nome comercial ≠ die!)

| Device ID | Die real | Nome comercial |
|---|---|---|
| `2487` | GA104 | [RTX 3060] |
| `2488` / `2489` | GA104 | [3070 / 3060 Ti LHR] |
| `24c7` / `24c9` | GA104 | [3060 8GB / 3060 Ti GDDR6X] |
| `2414` | GA103 | [3060 Ti] |
| `2582` / `2583` / `2584` | GA107 | [3050] |

## 3. Corroboração

- GA106-300-A1 12 GB GDDR6 192-bit PCIe4 x16:
  - <https://www.techpowerup.com/gpu-specs/geforce-rtx-3060-12-gb.c3682>
- Die 276 mm² / 12 B transistores, Samsung 8 nm:
  - <https://www.techpowerup.com/gpu-specs/nvidia-ga106.g966>

## 4. Kernel / Mesa (como o ID é — ou não — usado)

| Afirmação | Commit/tag observado |
|---|---|
| `nouveau_drm.c`: tabela ignora device ID (`PCI_ANY_ID`, `PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, PCI_ANY_ID)`). | `v6.6` + `master@986c24e0` (2026-09-04) |
| `nvkm/engine/device/pci.c`: lista só IDs legados. | `v6.6` + `master@986c24e0` (2026-09-04) |
| `base.c` `nv176_chipset` (`.name="GA106"`, `case 0x176`): classifica pós-`boot0`, independente do ID. | origem `46741e4f` (2021-11-18) · `v6.6` · `master@986c24e0` |
| Mesa expõe `device_id` / `chipset`, mas gates usam `cls_eng3d` / `sm`. | Mesa `main` (sem SHA pinado — ver §6) |

## 5. Não verificado

Exige confirmação no código-fonte local / bancada:

- Subsystem IDs.
- `2505` / `2544` — função exata.
- Rev GA106-300 vs 302 ↔ `2503` / `2504`.
- Patch recognise GA106 Nov/2021 (corpo não buscado):
  - <https://lists.freedesktop.org/archives/nouveau/2021-November/039581.html>

## 6. Correções Fase 0.5 (2026-09-04, somente leitura + webfetch upstream)

1. §4: `base.c ga106_chipset` → **`nv176_chipset` (`.name="GA106"`, `case 0x176`)** em `drivers/gpu/drm/nouveau/nvkm/engine/device/base.c`. Não existe `ga106_chipset`. Origem: [`46741e4f593ff1bd0e4a140ab7e566701946484b`](https://github.com/torvalds/linux/commit/46741e4f593ff1bd0e4a140ab7e566701946484b) ("drm/nouveau: recognise GA106", 2021-11-18); verificado em [`v6.6`](https://raw.githubusercontent.com/torvalds/linux/v6.6/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c) e [`master@986c24e0`](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c) (2026-09-04).
2. §4: caminhos curtos `nouveau_drm.c` / `pci.c` / `base.c` → caminhos completos `drivers/gpu/drm/nouveau/nouveau_drm.c`, `.../nvkm/engine/device/pci.c`, `.../nvkm/engine/device/base.c`; adicionada coluna commit/tag observado.
3. O link da lista Nov/2021 permanece como referência histórica; o commit conclusivo é o `46741e4f` acima.
