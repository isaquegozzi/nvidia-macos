# GPU-READ — Mapa dos 10 selectors 1.8.0 (offline, read-only)

```text
SESSION_ID: V4-GPU-READ-PACKAGE / MILESTONE: GPU_DERIVED_READ_PACKAGE
STATE: MUSE_WORK / ACTOR: MUSE_BUILD / LIVE_EXECUTION_AUTHORIZED = NO
TREE: GA106Lab-kext8-production-architecture-src/ (1.8.0, pinado; delta V4 = 0 claims impactados)
BASE: dispatch sMethods GA106LabUserClient.cpp:575-600; sel0 {0,0,2,0}; sel1-9 struct-out sizeof GA106LabProtocol.h:41-355
HASHES POR REFERENCIA EXTERNA (F1 P83): valores completos em P80-FINAL-HANDOFF.md:15-19 + MANIFEST-P80-1.8.0.txt:1-5; CLI FULL ga106ctl 9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840
live = 0 / nada executado / source intocado
```

Conferido contra o source pinado (uso + citacao, sem re-derivacao). Refs `file:line` na arvore acima.

## Tabela (1 linha por selector)

| Sel | Nome (Protocol.h:23-33) | Handler (UserClient.cpp) | ABI in/out (dispatch) | Owner? | BAR? | MMIO? | PCI? | Writes? | DMA? | GSP/FW? | Locks/sleep? | Lifetime | Copyout bound | Live-history | Valor novo |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | GetProtocolVersion | GA106LabUCGetVersion :108 | scalar-in 0, struct-in 0, scalar-out 2, struct-out 0 (`:576`) | NO (unico sem PinOwner) | NO | NO | NO | NO | NO | NO | none / NO | drain-INVISIVEL (sem active++; gate `fOwner!=NULL` em :758) | 2xu64, `count<2->BadArgument` (:116) | P83 1x transcript PASS (pacote P83; nao repetir) | NAO (constantes `[1,0x00010800]` Protocol.h:19-20) |
| 1 | GetDeviceIdentity | GA106LabUCGetIdentity :124 | in 0/0, struct-out `sizeof(GA106LabIdentityV1)=32` (`:578`) | YES PinOwner | NO | NO | NO (cache de `copyProperty` no start, zero PCI — GA106Lab.cpp:214-245) | NO | NO | NO | fOwnerLock curto + retain atomico / NO | pin+drain (active++/--) OwnerFlow.hpp:50-84 | 32B exato (`size<32->BadArgument`, `structureOutputSize=32`) | 0 calls | SIM (vendor/device/entryID/state GPU-derived) |
| 2 | GetServiceState | GA106LabUCGetStatus :178 | in 0/0, struct-out `sizeof(GA106LabStatusV1)=32` (`:580`) | YES PinOwner | NO | NO | NO (mesmo cache) | NO | NO | NO | fOwnerLock curto / NO | pin+drain | 32B exato | 0 calls | PARCIAL (state/confirmed cached; revision const 0x10800) |
| 3 | GetPciSnapshot | GA106LabUCGetPciSnapshot :232 | in 0/0, struct-out `sizeof(GA106LabPciSnapshotV1)=84` (`:582`) | YES PinOwner | NO-map (le BAR raw, sem map) | NO | YES live `configRead8/16/32` allowlist kIOPCIConfig* (GA106Lab.cpp:387-488) | NO (write-probe 0xffffffff PROIBIDO) | NO | NO | pin / NO | pin+drain, snapshot local | 84B exato | 0 calls | SIM (command/BARs/subsy/cap/irq live) |
| 4 | BarMapProbe | GA106LabUCBarMapProbe :273 | in 0/0 (+gate `scalarIn!=0\|\|structIn!=0->BadArgument`), struct-out `sizeof(GA106LabBarMapInfoV1)=96` (`:584`) | YES PinOwner | YES discovery+`mapDeviceMemoryWithIndex(RO\|Inhibit)` full-resource + release antes do return (GA106Lab.cpp:490-686) | NO (`getVirtualAddress` NUNCA) | YES vendor/device/cmd-before/after+BAR0 raw | NO | NO | NO | pin / NO | pin+drain, mapa local por chamada | 96B exato | 0 calls | SIM (physBase/resLen/mapLen/mapOpts metadata) |
| 5 | ReadFirstMmioRegister | GA106LabUCReadFirstMmio :323 | in 0/0 zero-input gate, struct-out `sizeof(GA106LabMmioReadV1)=48` (`:586`) | YES PinOwner | YES mesmo discovery/mapa RO+Inhibit | YES 1x `OSReadLittleInt32` BAR0+0x0 NV_PMC_BOOT_0, width 4 (Protocol.h:160-161; RealOps.hpp:102-103; MmioFlow.hpp:193) | YES vendor/device/MSE/BAR0-raw/cmd-before/after | NO (0 stores) | NO | NO | pin / NO | pin+drain, mapa local | 48B exato | 0 calls | SIM (rawValue BOOT0 + chip 0x176 bits 28:20) |
| 6 | ReadStaticIdentity | GA106LabUCReadStaticIdentity :372 | in 0/0 zero-input gate, struct-out `sizeof(GA106LabStaticIdentityV1)=32` (`:588`) | YES PinOwner | YES mesmo discovery/mapa | YES 1x read32 BAR0+0xA00 BOOT_42, width 4 (Protocol.h:203-204; RealOps.hpp:107-108; StaticIdentityFlow.hpp:159-168) | YES mesmo gate | NO (0 stores; BOOT_2 removido) | NO | NO | pin / NO | pin+drain, mapa local | 32B exato | 0 calls | SIM (boot42Raw + chip 10-bit 29:20) |
| 7 | PrepareGspSysmem | GA106LabUCPrepareGspSysmem :421 | in 0/0 zero-input gate, struct-out `sizeof(GA106LabGspSysmemV1)=48` (`:590`) | YES PinOwner + sysmem busy | NO | NO | NO writes (BME OFF; zero MMIO/config-writes) | NO trigger | YES DMA-KPI prepare+genSegments 4KiB/1seg (GA106Lab.cpp:1063+) | GSP-adjacente (pagina p/ futuro bootstrap; sem FW touch) | fOwnerLock+fSysmemLock seccoes curtas / NO (waitMs so em stop) | pin+drain+reserva busy | 48B exato | 0 calls | SIM mas EXCLUIDO (DMA-KPI) |
| 8 | ReadGfwBootReadiness | GA106LabUCReadGfwReadiness :516 | in 0/0 zero-input gate, struct-out `sizeof(GA106LabGfwReadinessV1)=32` (`:592`) | YES PinOwner + fGfwBusy + provider retain | YES BAR0 mapa RO+Inhibit | YES poll gate 0x118128:bit0 + progress 0x118234[7:0], <=4000 iters, 1ms/4s deadline, total <=8000 reads (CoreValidate.h:185-210; GfwFlow.hpp:25-70,285-387) | YES vendor/device/MSE/BAR0-raw | NO | NO | YES GFW/FWSEC semantica | pin+busy seccoes curtas / YES budgeted-wait fora de lock | pin+drain+busy+retain | 32B exato | 0 calls | SIM mas EXCLUIDO (poll+FW) |
| 9 | VerifySysmemFlushPreconditions | GA106LabUCVerifyFlushPrewrite :471 | in 0/0, struct-out `sizeof(GA106LabFlushPrewriteV1)=48` (`:594`) | YES PinOwner + flush busy | NO-map (PCI via snapshot interno) | NO | YES via `copyPciSnapshot` em checkTarget (GA106Lab.cpp:1501+) | NO (zero stores; sem Gate B) | YES flush-page DMA-KPI shared flow | NO (pre-write gate; sem FW) | pin+locks curtos / NO | pin+drain+reservas | 48B exato | 0 calls | SIM mas EXCLUIDO (DMA-adjacente) |

