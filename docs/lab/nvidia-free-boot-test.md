# Boot de teste sem NVIDIA — plano de UM único boot, reversível (Limine + mkinitcpio)

> Status: Fase 0 — **somente leitura, NÃO APLICADO**. Data: 2026-09-04.
> Kernel de referência: `7.2.2-1-cachyos` (CachyOS, Arch-like).
> Proibições respeitadas: sem editar `/boot`, sem `limine-entry-tool`,
> sem editar `/etc/default/limine`, sem regenerar initramfs, sem `modprobe`/`rmmod`,
> sem sudo. Este arquivo é **plano** — nada aqui foi executado como boot.

Objetivo do teste (quando o usuário decidir executar, por conta própria):
GNOME renderizado pela AMD (Cezanne `1002:1638`, `amdgpu`) **e**
RTX 3060 (`01:00.0`, `10de:2504`) presente no PCI **sem driver**
(`lspci -k` sem linha `Kernel driver in use`, só `Kernel modules:`).

## 1. Bootloader detectado: Limine (UEFI) — não assumir GRUB/systemd-boot

| # | Comando (read-only) | Evidência |
|---|---|---|
| 1 | `efibootmgr -v` | `BootCurrent: 0001`, `Boot0001* Limine` = `HD(...)/\EFI\LIMINE\LIMINE_X64.EFI`; `BootOrder: 0001,...` (Limine é o primeiro) |
| 2 | `bootctl status \| head -20` | `Current Boot Loader: Product: Limine 12.7.0` (mesmo sem systemd-boot instalado, `bootctl` identifica o loader ativo) |
| 3 | `ls /sys/firmware/efi/ \| head -3` | `config_table, efivars, esrt` → boot UEFI real (não legacy/CSM) |
| 4 | `cat /proc/cmdline` | `... root=UUID=ee1aee08-... rootflags=subvol=/@ ... nvidia_drm.modeset=1 nvidia_drm.fbdev=1 nvidia.NVreg_*=... amd_pstate=active ...` |
| 5 | `cat /etc/default/limine` | `ESP_PATH="/boot"`, `KERNEL_CMDLINE[default]="<mesma linha do /proc/cmdline>"`, `BOOT_ORDER="*, *lts, *fallback, Snapshots"`, `ENABLE_UKI=no` → entradas geradas por `limine-entry-tool`, cmdline persistida **nesse arquivo** |
| 6 | `ls /boot/`, `ls /boot/loader/entries/`, `ls /boot/grub/` | **Permissão negada** (`/boot` = `drwx------ root`, ESP montado só para root) — conteúdo direto do ESP = `UNKNOWN` sem root; identificação Limine independe disso (itens 1+2+5) |
| 7 | `ls /etc/kernel/cmdline /etc/dracut*` | inexistentes; `/etc/mkinitcpio.conf` + `/etc/mkinitcpio.conf.d/` presentes → sem dracut, sem UKI cmdline separada |

Conclusão: o menu que aparece no power-on é o **menu Limine**. Método de
single-boot = **editar a entrada no próprio menu, sem salvar** (nada é
persistido no ESP; o próximo reboot volta ao `KERNEL_CMDLINE[default]` intacto).

## 2. NVIDIA está no initramfs? SIM (via mkinitcpio + chwd)

Initramfs detectado: **mkinitcpio** (CachyOS). Dracut/booster **não aplicáveis**
(`which dracut booster` → ausentes; `/etc/dracut*` inexistente).

| # | Comando (read-only) | Evidência |
|---|---|---|
| 1 | `cat /etc/mkinitcpio.conf \| grep -vE '^#\|^$'` | `HOOKS=(base systemd autodetect microcode kms modconf block keyboard sd-vconsole plymouth filesystems)` — `modconf` presente (blacklists de modprobe entram na imagem) |
| 2 | `cat /etc/mkinitcpio.conf.d/10-chwd.conf` | `MODULES+=(nvidia nvidia_modeset nvidia_uvm nvidia_drm)` — gerado por `chwd`, carrega NVIDIA **cedo** (KMS precoce) |
| 3 | `cat /etc/mkinitcpio.conf.d/10-chwd-kms.conf` | `HOOKS=(${HOOKS[@]/kms/})` — hook `kms` removido (porque os módulos já estão em `MODULES`) |
| 4 | `lsinitcpio /boot/initramfs*.img \| grep -i nvidia` | `UNKNOWN` direto — `/boot` ilegível sem root (`ERROR: No such file` por permissão); **mas** item 2 já prova inclusão: `MODULES=` do mkinitcpio é empacotado na imagem |
| 5 | `cat /etc/modprobe.d/nvidia-optimize.conf` + `/usr/lib/modprobe.d/nvidia-utils.conf` | `options nvidia_drm modeset=1 fbdev=1` / `options nvidia NVreg_*=...` + `blacklist nouveau` (Nouveau já bloqueado via modprobe; entra na imagem pelo hook `modconf`) |

Consequência direta: **`modprobe.blacklist=nvidia,...` SOZINHO NÃO BASTA.**
`modprobe.blacklist=` bloqueia apenas autoload por alias; os `MODULES=`
do mkinitcpio são carregados por `modprobe` **explícito** no initramfs,
que ignora blacklist simples. É preciso **`module_blacklist=`**
(parâmetro do núcleo, bloqueia até carga explícita).
`rd.driver.blacklist=` / `omit_drivers=` são **só-dracut** → não usar.
`booster.blacklist=` é só-booster → não usar.

