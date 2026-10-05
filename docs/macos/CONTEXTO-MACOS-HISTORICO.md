# NVIDIA macOS / GA106Lab — MASTER PROJECT CONTEXT

> Contexto mestre reconstruído a partir desta conversa e do histórico técnico preservado até **2026-09-10**.
>
> Objetivo deste arquivo: permitir que qualquer nova sessão/agente recupere o projeto sem depender de memória implícita.
>
> **Regra de manutenção:** atualizar este arquivo sempre que houver uma nova mensagem com **evento técnico, resultado, decisão, teste, hash, mudança de arquitetura, novo risco, novo artefato ou avanço real do projeto**. Perguntas meramente explicativas/status (“onde estamos?”, “quanto falta?”, “o que isso significa?”) não precisam gerar uma nova revisão, a menos que introduzam uma decisão nova.
>
> Este documento tenta preservar **o máximo possível do histórico conhecido**, mas não deve ser tratado como transcrição literal de cada mensagem. Quando houver divergência entre este contexto e artefatos primários/logs/hashes do repo, **a evidência primária vence**.

---

# 1. OBJETIVO FINAL

Construir uma pilha gráfica experimental para fazer uma **NVIDIA RTX 3060 GA106** funcionar no macOS Tahoe com aceleração real, chegando ao caminho:

```text
RTX 3060 GA106
→ PCI / BAR / MMIO
→ driver/KEXT próprio
→ memória / VM / MMU
→ channels / scheduler / submission
→ execução real no hardware
→ completion / sync
→ integração macOS
→ IOAcceleratorFamily2 / IOGPU conforme arquitetura escolhida
→ MTLDevice
→ Metal
```

## Importante

- **Compute-only não é o objetivo final.**
- TinyGPU/tinygrad pode servir como referência/intermediário, mas não substitui integração gráfica.
- “GPU responde MMIO” != “GPU funciona”.
- “DMA funciona” != “gráficos funcionam”.
- “GSP funciona” != “Metal funciona”.
- “command submission funciona” != “Metal funciona”.
- Display/monitor direto na RTX fica **depois** da aceleração base; aceleração primeiro.

---

# 2. HARDWARE / SISTEMA

## CPU / GPUs

```text
CPU:
Ryzen 5 5600G

iGPU principal / macOS:
AMD Vega 7 / Cezanne
PCI ID: 1002:1638

GPU experimental:
NVIDIA RTX 3060 12 GB LHR
GA106 / nv176
PCI ID GPU: 10de:2504
PCI ID áudio HDMI/DP: 10de:228e
BDF GPU: 01:00.0
```

O Ryzen 5 5600G limita a plataforma a PCIe Gen3.

## macOS

```text
macOS Tahoe 26.6.2
Build: 25G83
Arquitetura: x86_64
SMBIOS/ambiente: MacPro7,1
Xcode: 26.5 (17F42)
clang: 21, x86_64
```

## OpenCore

Família usada:

```text
OpenCore REL-108-2026-09-05
versão/família 1.0.8
```

O `ocvalidate` **exato 1.0.8** não estava disponível publicamente/localmente.
Nunca declarar `ocvalidate exact PASS` sem provar o binário exato usado.

## Paths principais

Linux:

```text
~/Linux/Documentos/Projeto Nvidia/nvidia-macos/
```

macOS:

```text
~/Documents/nvidia-macos-BARE0/
```

EFI no macOS:

```text
/Volumes/MACOS PENDR/EFI/OC/
```

EFI no Linux:

```text
/run/media/isaque/MACOS PENDR/EFI/OC/
```

---

# 3. MODELO MENTAL DO PROJETO

Vocabulário usado no projeto:

```text
PCI         = localizar/identificar o dispositivo
BAR         = “portas”/janelas de acesso do dispositivo
MMIO        = registradores/controles expostos pela GPU
KEXT        = driver kernel do macOS
sysmem      = RAM do sistema
VRAM        = memória física da GPU
DMA         = GPU acessando memória diretamente
IOVA        = endereço que o dispositivo enxerga via DMA
IOMMU       = tradução/proteção de endereços de DMA
BME         = PCI Bus Master Enable, permissão para o device fazer DMA
GSP         = controlador interno das GPUs NVIDIA modernas
firmware    = código interno carregado em engines/controladores
MMU/PT      = tradução de endereços da GPU
FIFO/channel/runlist = filas/canais/scheduler de trabalho
command submission = enviar trabalho real para GPU
IOGPU/IOAccelerator = integração do driver com a pilha gráfica do macOS
MTLDevice   = GPU visível ao Metal
```

---

# 4. INVARIANTES DE SEGURANÇA — NÃO QUEBRAR

## OpenCore / boot

**NUNCA remover:**

```text
AAPL,iokit-ignore-ndrv
```

Esse property foi necessário para impedir o freeze do `IONDRVFramebuffer/.Display_boot`.

**NUNCA reintroduzir:**

```text
CLASS0 spoof
```

Não alterar propriedades AMD sem decisão explícita.

VoodooHDA pode existir na pasta, mas permanece **fora de `Kernel/Add`** / não injetado.

Somente **uma revisão GA106Lab** habilitada por vez.

Sempre backup antes de mutação de EFI.

Uma variável por vez quando possível.

## Autorizações de hardware

Salvo autorização explícita nova e específica:

```text
FIRST_MMIO_WRITE_AUTHORIZED = NO
PCI_CONFIG_WRITE_AUTHORIZED = NO
BME_ENABLE_AUTHORIZED = NO
DMA_AUTHORIZED = NO
INTERRUPTS_AUTHORIZED = NO
RESET_AUTHORIZED = NO
GSP_AUTHORIZED = NO
FIRMWARE_AUTHORIZED = NO
VRAM_WRITE_AUTHORIZED = NO
COMMAND_SUBMISSION_AUTHORIZED = NO
```

Também continuam proibidos por default:

```text
doorbell/PUT writes
IOGPU/IOAccelerator live experiments
Metal live integration
novos selectors live
reboot/EFI/KEXT mutation
```

sem a autorização correspondente.

## UserClient

- Root-only.
- API semântica fixa.
- Não expor arbitrary BDF/BAR/offset/address/value/width/timeout.
- Não expor endereços kernel/MMIO/DMA/IOVA ao userspace.
- Copyout deve estar inicializado em todos os paths.
- Lifecycle deve ser seguro em `stop/clientClose/teardown`.
- Nenhum selector pode transformar falha em sucesso por conversão incorreta de tipos.

---

# 5. EVIDÊNCIA PCI / BAR / IDENTIDADE HISTÓRICA

Linux / bring-up inicial confirmou GA106/nv176.

## BARs vistos no Linux

```text
BAR0: 16 MiB
BAR1: 16 GiB prefetchable
BAR3: 32 MiB
```

## macOS live IODeviceMemory

```text
BAR0 = 0xFB000000 / 16 MiB
BAR1 = 0x440000000 / 256 MiB
BAR3 = 0x450000000 / 32 MiB
ROM  = 0xFC000000 / 512 KiB
```

macOS usa `IOSubMemoryDescriptor` nesse caminho.

## Identidade PCI/read-only histórica

```text
vendor   = 10de
device   = 2504
class    = 030000
revision = a1

PCI Command = 0003
PCI Status  = 0010

MSE = ON
BME = OFF
```

## BOOT registers históricos

Selector 5 / BOOT0:

```text
0xb76000a1
decoded chipset = 0x176 = GA106
```

Selector 6 / BOOT42:

```text
0x176a1000
decoded chipset = 0x176
```

O antigo experimento de write em `0x140` foi **permanentemente rejeitado**.
Contexto permanece read-only.

---

# 6. TRAJETÓRIA INICIAL / BARE-NDRV

Foi escolhida a abordagem KEXT porque internal PCI não podia ser resolvido apenas com a rota compute/tinygrad usada como referência.

## BARE-NDRV0

Com:

```text
AAPL,iokit-ignore-ndrv=true
```

o boot ficou estável, RTX visível em PCI sem `IONDRVFramebuffer`, AMD saudável.

Tag histórica:

```text
GA106_MACOS_PCI_VISIBLE_STABLE
```

---

# 7. EVOLUÇÃO DOS SELECTORS / KEXT

Histórico simplificado:

```text
KEXT0  attach exato
KEXT1  UserClient selectors 0–2
KEXT2  selector3 PCI config read-only
KEXT3  selector4 BAR0 map
KEXT4  selector5 BOOT0
KEXT4B selector6 BOOT42
KEXT5  selector7 sysmem mapping prep
KEXT6  selector8 GFW readiness / 1.6.x offline
KEXT7  selector9 SYSMEM_FLUSH prewrite verification
KEXT8  production architecture 1.8.0
```

## Selector 7

KEXT5 / 1.5.3 live histórico:

- prepara mapeamento sysmem;
- page 4096;
- um segmento;
- não expõe DMA address;
- não faz DMA;
- não faz write.

## Selector 8

KEXT6 / 1.6.x:

- GFW readiness fixed semantic read.
- 1.6.2 corrigiu problema de lifetime do UserClient.
- Não assumir que selector8 antigo está automaticamente wired/executável na arquitetura 1.8.0 apenas porque a contagem de selectors é 10.

## Selector 9

`VERIFY_SYSMEM_FLUSH_PRECONDITIONS`

- ABI semântica fixa, 48 bytes.
- Apenas leitura de hardware + preparação interna de DMA mapping.
- Sem exposição de endereços.
- Sem write.
- Sem BME.
- Sem DMA real.

---

# 8. GSP / SYSMEM_FLUSH RESEARCH

Readiness relevante encontrada:

```text
0x118128 bit0
depois
0x118234 bits[7:0] == 0xff
```

Primeiro candidato histórico de write relevante:

```text
SYSMEM_FLUSH LO = 0x100c10
SYSMEM_FLUSH HI = 0x100c40
```

GA106 precisa dos dois.
Upstream observado escreve:

```text
HI
depois LO
```

Alinhamento exigido:

```text
256 bytes
```

Encoding estudado:

```text
LO = dma >> 8
HI = (dma >> 40) & 0x00ffffff
```

Regras de desenho:

- página dedicada 4096;
- mapping persiste;
- Gate A antes de write;
- Gate B no futuro: exatamente 2 writes + 2 readbacks;
- estado parcial HI/LO é desconhecido;
- recuperação = reboot, não “rollback MMIO”.

BME é necessário antes de DMA real da GPU, mas **não** para um store MMIO feito pela CPU.

Primeiro DMA significativo esperado no caminho GSP/Falcon seria FBDMA.

---

# 9. P68–P76 — HARDENING DO SELECTOR9 / CANDIDATE 1.7.x

## P68

1.7.0 selector9 Gate A.

## P69

Gemini secundário PASS.

## P70

Prelive antigo posteriormente invalidado por mudança de config de áudio.

## P73

Astra encontrou problemas importantes na 1.7.0:

- self-Busy blocker do selector9;
- risco de lifetime em teardown / `clearMemoryDescriptor`.

Resultado: FAIL.

## P74

Muse 1.7.1 corrigiu os dois.

## P75

1.7.2 corrigiu liveness:
Busy mantido durante GFW poll.

## P76

Encontrado mismatch de identidade:

```text
KMOD 1.6.2
plist 1.7.2
```

Muse 1.7.3 corrigiu identidade.

### Candidate 1.7.3 congelado

Path:

```text
~/Documents/nvidia-macos-BARE0/GA106Lab-kext7-sysmem-flush-prewrite-fix3-src/
```

Hashes:

```text
KEXT:
39b25bb9a3c469a74e1c42bf64417027b280cab8bbe870462c508e47cce08c51

plist:
fa32254e0a7e266e5f22431fd65a3a3d81cff8122739021ecc278257ed6673b0

CLI:
9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840

manifest:
f42487ce0d5a083c56189e63a53983716c6a35db9ca2ee8e83a3abdf0a091893
```

1.7.3 ficou frozen/read-only.
Historicamente houve repetidas checagens de integridade 452/452.

---

# 10. ÁUDIO / OPENCORE — NÃO REGREDIR

Board:

```text
Gigabyte B450 AORUS M
```

Áudio onboard:

```text
controller 1022:15e3
BDF 08:00.6
codec ALC892
codec ID 0x10ec0892
Address 0
```

KEXTs:

```text
AppleALC 1.9.8
Lilu 1.7.3
layout 20
data: 14 00 00 00
```

Config pré-áudio antiga:

```text
41e2a921f399c45b25186b574b5e03bc9d3846863fbf1c3411ad1fe2b002b63d
```

