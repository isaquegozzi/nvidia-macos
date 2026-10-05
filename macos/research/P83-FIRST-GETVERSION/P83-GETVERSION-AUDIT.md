# P83 — Auditoria GetVersion (selector 0, owner-free)

```text
SESSION_ID: V4-P83-GETVERSION / MILESTONE: P83_FIRST_USERCLIENT_GETVERSION_LIVE
STATE: MUSE_WORK / ACTOR: MUSE_BUILD / LIVE_EXECUTION_AUTHORIZED = NO
TREE: GA106Lab-kext8-production-architecture-src/ (1.8.0, pinado; delta V4 = 0 claims impactados)
KEXT LIVE: 1.8.0 idx 93 (único ativo, por referência externa) / HASHES POR REFERÊNCIA EXTERNA (rebaixado F1: valores completos vivem fora do pacote — `P80-FINAL-HANDOFF.md:15-19` — KEXT b6d4ea31…, plist 2d0d3cf9…, CLI (FULL gate-close F2) 9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840, build.sh 43f7ade2…, manifest 8cb5be2… — fonte: G0203 §4 + reconf. read-only 2026-09-09; binding source↔hash em `MANIFEST-P80-1.8.0.txt:1-5`; recomputo offline de prefixos em `Conversations/_relay_v3/subagents/P83-QWEN-ABI-01_qwen.md:34`; caller instanciado (F1): `~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl`; binário-em-RAM segue Q10 BLOCKED, Q-P83-01)
```

Conferido contra o source pinado (não re-derivado do zero). Todas as refs são `file:line` na árvore acima.

## 14 campos

| # | Campo | Valor | Evidência (`file:line`) |
|---|---|---|---|
| 01 | SELECTOR | `0` = `kGA106LabSelector_GetProtocolVersion` | `GA106LabProtocol.h:23` |
| 02 | HANDLER | `GA106LabUCGetVersion(OSObject*, void*, IOExternalMethodArguments*)` | `GA106LabUserClient.cpp:108` |
| 03 | DISPATCH_ENTRY | `{0,0,2,0}` = scalar-in 0, struct-in 0, scalar-out 2, struct-out 0 | `GA106LabUserClient.cpp:576` |
| 04 | INPUT_SCALAR | `0` (nenhum scalar lido; `scalarInputCount` deve ser 0 via dispatch) | `GA106LabUserClient.cpp:577` |
| 05 | INPUT_STRUCT | `0` (nenhum byte de input; handler nunca toca `structureInput`) | `GA106LabUserClient.cpp:108` |
| 06 | OUTPUT_SCALAR | `2` — `scalarOutput[0]=GA106LAB_UC_PROTOCOL_VERSION (1)`, `scalarOutput[1]=GA106LAB_DRIVER_VERSION_U32 (0x00010800)` | `GA106LabUserClient.cpp:119`, `GA106LabUserClient.cpp:120`, `GA106LabProtocol.h:19`, `GA106LabProtocol.h:20` |
| 07 | OUTPUT_STRUCT | `0` (nenhum struct out; handler nunca toca `structureOutput`) | `GA106LabUserClient.cpp:108` |
| 08 | REQUIRES_OWNER | `NO` — único handler sem `PinOwnerForExternalMethod`; sem `getOwnerService`, sem `copyCachedState`. NORMATIVA (F4, fail-closed): `REQUIRES_OWNER=NO` diz que o corpo do handler não lê nem pina owner, mas o `externalMethod` só despacha com `fOwner != NULL` (`GA106LabUserClient.cpp:758`) — sem owner publicado, GetVersion falha com `kIOReturnNotOpen` sem tocar hardware | `GA106LabUserClient.cpp:108`, exc. auditada em `build.sh:775`, `build.sh:787` |
| 09 | BAR_MMIO_PCI | `NO`/`NO`/`NO` — sem `IOPCIDevice*`, sem map/config, sem deref MMIO | `GA106LabUserClient.cpp:108` (corpo só escreve 2 scalars) |
| 10 | WRITES_HW | `NO` — zero escritas; só atribuição a `args->scalarOutput[0..1]` | `GA106LabUserClient.cpp:119`, `GA106LabUserClient.cpp:120` |
| 11 | CAN_SLEEP | `NO` — sem `IOSleep`/`IODelay`/wait; retorno direto `kIOReturnSuccess` | `GA106LabUserClient.cpp:108`, `GA106LabUserClient.cpp:121` |
| 12 | LOCKS | `none` — sem `IOSimpleLockLock`, sem `fOwnerLock`/`fSysmemLock` no caminho | `GA106LabUserClient.cpp:108` |
| 13 | COPYOUT | `2×u64` — guarda `scalarOutputCount < 2 → kIOReturnBadArgument` (fail-closed); `>=16B`, primeiros 2 u64 válidos (claim relaxado F4: sem write-back `scalarOutputCount = 2` — única ocorrência de `scalarOutputCount` no arquivo é a leitura em `GA106LabUserClient.cpp:116`; cauda além de 16B indefinida se caller passar count > 2; caller pinado passa exatamente 2 — `ga106ctl.c:56-57`) | `GA106LabUserClient.cpp:116`, `GA106LabUserClient.cpp:119`, `GA106LabUserClient.cpp:120` |
| 14 | LIFETIME | GetVersion INTENCIONALMENTE INVISÍVEL ao drain (nota F3, sem mudar código nem semântica owner-free): único handler sem `PinOwnerForExternalMethod`, logo sem incremento de `fActiveCalls` (`init` zera em `GA106LabUserClient.cpp:609`, `start` zera em `:652`, `free` só asserta `fActiveCalls != 0` em `:624`); `stop` drena só calls pinadas (`BeginUserClientStopAndDrainFlow`, gate `build.sh:770-773`). JUSTIFICATIVA: corpo `108-122` toca só `target`+`args` (2 atribuições scalar), sem `fOwner`/`fOwnerLock`/locks/MMIO/sleep — teardown concorrente não tem estado compartilhado para corromper neste caminho; única proteção in-flight é o gate rápido `fOpen && !fClosing && fOwner != NULL` em `GA106LabUserClient::externalMethod` (`GA106LabUserClient.cpp:758`). "Trap retém o UC" segue ASSUNÇÃO NÃO PROVADA (sem file:line; U2 retida). `close` nunca libera (só marca closing); `stop` release 1x. DECISÃO GATE-CLOSE (F3, 2026-09-09): risco ACEITO como residual — corpo `108-122` não toca estado compartilhado (só `target`+`args`), logo teardown concorrente nada tem a corromper neste caminho; pin leve futuro NÃO exigido, sem ambiguidade. P80 intocado (nota vive só em P83 por proibição de escrita em `P80-*`) | `GA106LabUserClient.cpp:108-122`, `:758`, `:624`, `build.sh:770-787` |

## Corpo auditado (integral, 15 linhas)

```text
GA106LabUserClient.cpp:108-122: OSDynamicCast + NULL-check → scalarOutputCount<2-check →
scalarOutput[0]=1 → scalarOutput[1]=0x00010800 → kIOReturnSuccess.
```

## Resposta esperada (PASS iff exato)

```text
scalar[0] = 1 (GA106LAB_UC_PROTOCOL_VERSION) / scalar[1] = 0x00010800 (1.8.0) / kr = kIOReturnSuccess (0)
```

```text
live = 0 / source intocado / nada executado / veredito pendente (REVIEW P83 GETVERSION)
```
