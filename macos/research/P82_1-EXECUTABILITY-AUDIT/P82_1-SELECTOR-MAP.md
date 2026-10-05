# P82.1 — Selector Map por versão (auditoria OFFLINE, só leitura de source)

```text
SESSION_ID: P82-1-AUDIT-RELAY-V3
MILESTONE: P82_1_EXECUTABILITY_AUDIT_C (OFFLINE ONLY, só leitura de source)
STATE: MUSE_WORK / ACTOR: MUSE_BUILD
LIVE_EXECUTION_AUTHORIZED = NO (live=0; nada executado, nenhum source modificado)
```

Método: leitura direta dos arquivos `GA106LabProtocol.h` (enum de selectors +
`GA106LAB_DRIVER_VERSION_U32`), `GA106LabUserClient.cpp` (tabela `sMethods[]` +
handlers), `GA106Lab.cpp` (funções owner), flows `*.hpp`, `Info.plist`
(`CFBundleVersion`), `SHA256.txt` / `shasum -a 256` recomputado, e `ga106ctl.c`.
Nenhum comando live executado.

## 1. Disponibilidade por versão (prova: enum + Count)

| Versão | Árvore auditada | `DRIVER_VERSION` | `Selector_Count` | Selector 8 | Selector 9 |
|---|---|---|---|---|---|
| 1.5.0 | `GA106Lab-kext5-gsp-sysmem-alloc-src/` | `0x00010500` | 8 (0–7) | AUSENTE | AUSENTE |
| 1.5.1 | `GA106Lab-kext5-gsp-sysmem-alloc-fix-src/` | `0x00010501` | 8 (0–7) | AUSENTE | AUSENTE |
| 1.5.2 | `GA106Lab-kext5-gsp-sysmem-alloc-fix2-src/` | `0x00010502` | 8 (0–7) | AUSENTE | AUSENTE |
| **1.5.3 (live atual)** | `GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/` | `0x00010503` | **8 (0–7)** | **AUSENTE** | AUSENTE |
| 1.6.0 | `GA106Lab-kext6-gsp-gfw-readiness-src/` | `0x00010600` | 9 (0–8) | PRESENTE | AUSENTE |
| 1.6.1 | `GA106Lab-kext6-gsp-gfw-readiness-astra-b-fix-src/` | `0x00010601` | 9 (0–8) | PRESENTE | AUSENTE |
| **1.6.2 (nunca-live)** | `GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src/` | `0x00010602` | **9 (0–8)** | **PRESENTE** | AUSENTE |
| 1.7.0 | `GA106Lab-kext7-sysmem-flush-prewrite-src/` | `0x00010700` | 10 (0–9) | PRESENTE | PRESENTE |
| 1.7.1 | `GA106Lab-kext7-sysmem-flush-prewrite-fix-src/` | `0x00010701` | 10 (0–9) | PRESENTE | PRESENTE |
| 1.7.2 | `GA106Lab-kext7-sysmem-flush-prewrite-fix2-src/` | `0x00010702` | 10 (0–9) | PRESENTE | PRESENTE |
| 1.7.3 | `GA106Lab-kext7-sysmem-flush-prewrite-fix3-src/` | `0x00010703` | 10 (0–9) | PRESENTE | PRESENTE |
| 1.8.0 (offline) | `GA106Lab-kext8-production-architecture-src/` | `0x00010800` | 10 (0–9) | PRESENTE | PRESENTE |

Provas de ausência/presença:

- 1.5.3: `GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/GA106LabProtocol.h:23-31`
  enum termina em `kGA106LabSelector_PrepareGspSysmem = 7`,
  `kGA106LabSelector_Count = 8`; `grep -rn "Gfw|gfw|118128|118234"`
  na árvore 1.5.3 retorna **zero ocorrências**.
- 1.6.2: `GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src/GA106LabProtocol.h:31-32`
  `kGA106LabSelector_ReadGfwBootReadiness = 8`, `kGA106LabSelector_Count = 9`.
- `Info.plist` confirma `CFBundleVersion` = `1.5.3` (árvore 1.5.3) e `1.6.2`
  (árvore 1.6.2), linhas 17–20 de cada `Info.plist`.
