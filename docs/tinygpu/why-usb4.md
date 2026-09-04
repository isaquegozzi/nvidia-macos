# TinyGPU — por que USB4 (TG0, análise estática)

> Fase TG0. SOMENTE análise estática. Nenhum hardware tocado, nenhum driver instalado.
> Escala: `CONFIRMED` = afirmado/provado no source/binário; `LIKELY` = implicado por constante/código sem afirmação direta; `UNCERTAIN` = insuficiente; `FALSE` = contradito pelo source; `UNKNOWN` = nenhuma evidência.
> Fontes pinadas: `tinygrad@e8c8ba1c`, `tinygpu_releases@c0d024f9` (`TinyGPU.zip sha256 0c47285e...`). Detalhes em `binary-architecture.md`, `internal-pcie-gap.md`, `pci-transport-contract.md`.

Requisito documentado: `USB4/Thunderbolt port` + `Plug the supported GPU into your Mac over USB4/Thunderbolt` (`docs/tinygpu.md:8,15@e8c8ba1c`), `macOS 13.0+`, `AMD RDNA3+ / NVIDIA Ampere+` (`docs/tinygpu.md:7,9`).

## Hipóteses A–F — vereditos

| Hipótese | Enunciado testado | Veredito | Evidência (source/binário) |
|---|---|---|---|
| A — física | Macs Apple Silicon não têm slot PCIe interno; USB4/TB é o único attach físico p/ GPU discreta | `LIKELY` | Consistente com `docs/tinygpu.md:8,15` (exige USB4/TB) + guia Apple Thunderbolt-first (`driver-architecture-gate.md` §1.1) + realidade de hardware; MAS o source TinyGPU sozinho não prova catálogo de hardware, e o gate registra exceção (slots em Mac Pro citado no WWDC20) — logo não é prova, é compatibilidade. Não confundir com enforcement (§D). |
| B — DriverKit `built-in` | Nó PCIe interno carrega propriedade `built-in`; sem entitlement Apple-internal `...builtin` o dext não casa; tunneled nunca tem `built-in`, então USB4 evita o gatekeeper | `LIKELY` | Mecanismo `CONFIRMED` fora deste doc (`IOPCIDevice::matchPropertyTable` checa `built-in` + `kIODriverKitTransportBuiltinEntitlementKey`, DTS confirma — ver `driver-architecture-gate.md` §3.1); shipped dext NÃO tem `...builtin` (entitlements extraídos em `binary-architecture.md` §5 têm só `driverkit` + `transport.pci` vendor); aplicabilidade ao nó interno específico é `UNCERTAIN` até `ioreg` (`built-in` depende de ACPI/OpenCore). Como explicação do why-USB4: `LIKELY` co-causa, não prova isolada. |
| C — deliberado (escopo/produto) | tinygrad escolheu deliberadamente escopo externo: class-match + allowlist de vendors + docs de setup | `LIKELY` (p/ "externo") — [SUPERADO: `CONFIRMED` — ver Correção TGM0] | Escolhas explícitas no source: `IOPCIClassMatch=0x03000000` (`TinyGPUDriverExtension/Info.plist:15-16@e8c8ba1c`, idêntico no binário), allowlist `10de+1002` (`TinyGPUDriver.Release.entitlements:10-19`, idêntico no provision shipped), `docs/tinygpu.md:9` (RDNA3+/Ampere+), `Start_Impl` loga `ven/dev` e registra `tinygpu` (`TinyGPUDriver.cpp:52-68`). Alguém setou cada flag — decisão deliberada provada. [Correção TGM0: este `CONFIRMED` prova escopo deliberado vendor/classe, NÃO "externo-only" como enforcement; o "externo" repousa só em `docs/tinygpu.md:8,15` (requisito USB4/TB) + `IOPCITunnelCompatible` como declaração de suporte — rebaixado a `LIKELY`.] |
| D — verificação de transporte (tunnel enforcement) | `IOPCITunnelCompatible=true` filtra provedores não-tunneled no matching; interna falha ali mesmo que classe/vendor batam | `UNKNOWN` (filtro) — [SUPERADO: `CONFIRMED` — ver Correção TGM0] | Flag presente nas DUAS pontas: source `Info.plist:17-18@e8c8ba1c` e dext shipped decodificado (`binary-architecture.md` §3: `IOPCITunnelCompatible=True`) — presença permanece `CONFIRMED` como fato. [Correção TGM0: a semântica "filtra não-tunneled" é SUPERADA. Apple documenta a chave como declaração de capacidade do driver, não como exclusão; sem evidência de checagem de `IOPCITunnelled` no driver/server, o filtro é `UNKNOWN`. É o ponto reaberto do gap (ver `internal-pcie-gap.md` Correção TGM0).] |
| E — ASM2464 (bridge específico) | TinyGPU.app exige bridge USB4-PCIe específico (ASMedia ASM2464PD / firmware custom) | `FALSE` (p/ TinyGPU.app) | Zero ocorrências de `ASM/2464/174c/LTSSM/custom-firmware` em `strings -a` do app (2898 linhas) e do dext (944 linhas) + zero no `server.c`/`TinyGPUDriver*.cpp/.iig` (`rg ASM extra/usbgpu/tbgpu` vazio). ASM vive em OUTRO transporte: `CustomASM24Controller` + `USBPCIDevice` Linux (`usb.py:135`, `system.py:228-248`, `ops_amd.py:979`), que exige `product custom/AS2462` (`usb.py:57`) — caminho distinto do APL/unix-socket. Bridge específico pode importar p/ enclosures em campo, mas NÃO é requisito do `.app` provado pelo source/binário. |
| F — outra (display/BAR/DMA/power/BIOS) | Algum outro gargalo (display attach, BAR grande/REBAR, DMA IOVA 40-bit, power, OptionROM) explica o USB4 | `UNCERTAIN` | Nenhuma evidência match-time além de A–D no source/binário. Downstream existem limites reais (`SetupDMA maxAddressBits=40` em `TinyGPUDriver.cpp:124`, `PrepareDMA ≥4097B`/`seg≤32`, `BULK 64MB`, `MAX_SYSMEM 128` em `server.c:40-50`, command `IOSpace\|BusMaster\|MemorySpace` em `TinyGPUDriver.cpp:59-60`, teto compute-only sem display em `driver-architecture-gate.md` §4-5), mas nenhum é filtro USB4-vs-interna no matching. Sem `ioreg`/teste vivo, F permanece hipótese aberta, não explicação. |

