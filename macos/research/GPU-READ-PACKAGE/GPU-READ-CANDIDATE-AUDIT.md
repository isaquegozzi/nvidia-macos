# GPU-READ — Auditoria delta SO do candidato (selector 1, offline)

```text
SESSION_ID: V4-GPU-READ-PACKAGE / MILESTONE: GPU_DERIVED_READ_PACKAGE
STATE: MUSE_WORK / ACTOR: MUSE_BUILD / LIVE_EXECUTION_AUTHORIZED = NO
TREE: GA106Lab-kext8-production-architecture-src/ (1.8.0-revB; C-022 boolfix; binario corrigido 8e387d53 ainda-nao-live; live/residente = b6d4ea31)
CANDIDATO: sel1 GetDeviceIdentity (GA106LabUCGetIdentity, GA106LabUserClient.cpp:124)
live = 0 / nada executado / source intocado
```

Conferido contra o source pinado (uso + citacao). Todas as refs sao `file:line` na arvore acima.

## 1. Hash pin (por referencia externa, padrao P83 F1/F2)

- KEXT/plist/CLI/build.sh/manifest: valores completos vivem fora do pacote — `P80-FINAL-HANDOFF.md:15-19` + `MANIFEST-P80-1.8.0.txt:1-5` (32/32 OK por referencia); caller `build/obj/ga106ctl` FULL `9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840` (gate pre-live, sem prefixo).
- Binding source<->hash em `MANIFEST-P80-1.8.0.txt:1-5`; arquivos auditados aqui: `GA106LabUserClient.cpp` (`6e7aab65…` revB; era `1ff7ff34…` no binario live b6d4ea31), `GA106LabProtocol.h` (`f976df32…`), `GA106Lab.cpp` (`429208f4…`), `GA106Lab.hpp` (`1d161e1b…`), `GA106LabUserClientOwnerFlow.hpp` (`372a1b81…`), `ga106ctl.c` (`0e03a3e2…`), `ga106ctl-validate.c/h` — por referencia ao manifest (sem re-hash neste pacote offline).
- Binario-em-RAM segue Q10 BLOCKED (herdado P83 Q-P83-01): presenca em kextstat != prova de bytes-em-RAM; declarado, nao fingido.

## 2. Lifetime / UAF

- Pin atomico load+retain sob `fOwnerLock` (`PinOwnerForExternalMethod`, OwnerFlow.hpp:50-72); reject sem retain se `closing||!open||owner==NULL`.
- `retain/release` = refcount atomico OSObject (seguro sob spinlock); `active++` no pin, `active--` apos release em TODOS os exits (`ReleasePinnedOwner`, OwnerFlow.hpp:73-84).
- `copyCachedState` (GA106Lab.cpp:372-378) retorna `bool` e copia por valor `*out=fCache` sem expor ponteiros; `const`, sem locks internos, sem acesso a provider. Handler consome como `bool copyOk = owner->copyCachedState(&cached)` + `if (!copyOk) return NotReady` (C-022 corrigido em sel1 E sel2; `cached = {}` defensivo; ABI/dispatch/owner-free inalterados).
- `stop` marca closing + drena `active` ate 0 fora da trava antes do release 1x do owner (UserClient.cpp:640-720); `close` so marca closing (nunca libera); `free` so asserta `fActiveCalls!=0` (:624). Trap de externalMethod retem o proprio UC: nenhum pin em progresso em `free`.
- Concorrencia stop/close durante sel1: pin ja obtido segue valido pelo retain proprio; novo pin apos closing falha `NotOpen`. Sem UAF por construcao auditada (P80 F-P80-02).
- Cache imutavel apos `start()` (GA106Lab.cpp:284-290); leitura concorrente e copia de bytes estaveis — sem teardown de estado compartilhado neste caminho.

## 3. Closed register list