## Notas normativas (usadas na escolha)

- Dispatch: tabela estatica `sMethods[selector]` (UserClient.cpp:575-596); framework valida bounds/count/size + cheques explicitos por handler; `selector>=Count->BadArgument` (:764).
- Gate rapido `fOpen && !fClosing && fOwner!=NULL` sob trava (:758); handlers 1-9 re-checam via pin atomico load+retain sob a mesma trava (OwnerFlow.hpp:50-72); sel0 owner-free mas fail-closed sem owner (P83 F4).
- `close` nunca libera (so marca closing); `stop` drena active bounded + release 1x (UserClient.cpp:640-720); `free` so asserta `fActiveCalls!=0` (:624).
- Tamanhos ABI com `static_assert` (Protocol.h:369-440): 32/32/84/96/48/32/48/32/48 para sel1-9; sel0 2x u64.
- Callers pinados: `ga106ctl.c:20-75` (open), `:111-168` (struct path sel1-3), `:245-308` (sel5-6), validate `ga106ctl-validate.c:6-30`.
- Live-history do pacote: todos os contadores live = 0 neste pacote; unico historico citado e `P83 1x GetVersion` por referencia externa (nao re-executado aqui).

```text
LIVE_EXECUTION_AUTHORIZED = NO / live = 0 / source intocado
```