## Leitura conjunta (1 frase por hipótese — vereditos finais)

- A (física): Macs alvo não têm slot interno, então USB4/TB é o único attach físico plausível, mas o source sozinho não prova catálogo de hardware.
- B (`built-in`): Enforcement `built-in` existe e o dext shipped não tem a isenção, então interna com `built-in` falharia, mas sem `ioreg` não se prova que toda interna tem `built-in`.
- C (deliberado): Escopo vendor/classe é decisão explícita e provada pelas flags/entitlements/docs no source e no binário (`CONFIRMED` restrito a isso); o "externo-only" como enforcement é `LIKELY`, não provado [SUPERADO o `CONFIRMED` amplo — ver Correção TGM0].
- D (transporte): `IOPCITunnelCompatible=true` nos dois lados prova declaração de suporte Thunderbolt, NÃO enforcement tunneled-only no matching [SUPERADO o "prova enforcement" — ver Correção TGM0: filtro = `UNKNOWN`].
- E (ASM2464): Requisito de bridge específico é falso para o `.app` (evidência zero; ASM é outro transporte Linux).
- F (outra): Qualquer outro gargalo permanece incerto sem evidência match-time ou teste vivo.

> **SUPERADO (Correção TGM0):** a conclusão abaixo foi substituída — ver § "Correção TGM0". Texto original preservado:
>
> Conclusão TG0 (original): why-USB4 = `C CONFIRMED` + `D CONFIRMED` (decisão + enforcement provados) com `A/B LIKELY` (física e `built-in` como co-causas consistentes), `E FALSE` (p/ `.app`), `F UNCERTAIN`.
>
> Conclusão TG0 corrigida: why-USB4 = `C LIKELY` (externo como escopo documentado/declarado, não como enforcement provado) + `D UNKNOWN` (filtro tunneled sem evidência) com `A/B LIKELY` (inalterados), `E FALSE` (p/ `.app`, inalterado), `F UNCERTAIN` (inalterado).
> Nada aqui autoriza escrita MMIO, firmware, power ou display; PCI lab ≠ driver gráfico (`driver-architecture-gate.md` §4).

