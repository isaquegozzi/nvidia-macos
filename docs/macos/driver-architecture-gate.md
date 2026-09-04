# Gate de arquitetura de driver macOS — PCIDriverKit (A) vs IOKit KEXT (B)

> Status: Fase 0 — somente pesquisa documental, sem código de driver.
> Escopo: Hackintosh Intel, RTX 3060 `10de:2504` (GA106) em slot PCIe interno,
> objetivo laboratório PCI → GA106 → futuro gráfico. macOS alvo: Tahoe (26.x).
> Data da pesquisa: 2026-09-04. Nenhum teste executado na bancada Tahoe nesta fase.
> Escala de confiança por afirmação: `CONFIRMED` / `LIKELY` / `UNCERTAIN` / `UNKNOWN`.

## 0. Método e regra anti-cegueira

Não se aceita cegamente que PCIDriverKit é o caminho. Cada afirmação abaixo
carrega confiança explícita e fonte Apple atual quando existir
(`developer.apple.com`, `support.apple.com`, vídeos WWDC, fóruns DTS).
Código-fonte aberto Apple (`apple-oss-distributions`) é tratado como
autoritativo para enforcement, mas distinto de documentação.
Relatos comunitários (StackOverflow, GitHub Lilu/WEG, UTM, mac-amdgpu)
são evidência de campo, nunca prova oficial — marcados `LIKELY` no máximo.

---

## 1. Caminho A — PCIDriverKit (DriverKit dext, user space)

### 1.1 Framework e disponibilidade

- `PCIDriverKit` existe para desenvolver drivers de PCI/PCIe gerenciando
  recursos customizados; ao carregar, o sistema passa um objeto `IOPCIDevice`
  como provider ao driver [CONFIRMED —
  `developer.apple.com/documentation/pcidriverkit` (.md verificado 2026-09-04)].
- `PCIDriverKit` está disponível no macOS para Intel e Apple Silicon, e no
  iPadOS para chips M [CONFIRMED — mesma página, nota de disponibilidade].
- `IOPCIDevice` é objeto provider gerenciado pelo sistema; o driver não o
  instancia diretamente, usa seus métodos para config space e MMIO
  [CONFIRMED — `developer.apple.com/documentation/pcidriverkit/iopcidevice`].
- Guia Apple de PCIe é enquadrado como "Creating Custom PCIe Drivers for
  **Thunderbolt** Devices"; o texto diz que em macOS 11+ drivers customizados
  devem ser DriverKit extensions usando `PCIDriverKit` [CONFIRMED —
  `.../creating-custom-pcie-drivers-for-thunderbolt-devices`].
  O enquadramento Thunderbolt-first é relevante para §4 (dispositivo interno).
- WWDC20 "Modernize PCI and SCSI drivers with DriverKit" declara drivers PCI
  kernel **deprecated quando uma System Extension pode fazer a mesma função**
  e incentiva adoção imediata de DriverKit [CONFIRMED —
  `developer.apple.com/videos/play/wwdc2020/10210/`].
  Usuários podem adicionar PCIe via Thunderbolt ou slots (ex. Mac Pro citado)
  [CONFIRMED — mesmo vídeo].

### 1.2 Matching PCI (Info.plist + entitlement)

- Matching usa os mesmos critérios `IOPCIFamily` no `Info.plist`
  (`IOProviderClass=IOPCIDevice`, `IOPCIMatch` / `IOPCIPrimaryMatch` /
  `IOPCISecondaryMatch` / `IOPCIClassMatch`, máscara com `&`), mais chaves
  `IOPCITunnelCompatible` (Thunderbolt), `IOPCIPauseCompatible`,
  `IOProbeScore` [CONFIRMED — página Thunderbolt + página do entitlement
  `.../entitlements/com.apple.developer.driverkit.transport.pci`].
- A página do entitlement documenta sintaxe de máscara e exemplos
  (`0x00261011`, `0x00789004&0x00ffffff`, `IOPCIClassMatch 0x02000000&0xffff0000`)
  e nota que as mesmas chaves vão no `Info.plist` [CONFIRMED — mesma página].