Config pós-áudio/current conhecida anteriormente:

```text
06cf022258d796c942b71786aeff5b9f730d9ebf507d2ca8f85732ab538315ef
```

Depois houve configs novas apenas por staging GA106Lab; áudio/AMD permaneceram invariantes.

---

# 11. P77 — CHANNEL / GSP MODEL

Status:

```text
APPROVE / CLOSED
```

Achado importante:

GSP channel usa:

```text
GSP_RM_ALLOC = 103
+
BIND/SCHEDULE NVA06F
```

e **não** o legacy `ALLOC_CHANNEL_DMA = 6`.

Runlist interna permaneceu privada/não provada.

Handoff:

```text
P77-FINAL-HANDOFF.md
```

---

# 12. P78 — VM / MMU

Status:

```text
APPROVED
```

Cobertura offline:

- VM model;
- MMU;
- PDE/PTE;
- aperture;
- page tables;
- ownership;
- mutations.

Resultado citado:

```text
238/238
mutations 10/10
```

Handoff:

```text
P78-FINAL-HANDOFF.md
```

---

# 13. P79 — COMMAND SUBMISSION MODEL

Status:

```text
APPROVED
```

Cobertura offline:

- GPFIFO;
- pushbuffer;
- methods;
- PUT/GET;
- channel lifetime;
- fence/completion model.

Resultado:

```text
166/166
```

Ainda **não** era execução real de hardware.

---

# 14. P80 — PRODUCTION ARCHITECTURE 1.8.0

Status:

```text
CLOSED / APPROVE
```

Tree:

```text
GA106Lab-kext8-production-architecture-src/
```

Versão:

```text
1.8.0
```

Revision:

```text
TG-KEXT8-PRODUCTION-ARCHITECTURE
```

Características:

- arquitetura de 11 estados;
- descriptors/contracts/ledger owned pelo service;
- `close != free`;
- `stop/clientClose` seguros/idempotentes em qualquer estado;
- guards de concorrência;
- 13 hardware stubs fail-closed;
- UserClient selectors = 10;
- sem exposição de endereços DMA/kernel/MMIO.

Gemini inicialmente REJECT por:

- identidade KMOD;
- lifetime audit;
- build audit.

Correções até M0239.

Gemini G0203:

```text
APPROVE
```

M0240:

```text
P80-FINAL-HANDOFF.md
```

**P80 não significava aceleração.**
Todos os caminhos de hardware relevantes continuavam fail-closed.

---

# 15. P81 / P81.1 / P81.2

## P81

Design-only para primeira validação live.

Escolheu Conservative Option D.

Todas as autorizações live perigosas = NO.

## P81.1

Corrigiu gate de clock/higiene.

`sntp -sS` foi proibido.

Substituído por:

```text
date -u
sysctl kern.boottime
EXIF-only quando aplicável
```

Q9 stale corrigido.
Gemini extractor hardened.

Status:

```text
APPROVE
```

## Option0

Non-target não pôde executar por falta de VM/non-target.

Resultado:

```text
HUMAN_BLOCK
```

## P81.2

Foi criado offline rehearsal harness em:

```text
P81_2-OFFLINE-REHEARSAL-HARNESS/
```

Resultados:

```text
20/20 scenarios
42/42 unit
25/25 mutations killed
```

Smoke encontrou 6 problemas reais, depois corrigidos.

Gemini G0207:

```text
APPROVE
```

Importante:
isso **não** substitui evidência live/non-target.

---

# 16. P82 / P82.1 / ROUTE RESOLUTION

P82 design selecionou Candidate C:

```text
GFW readiness read-only
```

Mas o design antigo assumia selector8.

P82.1 provou:

```text
C_EXECUTABLE_ON_CURRENT_1_5_3 = NO
CURRENT_1_5_3_HAS_GFW_READINESS_SELECTOR = NO
STAGE_OR_LOAD_REQUIRED_FOR_C_AS_DESIGNED = YES
```

Na 1.5.3 existiam selectors 0–7.

Selector8 começa em 1.6.0 e estabiliza em 1.6.2.

Q10 permaneceu BLOCKED:

não havia método read-only confiável para provar criptograficamente que bytes do KEXT carregado em RAM eram idênticos ao binário em disco.

## Route Resolver no Relay V4

Concluiu:

```text
ROUTE1_VALUE = NO_GO_REDUNDANT
selectors5/6 NEW_EVIDENCE = NO
selector7 EXCLUDE
FIRST_CONTROLLED_LOAD_REQUIRED = YES
NEXT_RISK_CLASS = R3
```

---

# 17. RELAY V3 / V4 — INFRA E GOVERNANÇA

## Agentes

### Muse Spark

Writer/implementer principal.

- escreve código;
- faz testes;
- pesquisa;
- implementa fixes.

Regra histórica:
**um único writer** para evitar múltiplas IAs editando repo simultaneamente.

### Gemini 3.8 Flash High

Reviewer final/independente via Antigravity agentapi.

Usar em checkpoints importantes, não como linter constante.

### B.AI subagents

OpenCode:

```text
GLM-5.3-Flash
hy3
qwen3.8-flash
mimo-v2.5
```

Read-only para pesquisa/pre-review.

Máximo de paralelismo normal:

```text
2 subagents
```

### Muse Smoke

Custom Muse Spark 1.3 jailbreak policy:

```text
UNTRUSTED_ADVERSARIAL_REVIEWER
```

Read-only via ACL/dispatcher.
Sem autoridade de verdict/live.

## Infra

B.AI:

```text
https://api.b.ai/v1
OpenAI-compatible Chat Completions
/v1/responses não suportado no caminho GLM usado
```

Key fica no Keychain:

```text
BAI_API_KEY
```

Nunca reproduzir o segredo.

OpenCode:

```text
1.18.30
```

Smoke config:

```text
~/.config/opencode/opencode.jsonc
```

Headless only.

GUI bridge é proibido após freeze/panic histórico.

## Relay V3 root

```text
~/Mac/Documents/nvidia-macos-BARE0/Tools/P80-Relay-V3
```

## Relay V4

Evidence graph:

```text
Conversations/_relay_v4/evidence_graph.json
```

Delta report / engine:

```text
Tools/Relay-V4/delta_engine.py
Tools/Relay-V4/policy.json
```

Estado inicial operacional:

```text
RELAY_V4_STATUS = OPERATIONAL
16 claims C-001..C-016
0 impacted / 16 reusable
```

## Classes de risco

```text
R0 = docs / simple offline
R1 = análise de código existente
R2 = alterações offline executáveis/KEXT/ABI
R3 = novo live read-only / selector / load
R4 = qualquer live mutation/write:
     MMIO/PCI/BME/DMA/GSP/FW/VRAM/reset/IRQ/doorbell/PUT/submission/IOGPU/Metal
```

Política:

```text
R0 → Muse + deterministic; ~5 min; sem Smoke/Gemini por default
R1 → Muse + 1 reviewer barato; ~10 min
R2 → Muse + até 2 reviewers; Smoke quando lifetime/ABI/kernel; Gemini no close; ~20 min
R3 → review apropriado + Smoke + Gemini + human boundary
R4 → review completo + autorização humana one-shot
```

Princípios:

- deterministic checks antes das IAs;
- evidence reuse;
- delta review;
- doc-only fix não reabre cadeia;
- no micro-milestones;
- batching até próximo risk boundary;
- 1 retry estreito em provider, depois delegate;
- 429/503/401 nunca = PASS;
- wrong-root/transport Gemini pode ser repetido quando claramente NEED_EVIDENCE técnico, não verdict;
- Smoke NEED_EVIDENCE por falta de raw evidence não é FAIL automático;
- provider failure não apaga state;
- retomar do evidence graph/state.

---

# 18. R3 — PRIMEIRO CONTROLLED LOAD DA 1.8.0

Pacote R3 escolheu candidate 1.8.0:

```text
TG-KEXT8-PRODUCTION-ARCHITECTURE
```

Hashes:

```text
CANDIDATE_KEXT_HASH =
b6d4ea3139f86be5b9ba562dc7cf5eb2fa6efce2fc1b3799d242ac45240faf2a

CANDIDATE_PLIST_HASH =
2d0d3cf922cb6855d1293abd51ec4a43b297aa5e716a91bbd5bc807f1a836e1e

CANDIDATE_MANIFEST_HASH =
8cb5be2efa4cb00f642693776806a436cedb6d5ed2d42a36a8011a991411dfb2
```

Candidate antigo 1.5.3 seria disabled na mesma transação.

Coexistência:

```text
NO
```

Primeiro live window:

```text
LOAD + PRESENCE VERIFY + STOP
```

Sem selector.

## Correção de recovery pin

Pacote inicial usou por engano SHA pre-áudio `41e2...`.

Foi corrigido por R3.1 para a config corrente pós-áudio:

```text
06cf022258d796c942b71786aeff5b9f730d9ebf507d2ca8f85732ab538315ef
```

GLM PASS.
Smoke/Gemini pulados por política V4 para essa microcorreção.

## Tentativa automatizada abortada corretamente

A IA não tinha sudo/reboot não-interativo.

Resultado:

```text
ABORT_BEFORE_WRITE
```

em vez de stage sem supervisão.

Foram criadas fases humanas separadas.

---

# 19. PRIMEIRO BOOT LIVE DA 1.8.0

## Fase A staging

Antes do reboot:

```text
READY_FOR_HUMAN_REBOOT
```

Backup:

```text
EFI-BACKUP-R3-LIVE-20260909-211312/config.plist
```

PRE config:

```text
06cf022258d796c942b71786aeff5b9f730d9ebf507d2ca8f85732ab538315ef
```

POST config:

```text
8570a1adf5379b809ae5ee10ff2c3b62af2d67b238a0db3ff0c3474fbb48bf5b
```

Diff:

- Kernel/Add 21 → 22;
- 1.5.3 Enabled true → false;
- 1.8.0 entry adicionada/enabled.

Invariantes preservados.

## Pós-boot

Boot:

```text
21:15:25
```

1.8.0 presente:

```text
kextstat idx 93
UUID começando 327A448E…
```

Somente um GA106Lab ativo.

Sem novo:

- panic;
- hang;
- no-signal;
- bootloop.

`shutdown_stall 21:14:55` era artefato imediatamente pré-boot do reboot autorizado.

Gemini:

```text
G0212 APPROVE
```

Resultado:

**primeiro boot real da arquitetura P80 / GA106Lab 1.8.0.**

---

# 20. P83 — PRIMEIRO USERCLIENT LIVE / GETVERSION

Selector0:

```text
kGA106LabSelector_GetProtocolVersion
```

Handler:

```text
GA106LabUCGetVersion
```

Dispatch:

```text
{0,0,2,0}
```

Esperado:

```text
protocol = 1
driver = 0x00010800
```

Não toca RTX hardware.

Root-only.

CLI pin:

```text
9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840
```

## Primeira execução humana

Resultado relatado:

```text
protocol: 1
driver: 0x00010800
```

Mas inicialmente faltavam:

- transcript raw;
- exit code;
- invocation counter.

Smoke detectou gap metodológico.
Gemini G0214:

```text
NEED_EVIDENCE
```

P83 não podia ser chamado PASS.

## Reexecução instrumentada explicitamente autorizada

Exatamente uma nova execução.

Transcript:

```text
=== T-BEGIN 2026-09-10T02:04:19Z ===

protocol: 1

driver: 0x00010800

exit_code=0

=== T-END 2026-09-10T02:04:21Z ===

INVOCATION_COUNT=1
```

Isso fechou o gap de evidência.

Conclusão técnica:

```text
userspace
→ IOKit
→ GA106Lab UserClient
→ selector0
→ response
```

funciona live na 1.8.0.

P83 prova comunicação, não GPU execution.

---

# 21. PANICS / INSTABILIDADE HISTÓRICA

## Panic 1

Arquivo:

```text
/Library/Logs/DiagnosticReports/Kernel-2026-09-09-032114.panic
```

Características:

- Kernel NX page fault;
- `mediaanalysisd`;
- `_ipc_kmsg_send`;
- zone ipc ports;
- possível UAF, confiança média;
- sem backtrace direto GA106Lab.

Também existiam `.gpuRestart` da AMD Renoir/Vega:

```text
DID1638 VM protection fault
```

AMD é forte suspeito para instabilidade/hotplug, mas não prova causalidade no panic.

## Panic 2

```text
/Library/Logs/DiagnosticReports/Kernel-2026-09-09-101303.panic
```

- general protection;
- `_copyinmsg+0x6b`;
- process `opencode`;
- sem graphics backtrace.

