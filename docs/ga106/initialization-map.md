> Status: Fase 0 — sequência **corrigida pelo código real** (somente leitura).
> Sem código copiado: apenas símbolos, arquivos e links. **NÃO implementar init.**
> Pin upstream: `torvalds/linux@986c24e0fe44f844b44d365b71ce831947f50298`
> (`master` em 2026-09-04; cache somente em `/tmp/nvidia-macos-research/linux`).
> NVIDIA Open:echo da documentação pública; símbolos internos = UNKNOWN (honesto).

# GA106 — Initialization dependency graph (Nouveau, corrigido pelo código)

Correção central vs. hipótese inicial: **nada de `init` acontece no `probe` PCI**.
O `probe` só faz `ctor` (aloca objetos); o `init` real é **preguiçoso (lazy)** e
**refcount-gated**: roda no primeiro `open`/uso do `udevice`
(`nvkm_udevice_init` → `nvkm_device_init`), e cada subdev só inicializa se houver
usuário (`nvkm_subdev_init` pula com `refcount == 0`; `oneinit` roda uma vez).
Ordem de init = ordem de `include/nvkm/core/layout.h`
(FSP → GSP → TOP → VFN → PCI → VBIOS → DEVINIT → PRIVRING → GPIO → I2C →
… → MC → BUS → TIMER → INSTMEM → FB → LTC → MMU → BAR → FAULT → ACR → … →
engines CE → DISP → DMA → FIFO → GR → … → NVDEC → … → SEC2 → SW).

## Etapa 0 — PCI discovery (antes de qualquer MMIO da GPU)

| Item | Nouveau impl | NVIDIA Open impl | Dependências | MMIO | Firmware? | Riscos | Confidence |
|---|---|---|---|---|---|---|---|
| Match PCI | `nouveau_drm_pci_table`: `PCI_DEVICE(10de, PCI_ANY_ID)` + classe display; `nouveau_drm_probe` (`nouveau_drm.c`) | UNKNOWN (driver `nvidia.ko` tem próprio match PCI; símbolos internos não mapeados) | kernel PCI core, `vga_switcheroo_client_probe_defer` | não | não | bind do driver errado (nvidia vs nouveau se excluem); `aperture_remove_conflicting_pci_devices` remove vesafb/efifb sem volta | CONFIRMED (Nouveau) / UNKNOWN (Open) |

## Etapa 1 — `nvkm_device_pci_new` + `nvkm_device_ctor` (ctor, NÃO init)

| Item | Nouveau impl | NVIDIA Open impl | Dependências | MMIO | Firmware? | Riscos | Confidence |
|---|---|---|---|---|---|---|---|
| `pci_enable_device`, quirk por subsystem | `nvkm_device_pci_new` (`nvkm/engine/device/pci.c`) | UNKNOWN | PCI core | config-space (kernel PCI) | não | BARs inválidas se BIOS/hypervisor mal configurou | CONFIRMED / UNKNOWN |
| Mapear BAR0 (PRI) | `resource_addr(NVKM_BAR0_PRI)` → `ioremap` (`nvkm_device_ctor`, `base.c`) | UNKNOWN | etapa anterior | **read-capaz** (map somente; ainda sem escrita de init) | não | `NULL` → `-ENOMEM`, aborta tudo | CONFIRMED / UNKNOWN |
| Endianness | `nvkm_device_endianness`: lê `PMC_BOOT_1 (0x04)`, escreve se preciso | UNKNOWN | BAR0 mapeada | **read + write** (`0x04`) | não | falha → `-ENOSYS`; em x86_64 LE é no-op na prática | CONFIRMED / UNKNOWN |
| Chip detect | `boot0 = rd32(0x00)` → `chipset 0x176` → `&nv176_chipset`, `card_type GA100` | UNKNOWN | endianness ok | **read** (`0x00`, depois `0x04` boot1 p/ vGPU, `0x101000` strap) | não | `unknown chipset` → `-ENODEV`; vGPU (`boot1 & 0x30000`, ≥TU100) → `-ENODEV` recusado | CONFIRMED / UNKNOWN |
| `ctor` em massa | loop `NVKM_LAYOUT_ONCE/INST` sobre `layout.h`; ramos GSP decididos **aqui** (`nvkm_gsp_rm()` FALSO ainda — FW ainda não carregado, então nascem os `*_new` nativos; shims/`-ENODEV` valem após o GSP subir… ver nota) | UNKNOWN | boot0 válido | não (só alocação) | não | `ctor` falho (≠ `-ENODEV`) aborta o probe | CONFIRMED / UNKNOWN |
| DMA mask | `dma_set_mask_and_coherent(mmu->dma_bits = 47)` (`pci.c`, vindo de `tu102_mmu`) | UNKNOWN | MMU ctor | não | não | fallback p/ 32 bits se falhar | CONFIRMED / UNKNOWN |

