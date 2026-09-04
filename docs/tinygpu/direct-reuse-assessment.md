# TinyGPU direct-reuse assessment — shipped `.app` → RTX interna `10de:2504` → NVDev (TGM0)

> Fase TGM0. SOMENTE análise estática, SOMENTE `.md`. Nada executado, nenhum hardware tocado, nenhum driver instalado.
> Fontes pinadas: `tinygrad@e8c8ba1c`, `tinygpu_releases@c0d024f9` (`TinyGPU.zip sha256 0c47285e…`, 1634625 B). Escala de evidência: `CONFIRMED`/`LIKELY`/`UNCERTAIN`/`UNKNOWN`. Escala da hipótese: `PLAUSIBLE`/`UNLIKELY`/`BLOCKED`/`UNKNOWN`.
> Não confundir com fork wire-compatível (estratégia C, `ADR-0002` §8 `LOW`): aqui se avalia **somente** o reuse direto sem fork.

## 0. Hipótese H0 e veredito

**H0:** o `TinyGPU.app` shipped (`c0d024f9`, sem modificação) casa a RTX interna `10de:2504` em slot x16 via matching DriverKit, expõe serviço `"tinygpu"`, e o caminho `APLRemotePCIDevice → PCIIface → NVDev → GSP → GPFIFO → compute` completa sem patch de `Info.plist`/entitlements.

**Veredito H0: `UNLIKELY`.**

Não é `PLAUSIBLE` (o escopo documentado é USB4/TB externo + há risco `built-in` + zero demonstração viva em interna), não é `BLOCKED` (bloqueio não provado: filtro tunneled = `UNKNOWN`, `built-in` na `2504` = `UNCERTAIN` até `ioreg`, FW `ga102`-em-GA106 = `UNCERTAIN` não `FALSE`), não é `UNKNOWN` puro (há evidência documental contra o caminho interno — requisito USB4/TB `LIKELY` + veredito `RISK` na iGPU — suficiente para pender a `UNLIKELY` sem fechar como impossível).

## 1. Evidências por camada (por que `UNLIKELY` e não outro nível)

