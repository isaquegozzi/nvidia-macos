# iGPU Readiness — Cezanne (Ryzen 5 5600G) — investigação somente leitura

> Status: Fase 0 — **somente leitura**. Data: 2026-09-04. Kernel: `7.2.2-1-cachyos`.
> CPU: `AMD Ryzen 5 5600G with Radeon Graphics` (`/proc/cpuinfo` + `smpboot`).
> Proibições respeitadas: sem alterar BIOS/UEFI, sem unbind, sem `rmmod`/`modprobe`,
> sem `mmap`, sem `/dev/mem`; `efivars` apenas listado por nome, conteúdo **não lido**.

## 1. Método (comandos executados, todos read-only)

| # | Comando | Propósito |
|---|---|---|
| 1 | `lspci -nn \| grep -i -E 'vga\|display\|amd'` | listar VGA/display + dispositivos AMD |
| 2 | `lspci -nn -d 1002:` | isolar vendor AMD display (`1002`) |
| 3 | scan `/sys/bus/pci/devices/*/vendor+device+class` filtrando `0x1002` e classe `0x030000` | prova via sysfs, independente do `lspci` |
| 4 | `lsmod \| grep -E 'amdgpu\|radeon'` | verificar driver AMD carregado |
| 5 | `modinfo amdgpu` (só cabeçalho) | provar disponibilidade sem carregar |
| 6 | `ls /sys/class/drm/` + `ls /dev/dri/` + `readlink .../device/driver` | quais cards DRM existem e a que driver pertencem |
| 7 | `journalctl -k --no-pager \| grep -i -E 'amdgpu\|radeon\|cezanne\|vgaarb\|simpledrm' \| head -30` | histórico kernel de GPU/VGA |
| 8 | `cat /sys/bus/pci/devices/0000:01:00.0/boot_vga` (+ tentativa `08:00.1`) | quem é boot VGA |
| 9 | `cat /proc/cmdline` | flags que afetam DRM (`nvidia_drm`, `amdgpu`, `nomodeset`) |
| 10 | `lspci -tv` | topologia PCIe (onde a iGPU deveria aparecer) |
| 11 | `lspci -Dnn \| grep 1638/1637`, `ls /sys/firmware/efi/efivars/` (só nomes) | checagem negativa `1638` + confirmação ambiente UEFI sem ler conteúdo |

## 2. Evidências (resumo fiel)

### 2.1 `1002:1638` (Cezanne Vega VGA) — AUSENTE

- `lspci -nn -d 1002:` retorna **apenas**:
  `08:00.1 Audio device [0403]: ... Renoir/Cezanne HDMI/DP Audio Controller [1002:1637]`
- `lspci -Dnn | grep 1638` → `NOT FOUND 1638`.
- Scan sysfs: único `vendor=0x1002` é `0000:08:00.1 device=0x1637 class=0x040300`.
- Filtro `class=0x030000`: único resultado `0000:01:00.0 vendor=0x10de device=0x2504`
  (NVIDIA GA106). Nenhum `0x03xx` AMD.
- `lspci -tv`: barramento `08` (ponte interna `00:08.1`) contém
  `08:00.0 Dummy Function [1022:145a]`, `08:00.1 áudio [1002:1637]`,
  `08:00.2 PSP [1022:15df]`, `08:00.3/08:00.4 USB [1022:1639]`, `08:00.6 áudio [1022:15e3]`.
  Nenhuma função VGA/display nesse barramento.

### 2.2 `1002:1637` (áudio HDMI/DP Cezanne) — PRESENTE

- `0000:08:00.1 Audio device [0403] [1002:1637]`, visível em `lspci -nn`,
  `lspci -Dnn`, scan sysfs e `lspci -tv`. Prova de que o complexo `08:00.x`
  está enumerado — só a função VGA está ausente.

### 2.3 Boot VGA — NVIDIA `0000:01:00.0`

- `cat /sys/bus/pci/devices/0000:01:00.0/boot_vga` → `1`.
- Único `boot_vga` existente em `/sys/bus/pci/devices/*/boot_vga`
  (tentativa em `08:00.1` → arquivo inexistente, esperado para não-VGA).
- `journalctl -k`: `pci 0000:01:00.0: vgaarb: setting as boot VGA device`.
- `lspci -nn`: `01:00.0 VGA compatible controller [0300] [10de:2504] (rev a1)`.

### 2.4 `amdgpu`/`radeon` — NÃO CARREGADOS

- `lsmod | grep -E 'amdgpu|radeon'` → vazio (exit 1).
- `lsmod | grep drm/nvidia` mostra só `nvidia_drm`, `nvidia_modeset`, `nvidia`,
  `drm_ttm_helper`, `ttm`, `video` — nenhum `amdgpu`, `radeon`, `simpledrm` como módulo.
- `journalctl -k | grep -i -E 'amdgpu|radeon|cezanne|vgaarb|simpledrm'` (head):
  só `smpboot: CPU0: AMD Ryzen 5 5600G with Radeon Graphics`,
  linhas `vgaarb` da `01:00.0`, `simpledrm` (`simple-framebuffer.0`, minor 0),
  e `nvidia-drm` (`GPU ID 0x00000100`, minor 1, `fbcon: nvidia-drmdrmfb primary`).
  **Zero linhas `amdgpu`/`radeon`/`cezanne`** além do nome da CPU.

