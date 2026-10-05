# P82.2 — Selector 5 Audit (P-D4 offline, árvore 1.5.3 exata)

```text
SESSION_ID: P82-2-ROUTE1-RELAY-V3
MILESTONE: P82_2_ROUTE1_DESIGN_SELECTORS_5_6 (OFFLINE ONLY)
STATE: MUSE_WORK / ACTOR: MUSE_BUILD
LIVE_EXECUTION_AUTHORIZED = NO (live=0; só leitura de source; nenhum source modificado)
TREE: GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/ (1.5.3 residente)
```

Método: leitura direta de `GA106LabProtocol.h`, `GA106LabUserClient.cpp`
(dispatch + handler), `GA106Lab.cpp` (owner wrapper), `GA106LabMmioFlow.hpp`
(shared flow), `GA106LabRealOps.hpp` (map/read/release), `GA106LabCoreValidate.h`
(helpers), `ga106ctl.c` (ABI userspace), `Info.plist`, `SHA256.txt`
(conferência textual MAP-vs-arquivo; `shasum -a 256` NÃO re-executado neste
turno — re-execução exigida antes de qualquer janela, cf. P82.1 smoke advisory).

## 1. Identidade

- Número: **5** (`kGA106LabSelector_ReadFirstMmioRegister = 5`,
  `GA106LabProtocol.h:23-31`; `Count = 8`, válidos 0–7).
- Nome semântico: **BOOT0 / ReadFirstMmioRegister** (`NV_PMC_BOOT_0`).
- ABI userspace exata: **zero-input** — `scalarInputCount == 0 &&
  structureInputSize == 0` exigidos no handler; struct-output
  `GA106LabMmioReadV1` **48 bytes**; tabela
  `sMethods[5] = { ReadFirstMmio, 0, 0, 0, sizeof(GA106LabMmioReadV1) }`
  (`GA106LabUserClient.cpp:312-324`, entrada `/* 5 READ_FIRST_MMIO */`);
  chamada via `IOConnectCallMethod(conn, 5, NULL,0, NULL,0, NULL,NULL,
  &out,&outSize)` (`ga106ctl.c:210-221`); CLI
  `ga106ctl read-first-mmio-register` imprime `register/offset/width/rawValue/
  chipId/status/validFlags`, exit via `ga106ctl_mmio_cli_exit`.
- Handler fonte exato: `GA106LabUCReadFirstMmio`
  (`GA106LabUserClient.cpp:~6066`) → owner `GA106Lab::readPmcBoot0`
  (`GA106Lab.cpp:591-602`, wrapper fino) →
  `RunFirstMmioReadFlow` (`GA106LabMmioFlow.hpp`, ONE_SHARED_MMIO_FLOW).
- Versão KEXT exata: **1.5.3** —
  `GA106LAB_DRIVER_VERSION_U32 0x00010503u` (`GA106LabProtocol.h:20`).
- CFBundleVersion exata: **1.5.3**
  (`Info.plist:19-20`, `CFBundleShortVersionString 1.5.3`).
- Hash source (SHA256.txt da árvore, conferência textual com P82.1 MAP):
  `GA106LabProtocol.h 2e39a274a744a299dea6a16ca1b55e39776c7bdc131c56c2f588b7150732b0c6`;
  `GA106LabUserClient.cpp aed3d33b715c046c7601eed004920823ebb7bbc13baa0d95c090e9db871a6a81`;
  `GA106Lab.cpp bec65dcd02bae6dc5ead613eadce42e155497609ff7e11e551b7113a5728f5f7`;
  `GA106LabCoreValidate.h 733df67bfa8d073bdbcb9eaf463d6c475572be2616ba8231511d9f455008b9eb`;
  `GA106LabMmioFlow.hpp e1cbb262b868f722b17c7ee16c2333d2617661fa8a9ef3d3328dec65fecfba40`;
  `GA106LabRealOps.hpp 4f485eaa276c2444e7cefb9e2f706e5c378da794d2fb1a64fe666b8ea39d9fa6`;
  `Info.plist 4e9d6335a90021a34367e2538069ab6a776f1c09e3ab7793833c587e10c25aee`.