Ambos levantaram preocupação com IPC/message/lifetime corruption.

O 1.5.3 antigo virou suspeito sério, mas **não provado**.
A 1.6.2 corrigiu lifetime de UserClient, fortalecendo a preocupação histórica.

GUI bridge que congelou não deve ser rerodado.
Headless only.

---

# 22. CONTINUOUS RELAY / ROUTE PARA PRIMEIRO GPU READ

Foi criado um fluxo contínuo para:

```text
fechar P83
→ mapear selectors 1.8.0
→ escolher primeira leitura GPU-derived
→ auditar delta
→ R3 review
→ human command apenas quando necessário
```

Regras:

- no microsteps burocráticos;
- não repetir BOOT0/BOOT42 sem necessidade;
- selector7 excluído;
- candidate deve:
  - existir na live 1.8;
  - ser fixed semantic API;
  - zero write;
  - zero BME/DMA/GSP boot/FW/VRAM;
  - closed BAR/offset/width;
  - one bounded invocation;
  - sem arbitrary address;
  - produzir evidência nova.

Autorização condicional era somente para **uma** future GPU-derived read, se o pipeline inteiro aprovasse.

---

# 23. C-022 — BUG bool → IOReturn EM SEL1/SEL2

Durante o fluxo contínuo, foi encontrado:

```text
C-022
bool → IOReturn
selectors 1/2
```

No binário live antigo, isso podia:

- retornar `NotReady`;
- no path de falha, produzir copyout não confiável;
- potencialmente vazar stack garbage ao userspace.

Decisão:
**não executar sel1 no binário live antigo.**

## R2 fix / revB

Turno:

```text
M0276
```

Houve provider instability:

- Muse 3x 401;
- 1 hang;
- depois recuperou em testes direct/json.

R2 foi fechado:

```text
R2_STATUS = APPROVED_AND_CLOSED
```

Fix:

```text
C-022 bool->IOReturn sel1/sel2 = FIXED
+ cached={} hardening
```

revB KEXT SHA:

```text
8e387d5386db02ac45ff66b9d44487cd3599e7da8e3218d738ea6c830d0b5c20
```

Build/test:

```text
BUILD_OK
65 PASS
sanitizers PASS
mutations PASS
manifest 1a6107be
32/32
```

Live antigo ainda era:

```text
b6d4ea31…
```

Smoke:

- SMOKE-01 NEED_EVIDENCE metodológico;
- SMOKE-02 FINDINGS advisory;
- fix confirmado file:line;
- resto era P80-scope já adjudicado em G0203.

Gemini:

```text
G0215 APPROVE
```

Qwen:

```text
PASS
```

GLM delegado:

```text
PASS
```

Mimo:

```text
2x timeout
```

## Erro operacional de placeholder

Durante o fluxo R2, a IA de setup escreveu `PLACEHOLDER` no arquivo errado:

```text
G0208
```

quando deveria ter sido:

```text
G0215
```

Foi detectado imediatamente pelo output.

Ação:

- restauração byte-a-byte usando `.bak` de 18:43;
- revalidação PASS/APPROVE;
- sem perda;
- sem segredos.

Nova regra operacional:

> Comandos destrutivos só usam path copiado de `ls`/evidência primária; nunca digitar path manualmente por pressa.

---

# 24. R3b — STAGE DO revB

Decisão humana:

```text
STAGE_REVB_FIRST = YES
RUN_SEL1_ON_OLD_LIVE_BINARY = NO
```

## Fase A

Status:

```text
READY_FOR_HUMAN_REBOOT_REVB
```

revB expected SHA:

```text
8e387d5386db02ac45ff66b9d44487cd3599e7da8e3218d738ea6c830d0b5c20
```

PRE config:

```text
8570a1ad…
```

Backup:

```text
EFI-BACKUP-R3B-LIVE-20260910-012601
```

POST config:

```text
45c29468…
```

Diff:

- 22 → 23 Kernel/Add entries;
- buggy 1.8.0 → Disabled;
- revB → Enabled.

Invariantes:

```text
ignore-ndrv PRESENT
CLASS0 ABSENT
AMD unchanged
audio unchanged
VoodooHDA not injected
```

No exact ocvalidate claim.

---

# 25. R3b — POSTBOOT revB

Novo boot:

```text
01:30:04
```

Boot anterior:

```text
21:15:25
```

Resultado Fase B:

```text
PASS
```

revB:

```text
present = YES
kextstat idx 87
UUID fresco começando A50C…
```

Como Q10 continua aberto:

```text
REVB_LOADED_VERIFIED = INFERENCE
LOADED_HASH_PROVEN = NO
```

A inferência foi baseada em:

- somente revB Enabled;
- staged artifact íntegro;
- boot fresco;
- old revision disabled;
- uma única GA106Lab ativa.

Old live revision:

```text
ACTIVE = NO
```

Health:

```text
PASS
```

Sem:

- bootloop;
- no-signal;
- panic novo.

EFI:

```text
45c29468…
```

permaneceu intacta.

---

# 26. R3b — PRIMEIRO LIVE `selector1 identity` NO revB

Fase C auditou source:

```text
C022_FIXED = YES
COPYOUT_ALL_PATHS_INITIALIZED = YES
cached={} presente
memset/copyOk paths corretos
```

Selector1 permaneceu:

- fixed semantic;
- read-only;
- no arbitrary address;
- one-shot;
- zero write;
- zero BME;
- zero DMA;
- zero GSP;
- zero FW;
- zero reset/IRQ;
- zero submission.

## Comando humano instrumentado

Foi executado uma única vez, sem retry.

Transcript:

```text
=== T-BEGIN 2026-09-10T04:34:49Z ===

vendor: 10de

device: 2504

providerEntryID: 4294967865

state: 1

exit_code=0

=== T-END 2026-09-10T04:34:51Z ===

INVOCATION_COUNT=1
```

## Classificação correta

Não inflar esse claim.

Provável classificação:

```text
LIVE_PROVIDER_DERIVED = YES
DIRECT_GPU_REGISTER_READ = NO
```

se o source mostrar que os valores vêm do provider/IOPCIDevice e não de BAR/MMIO.

Isso é a primeira evidência live instrumentada do path `selector1 identity` corrigido no revB.

Fase E/closeout deve:

- validar raw `/tmp/R3B-sel1-raw.txt`;
- provar provenance dos campos no source;
- checar health pós-call;
- atualizar evidence graph;
- decidir nomenclatura final do claim;
- auto-continuar apenas em R0/R1/R2 até próximo risk boundary.

---

# 27. PESQUISA PARALELA — REIMS / APPLE PARAVIRT GPU

Foi estudado:

```text
steelbrain/reims-vgpu
steelbrain/metal2vulkan
QEMU apple-gfx / apple-gfx-pci / vmapple
Apple ParavirtualizedGraphics
AppleParavirtGPU
experiment-macOS-arm64-on-asahi-linux-arm64
```

## Reims

Arquitetura geral:

```text
macOS guest
→ Metal.framework
→ AppleParavirtGPU / Metal plugin
→ protocolo PVG
→ QEMU/Reims device
→ backend Vulkan
→ driver Vulkan host
→ GPU física
```

Não é driver NVIDIA nativo para macOS.

Valor para o projeto:

- oracle do lado Metal/macOS;
- estudo de queues/contexts/fences/completion;
- estudo de serialization;
- laboratório paralelo;
- metodologia para distinguir parsing/meaning/execution.

## Importante

Reims pode usar uma RTX sob driver Vulkan Linux, mas o macOS guest vê uma **Apple paravirtual GPU**, não uma NVIDIA.

---

# 28. APPLE PARAVIRT / METAL — ACHADOS IMPORTANTES

No macOS Tahoe 26.6.2 / 25G83:

## x86/PCI

```text
AppleParavirtGPU.kext = 15.0.0
ProjectName = ParavirtualizedGraphics_IOAFKext
SourceVersion = 64004007000000
BuildVersion = 1537

AppleParavirtGPUMetal.bundle = 64.4.7
NSPrincipalClass = AppleParavirtDevice
```

O `64.4.7` é **bundle Metal**, não versão do KEXT.

`AppleParavirtGPU.kext/Contents/` não contém `MacOS/`;
executável fica na kernel collection.

Bundle Metal é universal:

```text
x86_64 + arm64e
```

Dependências observadas:

- Metal 373.7.0;
- IOKit 275.0.0;
- IOSurface;
- IOAccelerator privado;
- Foundation 5026.6.7;
- libobjc;
- libc++;
- libSystem;
- CoreFoundation.

## Cadeia x86/PCI

```text
IOPCIDevice 0xEEEE106B
→ AppleParavirtGPUControl
→ AcceleratorProperties:
   IOMatchCategory=IOAccelerator
   MetalPluginName=AppleParavirtGPUMetal
   MetalPluginClassName=AppleParavirtDevice
→ IOAcceleratorFamily2
→ AppleParavirtGPUMetal.bundle
→ AppleParavirtDevice
→ MTLDevice
```

Plist x86 confirmou:

```text
Personality = AppleParavirtGPUControl
IOClass = AppleParavirtGPUControl
IOProviderClass = IOPCIDevice
IOPCIMatch = 0xEEEE106B
IOMatchCategory = IOFramebuffer
IOProbeScore = 1000000
FramebufferCount = 0
```

Display capabilities:

```text
DisplayPipeSupported = true
TransactionsSupported = true
```

Dependências incluem:

```text
AppleGraphicsDeviceControl
IOAcceleratorFamily2
IOGraphicsFamily
IOPCIFamily
IOSurface
```

## ARM

Cadeia separada:

```text
AppleARMIODevice
name = paravirtualizedgraphics,gpu
→ AppleParavirtGPU
→ IOGPUFamily
→ AppleParavirtGPUMetalIOGPUFamily.bundle
```

ARM tem mapper IOSurface dedicado:

```text
IOSurfaceParavirtMapperDevice
IOSurfaceParavirtMapperService
```

Não misturar caminho ARM/IOGPUFamily com x86/IOAcceleratorFamily2.

---

# 29. APPLE PARAVIRT — USERCLIENT / ABI STATIC RESEARCH

A assinatura completa ainda é UNKNOWN.

Achados estáticos:

```text
0x100 create object/fence/sampler/depth-stencil/pipeline
0x101 delete/deallocation
0x102 function variant
0x103 texture/buffer
0x104 texture deallocation
0x105 device info
0x106 compute pipeline
```

Esses números **não são ABI pública** e **não devem ser chamados live**.

Achados associados a IOAccelerator:

```text
IOServiceOpen type 5
IOServiceOpen type 6
IOServiceOpen type 8
```

Type 8 aparece associado à command queue.

Foi observada uma struct de queue:

```text
0x404 bytes
```

Device info:

```text
selector-like call 0x105
output 0x11c
copia ~0xe0 para estado device
```

Semântica campo-a-campo continua UNKNOWN.

Símbolos encontrados incluem:

```text
AppleParavirtGPUControl::start
AppleParavirtAccelerator::start
createUserGPUTask
createKernelGPUTask
newSharedUserClient

AppleParavirtSharedUserClient::{
  createObject
  deleteObject
  createFunction
  addChildResource
  removeChildResource
  getDeviceInfo
  createComputePipeline
  externalMethod
}

AppleParavirtDisplayPipe::submitTransaction
AppleParavirtCommandAllocator::submitOnChannel
RootChannel::submitBuffer
VirtualChannel::submitBuffer

IOAccelContext2
IOAccelDevice2
IOAccelCommandQueue
sDeviceMethods
sContextMethods
sCommandQueueMethods
sSurfaceMethods
```

Símbolo != dispatch comprovado.
Não inventar ABI.

---

# 30. COMO `MTLDevice` PROVAVELMENTE NASCE

Reconstrução atual:

```text
plugin load
→ AppleParavirtDevice
→ initWithAcceleratorPort
→ shared connection
→ device info / capability handshake
→ compiler setup
→ resource pools
→ object-ref allocator
→ PGSerializer
→ command queue
→ notification/completion path
→ MTLDevice utilizável
```

Gates devem ser separados:

```text
aparecer no IORegistry
≠
aparecer em MTLCopyAllDevices()
≠
newCommandQueue funcionar
≠
command buffer executar
```

Conclusão importante:

> MTLDevice nasce de um **handshake**, não apenas de PCI matching.

Não copiar IDs Apple para tentar “enganar” o Metal.

---

# 31. PGSerializer / OBJECT REFS

Reims oferece um oracle de `PGSerializer`.

Elementos observados:

```text
PGSerializer
initWithDevice:objectRefAllocator:

PGSerializerAllocator
allocateOperationBytes:

PGSerializerCommandStream
getCommandBufferBytes:
```

O harness pode capturar bytes produzidos pela implementação Apple.

Útil para:

- descriptors de texture;
- sampler;
- depth/stencil;
- draw forms;
- bindings;
- object refs;
- identificar campos modificados por uma única mudança controlada.

Isso é oracle/protocolo virtual Apple, **não** comando NVIDIA.

Conceito importante para P79/P80:

```text
resource
→ object ref
→ backing
→ read/write refs
→ fences
→ submission
→ execution
→ visibility
→ completion
→ notification
→ retirement
```

---

# 32. METAL2VULKAN / SHADERS

Pipeline:

```text
AIR bitcode / LLVM IR
→ llvm-dis quando necessário
→ sanitização/parsing metadata
→ SPIR-V em Rust
→ lowering/interface/memory passes
→ Vulkan
```

APIs úteis para estudo:

```text
reflect_sanitized
reflect_sanitized_specialized
ShaderReflection
parsers em src/meta
```

Pode ser útil offline para extrair:

- entry point;
- stage;
- arguments;
- Metal indices;
- types/layouts;
- textures;
- samplers;
- function constants;
- argument buffers.

Isso **não** resolve o shader path nativo NVIDIA.

Ainda faltará:

- NVIDIA machine code/ISA;
- execution ABI;
- pipeline state;
- hardware descriptors;
- residency;
- submission real;
- fault handling.

SPIR-V não executa diretamente na RTX.

---

# 33. REIMS / PVG MMIO E PROTOCOLO — NÃO COPIAR COMO NVIDIA

Device virtual PCI Reims estudado:

```text
ID 106B:EEEE
BAR0 = 16 KiB control
BAR1 = 16 MiB linear GOP framebuffer
MSI = 1 vector
```

Alguns offsets PVG:

```text
0x1000 CONTROL_FIFO
0x1004 FIFO_LENGTH
0x1008 FIFO_WRITTEN
0x100c FIFO_READ
0x1010 FIFO_START
0x1014/0x1018 interrupt status
0x101c ROOT_PAGE
0x1020 CHILD_DOORBELL
0x1024 MAIN_KICK
0x1028 CHILD_REPLAY_DOORBELL
0x102c INTR_FAULT
0x1030/0x1034 FIFO_BASE_PAGE / VERSION
```

Esses são **registradores virtuais PVG**, não registradores GA106.

Doorbells/status/clear-on-read observados no Reims não podem ser transportados mecanicamente para o KEXT físico.

---

# 34. COMPLETION — APRENDIZADO PARA P79/P80

Reims reforçou que é preciso separar:

```text
aceito
→ submetido
→ executado
→ resultado visível
→ completion stamp publicado
→ IRQ/notificação
→ recurso retirado/liberado
```

Publicar completion cedo = risco de dados incompletos.

Notificar tarde = cliente pode permanecer dormindo.

Fences, events e completion não são sinônimos.

Essa separação deve permanecer no roadmap antes de submissão física real.

---

# 35. ARM HACKINTOSH — O QUE É E O QUE NÃO É

Demonstrado:

- Apple Silicon + macOS + QEMU/HVF/vmapple;
- experimento Apple Silicon + Asahi/KVM.

Não demonstrado pelo material estudado:

- GPU Asahi acelerando o guest;
- ARM genérico CIX/Rockchip/Orange Pi com macOS acelerado;
- macOS nativo genérico em ARM não-Apple.

No experimento Asahi citado:

```text
M2 Pro
Fedora 42
kernel 6.19.14-vmapple2
Ventura 13.6
8 CPUs
```

Renderização validada:

```text
llvmpipe
```

ou seja, CPU, não aceleração Asahi comprovada.

Não extrapolar vídeo/marketing para suporte ARM genérico.

---

# 36. ROADMAP EM DUAS TRILHAS

## Trilha A — hardware físico GA106

```text
provider/identity live
→ próxima leitura realmente hardware-derived
→ readiness
→ primeiro write controlado futuro
→ BME
→ DMA real
→ GSP
→ firmware
→ channel real
→ VM/MMU física
→ submission
→ completion
```

Cada classe sobe de risco.
Writes continuam proibidos sem nova autorização.

## Trilha B — macOS/Metal offline

```text
IOAcceleratorFamily2
→ matching
→ kernel ↔ plugin Metal ABI
→ device/capability handshake
→ memory mappings
→ shared/user clients
→ queues
→ notification/completion
→ compiler negotiation
→ resource pools/object refs
→ MTLDevice gates
→ shader metadata/oracles
```

Essa trilha pode avançar offline em paralelo.

---

# 37. PRÓXIMAS PESQUISAS OFFLINE DE MAIOR VALOR

1. Extrair dispatch table estática AppleParavirt/IOAccel:

```text
selector/type
→ símbolo
→ input
→ output
→ confidence
→ evidence
```

Sem `IOConnectCall` live.

2. Byte-compare:

```text
PGSerializer oracle
vs
streams Reims
```

com provenance/version tracking.

3. Capability matrix versionada:

```text
features
supportsBinaryArchives
supportsArgumentBuffers
supportsResourceHeaps
deviceInfo
KEXT/plugin versions
build provenance
```

4. Formalizar kernel↔Metal-plugin ABI como milestone próprio.

5. Atualizar P79/P80 para estados de completion explícitos:
`accepted/submitted/executed/visible/notified/retired`.

---

# 38. PROVIDERS / LIMITES / RECOVERY

Muse/OpenCode Zen pode retornar:

```text
401 Insufficient balance
429
503
hang
```

Regra:

- não transformar provider failure em PASS;
- não apagar state;
- não reiniciar milestone;
- preservar logs/evidence graph;
- se turno não começou, marcar NOT_RUN;
- retomar turno idêntico quando provider voltar;
- reviewers de outros providers não substituem writer quando writer é obrigatório;
- B.AI/Smoke/Gemini podem continuar independentes quando apropriado.

Astra Medium foi usado em pesquisa paralela.
Quando acabou limite do Astra, isso **não deveria bloquear o Relay**, porque Astra não é dependência do caminho crítico.

---

# 39. ESTADO ATUAL EXATO DO PROJETO

## KEXT/live

revB foi staged e bootou.

Expected SHA:

```text
8e387d5386db02ac45ff66b9d44487cd3599e7da8e3218d738ea6c830d0b5c20
```

Q10 continua:

```text
loaded full hash cryptographic proof = unavailable
```

Então loaded identity é inferência forte, não hash RAM provado.

EFI atual R3b:

```text
45c29468…
```

Old buggy revision:

```text
b6d4ea31…
```

não está ativa após reboot R3b.

## Health

Pós-R3b:

```text
PASS
NEW_PANIC = NO
```

## Última ação live

`ga106ctl identity` / selector1, uma vez:

```text
vendor: 10de
device: 2504
providerEntryID: 4294967865
state: 1
exit_code=0
INVOCATION_COUNT=1
```

Timestamp:

```text
BEGIN 2026-09-10T04:34:49Z
END   2026-09-10T04:34:51Z
```

Nenhum retry.

Nenhum write autorizado.

## Próximo passo imediato

A Fase E do R3b já fechou PASS.

Estado atual:

```text
LIVE_PROVIDER_DERIVED = YES
DIRECT_GPU_REGISTER_READ = NO
FIRST_LIVE_PROVIDER_DERIVED_READ = PASS
```

O Relay também marcou `FIRST_GPU_DERIVED_READ = PASS` apenas pela taxonomia histórica do pacote G0215; semanticamente, manter claro que **não foi leitura direta de registrador/MMIO da GPU**.

Pendência administrativa:

```text
evidence_graph.json ainda não recebeu C-025/C-026
```

porque a nova sessão confundiu `LIVE_WRITES_AUTHORIZED=NO` com proibição de bookkeeping filesystem. Corrigir isso offline sem re-review/live.

Próximo candidato real:

```text
selector3 GetPciSnapshot
R3 live read-only
```

Antes de qualquer execução, preparar pacote mínimo R3 e obter autorização humana nova.

---

# 40. AUTORIZAÇÕES ATUAIS — RESUMO

```text
LIVE_WRITES_AUTHORIZED = NO

PCI_CONFIG_WRITE_AUTHORIZED = NO
MMIO_WRITE_AUTHORIZED = NO
BME_ENABLE_AUTHORIZED = NO
DMA_AUTHORIZED = NO
INTERRUPTS_AUTHORIZED = NO
RESET_AUTHORIZED = NO
GSP_AUTHORIZED = NO
FIRMWARE_AUTHORIZED = NO
VRAM_WRITE_AUTHORIZED = NO
DOORBELL_PUT_AUTHORIZED = NO
COMMAND_SUBMISSION_AUTHORIZED = NO
IOGPU_IOACCEL_METAL_LIVE_AUTHORIZED = NO
```

A autorização one-shot para selector1 já foi consumida.

Não repetir selector1 sem nova decisão.

---

# 41. REGRAS DE COMUNICAÇÃO / WORKFLOW COM O USUÁRIO

- Português do Brasil.
- Direto, técnico, sem enrolação.
- Explicar de forma simples quando pedido.
- Long prompts para agentes devem ser entregues como `.md` baixável, não colados gigantes no chat.
- Sempre indicar destinatário:
  - `Enviar para: Muse`
  - `Enviar para: Gemini 3.8 Flash High`
  - `Enviar para: IA de Setup / OpenCode`
  - `Enviar para: Astra — Medium`
- Usuário não quer múltiplas IAs editando o repo ao mesmo tempo.
- Muse = writer.
- Reviewers = read-only.
- Usuário quer Relay rápido, contínuo, sem ciclos de 40 min para microdetalhes.
- Evidence reuse + delta review.
- Não “safety theater”; parar apenas em risco real/human boundary.
- Nunca exagerar progresso:
  - não dizer “Metal está perto” sem base;
  - não chamar provider identity de registrador GPU;
  - não chamar offline architecture de hardware working.

---

# 42. PROTOCOLO PARA NOVAS SESSÕES

Se este arquivo for usado em nova sessão/agente:

1. Ler este arquivo inteiro.
2. Ler:
   ```text
   Conversations/_relay_v4/evidence_graph.json
   Tools/Relay-V4/policy.json
   estado/logs atuais do Relay
   ```
3. Ler o último prompt/handoff relevante.
4. Verificar repo/source/hash antes de agir.
5. Não assumir que um claim deste arquivo substitui evidência primária.
6. Não refazer milestones já fechados salvo invalidation real.
7. Fazer delta review.
8. Não executar live/write sem autorização atual.
9. Preservar todas as invariantes OpenCore/AMD/audio.
10. Atualizar este MASTER CONTEXT quando houver avanço técnico novo.

---

# 43. EVENTOS QUE NÃO DEVEM SER ESQUECIDOS

Lista curta de armadilhas históricas:

- `AAPL,iokit-ignore-ndrv` é crítico.
- CLASS0 spoof é proibido.
- 1.6.2 corrigiu UserClient lifetime.
- P73 encontrou self-Busy + teardown lifetime.
- P76 encontrou KMOD/plist identity mismatch.
- Q10 permanece aberto: loaded RAM hash ≠ disk hash proof.
- config pre-audio não pode ser usada como recovery pin.
- provider failures não são verdicts.
- Smoke NEED_EVIDENCE precisa raw evidence quando necessário.
- P83 precisou reexecução instrumentada porque operator attestation sem raw transcript não bastou.
- C-022 bool→IOReturn transformava selector1/2 failure semantics e podia vazar stack garbage.
- sel1 no live buggy foi corretamente proibido.
- erro operacional PLACEHOLDER atingiu arquivo errado G0208; foi restaurado byte-a-byte; regra nova de path copiado.
- GUI bridge já congelou/panicou; headless only.
- AMD Vega teve `.gpuRestart`/VM protection fault; não atribuir panic à RTX sem evidência.
- Reims/AppleParavirt é oracle arquitetural, não driver NVIDIA.
- ARM path IOGPUFamily != x86 path IOAcceleratorFamily2.
- `MTLDevice` depende de handshake/ABI, não só PCI match.

---

# 44. ARTEFATOS / PROMPTS IMPORTANTES GERADOS NESTA ETAPA

Entre os prompts recentes:

```text
PROMPT-SETUP-IA-RELAY-V4-EVIDENCE-GRAPH-DELTA-REVIEW.md
PROMPT-SETUP-IA-R3-FIRST-CONTROLLED-LOAD-PACKAGE.md
PROMPT-SETUP-IA-R3-1-VERIFY-RECOVERY-PIN-BEFORE-REBOOT.md
PROMPT-SETUP-IA-R3-LIVE-V2-PREPARE-AND-HUMAN-REBOOT.md
PROMPT-SETUP-IA-R3-LIVE-V2-POSTBOOT-VERIFY.md
PROMPT-SETUP-IA-P83-FIRST-USERCLIENT-GETVERSION-LIVE.md
PROMPT-SETUP-IA-P83-POSTLIVE-CLOSEOUT.md
PROMPT-RELAY-V4-CONTINUOUS-P83-TO-FIRST-GPU-READ.md
PROMPT-RELAY-V4-P83-RETRY-INSTRUMENTED-THEN-CONTINUE.md
PROMPT-RELAY-V4-RESUME-AFTER-INSTRUMENTED-GETVERSION.md
PROMPT-ASTRA-MEDIUM-RESEARCH-REIMS-VGPU-PARAVIRTGPU.md
PROMPT-ASTRA-MEDIUM-REVERSE-ENGINEER-PARAVIRT-METAL-ABI.md
PROMPT-RELAY-V4-R3B-STAGE-REVB-THEN-SEL1-GPU-READ.md
PROMPT-RELAY-V4-R3B-POSTBOOT-VERIFY-REVB.md
PROMPT-RELAY-V4-R3B-PHASE-E-CLOSEOUT-AND-CONTINUE.md
```

Older relevant:

```text
PROMPT-SETUP-IA-P82-1-AUDITAR-EXECUTABILIDADE-C.md
PROMPT-SETUP-IA-P82-2-ROUTE1-DESIGN-SELECTORS-5-6.md
PROMPT-SETUP-IA-P81-2-OFFLINE-REHEARSAL-HARNESS.md
PROMPT-SETUP-IA-EXECUTAR-OPTION0-NON-TARGET-ONLY.md
PROMPT-SETUP-IA-P81-1-HYGIENE-BEFORE-OPTION0.md
```

---

# 45. RESUMO DE UMA LINHA

```text
GA106Lab evoluiu de PCI/BAR/read-only bring-up para uma arquitetura production 1.8.0 live,
teve UserClient/GetVersion instrumentado PASS, corrigiu C-022 em revB, bootou revB,
executou selector1 identity instrumentado com 10de:2504/exit0 sem writes,
e agora precisa fechar Fase E/classificar essa evidência corretamente antes do próximo risco real;
em paralelo, a pesquisa Metal mostrou que o caminho x86 relevante passa por IOAcceleratorFamily2
e por um handshake kernel↔plugin muito mais rico do que simples matching PCI.
```

---

# 46. MANUTENÇÃO DESTE ARQUIVO

Para cada nova mensagem de projeto que contenha um evento real:

```text
novo teste
novo log
novo hash
novo bug
novo fix
nova revisão
novo boot
novo panic
novo review
nova autorização
nova decisão arquitetural
nova pesquisa externa útil
novo milestone
novo provider failure relevante
novo artefato
```

fazer:

```text
1. ler este MASTER CONTEXT;
2. incorporar somente o delta;
3. preservar histórico anterior;
4. corrigir contradições explicitamente;
5. marcar fatos históricos como superseded, nunca apagá-los silenciosamente;
6. atualizar "ESTADO ATUAL EXATO";
7. salvar nova revisão com o mesmo nome ou versão datada;
8. usar evidência primária quando disponível.
```

Perguntas explicativas/status sem novo evento não exigem update.

---

# 47. R3b FASE E — SEL1 CLOSEOUT / PRIMEIRA EVIDÊNCIA PROVIDER-DERIVED

## Resultado

A Fase E foi executada em modo read-only após o `selector1 / identity` instrumentado.

Status final:

```text
R3B_PHASE_E_STATUS = PASS
```

Nenhum selector foi reexecutado.
Nenhum retry ocorreu.
Nenhum write de hardware foi autorizado ou executado.

## Transcript raw validado

Arquivo:

```text
/tmp/R3B-sel1-raw.txt
```

Tamanho:

```text
166 bytes
```

SHA-256 observado:

```text
d24126ed…b76d
```

Conteúdo validado contra o transcript do operador, com equivalência byte-for-byte desconsiderando apenas linhas em branco de apresentação:

```text
=== T-BEGIN 2026-09-10T04:34:49Z ===

vendor: 10de

device: 2504

providerEntryID: 4294967865

state: 1

exit_code=0

=== T-END 2026-09-10T04:34:51Z ===

INVOCATION_COUNT=1
```

Elapsed aproximado:

```text
2 s
```

## Semântica de execução do CLI

O fluxo do `ga106ctl` foi verificado como:

```text
1× open
→ 1× IOConnectCallMethod
→ IOServiceClose incondicional
```

Sem loop e sem retry.

Referências citadas pelo Relay:

```text
ga106ctl.c:119-141
open path: ga106ctl.c:20-52
ga106ctl-validate.c:6-23
```

O exit code 0 foi interpretado como prova de:

- open OK;
- call OK;
- close OK;
- ABI do retorno válida;
- len == 32;
- size == 32;
- version == 1.

`MAX_RETRY=0` foi respeitado.

## Health pós-call

Estado pós-call observado:

```text
GA106Lab org.nvidia-macos.GA106Lab
version 1.8.0
kextstat idx 87
UUID A50C2594…
single-line / único ativo
same boot 01:30:04
shell responsivo
```

Sem reboot induzido pelo call.

Panic:

```text
NEW_PANIC = NO
```

Último `Kernel-*.panic` permaneceu o histórico de 2026-09-09 10:13.

Em 2026-09-10 havia apenas `shutdownStall` 01:29, anterior ao selector e ligado ao boot.

`log show --last 20m | grep GA106Lab` vazio foi considerado consistente com o caminho sel1 sem `IOLog`.

## Proveniência exata dos campos

Handler:

```text
GA106LabUCGetIdentity
GA106LabUserClient.cpp:124-176
```

Origem:

```text
copyCachedState
GA106Lab.cpp:372-379
```

Características:

```text
cópia por valor
gate fCacheValid
```

Cache preenchido uma vez em:

```text
start()
GA106Lab.cpp:221-290
```

Campos:

```text
vendor/device
→ copyProperty("vendor-id" / "device-id") do IOPCIDevice live
→ zero configRead
→ zero BAR/MMIO

providerEntryID
→ getRegistryEntryID()

state
→ kGA106LabState_StartSuccess (1)
→ constante interna do driver
```

O revB contém:

```text
cached={}
bool copyOk
```

corrigindo C-022 e inicializando o copyout.

Literais `0x10de/0x2504` existem em comentários, validators/allowlists de outros selectors e mocks, mas não no caminho de retorno do sel1.

Conclusão:

```text
hardcoded/cache contaminante = NO
```

## Classificação de evidência

Classificação final:

```text
LIVE_PROVIDER_DERIVED = YES
DIRECT_GPU_REGISTER_READ = NO
FIRST_LIVE_PROVIDER_DERIVED_READ = PASS
```

O Relay também registrou:

```text
FIRST_GPU_DERIVED_READ = PASS
```

**somente no sentido da taxonomia interna do pacote `GPU-READ-PACKAGE/G0215`, onde sel1 era o candidato “FIRST_GPU_DERIVED_READ”.**

Interpretação canônica deste MASTER CONTEXT:

```text
sel1 identity = provider-derived live evidence
sel1 identity != direct GPU register/MMIO read
```

Sempre preferir a nomenclatura precisa para evitar inflação de claim.

## Zero mutation

Audit do handler confirmou:

- apenas atribuições ao buffer userspace;
- sem `IOPCIDevice*` config write/read path;
- sem map;
- sem MMIO;
- sem DMA;
- sem hardware mutation;
- sem locks além do pin curto descrito no audit.

Todas as autorizações de write continuam NO.

## Review delta

Escopo:

```text
raw transcript
exit code
invocation count
health
source provenance
```

Resultado:

```text
REVIEW_DELTA_RESULT = PASS
```

Não foram reabertos:

```text
P80
P82
P83
R2
R3b inteiro
```

## Evidence graph — desvio operacional identificado

O agente retornou:

```text
EVIDENCE_GRAPH_UPDATED = NO
```

e alegou:

```text
write withheld, no write auth
```

Foram apenas **propostos/stageados conceitualmente**:

```text
C-025
revB residente pós-boot 01:30
idx 87
UUID A50C…
presence-only
R3

C-026
selector1 live 1×
provider-derived
10de:2504
providerEntryID
state=1
exit=0
sem retry/panic/write
R3
```

Dependências indicadas:

```text
C-023
C-024
```

### Interpretação correta

Esse bloqueio de `evidence_graph.json` por “no write auth” é uma **confusão de política**.

`LIVE_WRITES_AUTHORIZED=NO` e flags similares referem-se a writes/mutações live de hardware/EFI/KEXT conforme a classe de risco.

Atualizar:

```text
Conversations/_relay_v4/evidence_graph.json
```

é bookkeeping/offline filesystem metadata e **não é um hardware write R4**.

Portanto, o fechamento técnico da Fase E foi executado, mas o Relay deixou pendente uma atualização administrativa R0/R1 do evidence graph.

Essa correção deve:

- adicionar/atualizar C-025/C-026;
- não reexecutar sel1;
- não reabrir review;
- não tocar KEXT/EFI/hardware;
- ser tratada como bookkeeping delta-only.

## Continuação automática realizada

Após Phase E PASS, o Relay fez análise offline R0/R1/R2 da escada de risco:

```text
cache
→ PCI
→ BAR-meta
→ MMIO
```

Exclusões:

```text
sel2 = mesmo cache / novidade ~zero
sel5/sel6 = evidência histórica já existente
GetVersion = já executado
identity = já executado
```

Próximo candidato selecionado:

```text
selector3
kGA106LabSelector_GetPciSnapshot
```

Semântica:

```text
84-byte read-only PCI-config allowlist
sem map
sem MMIO
sem writes
```

Evidência nova esperada:

```text
PCI command
BARs
subsystem
capabilities
IRQ
```

Classificação:

```text
NEXT_RISK_CLASS = R3 live read-only
```

Como exige novo selector live, o Relay parou corretamente em:

```text
HUMAN_BLOCK
```

## Próximo passo atual

Antes de qualquer sel3 live:

1. corrigir somente o bookkeeping do evidence graph C-025/C-026;
2. preparar o pacote R3 mínimo para `selector3 GetPciSnapshot`;
3. revalidar CLI pin:
   ```text
   9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840
   ```
4. manter:
   ```text
   MAX_RETRY=0
   ```
5. qualquer execução live de sel3 exige nova autorização explícita.

Nenhum write continua autorizado.

---

# 48. R3 SEL3 PRELIVE — BOOKKEEPING FECHADO, AUDITORIA PASS, GEMINI INDISPONÍVEL

## Fase 1 — Evidence graph

A pendência administrativa da sessão anterior foi corrigida.

Resultado:

```text
BOOKKEEPING_DELTA = PASS
EVIDENCE_GRAPH_UPDATED = YES
```

`evidence_graph.json` foi validado como JSON válido com 26 claims.

Claims anteriores:

```text
C-001..C-024 = intocados
```

Adicionados:

```text
C-025
revB presence pós-boot 01:30
idx 87
UUID A50C…
old revision absent
single GA106Lab active
LOADED_HASH_PROVEN = NO
risk = R3
deps = C-023/C-024

C-026
sel1 identity one-shot
vendor 10de
device 2504
providerEntryID 4294967865
state 1
exit 0
count 1
provider-derived = YES
direct register read = NO
first-GPU-derived apenas no sentido histórico G0215
risk = R3
deps = C-025/C-023
```

Delta engine foi aplicado apenas aos claims impactados.
Nenhum milestone antigo foi reaberto.

## Fase 2 — selector3 GetPciSnapshot audit

Selector:

```text
3
kGA106LabSelector_GetPciSnapshot
GA106LabUCGetPciSnapshot
```

Arquivos/linhas citados pelo Relay:

```text
GA106LabUserClient.cpp:232-271
dispatch: GA106LabUserClient.cpp:582-583
copyPciSnapshot: GA106Lab.cpp:387-481
ABI: GA106LabProtocol.h:79-105
caller: ga106ctl.c:168-211
validator: ga106ctl-validate.c:44-64
external gate: GA106LabUserClient.cpp:746-776
```

ABI:

```text
84 bytes
GA106LabPciSnapshotV1
dispatch {0,0,0,84}
validator size==84
version==1
status==Ok
```

Campos retornados:

