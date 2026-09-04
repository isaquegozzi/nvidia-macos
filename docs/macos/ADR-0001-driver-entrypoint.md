# ADR-0001: Entrypoint de driver macOS para laboratório PCI da GA106

> Status: **SUPERSEDED by ADR-0002 — UNDER_REVIEW (Fase TG0, documental)** — 2026-09-04.
> Sucedido por: `docs/macos/ADR-0002-tinygpu-reuse.md` (estratégia TinyGPU-compatible transport).
> Escopo histórico preservado abaixo; nenhuma conclusão nova neste arquivo.

## Contexto

Projeto `nvidia-macos`, Fase 0 (somente leitura no Linux). Alvo de longo prazo:
estudar viabilidade de suporte à GA106 no macOS Tahoe, começando por um
laboratório PCI (config space + BAR/MMIO somente leitura). Hardware: RTX 3060
interna em slot PCIe x16 de Hackintosh (não Thunderbolt, não Apple Silicon).
Sem entitlement Apple presumido. Sem teste executado no Tahoe nesta fase.

## Decisão (uma)

**`IOKit KEXT first` — o primeiro prototipado (quando/fase futura autorizar)
será um KEXT IOKit de laboratório somente-leitura; PCIDriverKit fica
explicitamente diferido como alternativa documentada, não como entrypoint.**

Esta decisão vale para o milestone **laboratório PCI**. Não é uma decisão de
driver gráfico e não autoriza escrita MMIO, firmware, power ou display.

## Drivers da decisão (por critério exigido)

1. **Carregabilidade real — vence B.** No Hackintosh Tahoe, KEXTs carregam por
   injeção OpenCore com evidência convergente (Lilu 1.7.1 / WEG 1.7.1–1.7.1d7,
   `kextstat` confirmando, Tahoe 26.1–26.2) [LIKELY — campo]. Dext PCI exige
   conta paga + wildcard dev + Mac registrado + aprovação SystemExtension +
   SIP off, sem nenhum precedente de GPU interna NVIDIA em slot x86 [LIKELY].
2. **Entitlements — vence B.** Dext de produção para `0x10DE` depende de
   aprovação Apple para vendor alheio (wildcard production indisponível nos
   relatos; processo de meses) [LIKELY]. KEXT de lab não tem gatekeeper Apple;
   com SIP off assinatura não é enforced em Intel [CONFIRMED — support page].
3. **PCI interno (`built-in`) — vence B por ausência de risco.** Enforcement
   `built-in` em dext é CONFIRMED (código `IOPCIDevice.cpp` + DTS), mas
   aplicabilidade à RTX `01:00.0` é UNCERTAIN até `ioreg` no Tahoe alvo.
   KEXT não tem esse check [CONFIRMED por ausência de mecanismo].
4. **MMIO — empate técnico, vantagem prática B.** Ambos leem config + BAR
   [CONFIRMED nas duas APIs]; dext media por `memoryIndex` + `Open` exclusivo
   + re-habilitar MSE/BME [CONFIRMED]; KEXT dá ponteiro direto via
   `mapDeviceMemoryWithRegister` [CONFIRMED doc legado]. Para bring-up futuro
   (GSP/doorbell/faults) o ponteiro direto é menos atrito.
5. **Futuro gráfico — empate distante, não decide.** Nem dext nem KEXT
   entrega `IOGPUFamily`/`IOAcceleratorFamily2`/`IOGraphics`/`WindowServer`/
   Metal; teto demonstrado é compute-only sem display (A) vs framebuffer
   patch legado (B) [LIKELY]. PCI lab ≠ driver gráfico — registrado para
   impedir extrapolação.
6. **Debugging — vence A, insuficiente para virar o jogo.** Dext depura em
   userspace sem parar o kernel [CONFIRMED — WWDC19/debugging doc]; KEXT
   paga panic=reboot + AuxKC+reboot por iteração [CONFIRMED]. Ergonomia não
   compensa gatekeeper + risco `built-in` no lab atual.
7. **Tahoe — vence B no horizonte do lab.** Tahoe é o último major Intel
   [CONFIRMED — anúncio Apple via imprensa]; KEXTs deprecated mas funcionais
   com aprovação em Intel [CONFIRMED]; alerta legacy existe mas não bloqueia
   [CONFIRMED]. Longevidade além-Tahoe favoreceria A, mas o alvo declarado é
   Tahoe Intel — otimizar para o alvo.
8. **Objetivo gráfico — neutro.** A escolha é pelo menor atrito até o primeiro
   byte PCI lido no macOS, não por quem chega ao Metal primeiro (ninguém
   chega sem GSP + stack completa, fora de escopo).

## Opções consideradas

- **A — PCIDriverKit first (rejeitado como first).** Ergonomia e
  modernidade corretas, mas pierde em carregabilidade lab, `built-in`
  condicional e gatekeeper `0x10DE`. Mantido como alternativa para futuro
  compute-only / Thunderbolt / Apple Silicon, ou se `ioreg` + teste dev
  wildcard eliminarem o risco interno.
- **B — IOKit KEXT first (aceito).** Deprecated porém carregável sem Apple,
  sem check `built-in`, com precedente Hackintosh Tahoe e mecanismo de spoof
  PCI (WEG) análogo ao lab. Custo assumido: reboots, panics, AuxKC/injeção.
- **NEED_MORE_EVIDENCE (rejeitado como decisão).** Há lacunas reais (ioreg
  `built-in`, teste wildcard dev na máquina), mas são follow-ups que não
  invertem a ordem: mesmo com `built-in` ausente, B segue com menor atrito
  institucional para o primeiro lab. Evidência faltante vira verificação,
  não indecisão.

## Consequências

- Próximo passo autorizado (fase futura, com `docs/SAFETY.md` + revisão):
  especificar KEXT de lab **somente-leitura** (match `10de:2504`,
  `configRead` + `mapDeviceMemory` read-only, logging, sem escrita/DMA/
  interrupção). Nenhum código nesta fase.
- Verificações obrigatórias antes de codificar (não bloqueiam este ADR):
  1. `ioreg -l -w0` no nó `10de:2504` no Tahoe: `built-in`,
     `IOServiceDEXTEntitlements`, `IOPCITunnelled`, `AAPL,slot-name`.
  2. Confirmar Lilu/WEG carregados via `kextstat` no Tahoe alvo.
  3. Se um dia testar A: wildcard dev `0xFFFFFFFF&0x00000000` + `dk=0x8001`
     em ambiente descartável, SIP off, sem promessa de produção.
- PCIDriverKit permanece documentado e reavaliável se: (a) Apple publicar
  isenção `built-in` para dev, (b) produção `0x10DE` for concedida, ou
  (c) alvo migrar para Thunderbolt/Apple Silicon compute-only.

## Referências

Gate completo e fontes pinadas em 2026-09-04:
`docs/macos/driver-architecture-gate.md` §6 (docs Apple, suporte, WWDC19/20,
código `IOPCIFamily/IOPCIDevice.cpp`, fóruns DTS, campo Lilu/WEG/mac-amdgpu/
vfio-apple). Nenhuma fonte nova introduzida por este ADR.