- Hash compilado/bundle (mesma árvore, `SHA256.txt`): bundle Mach-O
  `build/obj/GA106Lab.kext/Contents/MacOS/GA106Lab b2ea2218fc5adab8a896dd22b9aa1e54ae5939ef3da62d23c22e6395e59e6854`;
  CLI `build/obj/ga106ctl bba1a0404e1ce3159ecc478ebe0c4e4bd9bb060a3df360bbca05162a7f90b122`.
  Pin é textual; re-executar `shasum -a 256` na árvore exata antes de
  qualquer janela (P82.1 advisory 16, não executado aqui por ser execução).

## 2. Caminho (BAR / offset / largura / leituras / copyout / timeout)

- BAR: **BAR0 register aperture**, 16 MiB (`GA106LAB_BAR0_EXPECTED_LENGTH
  0x1000000`), discovery estrito: `getDeviceMemoryCount` + match único
  `len == 16MiB && phys == BAR0base` (`MmioFlow §5`); `matchCount != 1` →
  FAIL sem fallback; `resLen/resPhys` revalidados.
- Offset: **BAR0+`0x00000000`** (`kGA106LabMmio_PmcBoot0_Offset`,
  `GA106LabProtocol.h:158`); largura **4** (`_Width 4u`).
- Nº de leituras: **1x load 32-bit, 0 stores**
  (`GA106Lab.cpp:590-591`; `MmioFlow §11`: "A ÚNICA leitura (1x load 32-bit,
  offset fixo 0)", "sempre offset 0 / width 4"); única callsite `read32(off)`
  no flow + leituras config-space de guarda (vendor/device/command/BAR0raw —
  protocolo PCI, não MMIO).
- Copyout: `sizeof(GA106LabMmioReadV1)` = **48** (`size/version/status/
  registerId/offset/width/rawValue/validFlags/reserved[4]`, static_assert 48).
- Timeout: **nenhum** — sem polling, sem loop, sem sleep no caminho 5
  (single load; absent-device retorna direto, sem retry).

## 3. Locks / IRQ

- Locks no caminho 5: **nenhum lock adquirido**. `fSlotLock` (IOSimpleLock)
  só no caminho open/close (`newUserClient` reserva, `clientClosed` limpa);
  `fSysmemLock` só no caminho selector 7 + teardown `stop()`; flow 5 não
  chama `lock()/unlock()`; `RealOps::createMap/getVA/read32/releaseMap` não
  trancam. Sem workloop/command-gate no caminho.
- IRQ: **nenhum enable/mask/handler/wait**. Nenhum `registerInterrupt`,
  nenhuma doorbell; `interruptLine/Pin` só lidos no caminho selector 3, não
  aqui. Caminho IRQ-free por construção (nada a analisar além de confirmar
  ausência — confirmada por leitura do flow + RealOps + wrapper).

## 4. Lifetime / retain-release / detach-unload race

- Retain-release: `UserClient::start` faz `owner->retain()` (pareado com
  `release()` em `stop()`); `fOwner` ponteiro cru + flag `fOpen`;
  `externalMethod` checa `!arguments||!fOpen||!fOwner` + `selector >= Count`;
  handler faz `owner = self->getOwnerService(); if (!owner) NotOpen` e chama
  direto — **SEM pin atômico** (sem `PinOwnerForExternalMethod` /
  `ReleasePinnedOwner`, sem `isStopping/isBusy`, sem `isInactive` no handler;
  `GA106LabUserClient.cpp:353,385` retain/release simples).
- Mapa: stack-local `GA106LabRealOps ops(pci)` no wrapper; `createMap`
  (`mapDeviceMemoryWithIndex(idx, kIOMapReadOnly|kIOMapInhibitCache)`) →
  `releaseMap` (`fMap->release(), fVA=0`) em **todos** os exits (sucesso §15 +
  error-paths §§6-13); VA nunca exposta; nenhum ponteiro retido.
