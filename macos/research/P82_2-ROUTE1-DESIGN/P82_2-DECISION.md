# P82.2 — ROUTE_1 Decision (UMA decisão)

```text
SESSION_ID: P82-2-ROUTE1-RELAY-V3
MILESTONE: P82_2_ROUTE1_DESIGN_SELECTORS_5_6 (OFFLINE ONLY)
STATE: MUSE_WORK / ACTOR: MUSE_BUILD
LIVE_EXECUTION_AUTHORIZED = NO (live=0; esta decisao NAO autoriza nada)
```

## 1. Decisão (única)

```text
ROUTE1_DECISION = R1-D NO_GO_REDUNDANT
ROUTE1_CANDIDATE = NO_GO_REDUNDANT
RECOMMENDED_NEXT_ROUTE = FIRST_CONTROLLED_LOAD_MILESTONE
```

R1-A (SELECTOR5), R1-B (SELECTOR6) e R1-C (SELECTOR5_THEN_6) foram
consideradas e **rejeitadas**: ambas as leituras têm PASS histórico
(BOOT0 0xb76000a1/0x176; BOOT42 0x176a1000/0x176), valor hoje
`DUPLICATES_HISTORICAL_EVIDENCE` / LOW (ver `P82_2-EVIDENCE-VALUE.md`),
contra risco `LIFETIME HIGH-OPEN` em 1.5.3 sem pin (ver
`P82_2-LIFETIME-RISK.md`) + `clear-on-read OPEN` + gates P8/P9/P3
vermelhos (ver `P82_2-GATES.md`). R1-C é duplamente rejeitada: soma dois
riscos HIGH-OPEN por duas duplicatas (2 leituras necessárias nunca
demonstradas — cada uma basta para "MMIO-BAR0 funciona"; a segunda não
adiciona fato novo). **Mesmo se R1-A/B/C: NÃO executar, NÃO auto-autorizar,
PARAR para decisão humana** (nenhuma janela abre neste turno).

## 2. Por que R1-D (síntese)

- Evidência: 5/6 esgotados (BOOT0/BOOT42 COMPLETE); re-ler = duplicar.
- Risco: HIGH-OPEN (stop-race/UAF 1.5.3) não compensado por LOW redundante.
- Gates: P8/P9/P3 BLOCKING — R1-A/B/C estariam BLOCKED ainda que valiosas.
- Futuro: o próximo fato novo real (GFW readiness selector 8, pin de
  lifetime para 5/6, ou qualquer semântica nova) exige binário novo →
  stage/load controlado, não re-leitura do residente.

## 3. Próximo milestone desenhado (NÃO executar): FIRST_CONTROLLED_LOAD_MILESTONE

Escopo-desenho (milestone próprio futuro, com review + autorização humana
dedicada; NADA autorizado aqui):

1. **Objeto**: stage + load supervisionado de UM binário já auditado
   (candidato: 1.6.2 `GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src/`,
   Count 9, selector 8 GFW readiness + pin atômico; alternativa 1.7.x/1.8.0
   só com re-auditoria P-D4.0..P-D4.5 + hash pin + diff 1.6.2→alvo).
2. **Pré-condições duras** (todas VERDES antes de abrir janela, sem
   substituição silenciosa): P8 Option 0 non-target PASS publicado +
   segunda conferência; P9 reviewer + alternativa nomeados + SLA 7d; P3
   watchdog/serial externo pinado capaz de observar hang sem o alvo;
   re-exec `shasum -a 256` na árvore exata + bundle; prova Nova/OpenRM de
   ausência de clear-on-read nos offsets do plano; baseline HW GA106
   (BME/DMA/BARs/GSP-ausente) registrada; Q10 continua BLOCKED (nenhum PASS
   alega identidade residente); forma F-AUTH pinada 1-uso assinada.
3. **Plano de janela** (forma, não autorização): 1 boot supervisionado do
   binário novo → IDENT pré → 1 invocação mínima validada (a definir no
   milestone; candidata: selector 8 bounded OU 5/6 com pin backportado —
   UMA, não sequência) → IDENT pós → SAVE externo + INDEX com shasum →
   unload/teardown T3/T4 ensaiado + rollback para 1.5.3 publicado.
   Qualquer panic/hang/timeout/2ª invocação/mount-EFI extra = ABORT +
   FAIL sem atribuição + review antes de reabrir.
4. **Critério de sucesso do milestone**: pipeline stage/load/unload/teardown
   provado com evidência portátil — NÃO "PASS do selector" isolado.
   `TimedOut` de GFW-em-GA106-sem-FW = resultado protocolar esperado, nem
   PASS de safety nem FAIL do KEXT sem baseline (P82.1 advisory 15).

```text
FIRST_CONTROLLED_LOAD_AUTHORIZED = NO
STAGE_AUTHORIZED = NO / LOAD_AUTHORIZED = NO / UNLOAD_AUTHORIZED = NO
SELECTOR_LIVE_AUTHORIZED = NO (qualquer)
```

## 4. Contadores

```text
LIVE_ACTIONS = 0 / KEXT_LOAD_ACTIONS = 0 / SELECTOR_LIVE_ACTIONS = 0
MMIO_ACTIONS = 0 / BME_ACTIONS = 0 / DMA_ACTIONS = 0 / GSP_ACTIONS = 0
COMMAND_SUBMISSION_ACTIONS = 0 / METAL_ACTIONS = 0 / REBOOT = 0 / EFI = 0
```
