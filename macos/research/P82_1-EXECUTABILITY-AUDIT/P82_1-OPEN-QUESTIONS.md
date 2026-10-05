# P82.1 — Open Questions (pós-auditoria de executabilidade; honestas, em aberto)

```text
SESSION_ID: P82-1-AUDIT-RELAY-V3
P82_LIVE_EXECUTION_AUTHORIZED = NO (live=0)
```

1. **ROUTE_1 (selector 5/6 em 1.5.3) tem critério de "side-effect-free" fechado?**
   Falta auditoria P-D4.0..P-D4.5 da árvore 1.5.3 exata (hashes no SELECTOR-MAP §3),
   incluindo prova de ausência de clear-on-read em BOOT_0/BOOT_42 e análise
   locks/IRQ do caminho 1.5.3 (sem pin atômico). Quem audita e quem confere?
2. **BOOT_0/BOOT_42: onde termina "identidade estática" e começa "estado de boot"?**
   Lista fechada de offsets do 1.5.3 (0x0, 0xA00) é mínima, mas a semântica
   "estático" precisa de fonte primária (Nova/OpenRM) como a de C tem para
   `0x118128/0x118234`. Sem fonte, leitura é FILOSOFICAMENTE read-only, não
   PROVADAMENTE semântica-fixa.
3. **Q10 BLOCKED contamina ROUTE_1?** SIM, parcialmente: ROUTE_1 prova
   comportamento do caminho MMIO, nunca "qual binário respondeu". Qual é o
   teto de claim aceitável para um PASS de ROUTE_1 sem reabrir Q10?
4. **Quantas janelas ROUTE_1 até a evidência bastar?** Herdar a proposta P82
   (3 D PASS + auditoria + 1 PASS antes de discutir além; FAIL/ABORT zera) ou
   recalibrar para loads únicos? Sem power analysis, qualquer número é arbitrário.
5. **P3: qual meio concreto (câmera/serial/watchdog) para headless?** Meio
   substituível, existência não. Quem provê o hardware e quem pina o ID em
   F-P82-AUTH-06? Sem dono, o gate fica vermelho para sempre.
6. **P9: quem são reviewer + alternativa?** Sem dois nomes, tudo (D, C,
   ROUTE_1, ROUTE_2) permanece NO-GO. Escalar para o humano: indicar nomes ou
   declarar o programa live pausado por falta de reviewer.
7. **P8: existe non-target físico disponível?** Sem máquina secundária, Option 0
   é impossível e D nunca abre — e sem D, ROUTE_1 nunca abre. Declarar
   disponibilidade ou pausar o programa live explicitamente.
8. **ROUTE_2: 1.8.0 vs 1.6.2 mínima?** 1.8.0 tem barreira fail-closed (13 blocks)
   mas é a maior distância do residente; 1.6.2 é o menor delta (+selector 8)
   mas sem blocks/lifetime P80. Critério de escolha a pinar em design futuro.
9. **1.7.x deve ser descartado como veículo?** Histórico Astra (1.7.0 FAIL D,
   1.7.1 reaudit pendente) + Gate A nunca-live sugerem SIM, mas o descarte
   formal exige decisão humana registrada — não tomada neste turno.
10. **O mapping LEFT PREPARED do selector 7 afeta ROUTE_1/ROUTE_2?** O estado
    persistente do único selector live (1x4096) é pré-condição de qualquer
    caminho futuro (inclusive Gate A). Baseline desse estado foi documentada?
11. **Este audit envelhece?** SIM: P82.1-AUDIT-V1 (2026-09-09). Qualquer mudança
    macOS/EFI/HW/GPU/cabos/KEXT invalida hashes e veredito; re-auditoria exigida
    antes de qualquer janela. Auditoria em geração errada = FAIL.
12. **P82-C deve ser formalmente ARQUIVADA ou mantida como design de ROUTE_2?**
    Recomendação deste audit: arquivar C-como-primeiro-passo (NO-GO estrutural
    no live atual); reaproveitar sua auditoria de selector 8 (P-D4.0..P-D4.5 já
    extraível do source 1.6.2) como entrada do futuro milestone de stage/load.
    Decisão humana pendente.
