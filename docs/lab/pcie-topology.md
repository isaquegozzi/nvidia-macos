# PCIe Topology — GA106 (`01:00.0`) sob root-port `00:01.1` (Cezanne) — somente leitura

> Status: Fase 0 — **somente leitura**. Data: 2026-09-04. Kernel: `7.2.2-1-cachyos`.
> CPU: `AMD Ryzen 5 5600G with Radeon Graphics` (`/proc/cpuinfo`).
> GPU: `0000:01:00.0 [10de:2504]` rev `a1` (GA106 RTX 3060) + áudio `01:00.1 [10de:228e]`.
> Parent: `0000:00:01.1 [1022:1633]` (`AMD Renoir PCIe GPP Bridge`, driver `pcieport`).
> Proibições respeitadas: sem `mmap`, sem MMIO, sem unbind, sem `setpci` write,
> sem carga artificial, sem `/dev/mem`. Apenas `lspci` (leitura) + `cat` em sysfs.

## 1. Tabela canônica GT/s → Gen (referência deste repo)

| `current/max_link_speed` (sysfs) | Gen correta | Largura neste host |
|---|---|---|
| `2.5 GT/s PCIe` | **Gen1** | x16 (observado em idle) |
| `5.0 GT/s PCIe` | **Gen2** | x16 (baseline 19:17 UTC) |
| `8.0 GT/s PCIe` | **Gen3** | x16 (max do root-port) |
| `16.0 GT/s PCIe` | **Gen4** | x16 (max da GPU) |
| `32.0 GT/s PCIe` | **Gen5** | — (não presente neste host) |

> Erro a não propagar: **`5.0 GT/s = Gen3` está errado; `5.0 GT/s = Gen2`.**
> Corolário: **`16.0 GT/s = Gen4` (capacidade da GA106), `8.0 GT/s = Gen3`
> (capacidade do root-port Cezanne)** — ver §4.

### 1.1 Auditoria do erro neste repo (2026-09-04)

Busca exaustiva por propagação do erro (todos read-only, sem escrita):

```sh
rg -n "GT/s" .                       # só baselines com valores crus, sem rótulo Gen
rg -n -i "gen.?[12345]|PCIe ?[0-9]" . # só 3 ocorrências de "PCIe4 x16", todas corretas
rg -n "5\.0|8\.0|16\.0|Gen3 x16|abaixo.*Gen4" docs/ artifacts/ READMEs
```

Resultado: **zero arquivos propagam `5.0 GT/s = Gen3` ou `Gen3 x16 abaixo do Gen4`.**

- `docs/ampere/architecture-overview.md:23`, `docs/ga106/device-ids.md:44`,
  `docs/nvidia-open/GA106-kernel-map.md:77` dizem `PCIe4 x16` para a GA106 —
  **correto** (capacidade do endpoint, confirmada por `max_link_speed=16.0`).
- `artifacts/baselines/20260904-191707,191712` registram `5.0`/`16.0 GT/s` crus,
  **sem conversão para Gen** — nenhum rótulo a corrigir.
- `linux/ga106-lab/src/baseline.cpp` (`read_link_view`) lê as 4 strings sysfs
  e as imprime cruas (`Link current/max: width=… speed=…`) — **sem mapeamento
  Gen no código**, nenhum fix necessário.

Nenhum arquivo existente foi alterado por esta auditoria; esta tabela (§1) é a
referência canônica para impedir regressão.

## 2. Método (comandos executados, todos read-only)

| # | Comando | Propósito |
|---|---|---|
| 1 | `lspci -tv` | hierarquia: quem é pai de `01:00.x` |
| 2 | `lspci -nn \| grep -i bridge` | IDs de todos os bridges |
| 3 | `lspci -vv -s 01:00.0` | detalhe GPU (caps = `<access denied>` sem root; registrado como limitação) |
| 4 | `lspci -vv -s 00:01.1` | detalhe do bridge pai real (idem) |
| 5 | `cat /sys/bus/pci/devices/0000:01:00.0/current_link_width current_link_speed max_link_width max_link_speed` | capability vs estado atual da GPU |
| 6 | `cat /sys/bus/pci/devices/0000:00:01.1/current_link_width current_link_speed max_link_width max_link_speed` | capability vs estado atual do root-port |
| 7 | `cat /sys/bus/pci/devices/0000:01:00.0/device vendor` + `.../00:01.1/device vendor` | IDs `10de:2504` / `1022:1633` via sysfs |
| 8 | `lspci -nn -s 01:00.1` | confirmar função áudio irmã |
| 9 | Releitura `current_link_speed` GPU + pai em `T+5s` (`19:27:49Z` → `19:27:54Z`), sem gerar carga | testar estabilidade/variação só por observação |

`lspci -vv` como não-root retorna `Capabilities: <access denied>` nos dois
dispositivos — por isso `LnkCap/LnkSta` são lidos via sysfs (`max/current_link_*`),
fonte autoritativa aqui. Nenhum `setpci`, `mmap`, `/dev/mem`, unbind ou benchmark
foi executado.

## 3. Evidências (resumo fiel)

### 3.1 Hierarquia (`lspci -tv`, 2026-09-04)