- Para vencer driver genérico embutido (AHCI/XHCI/NVMe) pode ser preciso
  `IOProbeScore` explícito, pois PCI trata class-match e id-match com mesma
  prioridade por padrão (diferente do USB) [LIKELY — StackOverflow 76183589
  + `IOPCIBridge.cpp` citado; sem página Apple equivalente; não testado aqui].
- Dispositivo PCI bridge exige entitlement extra
  (`...transport.pci.bridge` observado em `IOServiceDEXTEntitlements` de
  bridges em ioreg) [LIKELY — ioreg excerpts em StackOverflow 76207058;
  sem documentação pública dedicada → não assumir para endpoint GPU].

### 1.3 Config space e BAR/MMIO (IOPCIDevice)

- Leitura/escrita de config: `ConfigurationRead8/16/32`,
  `ConfigurationWrite8/16/32`, offsets `kIOPCIConfigurationOffset*`,
  bits de command/status, `FindPCICapability` (inclui extended caps via ID
  negado) [CONFIRMED — página `IOPCIDevice` + subpáginas].
- Leitura/escrita MMIO: `MemoryRead8/16/32/64`, `MemoryWrite8/16/32/64` por
  `memoryIndex` + offset; para BARs 32-bit `memoryIndex == BAR index`; para
  BARs 64-bit podem divergir (memoryIndex é índice por entrada de memória,
  base zero) [CONFIRMED — WWDC20 10210 + página `IOPCIDevice`].
- Obrigatório `Open` antes de qualquer acesso (acesso exclusivo); `Close` ao
  parar; o stack desabilita bus-mastering e memory-space no `Close`/crash;
  o **endpoint driver é responsável por re-habilitar Memory Space Enable e
  Bus Master Enable a cada configuração** [CONFIRMED — WWDC20 + nota na
  página `IOPCIDevice`].
