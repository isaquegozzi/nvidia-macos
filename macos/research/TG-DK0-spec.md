> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# TG-DK0-SPEC — primeiro DEXT de laboratório GA106 (especificação, SEM implementar)

> Fase de DOCUMENTAÇÃO/ESPECIFICAÇÃO APENAS (2026-09-06, Tahoe saudável NDRV0).
> Nada compilado/instalado/ativado; EFI intocada; sem reboot; sem BAR/MMIO;
> TinyGPU original NÃO carregado. Baseline vigente: `GA106_MACOS_PCI_VISIBLE_STABLE`
> (RTX 10de:2504/030000/built-in PRESENT/ignore-ndrv runtime/SEM framebuffer;
> AMD desktop HEALTHY; MAC-COMPUTE-0 = MEDIUM). Destino final no repo:
> `docs/macos/TG-DK0-spec.md`.

## 1. Objetivo (única pergunta futura)

Um PCIDriverKit DEXT de desenvolvimento consegue realmente fazer match e iniciar
sobre a RTX 3060 interna `10de:2504`, marcada `built-in`, neste macOS?
SEM compute, SEM GSP, SEM mapear BAR, SEM escrita PCI config, SEM DMA.
Nome do milestone futuro: `MAC_COMPUTE_TRANSPORT_ATTACH_0`.

## 2. Signing vs built-in (auditoria 2026-09-06, sem "bypass" genérico)

Fontes Apple lidas hoje (texto integral, não cache de fase anterior):
`transport.pci` entitlement page + `IOPCIPrimaryMatch` page + `requesting-entitlements`
+ `debugging-and-testing-system-extensions` (todas `developer.apple.com`, 2026).

- A. Apple Development signed: fluxo com conta paga + team + provisioning + validação
  entitlements-vs-profile na ativação (aborta se divergir). Documentado.
- B. Development Only PCI entitlement (wildcard `0xFFFFFFFF&0x00000000`, guia DTS Kevin):
  captura prévia do gate TGM0 (`driver-architecture-gate.md` §1.4, LIKELY, fórum DTS —
  fonte Apple-não-doc). Fala de ALCANCE de hardware ("match any hardware" p/ prototipar),
  NADA sobre isenção de `built-in`. NÃO confundir alcance com autorização.
- C. Sign to Run Locally: modo de assinatura local Xcode; nenhum doc afirma que ele
  concede `...builtin` ou altera `matchPropertyTable`.
- D. developer mode (`systemextensionsctl developer on`): doc cobre SOMENTE bypass de
  checagem de LOCALIZAÇÃO do app/dext. Nada sobre entitlement/built-in.
- E. SIP/notarização off: doc cobre bypass de notarização/assinatura p/ debug. Nada
  sobre PCI entitlement ou `built-in`.
- Código de enforcement auditado (`IOPCIFamily/IOPCIDevice.cpp:897 @main`,
  último toque 2025-10-04 `4822b27a`): `getProperty("built-in") != NULL &&
  !builtInEntitled → isMatch=false`. Presença pura; sem carve-out dev-mode visível.

```text
LEGIT_DEV_SIGNING_AVAILABLE = NO (ambiente §3: sem Xcode/identidade/team hoje)
BUILTIN_BIND_WITH_DEV_ENTITLEMENT = UNKNOWN (nenhum doc prova bind built-in
  com dext development-signed; manter UNKNOWN — é EXATAMENTE o que TG-DK0 testará)
```

Nenhum patch de IOPCIFamily, nenhum entitlement privado proposto. Em nenhum cenário.

## 3. Ambiente real (read-only, 2026-09-06, sem expor PII)

```text
XCODE_PRESENT = NO (só CLT; xcodebuild exige Xcode; /Applications/Xcode*.app ausente)
VERSION = n/a
DRIVERKIT_SDK_PRESENT = NO (xcrun: SDK "driverkit" cannot be located)
PCIDRIVERKIT_SDK_PRESENT = NO (nenhum header PCIDriverKit nos SDKs CLT)
APPLE_DEVELOPMENT_IDENTITY = ABSENT (0 valid identities; valores redigidos)
DEVELOPMENT_TEAM = absent (sem Xcode/Accounts, sem provisioning profiles)
DRIVERKIT_DEV_CAPABILITY = unavailable
```

