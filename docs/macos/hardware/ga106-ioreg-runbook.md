# GA106 `ioreg` runbook — Tahoe, SOMENTE LEITURA (TGM0)

> Fase TGM0. Documento de procedimento, SOMENTE `.md`. **Nada aqui foi executado** — comandos abaixo são para a futura sessão no macOS Tahoe; nesta sessão (Linux) nenhum foi rodado, nenhum snapshot foi coletado.
> Regra do projeto: sem escrita MMIO, sem firmware/power/display, sem `unbind`/`bind`, sem reset, sem instalar dext. Este runbook é **inspeção passiva**: `ioreg` + `system_profiler` + `systemextensionsctl list` + `log show` (todos somente leitura).
> Leituras prévias obrigatórias: `docs/macos/driver-architecture-gate.md` §3 (mecanismo `built-in`), `docs/tinygpu/internal-pcie-gap.md` Addendum TGM0-2 (matriz A/B/C), `docs/tinygpu/amd-igpu-match-risk.md` (veredito `RISK` — NÃO instalar `TinyGPU.app`), `docs/macos/ADR-0002-tinygpu-reuse.md` §8 (validação macOS = 0%).

## 0. Pré-condição BLOCKER: desktop na AMD

- **Exigência:** sessão Tahoe logada com **display principal na AMD** (iGPU `1002:1638` ou dGPU AMD), **nunca** com a RTX `10de:2504` como único display.
- Porquê: qualquer inspeção errada não pode custar o display; além disso a dext shipped é elegível por classe+vendor para a iGPU AMD (`amd-igpu-match-risk.md` §5–§6, veredito `RISK`) — inspecionar com a AMD como display + **sem ativar** o `TinyGPU.app` mantém o risco contido.
- Gate antes de abrir o Terminal no Tahoe:
  1. `About This Mac > Displays` mostra AMD como principal (ou `System Report > Graphics/Displays` com AMD attached ao display).
  2. Sessão SSH ou segundo acesso **não** é exigida aqui (isso é gate Linux MMIO), mas ter como salvar o snapshot fora da máquina ajuda.
  3. **NÃO** abrir `TinyGPU.app`, **NÃO** clicar `Toggle TinyGPU ON` em `System Settings > General > Login Items & Extensions > Driver Extensions`, **NÃO** rodar `systemextensionsctl developer on` / `reset` / `uninstall` como "sondagem". `systemextensionsctl list` (leitura) é o único uso autorizado neste runbook.

## 1. Comandos exatos (copiar/colar no Tahoe, somente leitura)

Ordem sugerida: panorama → nó NVIDIA → nó AMD (controle) → estado dext → logs. Todos terminam em leitura; nenhum pede `sudo`, nenhum altera estado.

```sh
# 1a. Panorama PCI (para achar os nós antes de filtrar)
ioreg -l -w0 -c IOPCIDevice | head -n 200

# 1b. Filtro direto pedido: built-in / tunnel / vendor / device / classe / entitlements / slot
ioreg -l -w0 -c IOPCIDevice | grep -i -E "built-in|IOPCITunnelled|vendor-id|device-id|class-code|IOServiceDEXTEntitlements|AAPL,slot-name|IOProviderClass|tinygpu|0x2504|0x1638"

# 1c. Nó NVIDIA focado (contexto ±30 linhas por match — sem alterar nada)
ioreg -l -w0 -c IOPCIDevice | grep -B5 -A30 -i "0x2504"

# 1d. Nó AMD controle (mesmo filtro, para comparar built-in/tunnel no display)
ioreg -l -w0 -c IOPCIDevice | grep -B5 -A30 -i "0x1638"

# 1e. Busca recursiva de túnel nos pais (a presença real de túnel vive no caminho, não na personality)
ioreg -l -w0 -c IOPCIDevice | grep -i -E "IOPCITunnelled|IOPCITunnelCompatible|IOThunderbolt|AppleThunderbolt"

# 1f. Inventário gráfico/display do sistema (leitura)
system_profiler SPPCIDataType -detailLevel mini
system_profiler SPDisplaysDataType -detailLevel mini

# 1g. Estado da dext (LEITURA — nunca developer on / reset / uninstall aqui)
systemextensionsctl list

# 1h. Logs TinyGPU passados (LEITURA — prova se Start_Impl algum dia rodou)
log show --predicate 'eventMessage CONTAINS "tinygpu"' --last 1h | head -n 100
log show --predicate 'eventMessage CONTAINS "TinyGPU"' --last 1h | head -n 100
```