| Camada | Contribuição | Evidência (source/binário) |
|---|---|---|
| Classe PCI | Passa (`CONFIRMED`, não salva H0) | `IOPCIClassMatch=0x03000000` (plist dext decodificado + source `Info.plist:15-16@e8c8ba1c`, `binary-architecture.md` §3/§10) casa `base_class=0x03` do `PCIIface` (`ops_nv.py:556-562`); `10de:2504` é `0x030000` — classe não exclui a interna |
| Entitlement vendor | Passa (`CONFIRMED`, não salva H0) | `transport.pci=[10de any, 1002 any]` (`0x000010de&0x0000FFFF`, blob + provision convergentes, `binary-architecture.md` §5); `10de:2504` passa; variante só-NVIDIA não é a shipped |
| Escopo documentado externo | Contra H0 (`LIKELY`) | `docs/tinygpu.md:8,15@e8c8ba1c` exige `USB4/Thunderbolt port` + `Plug the supported GPU … over USB4/Thunderbolt`; `IOPCITunnelCompatible=True` declara suporte Thunderbolt (`why-usb4.md` Correção TGM0). Escopo externo é deliberado em vendor/classe (`CONFIRMED`) e documentado como requisito (`LIKELY`); nenhum doc mostra interna suportada |
| Filtro tunneled (`IOPCITunnelCompatible` como exclusão) | Aberto (`UNKNOWN`, impede `BLOCKED` e impede `PLAUSIBLE`) | Presença nas duas pontas `CONFIRMED` como fato; semântica de exclusão **não** documentada pela Apple + zero checagem de `IOPCITunnelled` no driver/server/strings (`binary-architecture.md` §7/§9, `why-usb4.md` D, `internal-pcie-gap.md` Resultado B). Interna pode ou não casar — só `ioreg` + teste vivo decidem |
| `built-in` | Risco aberto (`UNCERTAIN`, pesa contra H0 sem fechar) | Mecanismo de exclusão `CONFIRMED` (`IOPCIDevice::matchPropertyTable` + DTS, `driver-architecture-gate.md` §3.1); dext shipped sem `...builtin` (`CONFIRMED` ausência); presença de `built-in` no nó `10de:2504` depende de ACPI/OpenCore — `REQUIRES_MACOS_INSPECTION` (gate §3.2, `internal-pcie-gap.md` Resultado C) |
| iGPU AMD `1002:1638` como colateral | Contra teste vivo de H0 (`RISK`) | Classe casa + vendor `1002` na allowlist ⇒ elegível (`CONFIRMED`); precedência vs driver AMD de display = `UNKNOWN`; veredito `RISK` (`amd-igpu-match-risk.md` §6). H0 não pode ser testado "só vendo se acontece" sem aceitar risco de display — **NÃO instalar** |
| Transporte wire (se o match ocorresse) | A favor parcial (`LIKELY` condicional) | 8/8 cmds APL (`MAP_BAR/CFG/MMIO/SYSMEM_FD/RESET/RESIZE noop`) + fio 33B/17B + `SCM_RIGHTS` só cmd 2 provados estaticamente (`pci-transport-contract.md` §2–§3). Wire não distingue interna vs tunneled **após** o match — mas nunca foi observado vivo no Tahoe (validação macOS 0%) |
| Bring-up GA106 via NVDev (se o transporte abrisse) | Aberto (`UNCERTAIN`, impede `PLAUSIBLE`) | 10/10 genérico Ampere, 0/10 `ga106`-específico; GA106 cai em `GA1/ga102` por `arch 0x17`, FW `ga102-570.144` sem SHA `ga106` (`ga106-gap-analysis.md` §26-30, `nvdev-audit.md` §6); `GSP_INIT_DONE`/`promote_ctx`/`sm_86` nunca logados — fecha **se** o RM aceitar GA106 como `GA102`, o que é `UNCERTAIN` sem HW |
| SKU verificado | Ausente (`UNKNOWN`) | `grep 2504\|GA106` no clone = zero fora constante de header (`support-matrix.md` §5); arquitetura Ampere suportada = `CONFIRMED`, SKU `2504` verificado = `UNKNOWN` — H0 exige o segundo |

Leitura conjunta: duas camadas passam (classe, vendor) mas são pré-condições, não prova; duas camadas documentam escopo externo/risco (`LIKELY` + `RISK`); três camadas estruturais estão em aberto (`UNKNOWN`/`UNCERTAIN`/`UNKNOWN`); nenhuma camada prova impossibilidade. Soma = `UNLIKELY`.

## 2. Por que não os outros níveis (teste de honestidade)

- **Não `PLAUSIBLE`:** exigiria ao menos (a) `ioreg` sem `built-in` + sem dependência de túnel, ou (b) relato/log de shipped casando interna. Temos o oposto: requisito USB4 documentado + `RISK` AMD + validação 0% (`ADR-0002` §8.2).
- **Não `BLOCKED`:** exigiria MISS observado + causa isolada (ex. log `isMatch=false` por `built-in`, ou prova de filtro tunneled). Temos ausência de evidência, não evidência de ausência (B=`UNKNOWN`, C=`UNCERTAIN`).
- **Não `UNKNOWN` puro:** `UNKNOWN` diria "sem nenhuma razão para pender". Temos razões para pender contra (escopo USB4/TB + `RISK`), sem prova de impossibilidade — definição de `UNLIKELY`.

## 3. `PROVADO` vs `NÃO PROVADO` (nada além de source/binário + aritmética)

**PROVADO** (em `tinygrad@e8c8ba1c` + `tinygpu_releases@c0d024f9`):