Blocker de execução (não de especificação): sem Xcode + conta paga + DriverKit SDK,
o fork não pode sequer ser compilado/assinado. Personal Team (gratuito), se um dia
houver, NÃO basta presumir: capability DriverKit/PCI exige programa pago + pedido à
Apple — registrar como blocker até prova em contrário.

## 4. Personality RTX-only (estreita; TinyGPU ampla é PROIBIDA aqui)

TinyGPU shipped: `IOPCIClassMatch = 0x03000000` → casa RTX + AMD + qualquer display.
INACEITÁVEL (risco display comprovado RISK + built-in bloqueia ambas de qualquer forma).

Fork lab:

```xml
<key>IOProviderClass</key><string>IOPCIDevice</string>
<key>IOPCIPrimaryMatch</key><string>0x250410de</string>
```

Formato/endian PROVADO pela doc Apple (tabela `IOPCIMatch`: `0x00261011` = vendor
`0x1011` + device `0x0026` → layout `0xDDDDVVVV`, device-alto/vendor-baixo; máscara
opcional `&`). Logo `0x250410de` = device `0x2504` + vendor `0x10DE` (valor exato, sem
máscara = igualdade total). Corrobora o shipped (`0x000010de&0x0000FFFF`, mesmo layout).
SEM `IOPCIClassMatch`, SEM `IOPCIMatch/SecondaryMatch`, SEM `IOPCITunnelCompatible`
(não declarar capacidade que não testaremos), `IOProbeScore` default.

```text
MATCH RTX 10de:2504 (0x250410de == 0x250410de) = YES
MATCH AMD 1002:1638 (0x16381002 != 0x250410de) = NO (prova aritmética, sem HW)
MATCH HDAU 10de:228e (0x228e10de != 0x250410de) = NO
```

Não depende só de vendor 10de: o HDAU (mesmo vendor) é excluído pelo device.

## 5. Entitlement amplo + personality estreita (distinção explícita)

- Development entitlement: PODE ser amplo (ex.: wildcard DTS `0xFFFFFFFF&0x00000000`
  ou vendor `0x000010de&0x0000FFFF`) — autoriza o TRANSPORTE.
- Personality: DEVE ser estreita (`0x250410de`) — decide em quais PROVIDERS o driver
  tenta casar. Doc Apple (`transport.pci` page, Note): "You also use the keys defined
  by this entitlement in your app's Info.plist" — mesmas chaves, papéis distintos.
- DTS (captura gate §1.4, LIKELY): config PCI de desenvolvimento pode abranger HW amplo
  enquanto a personality determina os providers reais. Mantido como LIKELY, não prova.

## 6. Host app mínimo (obrigatório — doc Apple exige app p/ ativação)

`GA106LabHost.app`: app mínimo (CLI com runloop ou .app sem janela) cuja ÚNICA função
futura é `OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier:)`
para `GA106LabDriver.dext` (+ `deactivationRequest` p/ rollback). Entitlement do app:
`com.apple.developer.system-extension.install`. SEM UI elaborada, SEM socket server,
SEM tinygrad/compute, SEM user client neste milestone (Start() prova attach sozinho
via filho no registry + os_log; user client seria superfície extra sem propósito).

## 7. DEXT mínimo (`GA106LabDriver.dext`)

Classe `GA106LabDriver : IOUserService` (PCIDriverKit), Start() futuro faz SOMENTE:
confirmar provider (`OSDynamicCast IOPCIDevice`), ler propriedades PASSIVAS do registry
(`copyProperty`: vendor-id/device-id/class-code/built-in/BDF via `pcidebug`/acpi-path,
registryEntryID), `os_log` one-shot, retornar sucesso e permanecer vivo. Stop(): só log.
Leituras de registry são dados já publicados — não tocam hardware.