Notas de uso: `-w0` impede truncamento; `-l` mostra propriedades; `-c IOPCIDevice` restringe à classe provedora do dext. Se `grep -B/-A` esconder o nome do nó, repetir `1c/1d` sem `grep` e rolar até `IOPCIDevice@<slot>` correspondente. Não canalizar para `sudo`, `tee /System/*`, `nvram`, `kextload`, `kmutil`.

## 2. Quais campos registrar (por nó: `10de:2504` alvo + `1002:1638` controle)

| Campo | Onde aparece (`ioreg -l`) | Valor a copiar literal | Para que serve (A/B/C) |
|---|---|---|---|
| `vendor-id` / `device-id` | nó `IOPCIDevice` | hex (esperado `0x10de` / `0x2504`; controle `0x1002` / `0x1638`) | pré-condição entitlement (fora de `[10de,1002]` = MISS por entitlement, não por B/C) |
| `class-code` | nó `IOPCIDevice` | hex (esperado `0x030000`) | pré-condição classe (≠ `0x030000` = MISS por classe) |
| `built-in` | nó (tipo `Data`/booleano) | `presente (<00>/YES)` vs `ausente` | **decide C** (ausente ⇒ C=`FALSE` ali; presente + sem `...builtin` ⇒ C prevê MISS) |
| `IOPCITunnelled` | nó + pais (busca recursiva) | `YES` vs ausente | **decide A vs B** (presente sustenta A; ausente + HIT shipped ⇒ B=`FALSE`; ausente + MISS ⇒ B favorecido, sem provar mecanismo) |
| `IOServiceDEXTEntitlements` | nó (quando dext casado) | array/dict vivo | confirma allowlist viva vs shipped; ausência de `...builtin` confirma perna C |
| `AAPL,slot-name` | nó | string (`Internal` vs outro/ausente) | contexto de slot, não decide |
| `IOProviderClass` / `CFBundleIdentifier` do filho driver (ver §4) | filho `IOService` | ex. `TinyGPUDriver` vs driver AMD vs nenhum | desempate HIT/MISS |
| Build Tahoe + modelo | `sw_vers` + `system_profiler SPHardwareDataType` | `ProductVersion/BuildVersion`, `Model Identifier` | datar o snapshot (comportamento pode variar por build) |

Registrar **os dois nós** (NVIDIA + AMD) na mesma sessão, com o mesmo build Tahoe — o controle AMD distingue "propriedade do slot" de "propriedade da GPU".

## 3. Onde salvar snapshot sanitizado (política `artifacts/`, gitignored)

- **Política registrada (verificada nesta sessão):** `.gitignore:7` contém `artifacts/` (bare, casa qualquer `artifacts/`). Logo **tudo sob `artifacts/macos/` é gitignored por construção** — snapshots `ioreg` nunca entram em commit. Verificação: `cat .gitignore` mostra `artifacts/`; `git status --short` não deve listar `artifacts/macos/` após a coleta.
- Diretório de destino (criar só na sessão Tahoe, local): `artifacts/macos/` (hoje só existe `artifacts/baselines/` no repo; `artifacts/macos/` nasce fora de versionamento).
- Nomenclatura: `artifacts/macos/ga106-ioreg-YYYYMMDD-HHMM-<tahoe-build>.txt` (texto de `ioreg -l -w0 -c IOPCIDevice`) + `artifacts/macos/ga106-sysprof-YYYYMMDD-HHMM.txt` (`system_profiler`). Um par por sessão, sem sobrescrever.
- **Sanitização obrigatória antes de qualquer citação em `docs/`** (o snapshot cru nunca é colado no repo): remover/redigir `Serial Number`, `Hardware UUID`, `MAC address`, `hostname`, `Apple ID`/`Team ID` pessoais, `IOPlatformSerialNumber`, `board-id`/`mlb` se presentes. Manter `vendor-id`/`device-id`/`class-code`/`built-in`/`IOPCITunnelled`/`AAPL,slot-name`/`IOServiceDEXTEntitlements` (são os campos de decisão). Em `docs/` citar só os campos da tabela §2 + build Tahoe, nunca o dump integral.
- Comando de captura (Tahoe, leitura + redirect local fora do repo? Não — dentro de `artifacts/`, que já é ignorado):
  ```sh
  ioreg -l -w0 -c IOPCIDevice > artifacts/macos/ga106-ioreg-$(date +%Y%m%d-%H%M)-tahoe.txt
  system_profiler SPPCIDataType SPDisplaysDataType -detailLevel mini > artifacts/macos/ga106-sysprof-$(date +%Y%m%d-%H%M)-tahoe.txt
  git status --short  # deve NÃO listar artifacts/macos/
  ```

