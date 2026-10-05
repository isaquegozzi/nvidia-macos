> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# TG-KEXT0-RESEARCH — KEXT IOKit mínimo p/ GA106 (pesquisa, SEM implementar)

> Fase de PESQUISA APENAS (2026-09-06, NDRV0 saudável). Contexto: auditoria Astra
> classificou `THIRD_PARTY_BUILTIN_DRIVERKIT_DEV = LIKELY_BLOCKED` e
> `PAID_DEVELOPER_ACCOUNT_WOULD_UNLOCK_TG_DK0 = LIKELY_NO` → TG-DK0 SUSPENDED
> (spec preservada), sem Apple Developer Program. Nada compilado/instalado/alterado.
> Destino final no repo: `docs/macos/TG-KEXT0-research.md`.

## Tese

Um IOKit KEXT casa por personalities `Info.plist` SEM validação de entitlement.
O gate `built-in` auditado vive DENTRO do ramo dext do matching:

```cpp
// IOPCIFamily/IOPCIDevice.cpp:866-899 @main (tocado 2025-10-04 4822b27a)
if ((isMatch == true) && (table->getObject(kIOPCITransportDextEntitlement))) {
    // ... entitlement array / builtin-exemption scan ...
    if ((getProperty("built-in") != NULL) && (builtInEntitled != true))
        isMatch = false;   // linha ~897
}
```

Personalities KEXT nunca carregam `kIOPCITransportDextEntitlement` → ramo pulado →
sem gate `built-in` no caminho KEXT. Mecanismo: HIGH (source); byte-identidade com o
Tahoe 25G83: LIKELY (fonte aberta pode divergir do build; sem evidência contrária).

## Viabilidade Tahoe + Hackintosh/OpenCore (evidência viva local)

KEXTs de terceiros carregam NESTE Tahoe via injeção OC: `Lilu 1.7.3`,
`NootedRed 0.8.10`, `AppleALC 1.9.8`, `VirtualSMC 1.3.8` (kmutil/kextstat 2026-09-05/06,
`Kernel→Add` com 12 entradas, `config.plist` do pendrive). AuxKC não é obstáculo no
fluxo OC (injeção pré-kernel; aprovação/ rebuild AuxKC é o fluxo Apple p/ installs
normais — irrelevante aqui). Tahoe é o último macOS Intel — janela fechada mas aberta.

## Matching/timing

Personality `IOProviderClass=IOPCIDevice` + `IOPCIPrimaryMatch=0x250410de`
(formato `0xDDDDVVVV` provado pela doc Apple: `0x00261011`=1011:0026).
Casa RTX e SÓ ela (AMD `0x16381002`≠, HDAU `0x228e10de`≠ — aritmética).
Timing: matching ocorre no publish PCI (IOPCIConfigurator), KEXT injetado presente
antes do kernel iniciar → anexa no primeiro boot com a personality, sem userspace.

## Sem blocker builtin equivalente

`KEXT_BUILTIN_AUTH_BLOCKER = NO` (mecanismo: o único gate `built-in` do source é o
ramo dext citado; KEXT não tem entitlement a validar; nenhuma checagem `built-in`
fora desse ramo no `matchPropertyTable` auditado).

## Provider: anexar vs controlar (distinção crítica)

- Anexar (`probe`+`start` sem `Open()`): só observa (lê properties do registry já
  publicado: vendor/device/class/built-in/BDF). Zero mutação esperada.
- Controlar (`Open()` + `mapDeviceMemory`/`configRead|Write`/MSI/DMA): assume o
  dispositivo (exclusivo), toca Command Register/BARs/estado PCIe. FORA do KEXT0 —
  primeira fase prova SÓ attach, como TG-DK0 provaria SÓ match/start.

## ignore-ndrv, coexistência, tinygrad

- `AAPL,iokit-ignore-ndrv` PRESENTE é lido por `IONDRVFramebuffer::probe`, não pelo
  matching PCI → KEXT casa normalmente; gate e KEXT são ortogonais. Manter (baseline).
- Coexistência NootedRed/AMD: providers disjuntos (1:0:0 vs 8:0:0), categorias e IDs
  disjuntos; AMD segue com driversStarted como hoje. Sem interação prevista.
- Transporte p/ tinygrad futuro: KEXT exporia `IOUserClient` → daemon userspace →
  socket no formato do fio TinyGPU (ou shm BAR read-only mapeado via user client).
  NVDev/tinygrad continuariam em userspace; o KEXT seria só o "dedo" no PCI.
  `KEXT_TO_TINYGRAD_TRANSPORT = PLAUSIBLE` (arquitetura direta; trabalho real fica no
  daemon + validação GSP — não subestimar).

## Panics e requisitos

Riscos: bug no `Start()` (null-deref em provider, `OSDynamicCast` sem checar),
probe score disputando categoria errada, `Open()` acidental. Mitigação: Start() só
`copyProperty`+`IOLog`, sem Open/map/interrupt/DMA; categoria própria; teste com
`-v keepsyms debug=0x100` + foto de panic + rollback por `Kernel→Add Enabled=false`
(ou remover entrada) via qualquer OS que leia a EFI MSDOS. Requisito: OpenCore com
`Kernel→Add` (já operante) + coleta de logs; SEM AuxKC-approval, SEM conta Apple,
SEM assinatura especial (injeção OC não verifica — mesmo modelo de Lilu/NootedRed).

```text
KEXT_INTERNAL_GA106_FEASIBILITY = MEDIUM (mecanismo fechado em source + prova viva
  de KEXTs neste Tahoe; execução e Start() real ainda por validar)
KEXT_BUILTIN_AUTH_BLOCKER = NO
KEXT_TO_TINYGRAD_TRANSPORT = PLAUSIBLE
```

UM próximo passo (sem executar): especificar o KEXT0 mínimo (Info.plist RTX-only +
`Start()` só-observação + plano de rollback `Enabled=false`) como `TG-KEXT0-SPEC`,
reaproveitando o esqueleto do TG-DK0-SPEC com provider/entitlement trocados.
