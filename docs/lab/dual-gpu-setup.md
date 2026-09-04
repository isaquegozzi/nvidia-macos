# Dual-GPU Setup — estado desejado (Vega = display, RTX = target)

> Status: Fase 0 — planejamento somente leitura. Data: 2026-09-04.
> Estado atual: ver `igpu-readiness.md` — iGPU `1002:1638` ausente, boot VGA = RTX `01:00.0`.
> Nenhuma mudança foi executada neste documento; ele descreve o **alvo**, não o presente.

## 1. Objetivo

- **Display/host:** iGPU Vega Cezanne (`1002:1638`, driver `amdgpu`) — console, Wayland/Xorg,
  `simpledrm`→`amdgpu` como primary, cabo de vídeo nela.
- **Target de experimentos:** NVIDIA GA106 `0000:01:00.0 [10de:2504]` (+ áudio `01:00.1 [10de:228e]`) —
  livre do console/framebuffer para testes futuros, sem competir com o display.
- Motivação: isolar hangs da RTX do display do operador; permitir coleta via SSH
  (ver `recovery-plan.md`) e bloquear MMIO inseguro até lá
  (ver `SAFETY_MMIO_PREREQUISITES.md`).

## 2. Estado atual (2026-09-04, resumido)

| Item | Atual |
|---|---|
| iGPU VGA `1002:1638` | ausente (`NOT FOUND`) |
| Áudio Cezanne `1002:1637` | presente em `08:00.1` |
| Boot VGA | RTX `01:00.0` (`boot_vga=1`) |
| DRM | só `card1`/`renderD128` → `nvidia` |
| `amdgpu` | disponível (`modinfo`), não carregado |

## 3. Estado desejado (critérios de pronto, todos verificáveis de forma read-only)

1. `lspci -nn -d 1002:` lista **duas** funções: VGA/display (`1638`-família) + áudio `1637`.
2. Scan sysfs mostra `vendor=0x1002` com `class=0x030000` (além da NVIDIA).
3. `ls /sys/class/drm/` mostra card AMD (driver `amdgpu`) **e** card NVIDIA separados.
4. `boot_vga` da iGPU = `1` (ou ao menos RTX deixa de ser o único `boot_vga=1`);
   `vgaarb: setting as boot VGA device` aponta para a iGPU no `journalctl -k`.
5. Cabo de vídeo fisicamente na saída da placa-mãe (iGPU), imagem desde o POST.
6. `journalctl -k | grep -i amdgpu` mostra probe da Cezanne sem erro fatal.

Enquanto 1–6 não forem todos verdadeiros, o setup **não** está pronto.

## 4. Opções de firmware (genéricas — variam por fabricante)

> ⚠️ **Aviso:** nomes, abas e valores exatos variam por fabricante/placa/versão de BIOS/UEFI.
> Consulte o **manual da sua placa-mãe**. Este documento deliberadamente **não** cita
> nomes específicos de opções para não inventar caminhos que não existem no seu setup.
> Anote/fotografe os valores atuais **antes** de alterar qualquer coisa.

Categorias genéricas a localizar no setup UEFI (somente quando `recovery-plan.md`
estiver operacional e com acesso físico garantido):

- **Categoria A — habilitação dos gráficos integrados:** opção que liga/desliga a iGPU
  da APU (hoje suspeita-se `desabilitado`, dado `igpu-readiness.md`). Alvo: `habilitado`.
- **Categoria B — dispositivo gráfico primário/de boot:** opção que escolhe qual GPU
  inicializa o vídeo e recebe o POST. Alvo: gráficos integrados como primários.
- **Categoria C — modo de múltiplos dispositivos gráficos:** opção que permite iGPU +
  dGPU ativas simultaneamente (sem ela, uma pode ser desligada ao detectar a outra).
  Alvo: ambas ativas.
- **Categoria D — reserva de memória para a iGPU (se existir):** quantia de RAM
  reservada ao framebuffer da iGPU. Alvo: menor valor estável oferecido (suficiente
  para console/display; sem superalocação).
- **Categoria E — versão/modo do barramento da dGPU (se existir):** manter padrão
  estável atual; não alterar junto com A–C (uma variável por vez).

Princípios:

- Uma mudança por vez, com reboot + reinvestigação `igpu-readiness.md` entre elas.
- Se qualquer categoria não existir na sua placa, **pare e documente** (nome da placa,
  versão de firmware, fotos da tela) em vez de adivinhar equivalente.
- Não alterar voltagem/clock/boot-order/dispositivos não-GPU no mesmo ciclo.

## 5. Cabeamento e SO (após firmware pronto)

- Mover o cabo de vídeo da RTX para a saída da placa-mãe; confirmar POST na iGPU.
- Esperado no SO: `amdgpu` faz probe, `nvidia_drm` continua para a RTX mas sem ser
  `boot VGA` nem `fbcon primary`.
- Sem `unbind`/`bind`, sem blacklist, sem `modprobe` manual nesta fase — apenas observar
  o que o boot faz por padrão e registrar em `igpu-readiness.md`.

## 6. Anti-metas (o que este setup NÃO é)

- Não é autorização para MMIO na RTX (gate em `SAFETY_MMIO_PREREQUISITES.md` continua `BLOCKED`).
- Não é guia de passthrough/VFIO (fora do escopo Fase 0).
- Não substitui `recovery-plan.md`: sem SSH + Machine B, não tocar em firmware.