- I/O space legado é desencorajado; Macs Apple Silicon não o suportam —
  usar MMIO [CONFIRMED — guia Thunderbolt § "Read and Write From the
  Configuration and MMIO Spaces"]. Em Intel Tahoe I/O space ainda existe no
  HW, mas irrelevante para GA106 (lab é MMIO) [LIKELY — inferência; sem
  impacto na decisão].
- MSI é o esperado; legado é possível mas o próprio guia diz que a melhor
  alternativa para legado é KEXT [CONFIRMED — mesmo guia § MSI].
  Para lab somente-leitura (config + BAR read) interrupções são
  desnecessárias [CONFIRMED por construção do escopo].
- DMA usa `IODMACommand::PrepareForDMA` que retorna IOVA atribuída pelo
  sistema; não há como requisitar endereço específico — limitação
  documentada na prática para passthrough (guest precisa de companion
  device para remapear) [LIKELY — série RFC `vfio-apple` 2026-04
  (scottjg/qemu-vfio-apple); sem página Apple Contradizendo].
  Para lab read-only sem DMA, sem impacto [CONFIRMED por escopo].

### 1.4 Instalação via SystemExtensions, signing, entitlements

- No macOS, instalar/atualizar via `SystemExtensions` framework; dext vive em
  `Contents/Library/SystemExtensions` do app host; o app faz activation
  request; admin aprova [CONFIRMED — overview `PCIDriverKit` + WWDC19 702].
- Todo dext precisa de `com.apple.developer.driverkit` + entitlement de
  transporte (`...transport.pci` com array de dicionários de match) e,
  conforme família, entitlement de família; o sistema valida entitlements do
  arquivo contra o provisioning profile na ativação e aborta se divergir
  [CONFIRMED — `requesting-entitlements-for-driverkit-development`].
- Entitlements são amarrados ao time de desenvolvimento; pede-se o conjunto
  completo para um produto via `developer.apple.com/system-extensions/`;
  processo pode levar meses (relato de campo) [CONFIRMED para o fluxo;
  LIKELY para prazo — StackOverflow 68242561; não presumir prazo].
- Desenvolvimento local sem entitlement de produção: fluxo moderno usa
  variantes **development** dos entitlements, disponíveis para qualquer conta
  paga sem aprovação especial, com wildcard para HW arbitrário; para PCI a
  configuração development-only documentada por DTS é
  `IOPCIPrimaryMatch = 0xFFFFFFFF&0x00000000` [LIKELY — "Kevin's Guide to
  DEXT Signing", DTS engineer Kevin Elliott, tópico Drivers nos fóruns Apple;
  não é página de docs, mas é fonte Apple (DTS). Requer Xcode 16+ e signing
  automático, exceto distribution PCI/USB].
- Distribution PCI/USB **não** funciona no signing automático do Xcode
  (exceção explícita no mesmo guia DTS) [LIKELY — mesma fonte].
- Debug local documentado: desabilitar SIP (bypassa notarização), anexar
  `lldb` após o sistema lançar o dext, `systemextensionsctl developer on`,
  `dk=0x8001` (bit `0x8000` desliga checagem de entitlements DriverKit;
  sem `0x1` DriverKit fica desabilitado), `amfi_get_out_of_my_way=1`
  (desliga AMFI) [CONFIRMED para SIP+lldb —
  `.../debugging-and-testing-system-extensions`; LIKELY para `dk`/`amfi`
  flags — StackOverflow 68242561 citando comportamento; sem página Apple
  equivalente; usar só em lab].
- Gotchas de assinatura observados em campo e relevantes ao planejar teste:
  dext deve ser assinado com `-o library,runtime`, sem `get-task-allow` nem
  `application-identifier` embutidos, host sem hardened runtime, nome do
  bundle igual ao `CFBundleIdentifier` (bug FB15590713) [LIKELY —
  `lemonade-sdk/mac-amdgpu` README; não oficial, mas específico para dext
  PCI de GPU em Tahoe e consistente com guia DTS].

### 1.5 Precedentes de dext PCI para GPU (campo, não Apple)

- `mac-amdgpu` (lemonade-sdk): driver PCI DriverKit para AMD RDNA4 via
  **Thunderbolt 5 em Apple Silicon, Tahoe 26.2+**, bypassando Metal;
  usa entitlement development wildcard ou específico, SIP desabilitado,
  re-sign mínimo [LIKELY — README do projeto; prova que dext PCI de GPU
  carrega em Tahoe, mas em cenário externo/Apple Silicon, não interno/
  Hackintosh/NVIDIA].
- `qemu-vfio-apple` (scottjg) + thread UTM #4579: passthrough PCI via dext;
  sem entitlements da Apple não há binário distribuível; produção exige
  Apple assinar (wildcard production "atualmente não possível" no relato);
  tinygrad relata ter obtido entitlement PCI para GPUs que não possui, mas
  para **compute-only, sem display** [LIKELY — issues/PRs; reforça §5].
- Nenhum precedente encontrado de dext PCI assumindo GPU **interna em slot
  x86 Hackintosh com display** [UNKNOWN — ausência de evidência, não
  evidência de impossibilidade].

---

## 2. Caminho B — IOKit KEXT (kernel, IOPCIDevice kernel)

### 2.1 Status no Tahoe/Intel: deprecated mas funcional

- KEXTs foram deprecated; a partir de Big Sur releases não carregam por
  padrão KEXTs que usem KPIs deprecated [CONFIRMED —
  `developer.apple.com/support/kernel-extensions`].
- Em macOS 11+, KEXTs de terceiros não carregam sob demanda; são mescladas
  na Auxiliary Kernel Collection (AuxKC) no boot, exigindo aprovação do
  usuário + reboot; em Apple Silicon exige ainda Reduced Security via
  Recovery [CONFIRMED — `support.apple.com/guide/security/securely-extending-the-kernel-sec8e454101b/web`
  + `support.apple.com/guide/deployment/system-extensions-in-macos-depa5fb8376f/web`].
- Em **Intel** não há requisito de Reduced Security; o fluxo é aprovação em
  Privacy & Security + rebuild da AuxKC + reboot; com SIP desligado a
  assinatura do KEXT não é enforced [CONFIRMED — mesma página deployment
  ("If SIP is turned off, the kext signature isn't enforced")].
- Tahoe é o **último major para Intel Macs** (MacPro7,1, iMac 2020, MBP
  16" 2019, MBP 13" 2020 4-portas citados) com +3 anos de security updates
  [CONFIRMED — anúncio Apple WWDC25 Platforms State of the Union via
  The Verge 2025-06-09 + Wikipedia Tahoe com lista de modelos; sem página
  Apple persistente equivalente, mas anúncio oficial reportado].
- Alerta de "legacy system extension" existe e Apple recomenda migrar, mas
  KEXTs seguem carregáveis com aprovação até Tahoe [CONFIRMED —
  `support.apple.com/en-us/120363`].
- Ferramentas modernas: `kmutil` (load/unload/staging/diagnóstico),
  `kernelmanagerd`, `kextstat` legado; `kextload` direto que não esteja na
  AuxKC dispara staging + pedido de rebuild + reboot [CONFIRMED — man `kmutil`
  + páginas support acima].

### 2.2 Matching, config read, memory mapping (kernel IOPCIDevice)

- Modelo clássico: driver casa com nub `IOPCIDevice`/`IOAGPDevice`,
  configura via config space, mapeia BARs ou usa I/O space [CONFIRMED —
  Apple "Writing PCI Drivers" (leopard-adc archive) + referência
  `IOPCIDevice` kernel (`mapDeviceMemoryWithRegister`, `setBusMasterEnable`,
  `setConfigBits`, `configRead32`, `ioRead*`)]. Documento é antigo mas o
  mecanismo kernel segue referenciado em guias Thunderbolt atuais
  [LIKELY — sem doc substituto; código XNU/IOPCIFamily segue aberto].
- Idioma padrão: `mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0)` →
  `IOMemoryMap` → `getVirtualAddress()` → ponteiro direto para registradores
  (memória uncached, com barreiras quando ordem importa); `setMemoryEnable`,
  `setIOEnable`, `setBusMasterEnable`, `setConfigBits` para command register
  [CONFIRMED — Writing PCI Drivers §§ Memory-Mapped Device Access].
- Para lab somente-leitura, o KEXT obtém o mesmo que o dext (config + BAR),
  mas com ponteiro direto em vez de API mediada por índice [CONFIRMED por
  construção das duas APIs].

### 2.3 Assinatura, SIP, loading, debugging (B)

- Com SIP ligado, assinatura de cada KEXT é verificada antes da AuxKC;
  com SIP desligado, não é enforced [CONFIRMED — deployment page].
- No Hackintosh lab, o vetor real não é `/Library/Extensions` + AuxKC e
  sim **injeção pelo bootloader (OpenCore)**: KEXTs em `EFI/OC/Kexts`
  + `config.plist`, carregadas antes/without AuxKC approval flow
  [LIKELY — modelo OpenCore documentado por Dortania; comprovado em campo
  §2.4; não é fluxo Apple suportado → classificar como campo, não CONFIRMED].
- Debugging KEXT é mais custoso: panic = reboot, normalmente exige
  segunda máquina / kdp / serial; ciclo build→AuxKC rebuild→reboot é lento;
  WWDC19 contrasta explicitamente (dext: debugger sem parar o kernel;
  KEXT: para a máquina) [CONFIRMED — WWDC19 702].
- Em compensação, no Hackintosh o lab já opera nesse modo (boot-args
  `-v`, `-liludbg`, `liludump`, `kextstat | grep`) e tolera reboots como
  parte do fluxo de bancada [LIKELY — prática Lilu/WEG; ver §2.4].

### 2.4 Como Lilu/WhateverGreen operam no Tahoe (evidência de campo)

- Lilu é KEXT open-source de patch genérico (kext/process/framework patcher
  + plugin API) [LIKELY — README acidanthera/Lilu; versão 1.7.1 citada para
  Tahoe em fóruns].
- WhateverGreen é plugin Lilu para patch de GPUs (inclui NVIDIA Web-driver
  shims até 10.13.6, `agdpmod`, `device-id` spoof via wrap de PCI config
  read `t_PCIConfigRead16/32`) [LIKELY — README + DeepWiki WEG; mecanismo
  de spoof PCI é diretamente análogo ao lab PCI pretendido].
- Tahoe impôs correções reais, **o que prova que KEXTs ainda carregam e
  casam em Tahoe**:
  - Lilu PR #102 corrige address-slot overrun que paniçava `AMDSupport`
    em Tahoe (segmento `__TEXT` com ~16 bytes livres) — testado em Tahoe
    26.2 e Sonoma [LIKELY — PR + relatos; demonstra KEXT patch ativo no
    kernel Tahoe].
  - WEG PR #124/#126: panic Invalid Opcode (6) desde Tahoe DB1 em
    recoveryOS/update phases ao hookar `AMDSupport`; workaround ordenando
    após `IOGraphicsFamily` [LIKELY — PRs; demonstra dependência de ordem
    de famílias gráficas ainda válida].
  - Relatos de campo: WEG 1.7.1 / fork 1.7.1d7 + `agdpmod=pikera`,
    `-wegnoigpu`, `kextstat` confirmando `WhateverGreen` carregado com
    Metal 2 em RX 580 no Tahoe 26.1–26.2; instalação exige pasta mínima
    (`Lilu`, `VirtualSMC`) e `MaxKernel` para OCLP [LIKELY — fóruns
    olarila/amd-osx/osxlatitude 2025–2026; heterogêneo, mas convergente].
- Limitações demonstradas: WEG sem load = sem HIDPI no login / purple bar;
  `AppleALC` quebrou por remoção de `AppleHDA` no Tahoe (requer reinjeção);
  OCLP ainda sem suporte a Tahoe na janela observada [LIKELY — mesmos
  fóruns; mostra que Tahoe removeu/alterou KEXTs do sistema, mas não o
  mecanismo de KEXT de terceiros].
- Nenhuma evidência de que Apple tenha **removido** `IOService`/
  `IOPCIDevice` kernel ou matching PCI em Tahoe Intel [UNKNOWN — ausência
  de anúncio; aguardar KDK/notas Tahoe para confirmação final].

---

## 3. Restrição de entitlement PCI development vs dispositivos "Built In"

### 3.1 O que a fonte Apple atual diz — classificação

**Classificação da restrição: CONFIRMED** (enforcement em código Apple
open-source + confirmação DTS em fórum Apple; **não** em página
`developer.apple.com` principal — essa ausência é parte do risco).

- Código Apple open-source `IOPCIDevice::matchPropertyTable` (repositório
  `apple-oss-distributions/IOPCIFamily`, arquivo `IOPCIDevice.cpp`)
  implementa segunda etapa de validação após o match do `Info.plist`:
  percorre o array do entitlement `kIOPCITransportDextEntitlement`; se o
  device tem propriedade `built-in` e o array não contém a string
  `kIODriverKitTransportBuiltinEntitlementKey`, força `isMatch = false`
  [CONFIRMED — código citado; trecho com `getProperty("built-in")` +
  `builtInEntitled`]. Valor `true` booleano no lugar do array é marcado
  como "intended for Apple internal use only" no log [CONFIRMED — mesmo
  arquivo].
- Fórum Apple (DTS, thread "DriverKit Access to Built-In MacBook…",
  resposta DTS): `com.apple.developer.driverkit.builtin` é no mínimo
  necessário para casar interface HID interna; **não é entitlement público**;
  comportamento esperado como comum à maioria/todas famílias DriverKit,
  com link explícito para o check PCI acima [CONFIRMED como posição DTS;
  HID no exemplo, PCI por referência — extrapolar para PCI é LIKELY].
- Páginas `developer.apple.com` do entitlement PCI e do guia Thunderbolt
  **não mencionam** `built-in` nem `...builtin` [CONFIRMED por inspeção
  2026-09-04 — ausência verificada nas versões .md].
- Guia DTS "Kevin's Guide" documenta wildcard development
  `0xFFFFFFFF&0x00000000` como "match against any hardware" para
  prototipagem [LIKELY — fórum DTS; **não** afirma isenção de `built-in`].
  Ler "any hardware" como "inclui built-in" seria presunção —
  explicitamente proibida neste gate.

### 3.2 A RTX interna em Hackintosh cairia nela? — UNCERTAIN (verificar via ioreg)

- O gatilho é a presença da propriedade `built-in` no nó `IOPCIDevice` da
  GPU, não o fato de ser "interna" no sentido físico [CONFIRMED — código].
- Em Macs reais, `built-in` vem de ACPI/device-tree para controladores
  on-board; GPUs discretas em slot (ex. MPX no MacPro7,1) tendem a **não**
  ter `built-in`, enquanto Thunderbolt nunca tem; mas a regra exata por
  plataforma não é documentada [LIKELY por modelo; sem doc → não CONFIRMED].
- Em Hackintosh, a presença de `built-in` depende de DSDT/SSDT do firmware
  PC + injeções OpenCore (`DeviceProperties`, SSDTs). Perfis WEG comumente
  injetam `AAPL,slot-name=Internal` e `model`, não necessariamente
  `built-in`; mas nada impede que o firmware ou um SSDT o apresente
  [UNCERTAIN — requer `ioreg -l` no Tahoe alvo].
- Veredito para a RTX `01:00.0` em slot x16: **UNCERTAIN — LIKELY NÃO
  afetada, mas sem garantia**. Hipótese de trabalho: endpoint discreto em
  slot provavelmente não expõe `built-in` → wildcard development casaria;
  se expuser, só `...builtin` (Apple-internal) ou bypass (`dk=0x8001` /
  SIP+AMFI off) casaria — aceitável em lab, inaceitável como produto.
- Ação obrigatória antes de qualquer conclusão de produção: no Tahoe
  Hackintosh, inspecionar `ioreg -l -w0` no nó da `10de:2504` por
  `built-in`, `IOServiceDEXTEntitlements`, `IOPCITunnelled`,
  `AAPL,slot-name`, `pcidebug` [procedimento; sem resultado ainda → UNKNOWN].

### 3.3 Entitlement de produção para vendor 0x10de — bloqueador prático?

- Produção exige provisioning profile Apple contendo o match aprovado;
  Apple amarra ao time e ao escopo pedido; pedir para **vendor que não é o
  seu (0x10DE NVIDIA)** é, no melhor caso, improvável de ser aprovado e, no
  pior, negado por princípio (relato UTM: "Apple has to sign… fill in a form
  with your vendor ID"; wildcard production "currently not possible")
  [LIKELY — UTM #4579 + qemu-vfio-apple README "we have not yet received
  the necessary entitlements… cannot release binaries"].
- Exceção conhecida (tinygrad obteve PCI entitlement para GPUs de terceiros,
  anúncio via X citado na thread UTM) existe mas é para **compute-only via
  Thunderbolt em Apple Silicon, sem display**, não para GPU interna x86 com
  display [LIKELY — relato de segunda mão na thread; sem fonte primária
  verificada aqui → não basear plano nisso].
- Desenvolvimento com wildcard (`0xFFFFFFFF&0x00000000`, conta paga,
  Macs registrados, SIP off / `systemextensionsctl developer on`) é
  suficiente para **lab**, não para distribuição [LIKELY — guia DTS].
- **Não se presume que temos entitlement algum** (regra do projeto).
  Logo: para qualquer horizonte além do lab nesta máquina, (A) tem um
  gatekeeper Apple intransponível no curto prazo; (B) não tem gatekeeper
  (custo é técnico: AuxKC/reboot/panic) [CONFIRMED por construção dos fluxos].

---

## 4. Futuro gráfico (adequação arquitetural apenas, sem reversa pesada)

 Famílias citadas no escopo: `IOGPUFamily` / `IOAcceleratorFamily2` /
 `IOGraphics` / `WindowServer` / Metal.

- Pilha GPU macOS é em camadas: KEXT(s) + `IOUserClient` + plugins user-space
  (Metal/OpenGL) carregados em cada processo consumidor; `WindowServer` é o
  compositor [LIKELY — comentário HackerNews 2019 sobre `library-validation`
  + DeepWiki WEG + artigos Mac Internals 2026 sobre pipeline Metal→
  `IOConnectCallMethod`→firmware→shaders; sem doc Apple único que feche o
  modelo para terceiros].
- `library-validation` no `WindowServer` (e apps Apple) exige que código
  carregado seja Apple ou mesmo Team ID — bloqueou NVIDIA Web Drivers
  (10.12–10.13: janelas transparentes em apps Metal) e exigiu shims WEG
  (`-ngfxlibvalfix`, `IOVARendererID`) [LIKELY — DeepWiki WEG "Web Driver
  Support" + thread HN; demonstra que PCI OK ≠ display OK].
- eGPU graphics em Intel exigia AMD Polaris/Vega/RDNA específicos com
  drivers Apple; NVIDIA nunca listada; em Apple Silicon, graphics via eGPU
  foi removida — precedente compute-only (TinyCorp/mac-amdgpu: "calculator,
  not display") [LIKELY — GadgetHacks 2026-04 + Apple Support eGPU (Intel
  baseline) ; sem doc Apple atualizando para compute-only].
- `IOGPUFamily` segue superfície sensível e ativamente corrigida
  (CVEs 2024-44197, 2025-24257, entitlement `com.apple.developer.gpu-restricted`
  com 31/56 selectors restritos) [LIKELY — gist blacktop + DFSEC 2025-10;
  reforça que plugar GPU nova no acelerador é área de exploit, não de lab].
- Conclusão arquitetural (vale para A e B igualmente): **atingir PCI
  (config + BAR) é ordens de magnitude mais próximo que atingir
  framebuffer acelerado, e este é ordens mais próximo que Metal/WindowServer**.
  Nenhum dos dois entrypoints entrega `IOAccelerator`/`IOGraphics`/Metal de
  graça; ambos exigiriam, além do PCI, GSP firmware, scheduler, VM e driver
  user-space assinado pelo mesmo time — fora do escopo deste gate
  [LIKELY — síntese; honestidade exige declarar: PCI lab ≠ driver gráfico].
- Diferença residual: (A) é marginalmente mais limpo para um futuro
  **compute-only sem display** (userspace, DMA mediado, sem panic); (B) é
  marginalmente mais próximo de **framebuffer patching legado** (modelo WEG:
  spoof PCI + patch `AppleGraphicsDevicePolicy`/framebuffer). Nenhum dos
  dois é "caminho gráfico" — a decisão deve ser por carregabilidade do lab,
  não por promessa gráfica [decisão delegada ao ADR-0001].

---

## 5. Tabela comparativa (com confiança por linha)

| Dimensão | A — PCIDriverKit dext | B — IOKit KEXT | Leitura para este lab |
|---|---|---|---|
| Status oficial | Suportado, incentivado; PCI KEXT deprecated quando dext atende [CONFIRMED] | Deprecated, mas carregável com aprovação+reboot [CONFIRMED] | Empate formal; prática decide |
| Carregabilidade real no Hackintosh Tahoe | Exige conta paga + wildcard dev + Mac registrado + aprovação SystemExtension + SIP off; sem precedente interno/NVIDIA [LIKELY] | Injeção OpenCore + `kextstat` confirmado em Tahoe 26.1–26.2 (Lilu 1.7.1/WEG 1.7.1d7) [LIKELY] | **B vence no lab** |
| PCI interno (`built-in`) | Risco condicional: se `built-in` presente, dev wildcard falha sem entitlement Apple-internal [CONFIRMED mecanismo; UNCERTAIN aplicabilidade] | Sem check de entitlement; `built-in` irrelevante para match [CONFIRMED por ausência de mecanismo] | **B sem risco; A requer ioreg** |
| Produção 0x10DE | Gatekeeper Apple; wildcard production não disponível; pedir por vendor alheio LIKELY negado [LIKELY] | Sem gatekeeper (custo técnico) | **B vence fora do lab** |
| Config space + BAR read | `ConfigurationRead*` + `MemoryRead*` por índice; `Open` exclusivo; MSE/BME re-habilitar [CONFIRMED] | `configRead*` + `mapDeviceMemoryWithRegister` ponteiro direto [CONFIRMED] | Ambos atendem; B mais direto |
| DMA/interrupções | IOVA atribuída; MSI esperado, legado→KEXT [CONFIRMED/LIKELY] | Controle total kernel, legado OK [CONFIRMED doc legado] | Irrelevante p/ lab read-only |
| Debugging | Userspace: lldb sem parar kernel, dtrace, `dk=0x8001` [CONFIRMED/LIKELY] | Kernel: panic=reboot, AuxKC+reboot por iteração [CONFIRMED] | **A vence em ergonomia** |
| Tahoe Intel | Forward-compatible; sem alerta legacy [CONFIRMED] | Último major Intel; alerta legacy mas funcional [CONFIRMED/LIKELY] | B basta para Tahoe-lab |
| Futuro gráfico | Compute-only sem display é o teto demonstrado [LIKELY] | Framebuffer patch legado é o teto demonstrado [LIKELY] | Empate distante |
| Dependência Apple | Conta + aprovação + provisioning [CONFIRMED] | Nenhuma (lab) [LIKELY campo] | **B vence em autonomia** |

---

## 6. Referências (acesso 2026-09-04)

Apple (docs/vídeos/suporte — verificar versões .md quando JS for exigido):

- `https://developer.apple.com/documentation/pcidriverkit.md`
- `https://developer.apple.com/documentation/pcidriverkit/iopcidevice.md`
- `https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.developer.driverkit.transport.pci.md`
- `https://developer.apple.com/documentation/driverkit/requesting-entitlements-for-driverkit-development.md`
- `https://developer.apple.com/documentation/pcidriverkit/creating-custom-pcie-drivers-for-thunderbolt-devices.md`
- `https://developer.apple.com/documentation/driverkit/debugging-and-testing-system-extensions`
- `https://developer.apple.com/videos/play/wwdc2020/10210/` (Modernize PCI and SCSI drivers with DriverKit)
- `https://developer.apple.com/videos/play/wwdc2019/702/` (System Extensions and DriverKit)
- `https://developer.apple.com/support/kernel-extensions` (Deprecated KEXTs)
- `https://developer.apple.com/forums/thread/707850` (PCI transport entitlement)
- Tópico Drivers / "Kevin's Guide to DEXT Signing" (DTS) —
  `https://developer.apple.com/forums/topics/app-and-system-services/app-and-system-services-drivers`
- "DriverKit Access to Built-In MacBook…" (DTS sobre `...builtin`, fórum) —
  `https://origin-devforums.apple.com/forums/thread/818441`
- `https://developer.apple.com/library/archive/documentation/HardwareDrivers/Conceptual/ThunderboltDevGuide/Basics01/Basics01.html`
- `https://support.apple.com/guide/security/securely-extending-the-kernel-sec8e454101b/web` (AuxKC)
- `https://support.apple.com/guide/deployment/system-extensions-in-macos-depa5fb8376f/web` (aprovação KEXT Intel vs AS)
- `https://support.apple.com/en-us/120363` (legacy system extension alert)

Apple open-source (enforcement):

- `https://github.com/apple-oss-distributions/IOPCIFamily/blob/4822b27a36e2de70e231ecf2bf3021384fab6ec2/IOPCIDevice.cpp`
  (`IOPCIDevice::matchPropertyTable`, check `built-in` + `kIODriverKitTransportBuiltinEntitlementKey`)
- `https://developer.apple.com/documentation/kernel/iopcidevice/1811470-mapdevicememorywithregister` (referência kernel)

Campo (não-Apple; LIKELY no máximo):

- StackOverflow 68242561, 76207058, 76183589 (entitlement checks, `dk=0x8001`, probe score)
- acidanthera/Lilu PR #102; WhateverGreen PRs #124/#126; Changelog WEG (constantes macOS 26)
- Fóruns olarila/amd-osx/osxlatitude (WEG 1.7.1/1.7.1d7 em Tahoe 26.1–26.2)
- lemonade-sdk/mac-amdgpu; scottjg/qemu-vfio-apple; UTM #4579; RFC vfio-apple 2026-04
- HackerNews `library-validation` (WindowServer/Metal plugins terceiros)
- The Verge 2025-06-09 (Tahoe último major Intel); Wikipedia macOS Tahoe (lista modelos)

> Nota de honestidade: hashes de `apple-oss-distributions` e threads de
> fórum são móveis; os links acima são o pin disponível em 2026-09-04.
> Qualquer uso futuro deve re-verificar o conteúdo antes de citar como prova.
