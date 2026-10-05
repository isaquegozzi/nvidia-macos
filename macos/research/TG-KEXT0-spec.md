> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# TG-KEXT0-SPEC — KEXT mínimo GA106 (ESPECIFICAÇÃO, sem código)

> SOMENTE ESPECIFICAÇÃO (2026-09-06, NDRV0 saudável). NÃO criar código/compilar/
> instalar; EFI intocada; sem reboot/MMIO/config-writes/DMA/GSP. Baseline:
> `GA106_MACOS_PCI_VISIBLE_STABLE`. Pesquisa TG-KEXT0: FEASIBILITY MEDIUM,
> BUILTIN_AUTH NO, TRANSPORT PLAUSIBLE. Destino final no repo:
> `docs/macos/TG-KEXT0-SPEC.md` (+ RUNBOOK futuro, não agora).

## 1. Objetivo — MAC_COMPUTE_KEXT_ATTACH_0

Pergunta única: um IOKit KEXT mínimo casa exclusivamente com a RTX `10de:2504`,
executa `Start()` e permanece carregado sem alterar a GPU nem quebrar o AMD?
Sucesso = loaded + provider exatamente RTX + Start + inspeção registry read-only +
macOS saudável. SEM compute.

## 2. Arquitetura mínima — GA106Lab : IOService

```xml
<key>IOProviderClass</key><string>IOPCIDevice</string>
<key>IOPCIPrimaryMatch</key><string>0x250410de</string>
```

SEM `IOPCIClassMatch`, SEM vendor-only, SEM wildcard.
`0x250410de` = device `0x2504` + vendor `0x10DE` (formato `0xDDDDVVVV`, doc Apple).
`RTX→MATCH / AMD(0x16381002)→NO / HDAU(0x228e10de)→NO` — aritmética, sem HW.

## 3. Bundle identifier

`org.nvidia-macos.GA106Lab` — reverso-DNS exclusivo; sem colisão com
TinyGPU/NVIDIA/Lilu/NootedRed. Classe: `GA106Lab : IOService`, categoria própria
`IOMatchCategory = GA106Lab` (arbitragem isolada; ATTACH_ONLY, sem ownership).

## 4. Dependências (precedente vivo NootedRed neste Tahoe)

```xml
<key>OSBundleLibraries</key><dict>
  <key>com.apple.iokit.IOPCIFamily</key><string>1.0.0b1</string>
  <key>com.apple.kpi.iokit</key><string>10.0.0</string>
  <key>com.apple.kpi.libkern</key><string>10.0.0</string>
  <key>com.apple.kpi.mach</key><string>10.0.0</string>
</dict>
```

Valores copiados do NootedRed 0.8.10 (carrega neste Tahoe; IOPCIFamily viva = 2.9,
mínimo `1.0.0b1` = semântica de versão-mínima). SEM Lilu/bsd/dsep (desnecessários).
`kpi.unsupported` SOMENTE se o linker exigir (decidir no build, documentar).
SEM `OSBundleRequired` (não forçar inclusão no boot; carrega no match).
`CFBundleVersion 1.0.0`. Nada de valores legacy copiados cegamente — cada entrada
acima existe porque um símbolo usado a exige (`IOPCIDevice`, `IOService`,
`copyProperty`/`IOLog`, `OSDynamicCast`).

## 5–7. Start() mínimo / sem probe() / Stop()

`Start()` futuro: `super::start` → `OSDynamicCast<IOPCIDevice>` do provider →
`copyProperty` SÓ de `vendor-id/device-id/class-code/built-in` + path/entryID →
`IOLog("[GA106Lab] ...")` → `return true`. SEM `probe()` override (matching
Info.plist basta; menos código = menos risco). `Stop()`: só loga + `super::stop`.

## 8. Logging + 9. Mutação

