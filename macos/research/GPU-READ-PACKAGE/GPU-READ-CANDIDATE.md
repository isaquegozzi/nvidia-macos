# GPU-READ — Primeira leitura GPU-derived (UMA, offline)

```text
SESSION_ID: V4-GPU-READ-PACKAGE / MILESTONE: GPU_DERIVED_READ_PACKAGE
STATE: MUSE_WORK / ACTOR: MUSE_BUILD / LIVE_EXECUTION_AUTHORIZED = NO
TREE: GA106Lab-kext8-production-architecture-src/ (1.8.0-revB; boolfix sel1/sel2 C-022; binario corrigido 8e387d53 ainda-nao-live; live/residente segue b6d4ea31 P80-approved)
live = 0 / nada executado / source intocado
```

```text
FIRST_GPU_DERIVED_READ_CANDIDATE = selector 1 (kGA106LabSelector_GetDeviceIdentity)
```

## SELECTOR

```text
SELECTOR = 1
NOME = kGA106LabSelector_GetDeviceIdentity (GA106LabProtocol.h:24)
HANDLER = GA106LabUCGetIdentity (GA106LabUserClient.cpp:124)
DISPATCH = {0,0,0,sizeof(GA106LabIdentityV1)=32} (GA106LabUserClient.cpp:578)
CALLER = ga106ctl identity path (ga106ctl.c:119-141; open ga106ctl.c:20-52; close ga106ctl.c:66)
VALIDATE = ga106ctl_check_identity size+version (ga106ctl-validate.c:6-23)
```

## EXACT_SEMANTICS (o que a chamada faz, sem inventar)

- Input void/zero: nenhum scalar/structure input (dispatch 0/0; handler nunca toca `structureInput`).
- Pin atomico do owner (`PinOwnerForExternalMethod`, OwnerFlow.hpp:50-72); sem pin -> `kIOReturnNotOpen` sem efeito.
- Copia por valor do cache imutavel `GA106LabCachedState` (`copyCachedState`, GA106Lab.cpp:372-378; `*out=fCache`); cache preenchido UMA vez em `start()` a partir de propriedades IORegistry ja publicadas (`vendor-id`/`device-id` via `copyProperty`, zero acesso PCI — GA106Lab.cpp:214-245; `fCacheValid=(haveVendor&&haveDevice)`), nunca polling depois.
- Pre-zero campo a campo do buffer de saida antes do pin (sem `memset`, sem padding vazado); em falha de copia retorna `kIOReturnNotReady` com buffer zerado + `state=Unknown`.
- Em sucesso preenche ABI fixa `GA106LabIdentityV1` (Protocol.h:41-49 + asserts :369-377): `size=32, version=1, vendor=cached.vendor, device=cached.device, providerEntryID=cached.providerEntryID, state=cached.state, reserved=0`; `structureOutputSize=32`.
- Zero escritas em hardware: sem `IOPCIDevice*`, sem map/config, sem MMIO deref, sem BME/DMA/GSP/FW/VRAM, sem locks alem do pin curto, sem sleep, sem IRQ, sem polling.
- Bounded: 1 pin + 1 copia + 1 fill; sem loops, sem retry, sem fallback.

## EXPECTED_EVIDENCE (PASS iff exato, 1 invocacao)

```text
kr = kIOReturnSuccess (0) / outSize = 32 / size = 32 / version = 1
vendor = 0x10de / device = 0x2504 / state = 1 (kGA106LabState_StartSuccess)
providerEntryID != 0 / reserved = 0 / close OK / sem panic/hang
```

`ga106ctl` imprime `vendor/device/providerEntryID/state` (ga106ctl.c:137-141); validador exige `len==32 && size==32 && version==1` (ga106ctl-validate.c:6-23). Identidade `10de:2504` EXIGIDA em todos os paths (wrong-device nunca aceito).

## Check 10 criterios (todos SIM para sel1)

1. Existe no 1.8.0: SIM (`Protocol.h:24`, dispatch `:578`, handler `:124`).
2. Fixed API: SIM (struct 32B offsets fixos + asserts `:369-377`; dispatch size fixo).
3. Zero writes: SIM (so atribuicao ao buffer userspace; corpo nao toca HW).
4. Zero BME/DMA/GSP/FW/VRAM: SIM (sem PCI/BAR/DMA/FW; cache registry-only).
5. BAR/offsets/width fechados: SIM vacuoso-forte (nenhum BAR/offset/width — void input; nada do userspace).
6. 1 invocacao: SIM (`CALLS=1/RETRY=0/LOOPS=0`; open+sel1+close num unico processo `ga106ctl identity`).
7. Bounded: SIM (sem poll/sleep/loops; copia O(1)).
8. Sem arbitrary addr: SIM (nenhum endereco/size/flag do userspace).
9. Evidencia RTX nova: REBAIXADO (Qwen GR-QWEN-SEM-01: `10de:2504` ja conhecido desde kext2 ao vivo — vendor/device redundantes; valor novo = caminho sel1 1.8.0 end-to-end dispatch->pin->copy->copyout + entryID/state do boot corrente; PARCIAL, nao SIM).
10. Menor risco: SIM (unico com zero PCI-live + zero BAR-map + zero MMIO + drain visivel; ver eliminacao abaixo).

## Eliminacao (sem loops; max 3 considerados a fundo)

- sel0 EXCLUIDO por regra (nao repetir GetVersion).
- sel5/sel6 (BOOT0/BOOT_42) NAO justificados como baseline 1.8.0: dariam a mesma conclusao de identidade (`chip 0x176`) com custo de BAR discovery + mapa RO+Inhibit + 1 load MMIO + validacao MSE/BAR/cmd-after — risco estritamente maior sem evidencia nova alem do que sel1 ja prova (vendor/device). Baseline 1.8.0 fecha em sel1; MMIO fica para passo posterior dedicado.
- sel4 EXCLUIDO (BAR map + PCI-live; metadata util mas risco > sel1).
- sel3 considerado (2o lugar): passa 1-9 mas perde no 10 (leituras PCI config live ~20 regs vs zero HW de sel1).
- sel2 considerado (3o lugar): passa 1-8+10 mas perde no 9 (revision constante; menos GPU-especifico que vendor/device).
- sel7/sel9 EXCLUIDOS (DMA-KPI prepare, criterio 4); sel8 EXCLUIDO (poll ate 4000 + semantica FW, criterios 4/7).

```text
ESCOLHA FINAL = sel1 (GO unico) / sel3 = backup-1 / sel2 = backup-2 (sem re-auditoria neste pacote)
LIVE_EXECUTION_AUTHORIZED = NO / live = 0
```