```text
-[0000:00]-+-00.0  Renoir/Cezanne Root Complex
           +-01.0  Renoir PCIe Dummy Host Bridge
           +-01.1-[01]--+-00.0  NVIDIA GA106 [GeForce RTX 3060 Lite Hash Rate]
           |            \-00.1  NVIDIA GA106 High Definition Audio Controller
```

GPU `01:00.0` + áudio `01:00.1` estão sob o GPP Bridge `00:01.1`. Pai real
confirmado — não há outro candidato (`00:01.0` é Dummy Host Bridge, sem janela).

### 3.2 Bridges (`lspci -nn | grep -i bridge`, excerto)

```text
00:01.1 PCI bridge [0604]: AMD Renoir PCIe GPP Bridge [1022:1633]
```

### 3.3 sysfs — GPU `0000:01:00.0` (2026-09-04T19:27Z)

```text
current_link_width=16
current_link_speed=2.5 GT/s PCIe   (= Gen1 x16)
max_link_width=16
max_link_speed=16.0 GT/s PCIe      (= Gen4 x16)
device=0x2504  vendor=0x10de
```

### 3.4 sysfs — root-port `0000:00:01.1` (mesmo instante)

```text
current_link_width=16
current_link_speed=2.5 GT/s PCIe   (= Gen1 x16)
max_link_width=16
max_link_speed=8.0 GT/s PCIe       (= Gen3 x16)
device=0x1633  vendor=0x1022
```

### 3.5 Variação só por observação (sem carga)

- Baselines `artifacts/baselines/20260904-191707` e `191712` (19:17 UTC):
  GPU `current: width=16 speed=5.0 GT/s PCIe` (= **Gen2 x16**).
- Auditoria atual (19:27 UTC, duas leituras a 5 s, idle, sem benchmark):
  GPU **e** root-port `current: 16 / 2.5 GT/s` (= **Gen1 x16**), estável nas 2 leituras.
- Delta `5.0 → 2.5 GT/s` ocorreu **entre** sessões de leitura passiva; nenhuma
  carga foi gerada para forçar transição. Comportamento compatível com ASPM /
  economia de energia em idle — **não é defeito e não se chama de problema**.

## 4. Topologia real (os 4 valores pedidos)

| Ponto | Largura | Velocidade (sysfs) | Gen equivalente |
|---|---|---|---|
| **GPU max** (`01:00.0`) | x16 | `16.0 GT/s PCIe` | **Gen4 x16** |
| **Root-port max** (`00:01.1`) | x16 | `8.0 GT/s PCIe` | **Gen3 x16** |
| **GPU current** (19:27 UTC; era `5.0` às 19:17) | x16 | `2.5 GT/s PCIe` | **Gen1 x16** (antes Gen2 x16) |
| **Root-port current** (19:27 UTC) | x16 | `2.5 GT/s PCIe` | **Gen1 x16** |

Distinções obrigatórias:

- **GPU capability** (`16.0` = Gen4 x16): o que a GA106 anuncia que suporta.
  Confere com `PCIe4 x16` nos docs (`device-ids.md`, `architecture-overview.md`).
- **Upstream/root-port capability** (`8.0` = Gen3 x16 em `00:01.1 [1022:1633]`):
  o teto do outro lado do mesmo link. O link negociado nunca excede o menor
  dos dois tetos → teto operacional desta bancada = **Gen3 x16**.
- **Plataforma/CPU** (Ryzen 5 5600G, Cezanne): oficialmente limitada a
  **PCIe 3.0** — consistente com o teto `8.0` observado no root-port. A GPU ser
  Gen4-capaz **não** implica link Gen4 neste socket.
- **Current operating link** (`2.5` agora, `5.0` no baseline): estado de energia
  momentâneo em idle, **não** capacidade nem defeito. `5.0 GT/s = Gen2`, não Gen3.

## 5. Conclusão + Confidence

- **Conclusão:** O link `01:00.0 ↔ 00:01.1` opera hoje em Gen1/Gen2 x16 por economia
  de energia em idle, com teto Gen3 x16 imposto pelo root-port Cezanne embora a GA106
  seja Gen4-capaz; nenhum valor observado indica degradação ou configuração abaixo do esperado.
- **Confidence:**
  - `CONFIRMED` — hierarquia (`01:00.x` sob `00:01.1`), os 4 valores sysfs acima,
    IDs `10de:2504` / `1022:1633`, variação `5.0 → 2.5` só por observação passiva.
  - `LIKELY` — atribuição do teto `8.0` (Gen3) ao limite oficial PCIe 3.0 do
    Ryzen 5 5600G (coerente com root-port Renoir/Cezanne, sem re-checagem do
    datasheet AMD nesta passada).
  - `UNCERTAIN` — nada pendente nesta topologia; `LnkCap/LnkSta` via `lspci -vv`
    seguem `<access denied>` sem root (sysfs supre; re-ler com `sudo lspci -vv`
    é opcional e fora desta auditoria read-only).

## 6. Próximos passos (somente leitura)

1. Re-executar §2 itens 5–6 após qualquer mudança de firmware/BIOS e anexar saída
   datada aqui antes de qualquer experimento com a RTX.
2. Não gerar carga para "provar Gen3/Gen4" — Fase 0 proíbe carga artificial;
   o teto já está provado pelos `max_link_*`.
3. Se futura releitura mostrar `current` diferente (ex. `8.0`), registrar como
   variação de energia, não como fix/degradação.