- Live-history: `CURRENT-STATE.md` — residente = 1.5.3
  (`1.5.3 TG-KEXT5-GSP-SYSMEM-ALLOC-TEST-PATH-FIX`, `START_SUCCESS`,
  selector 7 executado 1x live); 1.6.2 = "offline PASS, nunca live"
  (pacote em `PRESTAGE-P60-1.6.2/`); selector 8/9 = "OFFLINE candidate / NOT live";
  `STAGING_1_7_0_AUTHORIZED = NO; LIVE_SELECTOR8_AUTHORIZED = NO;
  LIVE_SELECTOR9_AUTHORIZED = NO`.

## 2. Tabela por selector (número, semântica, função fonte, BAR/offsets, writes, side effects, lifetime, copyout, versões, live-history)

Legenda live-history: LIVE = executado no residente ao menos 1x;
NEVER-LIVE = existe em source nunca carregado; OFFLINE = só testado em mock/harness.

### Selector 0 — GetProtocolVersion

- Função fonte: `GA106LabUCGetVersion` (`GA106LabUserClient.cpp:21` em 1.5.3;
  `:108` em 1.6.2/1.7.3/1.8.0). Sem função owner (responde de constantes).
- BAR/offsets lidos: nenhum (sem acesso HW).
- Writes: 0. Side effects: nenhum além de trap userspace→kernel (S15 aplica-se
  a todo selector, mas sem lock/provider/HW aqui).
- Lifetime path: cheques `!arguments||!fOpen||!fOwner` em `externalMethod`;
  sem retain adicional (1.5.3: sem pin; 1.6.2+: pin atômico só nos handlers
  que tocam o owner — ver §9).
- Copyout: escalar (`{0,0,2,0}` na tabela `sMethods[]`), 0 bytes struct.
- Versões: 0–7 em TODAS (1.5.0→1.8.0). Live-history: nunca invocado live
  isoladamente como validação (superfície de D usa presença-OS-level).

### Selector 1 — GetDeviceIdentity

- Função fonte: `GA106LabUCGetIdentity` (1.5.3 `:37`; 1.6.2 `:124`) — lê cache
  imutável do serviço, sem tocar provider/HW no caminho.
- BAR/offsets: nenhum. Writes: 0. Side effects: nenhum HW; só trap.
- Lifetime: mesmo de §0; sem map/provider.
- Copyout: `sizeof(GA106LabIdentityV1)` = **32**.
- Versões: todas. Live-history: leitura de identidade cacheada; não é medida
  do residente (Q10 BLOCKED — cf. `P82-Q10-RESOLUTION.md`).

### Selector 2 — GetServiceState

- Função fonte: `GA106LabUCGetStatus` (1.5.3 `:75`; 1.6.2 `:162`);
  inclui `revision = GA106LAB_DRIVER_VERSION_U32` (string declarada, não medida).
- BAR/offsets: nenhum. Writes: 0. Side effects: nenhum HW.
- Copyout: `sizeof(GA106LabStatusV1)` = **32**.
- Versões: todas. Live-history: idem §1 (não fecha Q10).

### Selector 3 — GetPciSnapshot

- Função fonte: `GA106LabUCGetPciSnapshot` → owner `GA106Lab::copyPciSnapshot`
  (1.5.3 `GA106Lab.cpp:291`; 1.6.2 `:304`).
- Leituras: **PCI config space SOMENTE, via `configRead8/16/32`** —
  Vendor/Device/Command/Status/Revision/Header/BARs raw (SEM write-probe
  `0xffffffff` — proibido em comentário `:363`), Subsys/ROM/Cap/IRQ
  (`GA106Lab.cpp:337-377` em 1.5.3). Nenhum BAR mapeado, nenhum MMIO.
- Writes: 0. Side effects: leituras config-space são non-destructive por
  desenho do protocolo (sem clear-on-read em config space padrão).
- Lifetime: provider via serviço (sem map retido; sem VA exposta).
- Copyout: `sizeof(GA106LabPciSnapshotV1)` = **84**.
- Versões: todas. Live-history: classe OS-adjacente; D usa só presença via
  `kextstat` (P82: leitura nova além de presença = NO sem Q10).

### Selector 4 — BarMapProbe

- Função fonte: `GA106LabUCBarMapProbe` → owner `GA106Lab::probeBar0Map`
  (1.5.3 `GA106Lab.cpp:394`; 1.6.2 `:407`).