- Lista fechada = VAZIA (nenhum registrador): sel1 nao le PCI config, BAR, MMIO, GSP, FW, VRAM. Unica fonte: `fCache` (vendor/device/entryID/state/confirmed) + constantes `sizeof/version`.
- Prova negativa: corpo `:124-176` nao referencia `IOPCIDevice`, `mapDeviceMemory`, `configRead*`, `OSRead*`, `getVirtualAddress`, `IODMACommand`, `IOBufferMemoryDescriptor`, `0x0/0xA00/0x118xxx`, BME/MSE. Fonte do cache: `copyProperty("vendor-id"/"device-id")` + `getRegistryEntryID()` no `start` (GA106Lab.cpp:214-245) — zero acesso PCI por comentario normativo.
- Sem arbitrary addr: input void; nenhum offset/size/addr do userspace alcanca kernel.

## 4. Clear-on-read / side effects

- Sem clear-on-read: `fCache` nunca zerado/limpo pela leitura; `copyCachedState` e leitura pura (retorna `false->NotReady` se `!fCacheValid`, sem mutar).
- Side effects de kernel: NENHUM (sem milestone, sem contador, sem log no caminho sel1, sem store em estado global). Unicas escritas: campos do buffer `structureOutput` do caller (userspace) + `structureOutputSize=32`.
- Pre-zero antes do pin garante que qualquer early-return (`BadArgument`/`NotOpen`/`NotReady`) nao vaza bytes nao inicializados nem padding.

## 5. Poll-write absence

- Sem poll, sem wait, sem `IODelay`, sem deadline, sem `waitMs`/`waitShort` no caminho sel1 (corpo `:124-176` nao chama wait; `waitShort` so existe para drain de `stop`).
- Sem escrita de poll (criterio vacuoso-passado: nao ha loop para esconder write).

## 6. Copyout

- Dispatch struct-out `sizeof(GA106LabIdentityV1)=32` (`:578`); asserts `sizeof==32/align==8/offsets` (Protocol.h:369-377).
- Guarda `!structureOutput || size<32 -> BadArgument`; fill campo a campo + `reserved=0`; `structureOutputSize=32` exato.
- Caller passa `outSize=sizeof(out)` zerado (ga106ctl.c:119-124); validador exige `len==32 && size==32 && version==1` (ga106ctl-validate.c:6-23). Sem over-copy, sem write-back de count alem do fixo.

## 7. Locks / IRQ

- Locks: SOMENTE `fOwnerLock` em secoes curtas (pin/release); nunca provider/MMIO/wait sob a trava (disciplina OwnerFlow.hpp:1-49). Sem `fSysmemLock`, sem aninhamento, sem sleep sob lock.
- IRQ: NENHUMA (sem interrupts, sem doorbell, sem command submission; contadores live IRQ=0).
- Sleep: NAO (`CAN_SLEEP=NO`).

## 8. Timeout

- N/A por construcao (retorno imediato bounded O(1)); nao ha timeout a configurar. `stop`-drain do pin e curto (1 retain + 1 copia); in-flight de sel1 nao estende drain.

## 9. Close path

- `clientClose` marca closing + notifica slot + `terminate` (nunca libera); `clientDied` converge ao mesmo teardown; `stop` drena + release 1x (UserClient.cpp:640-720). Idempotente (2o close = no-op re-marcado).
- Sequencia live futura (1x, pos-APPROVE, fora deste pacote): open (`ga106ctl.c:40`) -> sel1 (`ga106ctl.c:124-141`) -> close incondicional (`IOServiceClose`); snapshots read-only emolduram; `CALLS=1/RETRY=0/LOOPS=0`.

## 10. Veredito

```text
AUDIT sel1 = GO pos-revB (nenhum write/side-effect nao-excluivel; todos os 9 eixos fechados acima; C-022 corrigido e rebuild 1.8.0-revB BUILD_OK)
NO-GO nao acionado; backups sel3/sel2 nao auditados a fundo neste pacote (sem loops, max 3 respeitado: 1 auditado)
```

```text
LIVE_EXECUTION_AUTHORIZED = NO / live = 0 / REVIEW pendente / GAP: nenhum teste/mutante offline cobre GA106LabUCGetIdentity/copyCachedState (zero refs em test-*/mutation-*) — handler validado so por auditoria de source + build
```