- Detach/unload race: **EXPOSTO**. `stop()`/`clientClose`/`clientDied` limpam
  `fOpen` e dão `release`, mas não há dreno de `externalMethod` em voo no
  caminho 5 (busy-counter + espera só existem no caminho sysmem/selector 7 e
  no `stop()` para sysmem). Janela: `getOwnerService()` (ponteiro cru) →
  uso em `readPmcBoot0` sem retain próprio → `stop()` concorrente pode
  liberar o owner no meio da chamada (UAF/stop-race). `fSlotLock` protege só
  o slot open/close, não a chamada em voo. Ver `P82_2-LIFETIME-RISK.md`.

## 5. Clear-on-read / side effects poll-write / writes escondidos

- Clear-on-read: **SEM fonte primária**. `NV_PMC_BOOT_0` é registrador de ID
  arquiteturalmente read-only; source tem zero stores e live-history mostra
  valor estável, mas NENHUMA fonte Nova/OpenRM citada prova ausência de
  semântica clear-on-read em BOOT_0. Status: FILOSOFICAMENTE read-only, NÃO
  PROVADAMENTE semântica-fixa (P82.1 advisory 14; re-verificação exigida
  antes de qualquer janela, não feita aqui).
- Side effects poll/write: **nenhum poll** (1 load único); **nenhum write**:
  MSE nunca habilitado (aborta `NotReady` se off); `commandAfter` é
  `configRead16` comparado (leitura, não write/restauração); sem BME/DMA/GSP/
  firmware/VRAM/IRQ/reset/doorbell/PUT/command no caminho (qualquer ocorrência
  seria fora do flow auditado).
- Writes escondidos: **nenhum encontrado** no caminho
  handler→wrapper→flow→RealOps (só `read32` via `OSReadLittleInt32`;
  `mapDeviceMemoryWithIndex` RO+Inhibit; `getLength/getPhysicalAddress/
  getVirtualAddress` são metadata). `IOLog` de start/stop fora do caminho.

## 6. Vereditos P-D4.0..P-D4.5 (selector 5)

```text
P-D4.0 (hash pin) ............ PASS-textual / RE-EXEC-REQUIRED (pin MAP-vs-SHA256.txt confere; shasum re-exec antes de janela)
P-D4.1 (lifetime/map) ........ FAIL-OPEN (map lifecycle PASS em todos os exits; lifetime SEM pin -> stop-race/UAF presente, ver LIFETIME-RISK)
P-D4.2 (lista fechada) ....... PASS (BAR0 16MiB + offset 0 + width 4 + 1 load; sem input do userspace)
P-D4.3 (clear/poll/write) .... OPEN (zero writes + sem poll provados no source; clear-on-read SEM fonte primaria -> nao fechar)
P-D4.4 (copyout/bounds/time) . PASS (48B exato; alignment+bounds overflow-safe; sem timeout por desenho single-load)
P-D4.5 (locks/IRQ) ........... PASS-descritivo (sem lock / IRQ-free; nada que cunhe; sem sleep sob lock)
```

## 7. Live-history (preservar, não re-provar)

LIVE PASS histórico: `TG-KEXT4-FIRST-MMIO-READ-LIVE/result.md` —
"Single call exit 0. NV_PMC_BOOT_0 BAR0+0 32-bit, raw **0xb76000a1** chip
**0x176**. 1 read, 0 writes, map released, Command unchanged."
`CURRENT-STATE.md`: BOOT0 **COMPLETE**. Selector 5 existe no 1.5.3 residente
mas **NUNCA foi invocado no residente 1.5.3** (PASS veio de geração KEXT4
anterior); repetir hoje no 1.5.3 seria primeira invocação NESTA árvore —
ver valor em `P82_2-EVIDENCE-VALUE.md` (conclusão: redundante).