### 2.5 `modinfo amdgpu` — DRIVER DISPONÍVEL (não carregado)

- `filename: /lib/modules/7.2.2-1-cachyos/kernel/drivers/gpu/drm/amd/amdgpu/amdgpu.ko.zst`,
  `author: AMD linux driver team`. Prova disponibilidade; módulo **não** foi carregado
  (nenhum `modprobe` executado, conforme proibição).

### 2.6 Cards DRM — SÓ NVIDIA, NENHUM AMD

- `ls /sys/class/drm/`: `card1`, `card1-DP-1/2/3`, `card1-HDMI-A-1`, `renderD128`
  — todos com symlink para `.../0000:01:00.0/drm/...`. Nenhum `card0`.
- `readlink /sys/class/drm/card1/device/driver` e `renderD128/...` → `.../bus/pci/drivers/nvidia`.
- `/dev/dri/`: `card1`, `renderD128` apenas.

### 2.7 Cmdline — sem tokens AMD DRM

- `quiet nowatchdog splash rw ... nvidia_drm.modeset=1 nvidia_drm.fbdev=1
  nvidia.NVreg_PreserveVideoMemoryAllocations=1 ... amd_pstate=active mitigations=off preempt=full`.
- Nenhum `amdgpu.*`, `radeon.*`, `nomodeset`, `simpledrm` após quebra por espaços.

### 2.8 CPU prova capacidade iGPU no die (mas função PCI oculta)

- `smpboot: CPU0: AMD Ryzen 5 5600G with Radeon Graphics (family 0x19, model 0x50)`.
- `/proc/cpuinfo model name`: `AMD Ryzen 5 5600G with Radeon Graphics`.
- Interpretação: o die contém Vega Cezanne, mas a função VGA não é enumerada
  pelo OS — padrão compatível com desabilitação em firmware.

## 3. Respostas diretas

| Pergunta | Resposta |
|---|---|
| `1002:1638` presente? | **Não** — três fontes concordam (`lspci -d 1002:`, grep `1638`, sysfs). |
| `1002:1637` áudio presente? | **Sim** — `0000:08:00.1 [1002:1637]`. |
| Boot VGA de quem? | **NVIDIA `0000:01:00.0`** (`boot_vga=1` + `vgaarb`). |
| `amdgpu` carregado? | **Não** (`lsmod` vazio + zero linhas no kernel log). |
| Card DRM AMD? | **Não** (só `card1`/`renderD128` → driver `nvidia`). |

## 4. Conclusão

- **Verdict: LIKELY — iGPU Vega Cezanne (`1002:1638`) não visível ao OS, provavelmente
  desabilitada em firmware UEFI/BIOS.**
- **Confidence:**
  - **Alta** de que a função VGA está ausente do OS (evidência tripla concordante).
  - **Média** de que a causa é desabilitação em firmware (alternativas como falha de
    enumeração/fuse não podem ser excluídas sem a tela de setup UEFI; por regra do
    projeto, "disabled in UEFI/BIOS" sem evidência inequívoca do setup = `LIKELY`, não `CONFIRMED`).
- Classificação segue a escala `CONFIRMED / LIKELY / UNCERTAIN / UNKNOWN` do projeto.

## 5. Próximos passos (somente leitura)

1. Conferência visual do setup UEFI (sem alterar nada): fotografar/anotar estado atual
   das categorias genéricas de opções gráficas antes de qualquer mudança (ver `dual-gpu-setup.md`).
2. Repetir esta investigação após qualquer mudança futura de firmware e anexar saída
   datada aqui antes de qualquer experimento com a RTX como target.
3. Mantido gate `SAFETY_MMIO_PREREQUISITES.md`: sem MMIO enquanto iGPU/display não
   estiverem em `dual-gpu-setup.md` estado desejado.

## 6. Adendo 2026-09-04 — iGPU habilitada no UEFI (verificação pós-mudança)

Usuário habilitou a iGPU manualmente no setup. Reverificação somente leitura:

- `08:00.0 VGA [1002:1638] rev c9` PRESENTE (subsys Gigabyte 1458:d000), driver `amdgpu`, IOMMU 13, boot_vga=0
- DRM: `card0 (226,0)` + `renderD129 (226,129)`, uevent DRIVER=amdgpu PCI_ID=1002:1638
- BARs AMD: BAR0 256M pref @7c10000000, BAR2 2M pref, IO d000/256, BAR5 MMIO 512K @fc500000
- Kernel: `RENOIR 0x1002:0x1638`, VBIOS `13-CEZANNE-019`, VRAM 32M + GTT 7909M, fb1=amdgpudrmfb
- Conector `card0-HDMI-A-2: connected/enabled/On` (1920x1080+) — monitor ativo na iGPU
- Desktop permanece na NVIDIA (ver `lab-status`: gnome-shell renderD128, glxinfo NVIDIA) → `PARTIALLY_READY`
- Verdict anterior (`LIKELY` desabilitada) **superado por evidência**: iGPU funcional — **CONFIRMED presente e operacional**.