> Nota de correção: os ramos `if (nvkm_gsp_rm(device->gsp))` nos `*_new`
> (shim `r535_*` vs `-ENODEV`) são avaliados no `ctor`, mas o GSP-RM só existe
> **depois** do `oneinit/init` do GSP. Na prática, com firmware GSP presente, o
> ciclo `preinit → init` sobe o GSP e os subdevs/engines dependentes resolvem
> para o caminho RM (shim ou ausente). Sem firmware GSP, vale o caminho nativo
> (`ga102_*` + FW `acr/gr/sec2/nvdec/scrubber` de `nvidia/ga106/`).

## Etapa 2 — `nouveau_drm_device_init` (camada DRM, ainda sem init NVKM)

| Item | Nouveau impl | NVIDIA Open impl | Dependências | MMIO | Firmware? | Riscos | Confidence |
|---|---|---|---|---|---|---|---|
| CLI/TTM/BIOS/acel/display | `nouveau_cli_init` → `nouveau_vga_init` → `nouveau_ttm_init` → `nouveau_bios_init` → `nouveau_accel_init` → `nouveau_display_create/init` → debugfs/hwmon/svm/dmem/led → runpm → `drm_dev_register` (`nouveau_drm_device_init`) | UNKNOWN (equiv.: `nvidia-modeset.ko`/NVKMS p/ display — LIKELY pela árvore `src/nvidia-modeset/`; `nvidia-drm.ko` p/ DRM — LIKELY) | `nvkm_device_pci_new` ok | reads via PRI/BIOS; modeset programa display | VBIOS (shadow) | display init falho → `fail_dispinit`; runpm mal configurado = resume quebrado | CONFIRMED (Nouveau) / LIKELY (Open, só divisão por módulo) |

## Etapa 3 — `nvkm_device_init` (lazy, no primeiro uso)

| Item | Nouveau impl | NVIDIA Open impl | Dependências | MMIO | Firmware? | Riscos | Confidence |
|---|---|---|---|---|---|---|---|
| Gatilho | `nvkm_udevice_init`: `refcount 0→1` ⇒ `nvkm_device_init` (`nvkm/engine/device/user.c` + `base.c`) | UNKNOWN | cliente (DRM/KMS, accel, NVK via ABI) abrir o device | não (dispatcher) | não | init concorrente é serializado por `device->mutex` | CONFIRMED / UNKNOWN |
| `preinit` | `nvkm_device_pci_preinit` → `preinit` de cada subdev (ordem `layout.h`) → **`nvkm_devinit_post` → `nvkm_top_parse` → `nvkm_fb_mem_unlock`** (`nvkm_device_preinit`) | UNKNOWN | todos os ctors | **read+write** (POST tables, TOP parse, unlock de VRAM) | VBIOS (POST scripts) | POST errado = estado indefinido; `fb_mem_unlock` falho trava VRAM | CONFIRMED / UNKNOWN |
| `fini(POWEROFF)` + `init` | `nvkm_device_fini(POWEROFF)`; `intr_rearm`; `func->init`; `nvkm_subdev_init` p/ cada subdev **em ordem** (refcount-gated, `oneinit` uma vez) | UNKNOWN | preinit ok | por-subdev (ver etapas 4–7) | por-subdev | qualquer `init` falho ⇒ unwind reverso + `fini(POWEROFF)` | CONFIRMED / UNKNOWN |

## Etapa 4 — GSP sobe (coração do bring-up GA106 com firmware)