- `PCIIface` enumera `10de:2504` (`0x2504&0xFF00=0x2500 ∈ devlist`, `ops_nv.py:560` + `system.py:80`) — YES restrito a enumeração (`ops-nv-audit.md` §2–§3).
- Personalidade shipped = `IOProviderClass=IOPCIDevice` + `IOPCIClassMatch=0x03000000` + `IOPCITunnelCompatible=True`, sem `IOPCI*Match` por ID; entitlements `[10de any, 1002 any]`, sem `...builtin` (`binary-architecture.md` §3/§5/§10).
- `IOPCITunnelCompatible` declara capacidade Thunderbolt, não prova exclusão (`why-usb4.md` Correção TGM0 + docs Apple canônicas).
- Fio APL 8/8 cmds + dext source ~95% além do matching (`pci-transport-contract.md`, `apl-remote-pci.md`, `remote-protocol.md`).
- 10/10 etapas NVDev genéricas Ampere; 0/10 `ga106`-específicas (`ga106-gap-analysis.md` §26-30).
- Dext elegível para `1002:1638` por classe+vendor; precedência vs display desconhecida (`amd-igpu-match-risk.md`).

**NÃO PROVADO** (exige `ioreg`/HW vivo, fora TGM0):

- Que o shipped instancia `TinyGPUDriver` no nó interno `10de:2504` (HIT com `Start_Impl ven/dev` + serviço `"tinygpu"`) ou falha deterministicamente (`server.c:195`).
- Qual perna excluiria a interna (B tunnel vs C `built-in`) — matriz em `internal-pcie-gap.md` Addendum TGM0-2, procedimento em `docs/macos/hardware/ga106-ioreg-runbook.md`.
- Que `NVDev → GSP_INIT_DONE → promote_ctx → SEND_PCAS_A/sm_86` completa em GA106 com FW `ga102-570.144` (qualquer transporte).
- Que `BOOT0 ≈ 0x176000A1`, `BAR1`/`0x1183a4`/`0xbb0000`, IOVA `PrepareDMA(40)` comportam-se igual na interna via `MAP_BAR`/`MMIO_READ`.
- Qualquer afirmação Metal/display (fora de escopo — `project-impact.md` §2).

## 4. Consequência (doc-only)

- H0 `UNLIKELY` ⇒ estratégia D (reuse direto sem fork) segue **rejeitada para a interna** como caminho não demonstrado (`ADR-0002` §D + §7/§8) — não como impossibilidade provada. Teste de controle válido só com eGPU USB4/TB + `ioreg` controle, nunca instalando às cegas no lab de display AMD.
- Caminho alternativo (fork wire-compatível, estratégia C) é hipótese distinta, avaliada em `ADR-0002` §8 (`MAC-COMPUTE-0 = LOW`) — H0 não a contamina nem a valida.
- Desbloqueio de H0 para reavaliação: `ioreg` Tahoe nos nós `2504`+`1638` (`built-in`, `IOPCITunnelled`, `class-code`, `IOServiceDEXTEntitlements`, `AAPL,slot-name`) + `systemextensionsctl list` + `log show --predicate tinygpu`, tudo pelo runbook somente-leitura. Sem isso, H0 permanece `UNLIKELY`.

## 5. Referências

- `docs/tinygpu/why-usb4.md` (C `LIKELY` externo, D `UNKNOWN` filtro, E `FALSE`, Correção TGM0).
- `docs/tinygpu/internal-pcie-gap.md` (gap + Resultado A/B/C + matriz TGM0-2).
- `docs/tinygpu/binary-architecture.md` §3/§5/§7/§9–§10 (matching, allowlist, zero tunnel nas strings, correção).
- `docs/tinygpu/amd-igpu-match-risk.md` (elegibilidade `1638`, veredito `RISK`).
- `docs/tinygrad-nv/ga106-gap-analysis.md` §26-30 + leitura do gap; `docs/tinygrad-nv/ops-nv-audit.md` §2–§4; `docs/tinygrad-nv/nvdev-audit.md` §0–§6; `docs/tinygpu/support-matrix.md` §5 (arquitetura vs SKU).
- `docs/macos/driver-architecture-gate.md` §3 (mecanismo `built-in`, aplicabilidade `UNCERTAIN`); `docs/macos/ADR-0002-tinygpu-reuse.md` §§2–8; `docs/macos/hardware/ga106-ioreg-runbook.md` (procedimento vivo).
- `docs/ga106/first-mmio-read.md` (candidato `BOOT0`, `BLOCKED`).
