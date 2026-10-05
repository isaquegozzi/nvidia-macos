# P82.2 — Selector 6 Audit (P-D4 offline, árvore 1.5.3 exata)

```text
SESSION_ID: P82-2-ROUTE1-RELAY-V3
MILESTONE: P82_2_ROUTE1_DESIGN_SELECTORS_5_6 (OFFLINE ONLY)
STATE: MUSE_WORK / ACTOR: MUSE_BUILD
LIVE_EXECUTION_AUTHORIZED = NO (live=0; só leitura de source; nenhum source modificado)
TREE: GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/ (1.5.3 residente)
```

Método: idêntico ao audit do selector 5 (leitura direta de `GA106LabProtocol.h`,
`GA106LabUserClient.cpp`, `GA106Lab.cpp`, `GA106LabStaticIdentityFlow.hpp`,
`GA106LabRealOps.hpp`, `GA106LabCoreValidate.h`, `ga106ctl.c`, `Info.plist`,
`SHA256.txt`; conferência textual; `shasum -a 256` NÃO re-executado — exigir
antes de qualquer janela).

## 1. Identidade

- Número: **6** (`kGA106LabSelector_ReadStaticIdentity = 6`,
  `GA106LabProtocol.h:23-31`; `Count = 8`).
- Nome semântico: **BOOT42 / ReadStaticIdentity** (`NV_PMC_BOOT_42` único;
  BOOT_2 @0x8 REMOVIDO nesta revisão por evidência primária insuficiente).
- ABI userspace exata: **zero-input** (`scalarInputCount == 0 &&
  structureInputSize == 0`); struct-output `GA106LabStaticIdentityV1`
  **32 bytes**; `sMethods[6] = { ReadStaticIdentity, 0,0,0,
  sizeof(GA106LabStaticIdentityV1) }`; `IOConnectCallMethod(conn, 6, NULL,0,
  NULL,0, NULL,NULL, &out,&outSize)` (`ga106ctl.c:244-251`); CLI
  `ga106ctl read-static-identity` imprime `boot42/decodedChipId/readCount/
  status/validMask`, exit via `ga106ctl_static_identity_cli_exit`.
- Handler fonte exato: `GA106LabUCReadStaticIdentity`
  (`GA106LabUserClient.cpp:~7326`) → owner `GA106Lab::readStaticIdentity`
  (`GA106Lab.cpp:612-625`, wrapper fino) →
  `RunStaticIdentityReadsFlow` (`GA106LabStaticIdentityFlow.hpp`,
  ONE_SHARED_STATIC_IDENTITY_FLOW).
- Versão KEXT exata: **1.5.3** (`0x00010503u`, `GA106LabProtocol.h:20`).
- CFBundleVersion exata: **1.5.3** (`Info.plist:19-20`).
- Hash source: `GA106LabProtocol.h 2e39a274a744a299dea6a16ca1b55e39776c7bdc131c56c2f588b7150732b0c6`;
  `GA106LabUserClient.cpp aed3d33b715c046c7601eed004920823ebb7bbc13baa0d95c090e9db871a6a81`;
  `GA106Lab.cpp bec65dcd02bae6dc5ead613eadce42e155497609ff7e11e551b7113a5728f5f7`;
  `GA106LabCoreValidate.h 733df67bfa8d073bdbcb9eaf463d6c475572be2616ba8231511d9f455008b9eb`;
  `GA106LabStaticIdentityFlow.hpp 1cd6b51ba9264d02f5966979436102a19b22c06c3ea3d6ae0c2550098a1c1797`;
  `GA106LabRealOps.hpp 4f485eaa276c2444e7cefb9e2f706e5c378da794d2fb1a64fe666b8ea39d9fa6`;
  `Info.plist 4e9d6335a90021a34367e2538069ab6a776f1c09e3ab7793833c587e10c25aee`.
- Hash compilado/bundle: idênticos aos do audit 5 (mesma árvore/bundle) —
  Mach-O `b2ea2218fc5adab8a896dd22b9aa1e54ae5939ef3da62d23c22e6395e59e6854`,
  `ga106ctl bba1a0404e1ce3159ecc478ebe0c4e4bd9bb060a3df360bbca05162a7f90b122`;
  re-exec exigido antes de janela.

## 2. Caminho (BAR / offset / largura / leituras / copyout / timeout)

- BAR: **BAR0 register aperture 16 MiB**, discovery estrito idêntico ao 5
  (`matchCount == 1`, `resLen == 16MiB`, `resPhys == BAR0base`, senão FAIL).