| Item | Nouveau impl | NVIDIA Open impl | Dependências | MMIO | Firmware? | Riscos | Confidence |
|---|---|---|---|---|---|---|---|
| `gsp.oneinit` | `tu102_gsp_oneinit`: `vidmem_size` → layout FB (WPR2/heap/meta) → `booter.ctor` **via falcon da SEC2** → `r535_gsp_oneinit` → FRTS → `reset` p/ RISC-V → `libos.addr` em `falcon 0x40/44` (`nvkm/subdev/gsp/tu102.c`) | LIKELY (RM também boota GSP; GSP padrão em Turing+ — docs NVIDIA GSP) | SEC2 (`device->sec2->falcon`), FB (tamanho VRAM) | **read+write** (falcon IMEM/DMEM, `0x1668` no `ga102_gsp_reset`) | **SIM — booter `ga106` (`535.113.01`/`570.144`, pref. `570.144`)** | FW ausente/versão errada = sem RM; WPR2 mal calculado = corrupção de FB; **`docs/SAFETY.md` se aplica a qualquer experimento** | CONFIRMED (Nouveau) / LIKELY (Open) |
| `gsp.init` | `booter_load(mbox0/1)` → `r535_gsp_init`: bootloader → GSP-RM → RPC `cmdq/msgq` (`r535.c`) | LIKELY (mesmo FW GSP, RM proprietário) | oneinit ok; `gsp-*.bin` + `bootloader-*.bin` | **read+write** (mailbox, queues) | **SIM** | RM não responde = GPU sem motores (só PRI); timeout de RPC | CONFIRMED (Nouveau) / LIKELY (Open) |
| Efeito colateral | `nvkm_gsp_rm()==true` ⇒ FB/FIFO/DISP/SEC2/MMU/BAR/DEVINIT viram **shims `r535_*`**; CE/GR/NVDEC/ACR/GPIO/LTC/MC/TOP/FAULT viram **`-ENODEV`** (sem objeto nvkm — dono é o RM) | N/A (no Open tudo já é RM) | GSP-RM no ar | — | — | assumir engine nativa presente sob GSP = bug de análise (foi o erro da hipótese inicial) | CONFIRMED / UNKNOWN |

## Etapa 5 — Memória: FB → BAR → MMU/VMM (ordem de `layout.h`)

| Item | Nouveau impl | NVIDIA Open impl | Dependências | MMIO | Firmware? | Riscos | Confidence |
|---|---|---|---|---|---|---|---|
| FB | `ga102_fb` nativo (`vidmem_size = rd32(0x1183a4)<<20`, scrubber via `nvdec[0]->falcon`) **ou** `r535_fb` sob GSP | UNKNOWN | GSP (decide o ramo); INSTMEM | **read** (`0x1183a4`); scrub **write** em VPR (nativo) | nativo: `nvidia/ga106/nvdec/scrubber.bin` + `sec2/*`; GSP: via RM | tamanho errado = WPR2/heap sobre VRAM útil | CONFIRMED / UNKNOWN |
| BAR | `tu102_bar` (`bar1/bar2` via `0xb80f48/50`; `r535_bar` sob GSP) | UNKNOWN | FB, MMU | **read+write** (`0xb80f48/50`) | não | janela errada = acesso FB corrompido / hang PCIe | CONFIRMED / UNKNOWN |
| MMU/VMM | `tu102_mmu` (`dma_bits 47`, `NVIF_CLASS_VMM_GP100`; `r535_mmu` sob GSP) + `nvkm_vmm_new_/get/put/map/boot/join` | UNKNOWN (equiv. LIKELY: RM + `nvidia-uvm.ko` p/ UVM) | BAR, FB | tabelas via BAR/instmem | não (nativo) | map errado = fault/hang; `fault` engine é `tu102` (`-ENODEV` sob GSP — faults via RM) | CONFIRMED (Nouveau) / LIKELY (Open, só nomes dos módulos) |

## Etapa 6 — Motores e display sob GSP-RM

