# SSH Readiness — acesso remoto para o lab (somente leitura, nada habilitado)

> Status: Fase 0 — **somente leitura**. Data: 2026-09-04. Host: CachyOS,
> usuário `isaque`, kernel `7.2.2-1-cachyos`.
> Proibições respeitadas: **nada foi habilitado** — sem `systemctl enable/start`,
> sem `ufw allow`, sem `ssh-keygen`, sem tocar em `/etc/ssh/` ou `~/.ssh/`.
> Este arquivo documenta o gap + comandos exatos para o usuário executar.

## 1. Estado atual (comandos executados, todos read-only)

| # | Comando | Resultado fiel |
|---|---|---|
| 1 | `which sshd` | `/usr/bin/sshd` — servidor instalado |
| 2 | `systemctl is-active sshd` | `inactive` |
| 3 | `systemctl is-enabled sshd` | `disabled` |
| 4 | `ss -tln \| head -20` | **nenhum listener em `:22`** (só `53` DNS, `631` CUPS, `5355`, `7070`, efêmeras). Confirma 2 |
| 5 | `ip -brief addr` | `eno1 UP 192.168.5.12/24` (+ IPv6 ULA/global); `CloudflareWARP UNKNOWN 172.16.0.2/32`; `lo` loopback. Alvo LAN: **`192.168.5.12`** |
| 6 | `cat /etc/ssh/sshd_config \| grep -vE '^#\|^$'` | mínimo: `Include sshd_config.d/*.conf`, `AuthorizedKeysFile .ssh/authorized_keys`, `Subsystem sftp ...` |
| 7 | `cat /etc/ssh/sshd_config.d/*.conf` | `AuthorizedKeysCommand userdbctl ...` (systemd-homed), `KbdInteractiveAuthentication no`, `UsePAM yes`, `PrintMotd no` — defaults Arch |
| 8 | `ls -la ~/.ssh/` | `a8b75d9992`, `id_devman` (privada, `600`), `known_hosts` — **sem `authorized_keys`** |
| 9 | `systemctl is-active ufw` | `active` — firewall ligado; `ufw status` exige root → regras = **UNKNOWN** (provável bloqueio de `:22` no default) |
| 10 | `lspci -k` (contexto lab) | RTX `01:00.0` driver `nvidia`; Cezanne `08:00.0` driver `amdgpu` — SSH é o plano de resgate para o teste de `nvidia-free-boot-test.md` |

## 2. Gap list — o que falta para teste externo

1. **Servidor parado e desabilitado** (`inactive` + `disabled`, sem `:22`).
2. **Sem `authorized_keys`** no host — login por chave (recomendado) impossível
   até a chave pública do dispositivo externo ser instalada.
3. **Firewall `ufw` ativo com regras UNKNOWN** — mesmo com `sshd` no ar,
   a porta pode continuar filtrada na LAN.
4. **Sem teste fim-a-fim** — nenhum `ssh` de outro dispositivo foi tentado
   (esta sessão é só inventário).
5. **Senha/autenticação não verificada** — `UsePAM yes` sugere senha possível,
   mas política de senha não foi auditada (fora do escopo read-only).

## 3. Para habilitar localmente (NÃO executado — rode por conta própria)

```bash
# 1. (Opcional mas recomendado) instale a chave pública do seu outro dispositivo:
#    rode ISSO NO OUTRO DISPOSITIVO, não aqui:
#    ssh-copy-id isaque@192.168.5.12
#    (cria ~/.ssh/authorized_keys com permissão correta)

# 2. Aqui no lab: libere e suba o servidor (persistente entre reboots):
sudo ufw allow 22/tcp comment 'lab ssh'
sudo systemctl enable --now sshd

# 3. Confira:
systemctl is-active sshd      # esperado: active
ss -tln | grep :22            # esperado: LISTEN em 0.0.0.0:22
sudo ufw status               # esperado: 22/tcp ALLOW
```

Variante mínima de um boot só (sem persistir enable):

```bash
sudo ufw allow 22/tcp comment 'lab ssh'
sudo systemctl start sshd
# ... após o teste, reversão:
# sudo systemctl stop sshd; sudo systemctl disable sshd
# sudo ufw delete allow 22/tcp
```

## 4. Para testar de outro dispositivo na mesma LAN (comandos exatos)

```bash
# Conectividade básica (troque se o IP mudar — confira com `ip -brief addr` no lab):
ping -c3 192.168.5.12
nc -vz 192.168.5.12 22        # ou: nmap -p22 192.168.5.12

# Login (chave primeiro; cai para senha se ainda não instalou a chave):
ssh isaque@192.168.5.12
ssh -v isaque@192.168.5.12    # debug se falhar

# Primeira instalação de chave (a partir do outro dispositivo):
ssh-copy-id isaque@192.168.5.12

# Uso típico de resgate durante o boot sem NVIDIA (sessão persistente + logs):
ssh -t isaque@192.168.5.12 'lspci -k -s 01:00.0; lsmod | grep -iE "nvidia|nouveau"; ls /sys/class/drm/'
```

Notas:

- Fique na **mesma sub-rede** (`192.168.5.0/24` via `eno1`); o IP do `CloudflareWARP`
  (`172.16.0.2`) **não** serve para SSH LAN.
- `KbdInteractiveAuthentication no` + `UsePAM yes`: prefira chave; senha do
  usuário pode funcionar via PAM, mas chave é o caminho suportado aqui.
- Se `nc` disser `refused` com `sshd` ativo → culpa do `ufw` (revise §3).
  Se disser `timeout` → rede/Wi-Fi isolado (AP isolation) ou IP mudou.
- Segurança: restrinja depois ao IP do seu dispositivo
  (`sudo ufw delete allow 22/tcp && sudo ufw allow from <IP-DO-DISPOSITIVO> to any port 22`)
  e desabilite o serviço ao fim da campanha de testes.