## 8. Efeitos implícitos (auditoria; Astra: Start() do TinyGPU escreve)

- TinyGPU `Start()` (habilita memory space + bus mastering, MapBar, Cfg, Reset, DMA,
  GSP): tudo PROIBIDO aqui — nenhum reuse de Start().
- `IOPCIDevice::Open()`: acesso exclusivo; no `Close`/crash o stack DESABILITA
  bus-mastering e memory-space; endpoint deve RE-HABILITAR MSE/BME (gate §1.3, WWDC).
  Logo Open()/Close() MUTAM estado PCI → NÃO chamar em TG-DK0 (sem Open não há
  `MemoryRead/ConfigRead`/map — aceito: identificação vem do registry).
- Sem Open/map/DMA/config-write/interrupt: `HARDWARE_MUTATION_EXPECTED = NO`.
  Ressalva honesta: attach/matching é mediado pelo SO; garantia = ausência, no nosso
  código, de qualquer chamada mutadora + verificação pós-teste (desktop AMD ok,
  `ioreg` command/status RTX inalterado vs baseline, sem `removing child` novo).

## 9. NDRV0 intocado

Teste futuro mantém `AAPL,iokit-ignore-ndrv = PRESENT` (é o baseline estável).
Sem voltar ao BARE. Sem CLASS0.

## 10–11. Critérios MAC_COMPUTE_TRANSPORT_ATTACH_0 (futuro)

Sucesso = boot YES + AMD HEALTHY + RTX PCI YES + ignore-ndrv YES + SEM framebuffer +
matched YES + Start YES + provider exatamente 10de:2504 + AMD NO + HDAU NO +
BAR NO + config-write NO + DMA NO + GSP NO.
Falhas distintas: DEXT_ACTIVATION_FAILED (sistema recusou: assinatura/entitlement) /
DEXT_ENTITLEMENT_FAILED / BUILTIN_AUTH_FAILED (casa mas `isMatch=false` p/ built-in) /
PERSONALITY_NO_MATCH / START_NOT_REACHED / WRONG_PROVIDER_MATCHED / MACOS_REGRESSION.
Nenhum "DriverKit não funciona" genérico.

## 12. Rollback futuro (sem tocar EFI; NDRV0 = recuperação)

Desativar SOMENTE o experimento: no host (`deactivationRequest`) ou
`systemextensionsctl uninstall <TEAMID> org.exemplo.ga106lab.driver`; confirmar
`systemextensionsctl list` vazio + filho sumido no `ioreg`; `developer off` + SIP on
se alterados. EFI NDRV0 intacta o tempo todo = fallback.

## 13. Mapa TinyGPU

REUSED_LATER: arquitetura dext DriverKit (provider/matching/lifecycle), modelo provider
IOPCIDevice, abordagem BAR mapping (memoryIndex), padrão sysmem/DMA (PrepareForDMA),
desenho do transporte socket/RPC (fio RemoteCmd), sequência bring-up NVDev (bem depois).
DISABLED_IN_TG-DK0: todo o Start() TinyGPU (MSE/BME, MapBar, Cfg R/W, Reset, PrepareDMA,
MMIO, MAP_SYSMEM_FD, server.c, app host, caminho APL, GSP/firmware), user client,
qualquer entitlement além do transporte-dev.

## Arquivos da próxima fase (NÃO criar agora)

`macos/GA106Lab/{Info.plist,GA106LabDriver.cpp,GA106LabDriver.entitlements}` +
`macos/GA106LabHost/{main,Info.plist,GA106LabHost.entitlements}` + projeto Xcode
(requer §3 resolvido) + `docs/macos/TG-DK0-runbook.md` + `docs/macos/TG-DK0-result.md`.

```text
TG_DK0_SPEC_READY = YES (especificação fechada e auditada; execução bloqueada em §3)
```

Próximo passo (UM, sem executar): resolver o blocker §3 (Xcode + conta dev paga +
DriverKit SDK/capability) e SÓ então instanciar os arquivos acima — nenhum código
antes de ambiente legítimo existir.