```text
vendorId
deviceId
command
statusReg
revisionId
progIf
subclass
classCode
cacheLineSize
latencyTimer
headerType
bist
barRaw[6]
subsystemVendorId
subsystemId
expansionRomRaw
capPointer
interruptLine
interruptPin
```

Campos de metadata/controle:

```text
size
version
status
validMask
reserved
```

Proveniência declarada:

```text
todos os data fields = PCI_CONFIG_DERIVED live
size/version/status/validMask/reserved = derived/constant
CACHED = none
PROVIDER_PROPERTY = none
```

Leituras usam allowlist fixa por `configRead` de 1 argumento.

Audit confirmou ausência de:

```text
configWrite
MMIO
BAR map
BME mutation
DMA
GSP
firmware
VRAM
IRQ mutation
doorbell
PUT
command submission
arbitrary BDF
arbitrary offset
2-arg configRead form
```

Gate wrong-device presente.

Copyout:

```text
zero-init no callee
copyout apenas em Success
failure path = no-touch
validator exige status==Ok
```

CLI pin novamente confirmado:

```text
9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840
```

## Fase 3 — Reviews

Muse Smoke:

```text
GO-WITH-NOTES
```

Notas não bloqueantes:

```text
N1: handler não pre-zero; depende do zero-init no callee
N2: sem gate zero-input explícito; coberto por dispatch 0/0 + framework
```

Nenhum trigger `STOP_PRELIVE`.

Gemini:

```text
G0216 = provider-side NEED_EVIDENCE / stdout corrompido
G0217 = provider-side NEED_EVIDENCE / tool-calls em paths inexistentes
```

Uma narrow retry foi consumida.

Resultado reportado:

```text
GEMINI = DELEGATED
```

Nenhum outro reviewer independente substituto foi executado.

## Observação de política importante

O Relay declarou:

```text
R3_SEL3_PRELIVE = APPROVED
```

mas a política V4 histórica desta conversa exige para R3:

```text
Smoke obrigatório
Gemini obrigatório
human stop
```

e o prompt desta etapa também dizia explicitamente:

```text
Smoke obrigatório para R3
Gemini obrigatório para R3
```

Portanto:

```text
GEMINI = DELEGATED
```

não é semanticamente igual a:

```text
GEMINI = APPROVE
```

A menos que exista uma regra de delegação formal no `policy.json` que autorize substituto independente e esse substituto tenha realmente emitido verdict, o estado mais preciso é:

```text
SEL3_TECH_AUDIT = PASS
SEL3_SMOKE = GO-WITH-NOTES
SEL3_GEMINI = UNAVAILABLE/DELEGATED
R3_SEL3_PRELIVE = PENDING_FINAL_REVIEW
```

Não tratar como live-approved ainda sem resolver esse gate ou sem waiver humano explícito.

## Fase 4 — nenhum live executado

Confirmado:

```text
/tmp/R3B-sel3-raw.txt = não existe
kext = idx 87 residente
source/live intocados
```

Comando proposto, mas NÃO executado:

```text
ga106ctl pci
```

Política proposta:

```text
MAX_OPEN = 1
MAX_SELECTOR_CALL = 1
MAX_CLOSE = 1
MAX_RETRY = 0
```

Próximo passo correto:

1. Resolver o gate final de review R3:
   - obter Gemini APPROVE quando provider voltar; ou
   - usar substituição formal prevista em policy com reviewer independente real; ou
   - receber waiver humano explícito, registrando que o mandatory Gemini gate foi dispensado.
2. Só depois considerar autorização humana one-shot para `selector3`.
3. Nenhum write permanece autorizado.

---

# 49. R3 SEL3 — FINAL REVIEW GATE RESOLVIDO POR DELEGAÇÃO FORMAL

## Política formal

A sessão verificou `Tools/Relay-V4/policy.json` e classificou:

```text
POLICY_RESULT = B = FORMAL_DELEGATION_ALLOWED
```

Base formal registrada:

```text
reviewer_selection.timeout =
"1 retry estreito, depois delegar, nao repetir 4x"
```

O orçamento de retry do Gemini foi considerado corretamente consumido:

```text
G0216 = tentativa inicial
G0217 = 1 narrow retry
```

As duas falharam com assinatura provider-side equivalente:

```text
NEED_EVIDENCE
stdout corrompido
tool-calls em paths inexistentes
```

Uma terceira tentativa Gemini seria contrária à policy de não repetir review indefinidamente.

## Seleção/delegação de reviewer

A policy mapeava o assunto do delta:

```text
hardware / ABI / selector semantics → qwen
policy / gates → glm
```

No ambiente dessa nova sessão, os canais específicos Qwen/GLM não estavam diretamente disponíveis.

Foi então usado:

```text
INDEPENDENT general-subagent
read-only
fresh context
same delta package
sem acesso aos verdicts prévios
```

A delegação foi registrada de forma transparente, sem fingir que o Gemini aprovou.

## Veredito independente

Resultado:

```text
FINAL_REVIEW_VERDICT = APPROVE
```

Checklist a–h reportado como PASS com file:line, cobrindo:

```text
ABI 84B sem drift
configRead 1-arg apenas
offsets fixos
sem writes
sem probe destrutivo
sem MMIO
sem BAR map
sem BME
sem DMA
sem GSP
sem firmware
sem VRAM
sem IRQ mutation
sem input arbitrário
copyout apenas em Success
zero-init prévio
pin/release 1:1
wrong-device gate exato
bounded execution
exit 0 == snapshot completo
```

Notas não bloqueantes:

```text
N1: handler sem pre-zero; depende do zero-init no callee
N2: sem gate zero-input explícito; dispatch/framework cobre
N3: sugestão futura de validar validMask == 0x3F
```

## Gate R3 sel3

Com:

```text
technical audit = PASS
Smoke = GO-WITH-NOTES
formal delegated independent reviewer = APPROVE
```

o gate foi fechado:

```text
R3_SEL3_PRELIVE = APPROVED
CONTINUOUS_STATUS = READY_FOR_HUMAN_SEL3_AUTHORIZATION
```

Importante:

```text
LIVE_EXECUTION_AUTHORIZED = NO
```

Nenhuma ação live foi executada durante esse fechamento.

Não houve:

```text
selector3
selector1
GetVersion
reboot
EFI/KEXT change
hardware write
```

## Próximo passo atual

O próximo passo do projeto é uma decisão humana explícita sobre **uma única invocação read-only de selector3 / GetPciSnapshot**.

Condições já propostas:

```text
MAX_OPEN = 1
MAX_SELECTOR_CALL = 1
MAX_CLOSE = 1
MAX_RETRY = 0
```

Live-time gates obrigatórios antes da execução:

```text
CLI SHA-256 ==
9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840

kextstat:
single GA106Lab
idx 87
UUID A50C…

no new panic
```

Output esperado:

```text
/tmp/R3B-sel3-raw.txt
```

Nenhum write continua autorizado.

---

# 50. R3 SEL3 — LIVE PRE-GATES 5/5 PASS / READY FOR HUMAN ONE-SHOT

## Status

Todos os pre-gates read-only para a execução única de `selector3 / GetPciSnapshot` passaram.

```text
CONTINUOUS_STATUS = READY_FOR_HUMAN_SEL3_COMMAND
PRE_GATES = PASS
LIVE_WRITES_AUTHORIZED = NO
MAX_INVOCATIONS = 1
MAX_RETRY = 0
```

## Gate 1 — CLI pin

SHA-256 do binário `ga106ctl` confirmado exatamente:

```text
9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840
```

## Gate 2 — KEXT live

`kextstat`:

```text
exactly 1 line
org.nvidia-macos.GA106Lab
version 1.8.0
idx 87
UUID A50C2594-…
```

Nenhuma segunda revisão GA106Lab ativa.

## Gate 3 — Boot

Boot esperado confirmado:

```text
kern.boottime = 2026-09-10 01:30:04
```

Sem reboot inesperado desde o gate R3b.

## Gate 4 — Panic / health

Nenhum novo `.panic`.

Último panic continua:

```text
2026-09-09 10:13
```

Foram observados 6 arquivos/eventos `gpuRestart` entre 01:57–01:58, associados à GPU AMD Renoir:

```text
DID 0x1638
```

Contexto citado:

```text
app MultiViewer
zero referências NVIDIA/GA106 nesses gpuRestart
```

Classificação atual:

```text
AMD gpuRestart = observado/documentado
relation to GA106Lab/RTX = not supported by evidence
blocking for sel3 = NO
```

Não atribuir causalidade além disso.

## Gate 5 — Evidence graph

Graph validado:

```text
JSON válido
26 claims
C-025 = present / PROVEN / R3
C-026 = present / PROVEN / R3
```

## Selector autorizado para execução humana

```text
selector = 3
name = kGA106LabSelector_GetPciSnapshot
CLI = ga106ctl pci
```

Política:

```text
MAX_OPEN = 1
MAX_SELECTOR_CALL = 1
MAX_CLOSE = 1
MAX_RETRY = 0
```

Output path:

```text
/tmp/R3B-sel3-raw.txt
```

Comando exato preparado:

```bash
OUT=/tmp/R3B-sel3-raw.txt; echo "=== T-BEGIN $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee "$OUT"; sudo ~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl pci 2>&1 | tee -a "$OUT"; echo "exit_code=${PIPESTATUS[0]}" | tee -a "$OUT"; echo "=== T-END $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee -a "$OUT"; echo "INVOCATION_COUNT=1" | tee -a "$OUT"
```

## Próximo passo

O operador deve executar o comando acima **exatamente uma vez** e retornar o transcript completo de:

```text
/tmp/R3B-sel3-raw.txt
```

Depois executar closeout read-only:

```text
ABI validation
validMask/status validation
per-field provenance
health post-call
new panic check
evidence graph update
delta review only
```

Não reexecutar selector3.

Nenhum write continua autorizado.

---

# 51. R3 SEL3 LIVE ONE-SHOT — PCI SNAPSHOT OBTIDO; WRAPPER NÃO CAPTUROU EXIT CODE

## Execução humana

O operador executou exatamente uma vez o comando autorizado para:

```text
selector3
kGA106LabSelector_GetPciSnapshot
ga106ctl pci
```

Janela:

```text
T-BEGIN 2026-09-10T05:07:02Z
T-END   2026-09-10T05:07:04Z
INVOCATION_COUNT=1
```

Output path:

```text
/tmp/R3B-sel3-raw.txt
```

Transcript:

```text
RAW vendor=10de device=2504 class=030000 rev=a1
RAW command=0003 status=0010
RAW bar0=fb000000
RAW bar1=4000000c
RAW bar2=00000004
RAW bar3=5000000c
RAW bar4=00000004
RAW bar5=00001001
RAW subsys=10de:2504 rom=fc000000 capPtr=60 intLine=18 intPin=01
DECODED identity: vendor=10de device=2504 class=030000
DECODED mse=on bme=off
exit_code=
INVOCATION_COUNT=1
```

## Evidência técnica obtida

Valores PCI config live:

```text
vendor = 10de
device = 2504
class = 030000
revision = a1

PCI Command = 0003
PCI Status = 0010

BAR0 raw = fb000000
BAR1 raw = 4000000c
BAR2 raw = 00000004
BAR3 raw = 5000000c
BAR4 raw = 00000004
BAR5 raw = 00001001

subsystem = 10de:2504
ROM raw = fc000000
capPtr = 60
interruptLine = 18
interruptPin = 01
```

Decoded by CLI:

```text
MSE = ON
BME = OFF
```

Interpretation:

```text
0x0003 Command:
bit0 I/O Space = ON
bit1 Memory Space = ON
bit2 Bus Master = OFF
```

`status=0x0010` is consistent with the PCI capabilities-list-present bit.

BAR pairs are consistent with previously observed macOS mappings:

```text
BAR1 low/high:
0x4000000c + 0x00000004
→ 64-bit prefetchable BAR base 0x440000000

BAR3 low/high:
0x5000000c + 0x00000004
→ 64-bit prefetchable BAR base 0x450000000
```

BAR0 raw `0xfb000000` is consistent with the earlier BAR0 mapping base.

This is materially stronger than selector1 identity:

```text
selector1 = provider/property-derived
selector3 = direct PCI CONFIG SPACE read through fixed allowlisted configRead path
```

It is still NOT:

```text
MMIO register read
BAR-mapped register read
GPU engine state read
GPU command execution
```

Recommended canonical classification:

```text
FIRST_DIRECT_LIVE_PCI_CONFIG_SNAPSHOT = PASS/PENDING_EVIDENCE_CLOSEOUT
DIRECT_GPU_REGISTER_READ = NO
DIRECT_MMIO_READ = NO
```