## 4. `current provider` — como identificar o driver anexado SEM detach

Objetivo: responder "quem detém o nó agora?" sem tocar no binding (sem `unbind`, sem `IOServiceClose` forçado, sem `reset`, sem toggle).

```sh
# 4a. Filhos IOService do nó (o driver anexado aparece como filho; ausência de filho TinyGPU = MISS)
ioreg -l -w0 -c IOPCIDevice | grep -i -E "CFBundleIdentifier|IOClass|IOProviderClass|IOMatchCategory|TinyGPUDriver|tinygpu|AMDRadeon|AppleIntel|NVDA|GeForce|IOUserService" | head -n 60

# 4b. Subárvore do nó NVIDIA (trocar NOME-DO-NO pelo label visto em 1c, ex. IOPCIDevice@1 — sem alterar nada)
ioreg -l -w0 -r -n "IOPCIDevice" | grep -B2 -A10 -i "2504\|TinyGPU" | head -n 120

# 4c. KEXTs gráficos carregados (leitura; confirma dono display sem tocar)
kextstat | grep -i -E "nvidia|amd|radeon|whatevergreen|lilu|IOGraphics|IOAccelerator" | head -n 30

# 4d. Dexts ativadas (leitura; [activated enabled] vs [waiting for user] — nunca aprovar aqui)
systemextensionsctl list | grep -B2 -A6 -i "tinygrad\|tinygpu\|driver" | head -n 40
```

Leitura dos resultados:

| Observado | Significado |
|---|---|
| Filho `TinyGPUDriver` + `SetName("tinygpu")` + serviço `"tinygpu"` + `log show` com `opened device ven=... dev=...` | HIT — `Start_Impl` rodou naquele nó (decide A; se no nó interno, B/C=`FALSE` ali) |
| Filho driver AMD/Apple no `1638` + nenhum filho no `2504` + `systemextensionsctl list` sem TinyGPU ativa | MISS por ausência de match/ativação — cruzar com `built-in`/`IOPCITunnelled` do §2 para favorecer B vs C (sem provar mecanismo isolado) |
| `systemextensionsctl list` = `waiting for user` | Dext instalada mas não aprovada — MISS esperado é administrativo, não evidência B/C; não aprovar nesta sessão |
| Qualquer `IOPCIDevice::matchPropertyTable ... built-in ... isMatch=false` em log | Evidência direta da perna C (anotar linha integral sanitizada) |

Proibições reafirmadas neste §4: sem `systemextensionsctl developer on/reset/uninstall`, sem `kmutil trigger-panic-medic`, sem `kextunload`, sem `IOServiceOpen` manual, sem `FunctionReset/HotReset`, sem clicar Toggle ON. Identificar ≠ anexar.

## 5. Critério de sessão válida (checklist de aceite)

1. Desktop AMD confirmado antes de qualquer comando (§0).
2. `1b+1c+1d` executados e campos §2 registrados para **ambos** os nós + build Tahoe.
3. Snapshots em `artifacts/macos/` com nomes datados + `git status --short` limpo para `artifacts/`.
4. §4 respondido ("quem detém cada nó?") sem nenhum comando de alteração.
5. Nenhum `TinyGPU.app` instalado/ativado nesta sessão (qualquer HIT futuro exige revisão explícita + `amd-igpu-match-risk.md` reavaliado).

## 6. Referências

- `docs/macos/driver-architecture-gate.md` §3 (mecanismo `built-in` + `ioreg` obrigatório), §1.2/§1.4 (matching/entitlements/dev wildcard).
- `docs/tinygpu/internal-pcie-gap.md` Addendum TGM0-2 (matriz propriedade→A/B/C).
- `docs/tinygpu/binary-architecture.md` §3/§5/§9–§10 (personality shipped, allowlist, semântica tunnel corrigida).
- `docs/tinygpu/amd-igpu-match-risk.md` (por que a AMD é controle + por que não instalar).
- `docs/macos/ADR-0002-tinygpu-reuse.md` §5/§8 (follow-ups vivos, validação 0%).
- `.gitignore:7` (`artifacts/`).
