# Recovery Plan — lab dual-machine (sem watchdog)

> Status: Fase 0 — somente leitura. Data: 2026-09-04.
> Escopo: como **observar, coletar e reiniciar** com segurança. Sem escrita MMIO,
> sem firmware, sem power-state, sem `/dev/mem` (ver `SAFETY.md`).
> **Sem watchdog:** nenhum watchdog de hardware/software é proposto ou requerido;
> o kernel atual tem `nowatchdog` na cmdline e assim permanece nesta fase.

## 1. Papéis

| Papel | Função | Requisitos mínimos |
|---|---|---|
| **Machine A (target)** | Ryzen 5 5600G + RTX 3060 `01:00.0`, objeto de todos os comandos read-only | SO funcional atual, `sshd` ativo, IP conhecido, acesso físico garantido |
| **Machine B (helper)** | Observador/coletor; nunca executa escrita na Machine A | SSH client, relógio sincronizado, espaço para logs, mesma rede |

Nenhum experimento futuro ocorre sem Machine B alcançando Machine A via SSH **antes**
do início da sessão.

## 2. SSH (pré-condição de qualquer sessão futura)

1. Machine A: serviço SSH ativo e habilitado na inicialização; conta com chave
   (sem senha interativa); IP fixo ou reserva DHCP anotada neste doc quando definida.
2. Machine B: `ssh -o ConnectTimeout=5 <user>@<machine-a>` responde **antes** de qualquer
   teste; manter **duas** sessões abertas (uma interativa, uma para `journalctl -f`).
3. Critério GO/NO-GO: se SSH não conecta com a máquina idle, a sessão é abortada
   (não se investiga causa "rapidinho" sem plano).

> Estado atual (2026-09-04): topologia SSH **não verificada** — item `BLOCKED` em
> `SAFETY_MMIO_PREREQUISITES.md` até teste real documentado.

## 3. Coleta de logs (somente leitura, ordem sugerida)

Executar da Machine B via SSH (ou na Machine A se SSH já caiu mas console local vive).
Todos read-only; anexar saída datada a `artifacts/` ou ao doc da sessão:

```sh
date -u +%Y-%m-%dT%H:%M:%SZ
uname -r; cat /proc/cmdline
lspci -nn | grep -i -E 'vga|display|audio|amd|nvidia'
lspci -nn -d 1002:; lspci -nn -s 01:00.0; lspci -tv
ls /sys/class/drm/; readlink /sys/class/drm/card*/device/driver 2>/dev/null
lsmod | grep -E 'amdgpu|radeon|nvidia|drm'
journalctl -k --no-pager | tail -n 100
journalctl -k --no-pager | grep -i -E 'amdgpu|radeon|cezanne|vgaarb|simpledrm|nvidia-drm|nouveau' | tail -n 50
dmesg --ctime --no-pager | tail -n 100
```

- Se `lspci` ainda responde mas DRM travou: priorizar `lspci` + `journalctl` antes de reboot.
- Se SSH vivo mas gráfico morto: **não** reiniciar display manager "para testar";
  coletar primeiro, reiniciar depois (seção 4).
- Nunca `cat` conteúdo de `efivars`, nunca tocar em `resource*` com escrita,
  nunca `mmap`/`dd` em BAR.

## 4. Reinício (escada, sem pular etapas)

1. **Reboot limpo via SSH:** `sudo reboot` (ou `systemctl reboot`); aguardar retorno do SSH;
   re-executar coleta mínima (`lspci`, `ls /sys/class/drm`, `journalctl -k | tail`).
2. **SysRq via SSH (só se `1` travou mas SSH vive):** sequência documentada pelo SO,
   sem adivinhar combos; registrar o que foi usado.
3. **Botão power / reset físico (só com acesso físico):** pressionamento curto;
   aguardar POST na GPU primária atual (hoje: RTX).
4. **Power-cycle (último recurso):** desligar, aguardar ~30 s, religar; anotar hora
   para correlacionar com o próximo `journalctl -k`.

O que **não** fazer: segurar power durante escrita em disco sem necessidade, desplugar
com o sistema montado RW sem tentar 1–3 antes, ou "testar de novo rapidinho" sem coletar.

## 5. Matriz SSH vivo × gráfico vivo

| SSH | Console/gráfico | Ação |
|---|---|---|
| vivo | vivo | coletar (seção 3), depois reboot `1` se preciso |
| vivo | morto | coletar tudo via SSH primeiro; só então reboot `1` |
| morto | vivo | coletar localmente, fotografar tela, reboot `1` local |
| morto | morto | ir para `3`–`4` físico; anotar última hora SSH viva |

## 6. Pós-incidente (obrigatório)

- Anexar logs datados + hora do reboot + o que foi observado (tela congelada? `lspci` respondeu?).
- Atualizar `lab/igpu-readiness.md` se BDF/driver/boot-VGA mudou.
- Sessão sem log anexado conta como **não executada** para fins de Fase 0.
