# P82.2 — Lifetime Risk 1.5.3 (selectors 5/6, sem suavização read-only)

```text
SESSION_ID: P82-2-ROUTE1-RELAY-V3
MILESTONE: P82_2_ROUTE1_DESIGN_SELECTORS_5_6 (OFFLINE ONLY)
STATE: MUSE_WORK / ACTOR: MUSE_BUILD
LIVE_EXECUTION_AUTHORIZED = NO (live=0)
```

Regra deste arquivo: risco NÃO é suavizado por ser read-only (1.5.3 sem
hardening posterior; S15: selector read-only executa código no kernel).

## 1. Respostas obrigatórias

```text
SELECTOR5_1_5_3_LIFETIME_RISK = HIGH (stop-race/UAF nao quantificado; sem pin; sem dreno de chamada em voo no caminho 5)
SELECTOR6_1_5_3_LIFETIME_RISK = HIGH (idem; mesmo dispatch/wrapper/mecanismo — offset distinto nao reduz risco)
KNOWN_1_6_2_LIFETIME_FIX_RELEVANT_TO_SELECTOR5 = NO
KNOWN_1_6_2_LIFETIME_FIX_RELEVANT_TO_SELECTOR6 = NO
```

## 2. Por que HIGH (1.5.3, caminho 5/6 exato)

- Dispatch sem pin: `externalMethod` checa `!arguments||!fOpen||!fOwner` e
  despacha; handler resolve `owner = getOwnerService()` (ponteiro cru, sem
  retain próprio) e usa por toda a chamada (`readPmcBoot0` /
  `readStaticIdentity` → map → load → release → copyout).
- Teardown sem dreno para este caminho: `clientClose`/`clientDied`/`stop()`
  limpam `fOpen`, chamam `clientClosed()` e dão `release()`; o único
  dreno (`busy` + espera + teardown central) pertence ao caminho
  sysmem/selector 7 e ao `stop()` para sysmem — o caminho MMIO 5/6 não
  marca busy nem espera. `fSlotLock` serializa open/close, NÃO chamada em voo.
- Janela concreta: thread A em `read32`/mapa com `owner`/`pci` crus × thread B
  em `stop()`/`terminate` (unload/detach, morte do client, teardown do
  provider) → UAF de `GA106Lab`/`IOPCIDevice`/`IOMemoryMap` no meio do load ou
  do `releaseMap`/copyout. Probabilidade BAIXA em uso single-shot supervisionado
  (slot exclusivo, 1 invocação, sem concorrência intencional), severidade
  ALTA (UAF em kernel = panic/corrupção). Risco = HIGH até quantificação +
  mitigação revisada — "1 load rápido" estreita a janela, NÃO a fecha.
- Sem `isInactive`/`isStopping` no handler 5/6 1.5.3; sem retain do provider
  além do `ops(pci)` stack-local (o `pci` vem de `getProvider()` sem pin).

## 3. Por que o fix 1.6.2 é NO (para 5/6)

- O fix existe e é real, mas é **escopado ao selector 8**: em
  `GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src/GA106LabUserClient.cpp:418-453`,
  só o handler GFW usa `PinOwnerForExternalMethod` + `ReleasePinnedOwner` em
  todos os exits (+ `setBusy` antecipado + `acquireProvider` + `GFW_FAIL_RELEASE_ALL`
  no owner). Os handlers 5/6 **na mesma árvore 1.6.2** (`:275-360`) continuam
  com `owner = self->getOwnerService(); if (!owner) NotOpen` — idênticos ao
  1.5.3, SEM pin.
- Logo o fix 1.6.2, como landed, NÃO mitiga 5/6 em nenhuma árvore existente.
  Torná-lo relevante exigiria **backport do pin para os handlers 5/6** (novo
  source + novo KEXT + stage/load + re-auditoria + review) — isto é um futuro
  milestone de stage/load, NÃO um atenuante do residente 1.5.3. Citar o fix
  1.6.2 como atenuante de janela 5/6-na-1.5.3 é proibido (upgrade indevido).

## 4. O que fecharia este risco (design, NÃO executar)

Backport do pin atômico aos handlers 5/6 + busy-drain para o caminho MMIO +
`isInactive`/`isStopping` checks + prova de balanceamento retain/release 1:1 em
todos os exits + re-auditoria P-D4.1 + review — tudo dentro do futuro
`FIRST_CONTROLLED_LOAD_MILESTONE` (novo binário, stage/load dedicados). Sem
isso, qualquer janela 5/6-na-1.5_3 carrega `LIFETIME = HIGH-OPEN`.