- Offset: **BAR0+`0x00000A00`** (`kGA106LabStaticIdentity_Boot42_Offset`,
  `GA106LabProtocol.h:201`); largura **4** (`_Width 4u`).
- Nº de leituras: **1x load 32-bit, 0 stores** (única callsite `read32(off42)`
  no flow; decode `CHIP_ID bits 29:20`, máscara `0x3FF00000>>20`,
  `GA106LabCoreValidate.h:166` + `BOOT42_*`; allowlist compile-time "somente
  BOOT_42", `GA106LabRealOps.hpp:104-112`, "Zero stores"). Nota: comentário
  no handler 1.5.3 diz "map→2 leituras fixas" mas o flow executa **1** load
  (BOOT_2 removido; comentário stale inofensivo — o código manda).
- Copyout: `sizeof(GA106LabStaticIdentityV1)` = **32** (`size/version/status/
  validMask/readCount/boot42Raw/decodedChipId/reserved`; `readCount = 1` fixo
  em sucesso).
- Timeout: **nenhum** — sem polling/loop/sleep (single load; absent → FAIL
  direto; `chip != 0x176` → `NoDevice`).

## 3. Locks / IRQ

- Idêntico ao 5: **nenhum lock no caminho 6**, IRQ-free (sem enable/mask/
  handler; sem sleep sob lock). `fSlotLock`/`fSysmemLock` fora do caminho.

## 4. Lifetime / retain-release / detach-unload race

- Idêntico ao 5 e igualmente exposto: retain em `start`/release em `stop`,
  ponteiro cru `getOwnerService()` sem pin, sem `isStopping/isBusy/
  isInactive`; mapa stack-local liberado em todos os exits (release antes de
  cada `return` + path de sucesso; `commandAfter` comparado após release);
  **stop-race/UAF presente** (ver `P82_2-LIFETIME-RISK.md`). Mecanismo,
  linhas e janela iguais às do selector 5 — risco NÃO menor por ser "outro
  offset".

## 5. Clear-on-read / side effects poll-write / writes escondidos

- Clear-on-read: **SEM fonte primária** para BOOT_42 (mesma ressalva do BOOT_0;
  P82.1 advisory 14 cobre ambos os offsets + `0x118128/0x118234`).
- Side effects poll/write: **nenhum poll, nenhum write** (MSE-gate aborta se
  off; `commandAfter` só lido; sem BME/DMA/GSP/firmware/VRAM/IRQ/reset/
  doorbell/PUT/command).
- Writes escondidos: **nenhum encontrado** (só `read32` + metadata de mapa).

## 6. Vereditos P-D4.0..P-D4.5 (selector 6)

```text
P-D4.0 (hash pin) ............ PASS-textual / RE-EXEC-REQUIRED (mesmo pin/bundle do audit 5)
P-D4.1 (lifetime/map) ........ FAIL-OPEN (map PASS; lifetime SEM pin -> stop-race/UAF, ver LIFETIME-RISK)
P-D4.2 (lista fechada) ....... PASS (BAR0 16MiB + offset 0xA00 + width 4 + 1 load; zero input)
P-D4.3 (clear/poll/write) .... OPEN (zero writes + sem poll no source; clear-on-read SEM fonte primaria)
P-D4.4 (copyout/bounds/time) . PASS (32B exato; alignment+bounds overflow-safe; single-load sem timeout)
P-D4.5 (locks/IRQ) ........... PASS-descritivo (sem lock / IRQ-free)
```

Diferença real 5-vs-6: **só offset, struct e decode** (0x0/48B/9-bit 0x176 vs
0xA00/32B/10-bit 0x176). Lifetime, locks, IRQ, classe de risco: idênticos.

## 7. Live-history (preservar, não re-provar)

LIVE PASS histórico: `TG-KEXT4-STATIC-IDENTITY-READS-LIVE/result.md` —
"Single selector-6 call exit 0. BOOT_42 **0x176a1000** chip **0x176**. 1 read,
0 writes, released." + `boot42-decode.txt` (10-bit decode correto;
contraexemplo 0x376 offline). `CURRENT-STATE.md`: BOOT42 **COMPLETE**.
Como o 5: existe no 1.5.3 residente mas nunca invocado NESTA árvore —
ver valor em `P82_2-EVIDENCE-VALUE.md` (conclusão: redundante).