- Leituras: Vendor/Device/Command (MSE gate) + `BAR0 raw`
  (`GA106Lab.cpp:441-461`); discovery de resource com match estrito
  (16 MiB + base == BAR0 atual; rejeita VRAM/BAR3/ROM/BAR5 — `:480-482`);
  **mapeia, valida metadata, desmapeia antes de retornar; NUNCA dereferencia
  MMIO, nunca expõe VA** (contrato em `GA106LabProtocol.h`, doc do selector 4).
- Writes: 0. Side effects: cria/destrói mapa temporário (metadata apenas);
  post-read `commandAfter` comparado (`:574` em 1.5.3) — leitura, não write.
- Lifetime: mapa local por chamada, liberado antes do retorno; sem ponteiro
  retido (prova P-D4.1 parcial já documentada no source).
- Copyout: `sizeof(GA106LabBarMapInfoV1)` = **96**.
- Versões: todas. Live-history: NEVER-LIVE como invocação de validação
  (nenhum selector além do 7 foi live).

### Selector 5 — ReadFirstMmioRegister (BOOT_0)

- Função fonte: `GA106LabUCReadFirstMmio` → owner `GA106Lab::readPmcBoot0`
  (1.5.3 `GA106Lab.cpp:591`; 1.6.2 `:604`) → `RunFirstMmioReadFlow`
  (`GA106LabMmioFlow.hpp`).
- Leituras: **BAR0+`kGA106LabMmio_PmcBoot0_Offset` = `0x00000000`**
  (`GA106LabProtocol.h:158`), **1x load 32-bit, 0 stores**
  (`GA106Lab.cpp:590-591`; `GA106LabMmioFlow.hpp:192,211-215`:
  "A ÚNICA leitura (1x load 32-bit, offset fixo 0)", "sempre offset 0 / width 4").
- Writes: 0. Side effects: 1 load MMIO em registrador `NV_PMC_BOOT_0`
  (read-only arquitetural; sem clear-on-read documentado).
- Lifetime: mapa temporário RO+Inhibit por chamada; liberado antes do retorno (create `GA106LabRealOps.hpp:79-90` `mapDeviceMemoryWithIndex(idx, kIOMapReadOnly|kIOMapInhibitCache)`; release-impl `:118-125` `fMap->release(), fVA=0`; success-path `GA106LabMmioFlow.hpp:224-226` `ops.releaseMap(), MapReleased`; error-paths `:151,157,165,174,182,187,198,206` — `releaseMap` antes de cada `return`; owner-wrapper fino `GA106Lab.cpp:591-602`, `ops` stack-local, sem estado retido).
- Copyout: `sizeof(GA106LabMmioReadV1)` = **48**.
- Versões: todas. Live-history: NEVER-LIVE (candidato ROUTE_1 natural —
  já existe no 1.5.3 residente, mas NUNCA invocado; ver VERDICT).

### Selector 6 — ReadStaticIdentity (BOOT_42)

- Função fonte: `GA106LabUCReadStaticIdentity` → owner
  `GA106Lab::readStaticIdentity` (1.5.3 `GA106Lab.cpp:612`; 1.6.2 `:625`)
  → `RunStaticIdentityReadsFlow` (`GA106LabStaticIdentityFlow.hpp`;
  BOOT_2 REMOVIDO por evidência insuficiente, só BOOT_42).
- Leituras: **BAR0+`kGA106LabStaticIdentity_Boot42_Offset` = `0x00000A00`**
  (`GA106LabProtocol.h:201`), 1x load 32-bit; `BOOT_42 CHIP_ID` = bits 29:20
  (`GA106LabCoreValidate.h:166`); allowlist compile-time "somente BOOT_42"
  (`GA106LabRealOps.hpp:104-112`, "Zero stores").
- Writes: 0. Side effects: idem §5 (1 load em registrador estático).
- Lifetime: idem §5 no mecanismo (mesmo `GA106LabRealOps::createMap/releaseMap` `:79-90`/`:118-125`), com release próprio em `GA106LabStaticIdentityFlow.hpp:196` (`MapReleased` `:188`) e error-paths `:135,141,146,153,162,167,174,181`; owner-wrapper fino `GA106Lab.cpp:612-623` (`ops` stack-local).
- Copyout: `sizeof(GA106LabStaticIdentityV1)` = **32**.
- Versões: todas. Live-history: NEVER-LIVE (segundo candidato ROUTE_1).

### Selector 7 — PrepareGspSysmem

