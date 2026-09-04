> Status: Fase 0 — compilado de fontes upstream via websearch/webfetch em 2026-09-04. Sem código copiado, apenas referências. Seções "Não verificado" exigem confirmação no código-fonte local.

# NVK (Mesa Vulkan) — Ampere discovery

NVK não duplica tabela PCI do kernel; consome `struct nv_device_info` preenchida pelo backend (`nvkmd` / libdrm + ioctl).

## 1. Identificação (backend → `nv_device_info`)

| Arquivo | Símbolo | Papel | Commit/tag observado |
|---|---|---|---|
| `src/nouveau/headers/nv_device_info.h` | `struct nv_device_info` (`chipset` / `device_id` / `sm` / `cls_eng3d` / `cls_copy` / `cls_m2mf` / `cls_compute` / `cls_gpfifo` / `vram_size_B` / `bar_size_B`) | Identidade da GPU consumida pelo NVK | Mesa `main` (link sem SHA pinado — segue sem commit) |
| `src/nouveau/vulkan/nvk_physical_device.c` | `version` / `conformant` / `extensions` / `features` / `properties` com gates por `cls_eng3d` | Vulkan 1.4 exige `TURING_A+`; `hostImageCopy` Turing+ | Mesa `main` (link sem SHA pinado — segue sem commit) |

Lado kernel (verificado na Fase 0.5): GA106 = chipset `0x176` ↔ `nv176_chipset` (`.name="GA106"`) em `drivers/gpu/drm/nouveau/nvkm/engine/device/base.c` — origem [`46741e4f`](https://github.com/torvalds/linux/commit/46741e4f593ff1bd0e4a140ab7e566701946484b) (2021-11-18), [`v6.6`](https://raw.githubusercontent.com/torvalds/linux/v6.6/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c), `master@986c24e0` (2026-09-04).

Fontes:

- <https://github.com/mirror/mesa/blob/main/src/nouveau/headers/nv_device_info.h>
- <https://github.com/mirror/mesa/blob/main/src/nouveau/vulkan/nvk_physical_device.c>
- <https://docs.mesa3d.org/drivers/nvk.html> (kernel ≥ 6.6, Ampere suportado, Mesa 25.1 NVK+Zink default Turing+)
- <https://nouveau.freedesktop.org/>

## 2. Classes de engine

- Headers `cl90c0` / `cl91c0` / `cla097` / `clc997` / `clcdc0` / `clce97` (família A/B; `AMPERE_B` no kernel).
- Kernel: `AMPERE_DMA_COPY_A` / `B`, `AMPERE_B` / `COMPUTE_B`, `AMPERE_CHANNEL_GPFIFO_A`, `GA102_DISP`.
- `src/nouveau/vulkan/nvk_queue.c` :: `nvk_queue_create` / `destroy` / `submit` / `push` / `exec` / `bind` / `init_context_state` (`nvkmd_ctx_wait` / `exec` / `signal`, pushbufs `P_MTHD` / `P_IMMD` `NV9097` / `NVA0C0`):
  - <https://github.com/mirror/mesa/blob/main/src/nouveau/vulkan/nvk_queue.c>

## 3. Memória

- `vram_size_B` / `bar_size_B` / `nc_atom_size`, `bufferImageGranularity` `0x400` se `>= MAXWELL_B`, sparse, `minUniformBufferOffsetAlignment`.
- Backend kernel `nvkm_vmm_*` / `tu102_mmu` / `tu102_bar`.

## 4. Display / WSI

- `KHR_swapchain` / `EXT_display_control` sobre KMS Nouveau (`dispnv50` + `ga102` disp), sem tabela PCI própria.

## 5. Não verificado

Exige confirmação no código-fonte local:

- Arquivo que preenche `chipset` via ioctl (`nvkmd_nouveau` / `nouveau_ws`, `DRM_NOUVEAU_GETPARAM`?).
- ~~Chipset `0x176` ↔ GA106 e `sm 0x86` no Mesa.~~ Parcialmente verificado na Fase 0.5: `0x176` ↔ GA106 confirmado no kernel (`nv176_chipset`, `case 0x176`, origem `46741e4f`, `v6.6`, `master@986c24e0`); `sm 0x86` no Mesa segue não verificado.
- Backend `nvkmd` `ctx_exec` / `mem` / `vm_bind` / `syncobj`.
- NAK `sm_86`.

## 6. Correções Fase 0.5 (2026-09-04, somente leitura + webfetch upstream)

1. §1: adicionada coluna "Commit/tag observado". Lado Mesa segue sem SHA pinado (links `main` são alvo móvel) — registrado como pendência, sem inventar hash.
2. §5: `0x176 ↔ GA106` promovido a verificado no lado kernel (`drivers/gpu/drm/nouveau/nvkm/engine/device/base.c`: `nv176_chipset`, `.name="GA106"`, `case 0x176`; origem [`46741e4f`](https://github.com/torvalds/linux/commit/46741e4f593ff1bd0e4a140ab7e566701946484b), `v6.6`, `master@986c24e0` 2026-09-04). `sm_86`/preenchimento via ioctl seguem não verificados.
3. Nenhum símbolo `ga106_chipset` existe no kernel; o NVK não possui tabela PCI própria (consome `nv_device_info` do backend) — afirmação mantida, agora com o mapeamento kernel explícito acima.