| Item | Nouveau impl | NVIDIA Open impl | Dependências | MMIO | Firmware? | Riscos | Confidence |
|---|---|---|---|---|---|---|---|
| FIFO | `ga102_fifo` nativo **ou** `r535_fifo` (canais/runlists geridos pelo RM) | UNKNOWN | MMU/BAR, GSP | **read+write** (nativo) / RPC (GSP) | nativo: indireto (ctx); GSP: RM | canal mal configurado = job perdido/hang | CONFIRMED / UNKNOWN |
| GR/CE/NVDEC | nativo (`ga102_*`) **ou `-ENODEV`** sob GSP (GR golden-ctx via `r535_gr.c`; CE via `r535_ce` RM API; NVDEC sem engine nvkm — vídeo via RM; cf. `VK_KHR_video_decode` em NVK ainda exige patches de ABI) | UNKNOWN | GSP, SEC2/ACR (nativo) | **read+write** (nativo) | nativo: `gr/*`, `acr/*`, `sec2/*` | FW de GR assinado errado = GR morto; sem reclock sem GSP | CONFIRMED (Nouveau) / UNCERTAIN (Open) |
| SEC2/ACR | `ga102_sec2` (HS; `r535_sec2` sob GSP) carrega booter; `ga102_acr` autentica FW no caminho nativo (`-ENODEV` sob GSP) | UNKNOWN | TOP (nativo) | **read+write** (falcon) | **SIM** (`sec2/*`, `acr/*`) | falha de auth = engine sem FW = `-ENODEV` em cascata | CONFIRMED / UNKNOWN |
| Display | `ga102_disp` **ou** `r535_disp` + `dispnv50` atomic/KMS (kernel) | LIKELY `nvidia-modeset.ko` (NVKMS) + `nvidia-drm.ko` | FIFO/DISP, VBIOS | **read+write** (SOR/DP) | VBIOS (+ RM sob GSP) | modeset errado = tela apagada (recuperável via reboot na Fase 0 — mas Fase 0 **não faz modeset**) | CONFIRMED (Nouveau) / LIKELY (Open) |

## Etapa 7 — Command submission (fim da cadeia)

| Item | Nouveau impl | NVIDIA Open impl | Dependências | MMIO | Firmware? | Riscos | Confidence |
|---|---|---|---|---|---|---|---|
| Submit | GEM/TTM alloc → VMM map → canal FIFO (`r535_fifo` sob GSP) → pushbuf → doorbell; fences p/ sync; NVK (userspace, Mesa) submete **por esta ABI** (nova API mem-bind/exec, kernel ≥ 6.6) | LIKELY ioctls `/dev/nvidia*` → RM → GSP (símbolos UNKNOWN) | toda a cadeia acima | doorbell **write** | GSP-RM no ar | pushbuf malformado = hang de engine (recuperação: reset via RM/reboot) — **proibido na Fase 0** | LIKELY (Nouveau) / LIKELY (Open) |

## Tabela curta (sequência confirmada)

| # | Etapa | Símbolo-âncora | Confiança |
|---|---|---|---|
| 0 | PCI match (ANY_ID + classe) | `nouveau_drm_pci_table` / `nouveau_drm_probe` | CONFIRMED |
| 1 | `pci_enable` → ioremap BAR0 → endian → `boot0=0x176` → ctors | `nvkm_device_pci_new` / `nvkm_device_ctor` | CONFIRMED |
| 2 | DRM device (CLI/TTM/BIOS/display) | `nouveau_drm_device_init` | CONFIRMED |
| 3 | Lazy `init`: preinit (`devinit_post→top_parse→fb_mem_unlock`) → `init` em ordem | `nvkm_udevice_init` / `nvkm_device_init` / `nvkm_device_preinit` | CONFIRMED |
| 4 | GSP oneinit (layout WPR2 via SEC2 falcon) → boot → RM/RPC | `tu102_gsp_oneinit` / `ga102_gsp_new` / `r535_gsp_init` | CONFIRMED (Nouveau) |
| 5 | FB (`0x1183a4`) → BAR (`0xb80f48/50`) → MMU47/VMM (ou shims `r535_*`) | `ga102_fb_new` / `tu102_bar_new` / `tu102_mmu_new` | CONFIRMED |
| 6 | FIFO/DISP/SEC2 (shims) ; CE/GR/NVDEC nativos ou `-ENODEV` | `r535_fifo/disp/sec2`, `ga102_*` | CONFIRMED |
| 7 | GEM → map → canal → pushbuf → doorbell (NVK usa esta ABI) | ABI `nouveau` (exec/bind, k≥6.6) | LIKELY |

## Riscos por etapa (leitura obrigatória antes de qualquer fase futura)

- **Etapas 0–2:** somente leitura + alocação; risco ≈ 0 além de bind de driver
  (respeitar `docs/SAFETY.md`: sem unbind/bind na Fase 0).
- **Etapas 3–4:** POST, unlock de FB, WPR2, booter, RPC — **qualquer escrita aqui
  exige checklist SAFETY completo** (hang PCIe, corrupção de FB, brick de GSP).
- **Etapas 5–7:** BAR/MMU/pushbuf/doorbell — escrita = potencial hang irreversível
  sem power-cycle; **proibidas na Fase 0** (`README.md` + `docs/SAFETY.md`).