- Função fonte: `GA106LabUCPrepareGspSysmem` → owner
  `GA106Lab::prepareGspSysmem` (1.5.3 `GA106Lab.cpp:962`; 1.6.2 `:975`)
  → `RunPrepareGspSysmemFlow` (`GA106LabGspSysmemFlow.hpp`).
- Leituras: sem MMIO de GPU (fluxo de alloc sysmem 1x4096; estado
  `ADDRESS_READY`; `ga106ctl.c:272-280` "só tamanhos/estado (sem endereços)").
- Writes HW: 0 (alloc de memória sysmem, não write em registrador/BAR).
- Lifetime: **o único selector com estado persistente**: mapping LEFT PREPARED
  após a invocação live única (`CURRENT-STATE.md:8`).
- Copyout: `sizeof(GA106LabGspSysmemV1)` = **48**.
- Versões: todas. Live-history: **LIVE — executado EXATAMENTE 1x no 1.5.3
  residente** (`CURRENT-STATE.md`). É o único selector com execução live
  comprovada neste milestone.

### Selector 8 — ReadGfwBootReadiness (CANDIDATA C) ⚠️ NÃO EXISTE EM 1.5.3

- Função fonte: `GA106LabUCReadGfwReadiness`
  (`GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src/GA106LabUserClient.cpp:399`;
  1.7.3 `:444`; 1.8.0 `:516`) → owner `GA106Lab::readGfwBootReadiness`
  (`GA106Lab.cpp:1152`; decl. `GA106Lab.hpp:112`) →
  `RunGfwBootReadinessFlow` (`GA106LabGfwReadinessFlow.hpp`, ONE_SHARED_FLOW).
- Leituras (lista fechada, prova P-D4.2 já extraível do source):
  - gate `NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_PRIV_LEVEL_MASK` **BAR0+`0x118128`,
    bit 0** (`GA106LAB_GFW_GATE_OFFSET 0x000118128u`,
    `GA106LAB_GFW_GATE_MASK 0x1u` — `GA106LabCoreValidate.h:191,193`);
  - progress `NV_PGC6_AON_SECURE_SCRATCH_GROUP_05[0]` **BAR0+`0x118234`,
    bits 7:0** (`GA106LAB_GFW_PROGRESS_OFFSET 0x000118234u`,
    `GA106LAB_GFW_PROGRESS_MASK 0xFFu` — `:192,194`);
  - `0xFF = completed`, demais = not-complete (`GA106LAB_GFW_COMPLETED_VALUE
    0xFFu` — `:195`); progress SOMENTE lido com gate=1
    (`GA106LabGfwReadinessFlow.hpp` §11d); bounds `0x118234+4` dentro do mapa,
    overflow-safe (§9); pré-gates: identidade vendor/device, **MSE on**
    (aborta se off — §4), BAR0 raw válido, discovery estrito 16MiB+base.
- Polling: **bounded duplo — contagem `GA106LAB_GFW_POLL_MAX_ITERS 4000`
  E deadline monotônico `GA106LAB_GFW_POLL_TIMEOUT_NS 4 s`** (`:197-198,205`;
  intervalo 1 ms — `:196,204`); stop-check por iteração (aborta sem travar);
  espera orçada fora de lock. Máx. leituras: 4000 gate + 4000 progress =
  8000 (`GA106LAB_GFW_MAX_TOTAL_READS` — `:201`).
- Writes: **0 stores de qualquer tipo** (contrato em `GA106LabProtocol.h:270-300`;
  `ZERO-WRITE-PROOF` por construção do flow: só `read32` via `GfwRealOps`;
  `build.sh:476` tem asserção anti-`prepareGspSysmem/completeDma/clearDma`
  no caminho GFW). Side effects a auditar (P-D4.3): MMIO loads em
  registradores PGC6 AON scratch (sem clear-on-read documentado em
  `evidence/upstream-gfw/UPSTREAM-EVIDENCE.md` — a re-verificar contra fonte
  primária Nova/OpenRM antes de qualquer janela); sem doorbell/handshake/IRQ
  no caminho (sem enable/mask de IRQ; `waitBudgetedNs` fora de lock).