Prefixo `[GA106Lab]`; eventos `MATCH/START_ENTER/PROVIDER_CONFIRMED/START_SUCCESS/STOP`
+ `10de:2504`; sem dumps gigantes.
PROIBIDAS: `open`, `mapDeviceMemoryWithRegister`, `configRead*` (leitura mas É acesso
PCI — critério §15 exige `PCI_CONFIG_ACCESS = NO`), `configWrite*`,
`setMemoryEnable/setBusMasterEnable`, `reset`, DMA, BAR, GSP/firmware.
(`configRead*` documentada aqui como aparentemente-read-only MAS excluída: qualquer
acesso a config space viola o sucesso estrito e pode ter side effects em bridges.)
`HARDWARE_MUTATION_EXPECTED = NO` (só `copyProperty` de dados já publicados + log;
verificação pós-teste: desktop ok + command/status RTX inalterado + sem removing novo).

## 10. NDRV0 mantido

Teste futuro SOMENTE sobre NDRV0 (`AAPL,iokit-ignore-ndrv` PRESENT, sem framebuffer).
KEXT não remove property, não registra IOFramebuffer, não publica display, fora de
IOGraphics. (Gate NDRV e matching KEXT são ortogonais — pesquisa TG-KEXT0.)

## 11. Attach vs controle

ATTACH_ONLY: anexar `GA106Lab` ao IOPCIDevice em categoria própria, sem `Open()`,
sem exclusividade, coexistindo com a pilha (que hoje deixa a RTX sem driver).
EXCLUSIVE_CONTROL (Open/map/DMA) = fases futuras (KEXT1+), fora daqui.

## 12–14. OpenCore / rollback / recovery (futuros, sem editar agora)

```xml
<dict>
  <key>Arch</key><string>Any</string>
  <key>BundlePath</key><string>GA106Lab.kext</string>
  <key>Enabled</key><true/>
  <key>ExecutablePath</key><string>Contents/MacOS/GA106Lab</string>
  <key>PlistPath</key><string>Contents/Info.plist</string>
</dict>
```

Rollback: `Enabled=false` (ou remover entry) via qualquer OS que leia a EFI MSDOS;
base BARE-NDRV0 intacta; `ignore-ndrv` nunca removido. Recovery: boot ok / freeze /
panic / reboot-early → outro SO → `Enabled=false` → NDRV0 de volta; sem depender do
macOS alvo (mesmo runbook BARE, §4 do plano H0).

## 15–16. Critérios (futuro TG-KEXT0)

Sucesso: boot SUCCESS + AMD HEALTHY + RTX PRESENT + ignore-ndrv PRESENT + SEM
framebuffer + loaded YES + Start YES + provider 10de:2504 + AMD/HDAU NO MATCH +
MMIO/PCI_CONFIG/DMA/GSP NO.
Falhas: KEXT_NOT_LOADED / PERSONALITY_NO_MATCH / START_NOT_REACHED / WRONG_PROVIDER /
START_FAILED / KERNEL_PANIC / DESKTOP_REGRESSION / NDRV_REGRESSION. Sem "KEXT falhou"
genérico.

## 17. Depois do KEXT0 (só mapa)

KEXT0 attach/registry → KEXT1 `IOUserClient` mínimo → daemon userspace →
RPC TinyGPU-compatible → NVDev tinygrad → BAR/GSP/compute. Kernel = transporte mínimo;
lógica TinyGPU/tinygrad = userspace. (TinyGPU inteiro no kernel: proibido pelo mapa.)

## 18. Arquivos previstos (NÃO criar — só documentação externa nesta fase)

`macos/GA106Lab/{Info.plist,GA106Lab.hpp,GA106Lab.cpp}`,
`docs/macos/TG-KEXT0-SPEC.md` (este), `docs/macos/TG-KEXT0-RUNBOOK.md` (futuro).

```text
TG_KEXT0_SPEC_READY = YES
```

UM próximo passo (sem executar): abrir a próxima sessão JÁ com Xcode instalado
(pendente do download na App Store) e instanciar `macos/GA106Lab/` exatamente por
esta spec — nenhum código antes disso.
