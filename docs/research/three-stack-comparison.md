> Status: Fase 0 — comparação documental (somente leitura, sem código).
> Fontes: `torvalds/linux@986c24e0` (Nouveau, verificado no código),
> `NVIDIA/open-gpu-kernel-modules` (árvore pública + docs; símbolos internos UNKNOWN),
> Mesa/NVK (docs.mesa3d.org + nouveau.freedesktop.org + Collabora/LWN).
> Pin Nouveau: `master` em 2026-09-04. Clones/cache só em `/tmp`.

# Three-stack comparison: Nouveau (kernel) vs NVIDIA Open (kernel) vs NVK (userspace)

## Tese em uma frase

**NVK não é um driver — é um cliente.** Todo acesso a probe, modeset, memória,
VM e submissão passa pelo driver **kernel** (Nouveau *ou* NVIDIA proprietário);
o NVK só traduz Vulkan → comandos e usa a ABI do kernel. Quem troca o Nouveau
pelo NVIDIA Open troca o kernel inteiro sob o NVK — e o NVK deixa de funcionar
(os dois kernels se excluem no mesmo hardware).

## Tabela: quem faz o quê

| Responsabilidade | Nouveau (kernel, `drm/nouveau`) | NVIDIA Open (kernel, `open-gpu-kernel-modules`) | NVK (userspace, Mesa) |
|---|---|---|---|
| PCI probe / BAR0 ioremap / `boot0` → chipset | **SIM** — `nouveau_drm_probe` → `nvkm_device_pci_new` → `nvkm_device_ctor` ([base.c@986c24e0](https://github.com/torvalds/linux/blob/986c24e0fe44f844b44d365b71ce831947f50298/drivers/gpu/drm/nouveau/nvkm/engine/device/base.c)) | **SIM** — `nvidia.ko` tem PCI driver próprio (camada `kernel-open/nvidia/` + RM em `src/nvidia/`; símbolos UNKNOWN) | **NÃO** — NVK nunca toca PCI/BAR; abre o DRM fd já criado |
| Device init / GSP boot / RM | **SIM** — `nvkm_device_init` lazy; `tu102_gsp_oneinit` + `r535_gsp_init` (GSP-RM); FW `nvidia/ga106/*` | **SIM** — RM proprietário em `src/`; **GSP obrigatório em Turing+** (docs: "must be used with GSP firmware"); `nouveau/` na árvore extrai FW p/ o Nouveau | **NÃO** — NVK assume GPU já inicializada pelo kernel |
| Memória (VRAM/TTM/GEM) | **SIM** — `nouveau_ttm_init`, `ga102_fb`/`r535_fb`, `tu102_mmu`/`r535_mmu` | **SIM** — RM (`src/nvidia/`) + `nvidia-uvm.ko` p/ UVM (LIKELY; divisão por módulo CONFIRMED no README) | **NÃO** — aloca **via** Nouveau (GEM + `vm_bind`/exec ABI, kernel ≥ 6.6); não gerencia VRAM sozinho |
| VM / page tables / faults | **SIM** — `nvkm_vmm_*`, `fault/tu102` (nativo) ou RM (GSP) | **SIM** — RM + UVM | **NÃO** — pede mapas ao kernel |
| Command submission / sched | **SIM** — canais FIFO (`ga102_fifo`/`r535_fifo`), runlists, fences; ABI `exec/pushbuf` (nova API p/ NVK no 6.6) | **SIM** — ioctls `/dev/nvidia*` → RM → GSP (LIKELY; símbolos UNKNOWN) | **NÃO** — monta pushbufs (NIR→NAK→comandos) e **submete pela ABI do Nouveau** |
| Modeset / display / KMS | **SIM** — `ga102_disp`/`r535_disp` + `dispnv50` atomic; DRM/KMS no kernel | **SIM** — `nvidia-modeset.ko` (NVKMS, `src/nvidia-modeset/`) + `nvidia-drm.ko` p/ DRM (LIKELY) | **NÃO** — NVK não faz modeset; apresentação via WSI → swapchain → KMS do kernel |
| Shader compile (Vulkan) | N/A (kernel não compila shader) | N/A (userspace proprietário faz) | **SIM — único "SIM" do NVK**: NIR + compilador NAK (Rust) em `src/nouveau/vulkan/` (Mesa); exige Mesa recente + `linux-firmware` |
| Reclock / power | Parcial — via GSP (reclock automático Turing+ c/ FW; `pstate` debugfs limitado) | **SIM** — completo no RM | **NÃO** |

## Diagrama corrigido

```text
ERRADO (pedido conceitual ingênuo):
  App Vulkan ──▶ NVK ──▶ GPU            (NVK como "driver substituto")

CORRETO:
  App Vulkan ──▶ loader (ICD: NVK ou proprietário)
                      │
                      ▼
              ┌───────────────┐   userspace (Mesa)
              │  NVK (nvidia) │──▶ compila shaders (NIR/NAK), monta
              │  nouveau GL   │    pushbufs, gerencia VkDevice/queues
              └───────┬───────┘
                      │  ABI Nouveau (GEM / vm_bind / exec / KMS)
                      ▼
              ┌───────────────┐   kernel (UM dos dois, nunca ambos)
              │    Nouveau    │──▶ probe, GSP-RM, mem/VM, FIFO,
              │  drm/nouveau  │    modeset/KMS, sync, reclock
              └───────┬───────┘
                      ▼
              ┌───────────────┐   firmware (linux-firmware)
              │ nvidia/ga106/ │──▶ gsp, acr, gr, sec2, nvdec/scrubber
              │  GSP-RM vivo  │
              └───────────────┘

  Pilha alternativa (troca o kernel inteiro; NVK sai de cena):
  App GL/VK ──▶ userspace NVIDIA ──▶ nvidia.ko + nvidia-modeset.ko
                                        + nvidia-drm.ko + nvidia-uvm.ko ──▶ GSP ──▶ GPU
```

Correções ao diagrama conceitual do pedido:

1. **NVK não substitui driver kernel** — sem Nouveau bindado, `vkEnumeratePhysicalDevices`
   falha (`ERROR_INITIALIZATION_FAILED`); NVK e driver NVIDIA não coexistem.
2. **Modeset é sempre kernel** (Nouveau KMS *ou* NVKMS) — NVK não tem saída de vídeo própria.
3. **Memória/VM é sempre kernel** — NVK não mapeia VRAM diretamente; a nova ABI
   (bind/exec, Linux ≥ 6.6, trabalho de Danilo Krummrich/Red Hat) existe justamente
   para o NVK pedir isso ao Nouveau de forma moderna.
4. **GSP-RM aparece nas duas pilhas kernel**, mas com RMs diferentes:
   Nouveau usa o RM redistribuível (`r535_*` + FW extraído de `open-gpu-kernel-modules/nouveau/`);
   o NVIDIA Open usa o RM completo proprietário. O FW GSP (`535.113.01`/`570.144`)
   é o mesmo trem de release.

## Fontes

- Nouveau: `nvkm/engine/device/{base,pci,user}.c`, `include/nvkm/core/layout.h`,
  `nvkm/subdev/gsp/{ga102,tu102}.c`, `nvkm/subdev/gsp/rm/r535/` — pin `986c24e0`.
- NVIDIA Open: [README — divisão `kernel-open/` vs `src/`, módulos `nvidia.ko`,
  `nvidia-modeset.ko`, `nvidia-uvm.ko`, `nvidia-drm.ko`](https://github.com/NVIDIA/open-gpu-kernel-modules);
  [docs: GSP firmware obrigatório](https://download.nvidia.com/XFree86/Linux-x86_64/555.42.02/README/gsp.html);
  [`nouveau/` extrai FW p/ o Nouveau](https://github.com/NVIDIA/open-gpu-kernel-modules).
- NVK: [docs.mesa3d.org — NVK, Vulkan 1.4 conformant, kernel ≥ 6.6](https://docs.mesa3d.org/drivers/nvk.html);
  [nouveau.freedesktop.org — NVK usa o mesmo kernel Nouveau, nova API de bind/exec](https://nouveau.freedesktop.org/);
  [Collabora — NVK landed: nova userspace API no Linux 6.6](https://www.collabora.com/news-and-blog/news-and-events/nvk-has-landed.html).

## Implicação para o projeto (Fase 0)

Qualquer bring-up futuro precisa de **um** kernel-dono da GPU. Estudar o NVK sem o
Nouveau kernel é estudar um cliente sem servidor. Por isso este projeto mapeia
primeiro o `nvkm` (implementation-map + initialization-map) e trata o NVK como
consumidor da ABI — nunca como substituto.