- Lifetime path: reserva antecipada (`setBusy` antes de qualquer acesso) +
  provider retido (`acquireProvider`) + `releaseMap/releaseProvider` em TODOS
  os exits (`GFW_FAIL_RELEASE_ALL`); UserClient com **pin atômico de lifetime
  (PROMPT 57 §3)**: `PinOwnerForExternalMethod` + `ReleasePinnedOwner` em
  todos os exits (`GA106LabUserClient.cpp:418-453`); `stop()` drena `busy`
  (comentário `:573`). **Este modelo de pin NÃO EXISTE na árvore 1.5.3**
  (lá: `retain` simples em `start` / `release` em `stop`, `:353,385` —
  sem `PinOwner`, sem `isStopping/isBusy` no UserClient).
- Copyout: `sizeof(GA106LabGfwReadinessV1)` = **32** (struct-output exato;
  zero-input: `scalarInputCount==0 && structureInputSize==0` exigidos — `:414`).
- Disponibilidade: **1.6.0, 1.6.1, 1.6.2, 1.7.0–1.7.3, 1.8.0. AUSENTE em
  1.5.0–1.5.3.** Live-history: **NEVER-LIVE** (1.6.2 "nunca foi live",
  `CURRENT-STATE.md:26`; `LIVE_SELECTOR8_AUTHORIZED = NO`).
- CLI: `ga106ctl read-gfw-boot-readiness` existe SÓ a partir de 1.6.2
  (`ga106ctl.c:303-316` em 1.6.2; ausente em 1.5.3 cujo `usage` termina em
  `prepare-gsp-sysmem` — 1.5.3 `ga106ctl.c:306-344`).

### Selector 9 — VerifySysmemFlushPreconditions (Gate A)

- Função fonte: `GA106LabUCVerifyFlushPrewrite`
  (1.7.3 `GA106LabUserClient.cpp:399`; 1.8.0 `:471`); output
  `GA106LabFlushPrewriteV1` **48B** (`SELECTOR9-ABI.md` em 1.8.0:
  `VerifySysmemFlushPreconditions = 9`, Count = 10; `programmed=0,
  writeCount=0, readCount=0` por desenho — Gate A read-only).
- Writes: 0 por desenho (pré-condição de flush, não o flush).
- Disponibilidade: **1.7.0–1.7.3, 1.8.0. AUSENTE em ≤1.6.2 e em 1.5.3.**
- Live-history: **NEVER-LIVE** (`LIVE_SELECTOR9_AUTHORIZED = NO`).

## 3. Hashes pinados (recomputados offline via `shasum -a 256`, conferem com `SHA256.txt`)

1.5.3 (`GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/`):

- `GA106LabProtocol.h` `2e39a274a744a299dea6a16ca1b55e39776c7bdc131c56c2f588b7150732b0c6`
- `GA106LabUserClient.cpp` `aed3d33b715c046c7601eed004920823ebb7bbc13baa0d95c090e9db871a6a81`
- `GA106Lab.cpp` `bec65dcd02bae6dc5ead613eadce42e155497609ff7e11e551b7113a5728f5f7`
- `Info.plist` `4e9d6335a90021a34367e2538069ab6a776f1c09e3ab7793833c587e10c25aee`

1.6.2 (`GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src/`):

- `GA106LabProtocol.h` `055016d9ea4d073e1f6da976409f2f9507dff43bf89b3913eb8a0ad7eab4641d`
- `GA106LabUserClient.cpp` `2d600648ed04cfba83804c5fbffb72e28b8bf59d8292d7b222c46f04b819d533`
- `GA106Lab.cpp` `b989c122d04db115d3d2f195b345861e8347ac1a38af73506ddbcee4ea3913ac`
- `GA106LabGfwReadinessFlow.hpp` `fd630ee7b3d61cd4e5d725073da586595bf08384ee7b0eb40067a8c31e1a2214`
- `GA106LabCoreValidate.h` `a18ac8a1ebd26fcf6a2d303138afc690e280f30ed4c575a2cab9b1abdef533b8`
- `Info.plist` `97626d82fb9f869154c601c53e67c73d211fc30f46b6e5fbc9584ee82091553b`

## 4. Conclusão do mapa

O selector 8 (`ReadGfwBootReadiness`) nasce na geração 1.6.0 e estabiliza em
1.6.2; **não existe em nenhuma árvore 1.5.x**. O residente live (1.5.3,
`Count = 8`) rejeitaria `selector >= Count` com `kIOReturnBadArgument`
(`GA106LabUserClient.cpp:404` em 1.5.3) — C, como desenhada, **não é
invocável no estado live atual**. Ver
`P82_1-EXECUTABILITY-VERDICT.md` (Caso B).