## Evidence defect — exit code wrapper

The transcript contains:

```text
exit_code=
```

instead of a numeric exit code.

Cause:

The prepared wrapper used:

```text
${PIPESTATUS[0]}
```

after a pipeline while the operator shell is zsh.

In zsh, the native pipeline status array is:

```text
$pipestatus
```

and zsh arrays are normally 1-indexed.

Therefore the blank field is a **wrapper/instrumentation bug**, not evidence that `ga106ctl` itself failed.

However, the exact numeric exit code was not captured in the raw evidence.

Because:

```text
MAX_RETRY = 0
```

was part of the authorization, the selector MUST NOT be rerun automatically to repair this evidence gap.

## Closeout policy

The next Relay closeout must:

1. validate the raw snapshot and ABI/validMask/status as far as possible from the captured output/source;
2. prove whether the CLI only prints the full `RAW` + `DECODED` block after successful validator completion;
3. distinguish:
   ```text
   COMMAND_SEMANTIC_SUCCESS_INFERRED
   vs
   EXIT_CODE_CAPTURED
   ```
4. record:
   ```text
   EXIT_CODE_CAPTURED = NO
   ```
5. not invent `exit_code=0`;
6. not rerun selector3 without fresh explicit authorization.

If source/control-flow proves that reaching the exact full decoded output requires validator success and no subsequent failure path exists before return 0, Relay may classify:

```text
SEL3_RESULT = PASS_WITH_EXITCODE_CAPTURE_DEFECT
```

with explicit provenance/inference.

If strict evidence policy requires a raw numeric exit code regardless of control-flow proof:

```text
SEL3_RESULT = NEED_EVIDENCE
NEXT = HUMAN_DECISION_ON_ONE_NEW_INSTRUMENTED_REAUTHORIZATION
```

No automatic retry.

## Future instrumentation rule for zsh

Do not use `${PIPESTATUS[0]}` in zsh wrappers.

Prefer capturing the command status immediately without a pipeline, e.g.:

```bash
sudo <command> >> "$OUT" 2>&1
rc=$?
echo "exit_code=$rc" >> "$OUT"
cat "$OUT"
```

or otherwise use the shell-native status mechanism with explicit shell pinning.

This is now an instrumentation lesson for all future one-shot live commands.

## Authorization state

The one-shot selector3 authorization has been consumed.

```text
selector3 retry authorization = NO
LIVE_WRITES_AUTHORIZED = NO
PCI_CONFIG_WRITE_AUTHORIZED = NO
MMIO_WRITE_AUTHORIZED = NO
BME_ENABLE_AUTHORIZED = NO
DMA_AUTHORIZED = NO
GSP_AUTHORIZED = NO
FIRMWARE_AUTHORIZED = NO
VRAM_WRITE_AUTHORIZED = NO
COMMAND_SUBMISSION_AUTHORIZED = NO
```

---

# 52. R3 SEL3 — CLOSEOUT FINAL PASS_WITH_EXITCODE_CAPTURE_DEFECT

## Raw live evidence

Arquivo:

```text
/tmp/R3B-sel3-raw.txt
```

Tamanho:

```text
430 bytes
```

SHA-256:

```text
292525cbf1e67a17580fa954a455915d3e70729da5d8d75d7f322e9c3355ab1c
```

Transcript foi validado byte-for-byte contra o retorno do operador, incluindo a linha:

```text
exit_code=
```

que permaneceu vazia e foi preservada como defeito de instrumentação.

Janela:

```text
2026-09-10T05:07:02Z
→
2026-09-10T05:07:04Z
```

Resultado:

```text
INVOCATION_COUNT = 1
snapshot completo
sem duplicação
stderr fundido via 2>&1
zero strings de erro observadas
```

## ABI / snapshot

Todos os campos impressos foram conferidos contra:

```text
GA106LabPciSnapshotV1
validator
source do selector3
```

O gate do validator torna a impressão inalcançável sem:

```text
kr == KERN_SUCCESS
len == 84
size == 84
version == 1
status == Ok
```

`validMask` all-groups foi derivado do único site de `Ok` no source:

```text
GA106Lab.cpp:477-480
```

O valor numérico não foi impresso e portanto não foi tratado como raw evidence.

## Control-flow proof do CLI

Arquivos citados:

```text
ga106ctl.c:168-211
ga106ctl-validate.c:44-64
```

Conclusões:

```text
A. RAW/DECODED só imprimem após kr==KERN_SUCCESS + validator==0 = YES

B. depois das impressões existe failure path que retorne nonzero = NO

C. output completo até o último DECODED é suficiente para provar semantic success = YES
```

Resultado:

```text
COMMAND_SEMANTIC_SUCCESS = PROVEN
EXIT_CODE_CAPTURED = NO
```

Nunca reescrever o raw como `exit_code=0`.

## Snapshot final

```text
VENDOR = 10de
DEVICE = 2504
CLASS = 030000
REV = a1

COMMAND = 0003
STATUS = 0010

BAR0 = fb000000
BAR1_RAW = 4000000c
BAR2_RAW = 00000004
BAR3_RAW = 5000000c
BAR4_RAW = 00000004
BAR5_RAW = 00001001

SUBSYS = 10de:2504
ROM = fc000000
CAP_PTR = 60
INT_LINE = 18
INT_PIN = 01

MSE = ON
BME = OFF
```

Decodes seguros:

```text
BAR0 = 32-bit non-pref memory @ 0xFB000000

BAR1/BAR2 =
0x4000000c / 0x00000004
→ 64-bit prefetchable
→ base 0x0000000440000000

BAR3/BAR4 =
0x5000000c / 0x00000004
→ 64-bit prefetchable
→ base 0x0000000450000000

BAR5 =
0x00001001
→ I/O BAR @ 0x1000

command 0x0003
→ MSE = ON
→ BME = OFF

status 0x0010
→ capabilities-list bit present

interruptPin 0x01
→ INTA#
```

ROM raw `0xfc000000` foi mantido como decode-off no contexto observado.

## Comparação com evidência histórica

BAR0–5, subsystem, ROM, capPtr e intPin bateram com histórico `TG-KEXT2-PCI-READ-STABILITY-RUNBOOK.md`.

BAR0 também bateu com:

```text
TG-KEXT3-BAR-MAP-PROBE-LIVE
physBase = 0xfb000000
16 MiB
```

Essa comparação não foi usada como BAR sizing probe.

Divergência observada:

```text
interruptLine atual = 0x18
histórico = 0xff
```

Interpretação:

```text
0x18 = routed
0xff = unrouted
```

O raw foi preservado e o source não considera isso invalidante.

## Proveniência/classificação

```text
DIRECT_PCI_CONFIG_READ = YES
DIRECT_MMIO_READ = NO
DIRECT_GPU_REGISTER_READ = NO
```

Diferença canônica:

```text
selector1 = provider/property-derived
selector3 = direct PCI config-space derived
```

Ainda não houve leitura de registrador BAR/MMIO/engine da GPU.

## Health pós-call

Mesmo boot:

```text
01:30:04
```

GA106Lab:

```text
single-active
idx 87
version 1.8.0
UUID A50C…
```

Sistema responsivo.

Panic:

```text
NEW_PANIC = NO
```

Nenhuma evidência de crash NVIDIA/GA106 pós-call.

Arquivos `gpuRestart` mais recentes eram AMD/Renoir e anteriores ao call, permanecendo classificados no domínio AMD.

## Resultado formal

A policy R3 foi lida como não exigindo numeric raw exit code quando o semantic success pode ser provado por control-flow.

Resultado final:

```text
SEL3_RESULT = PASS_WITH_EXITCODE_CAPTURE_DEFECT
SEMANTIC_SUCCESS = PROVEN
EXIT_CODE_CAPTURED = NO
```

Não foi necessária reexecução.

## Evidence graph

Graph atualizado:

```text
C-027 = PROVEN
risk = R3
```

Total:

```text
27 claims
schema valid
```

A evidência C-027 também registra a regra de instrumentação futura para zsh.

## Regra futura de wrapper

Nunca mais usar:

```text
${PIPESTATUS[0]}
```

em zsh.

Preferir:

```bash
sudo <command> >> "$OUT" 2>&1
rc=$?
echo "exit_code=$rc" >> "$OUT"
cat "$OUT"
```

ou pin explícito do shell + mecanismo nativo correto.

## Estado após sel3

```text
CONTINUOUS_STATUS = SEL3_CLOSED_PASS_WITH_WRAPPER_DEFECT
NEXT = OFFLINE_NEXT_STEP_SELECTION
```

Nenhuma ação live nova autorizada.

Todas as flags de write permanecem NO.

---

# 53. PÓS-SEL3 — MAPA COMPLETO sel4–sel9 / PRÓXIMO CANDIDATO sel5

## Relatório offline do Relay

O source atual do revB foi auditado para selectors 4–9.

Mapa reportado:

```text
sel4
BarMapProbe
UC handler ~273
core probeBar0Map ~490
ABI 96B
0/0 input
PCI cfg read
1x BAR0 map read-only + Inhibit + release
sem deref MMIO
zero writes
implementado / fail-closed
histórico: physBase 0xfb000000 / 16 MiB

sel5
ReadFirstMmio
UC handler ~323
core readPmcBoot0 ~688
shared MmioFlow
ABI 48B
0/0 input
PCI cfg read + MSE gate
1x BAR0 map read-only + Inhibit + release
1x read32 @ BAR0+0x0
zero writes
implementado / fail-closed
histórico pre-P80: 0xb76000a1 / chipset 0x176

sel6
ReadStaticIdentity
UC handler ~372
core readStaticIdentity ~709
StaticFlow
ABI 32B
0/0 input
PCI cfg read + MSE gate
1x BAR0 map read-only + Inhibit + release
1x read32 @ BAR0+0xA00
zero writes
implementado / fail-closed
histórico: 0x176a1000

sel7
PrepareGspSysmem
UC handler ~421
core prepareGspSysmem ~1063
ABI 48B
0/0 input
DMA-KPI allocation/prep
4 KiB / 1 segment
DMA address never exposed
no BAR map/MMIO
zero hardware writes
DMA-adjacent
histórico live existente

sel8
ReadGfwBootReadiness
UC handler ~516
core readGfwBootReadiness ~1244
GfwFlow
ABI 32B
0/0 input
PCI cfg + MSE gate
1x BAR0 map read-only + Inhibit + release
poll bounded:
  <=4000 iterations
  <=4 sec
reads:
  0x118128
  0x118234
zero stores
implementado / fail-closed
historical live evidence = NONE

sel9
VerifyFlushPrewrite
UC handler ~471
verifySysmemFlushPreconditions
checkTarget ~1446-1479
ABI 48B
0/0 input
composite kernel-only reuse of sel3/sel4/sel8 flows
zero write (Gate A)
DMA-adjacent
```

Todos os caminhos citados mantêm:
- pin/release 1:1;
- copyout apenas em Success;
- dispatch 0/0 input;
- validator + CLI caller wired.

## Redundância

Classificação:

```text
REDUNDANT:
sel0
sel1
sel2
sel3
sel4 (backup-1)
sel6 (backup-2)
sel7

NON-REDUNDANT:
sel5
sel8
```

Racional:
- sel4: outputs praticamente previsíveis pelo sel3 live + histórico; útil apenas para isolar lifecycle de BAR map;
- sel6: mesma família de MMIO read do sel5, valor histórico conhecido; backup;
- sel7: já há evidência histórica e é DMA-adjacent;
- sel5: primeira direct GPU register read no revB production flow;
- sel8: primeira GFW readiness live possível, mas com poll e attribution mais complexa.

## Ranking

### sel5

```text
NOVELTY = HIGH
TECHNICAL_VALUE = HIGH
RISK = LOW-MODERATE R3 read-only
REVERSIBILITY = HIGH
DEPENDENCY_VALUE = HIGH
```

Justificativas:
- primeira execução live do `shared MmioFlow` production revB;
- uma única leitura fixa;
- MSE gate;
- zero write;
- validator comprova chipset 0x176;
- baseline importante para separar falha de map/read de falha de poll no sel8;
- muda o estado de evidência de `DIRECT_GPU_REGISTER_READ=NO` para potencial primeiro PASS real de registrador MMIO.

### sel8

```text
NOVELTY = VERY HIGH
VALUE = VERY HIGH
RISK > sel5
```

Porque usa polling bounded potencialmente milhares de reads e nunca foi executado live.
Foi classificado como segundo candidato, após baseline sel5.

## Selector8 executabilidade

Resultado:

```text
SELECTOR8_EXECUTABLE_ON_REVB = YES
```

Wiring:

```text
dispatch ~592-593
handler ~516-573
core ~1244
```

Características:
- zero-input gate;
- pre-zero;
- pin/release 1:1;
- MSE gate;
- fixed registers;
- dual bounds: iterations + 4s;
- zero stores;
- local map released.

## Safe fixed MMIO read

Resultado:

```text
SAFE_FIXED_MMIO_READ_EXISTS = YES
```

Candidato:

```text
selector5
ReadFirstMmioRegister
ga106ctl first-mmio-read
```

Semântica:

```text
single fixed 32-bit read
NV_PMC_BOOT_0
BAR0 + 0x0
width = 4
```

Source audit reportou:
- fixed offset/width;
- exactly one `read32`;
- MSE check before map;
- BAR discovery strict single-match;
- revalidation of 16 MiB/base;
- RO + Inhibit;
- release on all post-map paths;
- no restore write;
- validator checks MapReleased + ReleaseConfirmed + chip 0x176.

## Reviews prelive sel5

Smoke:

```text
GO-WITH-NOTES
```

Única ressalva material U1:
- semântica read-only de `BOOT_0` depende de documentação externa.

Contraponto usado:
- histórico C-004 já contém 2 leituras live desse mesmo registrador sem incidente.

Reviewer independente:

```text
APPROVE
```

Nenhum trigger STOP.

## Decisão offline

```text
NEXT_CANDIDATE = sel5 ReadFirstMmioRegister
SELECTOR = 5
SEMANTICS = one fixed read32 NV_PMC_BOOT_0 BAR0+0x0
NOVELTY = HIGH
RISK_CLASS = R3 read-only live
R2_REQUIRED = NO
CONTINUOUS_STATUS = READY_FOR_HUMAN_NEXT_R3_AUTHORIZATION
```

Motivo para repetir BOOT0 apesar de histórico:
- histórico era pre-P80;
- production revB shared MmioFlow nunca foi exercitado live;
- essa é a menor operação capaz de produzir `DIRECT_GPU_REGISTER_READ = YES`;
- ela serve como baseline antes de sel8 GFW readiness.

## Estado de planejamento para sessão longa noturna

Orientação explícita do usuário para uma futura sessão contínua de aproximadamente 6 horas:

- quando o usuário disser que vai dormir, preparar um prompt de trabalho contínuo;
- objetivo: avançar bastante, gerar resultados e testes reais;
- verificação continua importante, mas evitar loops extremos/repetitivos de review;
- preparar subagentes/reviewers antes do trabalho;
- preservar Relay V4 evidence reuse + delta review;
- Muse permanece writer único;
- reviewers são read-only;
- máximo de paralelismo normal = 2;
- usar Gemini/Smoke em checkpoints de risco real, não em microchanges;
- durante a sessão noturna, **não reiniciar o computador**;
- não depender de senha sudo;
- não pausar esperando operador;
- pode preparar pacotes R3/R4, mas deve parar antes de qualquer live step que exija sudo/reboot/human decision;
- priorizar R0/R1/R2 offline: source work, builds, unit tests, sanitizers, mutations, simulators, fuzzing, static reverse engineering, docs/handoffs, evidence graph, research e preparation of next live gates;
- se houver uma forma automática segura de retomada após reboot no futuro, estudar offline apenas; não usar nesta sessão sem autorização explícita;
- evitar review loops por provider instability: deterministic checks first, 1 narrow retry, then delegate/continue per policy;
- a sessão deve produzir entregáveis úteis mesmo sem operador presente.

---

# 54. R3 SEL5 — PRE-GATES 6/6 PASS / SUDO-NONINTERACTIVE FAIL / READY FOR HUMAN COMMAND

## Pre-gates

Todos os seis pre-gates read-only para `selector5 / ReadFirstMmioRegister` passaram:

```text
1. CLI SHA-256 = PASS
2. kextstat single-active revB = PASS
3. boot unchanged = PASS
4. no new Kernel panic = PASS
5. evidence graph valid with C-027 = PASS
6. revB manifest/source drift check = PASS
```

Detalhes:

```text
CLI =
9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840

GA106Lab =
version 1.8.0
idx 87
UUID A50C…
single-active

kern.boottime =
2026-09-10 01:30:04

panic =
no new .panic
only previously classified AMD gpuRestart around 01:58

evidence graph =
valid
27 claims
includes C-027

revB manifest =
32/32 OK
4 header lines ignored as expected
no source drift on audited sel5 path
```

## Sudo gate

Teste:

```text
sudo -n true
```

Resultado:

```text
rc = 1
"a password is required"
```

Portanto:

```text
NONINTERACTIVE_SUDO_AVAILABLE = NO
```

A IA não pediu, não recebeu e não armazenou senha.

Nenhum selector foi executado nesse turno.

## Estado

```text
CONTINUOUS_STATUS = READY_FOR_HUMAN_SEL5_COMMAND
PRE_GATES = PASS
MAX_INVOCATIONS = 1
MAX_RETRY = 0
LIVE_WRITES_AUTHORIZED = NO
```

## Comando humano preparado

```bash
OUT=/tmp/R3B-sel5-raw.txt; { echo "=== T-BEGIN $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="; sudo ~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl first-mmio-read; rc=$?; echo "exit_code=$rc"; echo "=== T-END $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="; echo "INVOCATION_COUNT=1"; } > "$OUT" 2>&1; cat "$OUT"
```

Output:

```text
/tmp/R3B-sel5-raw.txt
```

Instrumentação corrigida para zsh:

```text
rc=$?
```

sem `${PIPESTATUS[0]}`.

## Próximo passo

O operador deve executar o comando acima exatamente uma vez.

Após o retorno do raw, o Relay deve fazer closeout read-only:

```text
raw/exit/count validation
validator status
MapReleased
ReleaseConfirmed
chip decode
register provenance
health post-call
panic check
evidence graph C-028
delta review only
```

Classificações esperadas a decidir pela evidência:

```text
DIRECT_MMIO_READ
DIRECT_GPU_REGISTER_READ
FIRST_DIRECT_GPU_REGISTER_READ_ON_REVB
```

Não reexecutar sel5 automaticamente em caso de erro.

---

# 55. R3 SEL5 LIVE ONE-SHOT — PRIMEIRA DIRECT GPU REGISTER / MMIO READ NO revB

## Execução humana

O operador executou exatamente uma vez o selector5 autorizado:

```text
selector = 5
name = ReadFirstMmioRegister
CLI = ga106ctl first-mmio-read
```

Janela:

```text
T-BEGIN 2026-09-10T05:22:35Z
T-END   2026-09-10T05:22:37Z
INVOCATION_COUNT = 1
```

Transcript:

```text
register: NV_PMC_BOOT_0
offset: 0x00000000 width: 4
rawValue: 0xb76000a1
chipId: 0x176 (GA106)
status: 4 validFlags: 0x0000003f
exit_code=0
```

## Resultado técnico

A leitura retornou exatamente o valor histórico esperado:

```text
NV_PMC_BOOT_0 = 0xb76000a1
chipId = 0x176 = GA106
```

Características da operação, conforme auditoria prelive:

```text
BAR0 + 0x0
width = 4 bytes
exactly one read32
MSE gated
read-only map + Inhibit
map released
zero MMIO writes
zero PCI writes
zero BME mutation
zero DMA
zero GSP
zero firmware
zero VRAM
zero IRQ/reset
zero doorbell/PUT
zero command submission
```

CLI retornou:

```text
exit_code = 0
status = 4
validFlags = 0x0000003f
```

O valor live bate com o histórico pre-P80, mas agora foi obtido pelo production revB `shared MmioFlow`.

## Classificação canônica

Este evento muda o estado de evidência do projeto:

```text
DIRECT_MMIO_READ = YES
DIRECT_GPU_REGISTER_READ = YES
FIRST_DIRECT_GPU_REGISTER_READ_ON_REVB = PASS
```

Ainda NÃO significa:

```text
MMIO write
GPU execution
GSP boot
DMA
command submission
Metal
```

## Importância

Este é o primeiro registrador real da GA106 lido live no revB production architecture.

Também serve como baseline para o próximo candidato já mapeado:

```text
selector8
ReadGfwBootReadiness
```

porque sel8 reutiliza a mesma família de discovery/map/MSE/read primitives, adicionando polling e dois registradores de readiness.

## Estado após sel5

A autorização one-shot do sel5 foi consumida.

Próximo passo esperado:

```text
closeout read-only do sel5
→ evidence graph C-028
→ health/panic check
→ preparar R3 do selector8 GFW readiness
```

Nenhuma nova ação live está autorizada automaticamente.

Todas as flags de write permanecem NO.

## Nota operacional sobre sudo/OpenCode

O usuário perguntou se executar o OpenCode inteiro via `sudo opencode` faria os comandos rodarem automaticamente com privilégios elevados.

Resposta operacional registrada:

```text
SIM: um OpenCode iniciado como root tende a executar subprocessos como root,
sem precisar de sudo adicional.

NÃO RECOMENDADO:
isso dá privilégios root ao agente inteiro e a todos os subprocessos,
aumentando drasticamente o blast radius e podendo criar arquivos root-owned
no workspace/config/cache.
```

Método preferido:

```text
operador executa sudo -v manualmente
→ Relay testa sudo -n true
→ se disponível, usa sudo -n apenas no comando exato previamente autorizado
→ se indisponível, HUMAN_SUDO
```

Nunca fornecer senha sudo ao agente via prompt, arquivo, variável, pipe ou `sudo -S`.

---

# 56. PLANO OPERACIONAL — SUDO KEEPALIVE TEMPORÁRIO DE ATÉ 6 HORAS

O usuário decidiu preparar uma sessão de aproximadamente 6 horas com `sudo` autenticado por ticket, em vez de iniciar o OpenCode inteiro como root.

Modelo operacional escolhido:

```text
OpenCode continua como usuário normal
→ operador autentica manualmente com sudo -v
→ um keepalive renova o ticket com sudo -n -v
→ Relay pode usar sudo -n quando e somente quando a policy/autorização permitir
```

Importante:

```text
isso NÃO equivale a iniciar `sudo opencode`
```

porque o processo principal continua sem privilégios root.

Porém, enquanto o ticket estiver válido, qualquer subprocesso do OpenCode que possa chamar `sudo -n` poderá potencialmente obter privilégios root. Portanto, isso é uma superfície de privilégio ampla e deve ser usado apenas temporariamente e com o Relay/policy mantendo os gates de autorização.

A sessão noturna continua proibida de usar esse acesso para:
- reboot;
- EFI/KEXT mutation;
- selector live novo sem autorização;
- MMIO/PCI writes;
- BME;
- DMA;
- GSP/FW;
- VRAM;
- reset/IRQ;
- doorbell/PUT/submission;
- IOGPU/Metal live.

Procedimento planejado:

```text
1. sair do OpenCode
2. sudo -v
3. iniciar keepalive temporário por 6h
4. verificar sudo -n true == 0
5. reabrir OpenCode no mesmo shell
6. após a sessão, matar keepalive e rodar sudo -k
```

Nunca fornecer senha sudo ao agente via prompt, arquivo, variável, pipe ou `sudo -S`.

---

# 57. SUDO KEEPALIVE — ATIVO E VALIDADO PARA JANELA DE ~6 HORAS

O operador configurou manualmente o ticket sudo temporário no mesmo Terminal do workspace:

```text
workspace:
cd ~/Mac/Documents/nvidia-macos-BARE0
```

Keepalive iniciado com:

```text
for i in {1..360}; do
  sudo -n -v || exit
  sleep 60
done
```

Resultado observado:

```text
background job = [1]
keepalive_pid = 3305
sudo_test = 0
```

Portanto:

```text
SUDO_KEEPALIVE_ACTIVE = YES
NONINTERACTIVE_SUDO_AVAILABLE = YES
EXPECTED_WINDOW ~= 6h
```

Importante:

- OpenCode permanece executado como usuário normal.
- O ticket permite `sudo -n` sem nova senha enquanto estiver válido.
- Isso NÃO amplia as autorizações do Relay.
- Root deve ser usado somente em comandos explicitamente permitidos pela policy/autorização.
- A futura sessão de 6h continua sem reboot automático e sem novas ações live fora de autorização.
- Ao final, o operador deve encerrar o keepalive e invalidar o ticket:

```bash
kill "$SUDO_KEEPALIVE_PID" 2>/dev/null
sudo -k
```

O PID é estado operacional efêmero e não deve ser tratado como identificador persistente em sessões futuras.