## Correção TGM0 (2026-09-04) — semântica `IOPCITunnelCompatible`

Bug TG0: `IOPCITunnelCompatible=true` foi lido como prova de "Thunderbolt-only" (i.e., o matching excluiria provedores não-tunneled). Essa leitura é inválida perante a documentação Apple atual (fonte canônica):

- "To indicate that your PCIe driver supports Thunderbolt, include the `IOPCITunnelCompatible` key in your driver's personality dictionaries. Include this key only if the `IOService` subclass [...] is able to communicate with your Thunderbolt device." — `developer.apple.com/documentation/pcidriverkit/creating-custom-pcie-drivers-for-thunderbolt-devices` (seção "Add Thunderbolt Support to Your Driver's Personality Data"). A chave é declaração de **capacidade do driver**, não filtro de exclusão.
- "To declare support for Thunderbolt technology, the following key must be added [...] or the `IOPCIFamily` will not load the drivers for Thunderbolt connected PCI devices. This opt-in key protects consumers against older drivers that have not yet been updated to support Thunderbolt technology." — Thunderbolt Device Driver Programming Guide (arquivo Apple, "Enabling PCI Device Drivers for Thunderbolt Operation"). O opt-in protege o caminho tunneled contra drivers antigos; a recíproca (presença excluiria o caminho não-tunneled) **não** é documentada.
- "PCI device drivers can determine if a Thunderbolt device is connected by recursively searching over parents in the I/O Registry for the key `IOPCITunnelled`." (mesmo guia, "Verifying Thunderbolt Connections in PCI Devices", `getProperty(kIOPCITunnelledKey, gIOServicePlane, kIORegistryIterateRecursively | kIORegistryIterateParents)`). A presença real de túnel é indicada por **`IOPCITunnelled` no IORegistry** (propriedade do dispositivo/caminho), não pela personality do driver.

Nova classificação (o que muda e o que permanece):

| Hipótese | Nova classificação | Base |
|---|---|---|
| C — deliberado (externo) | `LIKELY` (rebaixado de `CONFIRMED`); `CONFIRMED` restrito a "escopo deliberado vendor/classe" | Class-match + allowlist + `Start_Impl`/`tinygpu` provam decisão deliberada de escopo (inalterado). O "externo-only" repousa agora só em `docs/tinygpu.md:8,15` (requisito USB4/TB documentado) + `IOPCITunnelCompatible` como suporte declarado — compatível com escolha externa, sem provar enforcement. Nova base documentada, sem a base inválida (filtro). |
| D — verificação de transporte (tunnel enforcement) | `UNKNOWN` (filtro; rebaixado de `CONFIRMED`) | Presença da flag nas duas pontas segue `CONFIRMED` como fato. Mas zero evidência TG0 de exclusão de não-tunneled: nenhuma checagem de `IOPCITunnelled` no código/server (`TinyGPUDriver*.cpp/.iig`, `server.c`) nem nas strings do dext (`binary-architecture.md` §7: zero `Thunderbolt/USB4/tunnel` no texto do binário — tunnel vive só no plist). Sem essa evidência, "filtra" é `UNKNOWN` (nenhuma evidência de enforcement), não `CONFIRMED`. |

A–B, E–F inalterados. Consequências: `internal-pcie-gap.md` (gap/tunnel-✘/Resultado A/B/C reabertos), `binary-architecture.md` §3 ("Thunderbolt-only? SIM" superado), `ADR-0002` (passagens C+D como prova superadas — ver addendum naquele arquivo).