## 3. Procedimento — um único boot, sem persistir (executar só quando quiser)

Princípio: tocar **só na cópia em memória** da cmdline dentro do editor do
menu Limine. Não editar nenhum arquivo, não rodar nenhuma ferramenta.

1. Feche trabalho não salvo no GNOME. Deixe um terminal com
   `cat /proc/cmdline` pronto para conferência pós-boot (opcional).
2. Reinicie. O firmware entrega ao **Limine** (`Boot0001`, timeout curto de
   ~1s — se o menu não aparecer, segure `Espaço` durante o POST até ele surgir).
3. No menu Limine, selecione com as setas a entrada **padrão CachyOS**
   (a mesma de sempre — **não** a `fallback`, **não** snapshot).
4. Pressione **`E`** (editar). O rodapé do menu confirma a tecla de edição
   e a tecla de boot na sua versão (`12.7.0`) — siga o rodapé se divergir.
5. Localize a linha de cmdline (ela começa com o conteúdo de
   `KERNEL_CMDLINE[default]`, §1 item 5). Faça **só estas duas operações**:
   - (a) **Remover** estes 4 tokens (evita que o driver, se algo escapar ao
     bloqueio, assuma o framebuffer):
     `nvidia_drm.modeset=1 nvidia_drm.fbdev=1 nvidia.NVreg_PreserveVideoMemoryAllocations=1 nvidia.NVreg_TemporaryFilePath=/var/tmp`
   - (b) **Acrescentar** ao final (uma linha, espaços simples):
     `module_blacklist=nvidia,nvidia_drm,nvidia_modeset,nvidia_uvm,nouveau modprobe.blacklist=nvidia,nvidia_drm,nvidia_modeset,nvidia_uvm,nouveau nouveau.modeset=0`
   - **Manter todo o resto** intacto (`root=UUID=... rootflags=subvol=/@ quiet ... amd_pstate=active mitigations=off preempt=full`).
6. Boot pela tecla indicada no rodapé (sem salvar — o editor do Limine
   **não grava** no ESP; é só esta sessão).
7. Opcional, se o GNOME subir: confira `cat /proc/cmdline` contém
   `module_blacklist=` antes de qualquer conclusão.

Resultado esperado da cmdline de teste (referência completa):

```text
quiet nowatchdog splash rw rootflags=subvol=/@ root=UUID=ee1aee08-b789-4bd8-9f97-19571fa7301f amd_pstate=active mitigations=off preempt=full module_blacklist=nvidia,nvidia_drm,nvidia_modeset,nvidia_uvm,nouveau modprobe.blacklist=nvidia,nvidia_drm,nvidia_modeset,nvidia_uvm,nouveau nouveau.modeset=0
```

### O que NÃO fazer (para o teste continuar reversível)

- Não editar `/etc/default/limine`, não rodar `limine-entry-tool` / `limine-install`,
  não tocar em nada sob `/boot`, não regenerar initramfs, não `systemctl set-default`,
  não `rmmod`/`modprobe`, não unbind de PCI.

## 4. Verificação pós-boot (tudo read-only)

| # | Comando | Esperado no teste bem-sucedido |
|---|---|---|
| 1 | `cat /proc/cmdline` | contém `module_blacklist=nvidia,...` e **não** contém `nvidia_drm.modeset=1` |
| 2 | `lspci -k -s 01:00.0` | `VGA ... GA106 [GeForce RTX 3060 Lite Hash Rate]` **sem** `Kernel driver in use` (só `Kernel modules: nouveau, nvidia_drm, nvidia`) |
| 3 | `lsmod \| grep -iE 'nvidia\|nouveau'` | vazio (exit 1) |
| 4 | `ls /sys/class/drm/` + `readlink /sys/class/drm/card*/device/driver` | `card0`/`renderD129` → `amdgpu`; nenhum card → `nvidia` |
| 5 | `ls /dev/dri/` | `card0` (+`renderD129`) AMD presente; nada NVIDIA |
| 6 | GNOME | sessão visível no monitor ligado à iGPU (`card0-HDMI-A-2` já `connected` no baseline); `glxinfo`/detalhes do sistema citando AMD, não NVIDIA |

## 5. Rollback (volta ao normal)

- **Basta reiniciar.** Como nada foi persistido, o próximo boot usa de novo
  o `KERNEL_CMDLINE[default]` com NVIDIA. Se o teste travar sem vídeo:
  power-cycle → menu Limine → Enter na entrada padrão (sem `E`) → sistema original.

## 6. Riscos e UNKNOWNs honestos

- `/boot/limine.conf` **não foi lido** (permissão) — nomes exatos das entradas
  no menu podem variar (`CachyOS`, kernel `linux-cachyos` vs `linux-zen`/`lts`);
  escolha sempre a entrada padrão que corresponde ao kernel `7.2.2-1-cachyos` corrente.
- Teclas do editor Limine seguir o **rodapé da v12.7.0** na hora; `E` é o padrão
  documentado dessa família, mas o rodapé manda.
- Com NVIDIA fora, o conector físico do monitor **precisa** estar na saída da
  placa-mãe (iGPU) — o adendo de `igpu-readiness.md` já registra monitor ativo
  em `card0-HDMI-A-2`, então o pré-requisito parece atendido.
- `plymouth`/`splash` podem mostrar tela preta breve durante a troca de DRM;
  aguardar o GDM antes de declarar falha.