13. **[SMOKE-ADVISORY P82-1-SMOKE-01] Lifetime 1.5.3 mais fraco (sem pin) — stop-race/UAF antes de qualquer janela.** ROUTE_1 herdaria caminho sem pin atômico: 1.5.3 `GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/GA106LabUserClient.cpp:353,385` (`retain`/`release` simples, sem `PinOwner`/`isStopping`/`isBusy`); pin atômico afirmado só em 1.6.2+ (`GA106LabUserClient.cpp:418-453` + `GFW_FAIL_RELEASE_ALL`). Superfície UAF/stop-race de 5/6-na-1.5.3 é maior que 8-na-1.6.2 e segue não quantificada (cf. `Conversations/_relay_v3/subagents/P82-1-SMOKE-01_muse_smoke.md`, Extra red-team lifetime; `P82_1-EXECUTABILITY-VERDICT.md` ROUTE_1 §3). Regra: antes de qualquer janela ROUTE_1, exigir análise locks/IRQ do caminho 1.5.3 exato + quantificação UAF; sem isso, ROUTE_1 permanece NO-GO.
14. **[SMOKE-ADVISORY P82-1-SMOKE-01] Clear-on-read sem fonte primária (BOOT_0/BOOT_42 e 0x118128/0x118234).** `GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src/evidence/upstream-gfw/UPSTREAM-EVIDENCE.md:9-25,36-40,52-64` prova identidade de registrador + `0xff=completed` + ordem gate-antes-status + poll 1ms/4s + aplicabilidade GA106-via-TU102, mas NADA sobre clear-on-read de `0x118128`/`0x118234` ou BOOT_0/BOOT_42 (cf. `Conversations/_relay_v3/subagents/P82-1-SMOKE-01_muse_smoke.md`, Extra red-team UPSTREAM + UNPROVEN_ASSUMPTIONS). Regra: antes de qualquer janela, exigir prova contra fonte primária Nova/OpenRM de ausência de clear-on-read em BOOT_0/BOOT_42 e `0x118128`/`0x118234` (offsets cf. `GA106LabCoreValidate.h:191-198,205`); leitura sem fonte é FILOSOFICAMENTE read-only, não PROVADAMENTE semântica-fixa.
15. **[SMOKE-ADVISORY P82-1-SMOKE-01] TimedOut nunca é upgrade para Completed nem FAIL do KEXT sem baseline HW.** GA106 sem GSP/FW tem como resultado esperado de C `TimedOut`/`not-complete`, não `Completed`; UPSTREAM diz `TIMEOUT arbitrarily large` e `gate-means-access (não readiness)` (cf. `Conversations/_relay_v3/subagents/P82-1-SMOKE-01_muse_smoke.md`, Extra red-team GA106). Regra explícita: proibir ler `TimedOut` como PASS de safety e proibir ler `TimedOut` como FAIL do KEXT sem baseline HW + critério de interpretação pinado; sem baseline, `TimedOut` é apenas `TimedOut`.
16. **[SMOKE-ADVISORY P82-1-SMOKE-01] shasum do MAP §3: conferência foi textual, re-execução exigida antes de qualquer janela.** Par crítico confere byte-a-byte MAP-vs-SHA256.txt — 1.5.3 `P82_1-SELECTOR-MAP.md:227-232` vs `GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/SHA256.txt:1,3,5,6`; 1.6.2 `P82_1-SELECTOR-MAP.md:234-241` vs `GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src/SHA256.txt:1,3,5,6,7,12` — mas `shasum -a 256` NÃO foi re-executado por este smoke (read-only sem execução; cf. `Conversations/_relay_v3/subagents/P82-1-SMOKE-01_muse_smoke.md`, UNPROVEN_ASSUMPTIONS; pin seletivo: 4 de 32 linhas 1.5.3, 6 de 45 linhas 1.6.2; build/obj e evidence/*.rs não pinados). Regra (não execução agora): antes de qualquer janela, re-executar `shasum -a 256` sobre a árvore exata e arquivar a saída.
17. **[SMOKE-ADVISORY P82-1-SMOKE-01] Expiry/re-auditoria: qualquer mudança invalida; anti contaminação cross-generation.** Este audit é P82.1-AUDIT-V1 (2026-09-09). Regra: qualquer mudança macOS/EFI/HW/GPU/cabos/KEXT invalida hashes e veredito e exige re-auditoria na geração exata antes de qualquer janela; auditoria em geração errada = FAIL (cf. OPEN Q11; `Conversations/_relay_v3/subagents/P82-1-SMOKE-01_muse_smoke.md`, RECOMMENDED_ACTION re-auditar). Tabela MAP para 1.5.0-1.5.2/1.6.0/1.6.1/1.7.x/1.8.0 não foi re-verificada pelo smoke (confiança herdada) — não citar como re-verificada.
18. **[SMOKE-ADVISORY P82-1-SMOKE-01] Proibição de upgrade semântico: REGISTER_SEMANTICS PASS e AMFI load-time.** `REGISTER_SEMANTICS_PRIMARY_EVIDENCE=PASS` nunca citar como MMIO-safety (false-pass path por upgrade semântico para leitor apressado); harness P81.2 `20/20 MATCH` nunca citar como KEXT-safety nem MMIO-safety (`P82_1-GATE-STATUS.md:54-71`, `HARNESS_IS_NOT_LIVE_EVIDENCE=YES, pipeline PASS != KEXT safety != MMIO safety`); AMFI load-time nunca citar como atenuante de risco live (cf. `Conversations/_relay_v3/subagents/P82-1-SMOKE-01_muse_smoke.md`, Extra red-team UPSTREAM + RECOMMENDED_ACTION proibir citar).
