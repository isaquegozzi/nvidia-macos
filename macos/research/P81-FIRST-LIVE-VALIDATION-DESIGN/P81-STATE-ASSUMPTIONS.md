# P81 — State Assumptions (cada uma com status e fonte)

```text
P81_EXECUTION_AUTHORIZATION = NONE / LIVE_EXECUTION_AUTHORIZED = NO
Regra: tudo NÃO-PROVADO permanece NÃO-PROVADO até evidência live futura com review.
```

| # | Assumption | Status | Fonte |
|---|---|---|---|
| S01 | P77+P78+P79+P80 APPROVED offline | PROVADA (offline) | `P77/P78/P79-FINAL-HANDOFF.md`, `P80-FINAL-HANDOFF.md` + reviews Gemini citados neles |
| S02 | Candidato 1.8.0 == hashes P80 (kext/plist/cli/build.sh/manifest) | PROVADA (offline, read-only) | `P80-FINAL-HANDOFF.md` hashes + `MANIFEST-P80-1.8.0.txt`; reconfirmável por `shasum -c` (não executado neste turno) |
| S03 | Árvore 1.7.3 intocada (452/452) | PROVADA (offline, read-only) | `baseline_173.sha256` citada em `P80-FINAL-HANDOFF.md` |
| S04 | GA106Lab 1.5.3 estava carregado no live histórico | PROVADA (histórico) | Evidências live TG-KEXT* / kextstat históricos; NÃO afirma estado atual |
| S05 | Estado atual do HW/OS (quem está carregado, BME, BAR, GSP, PUT/GET, VRAM, IRQ) | NÃO-PROVADA (persiste após PASS: veredito carrega `GPU-STATE = UNKNOWN`, PASS-6) | Nenhuma coleta executada neste turno; qualquer afirmação sobre "agora" exige coleta futura autorizada; 15 min sem panic não medem panic-rate (S13) |
| S06 | Dois panics Tahoe recentes ocorreram | PROVADA (histórico, fonte indireta) | Assinaturas IPC/UAF e `_copyinmsg`/GP em `P80-LIFETIME-SAFETY-AUDIT.md:7` (sem nomes de processo); atribuições `mediaanalysisd`/`opencode` só em docs P81, sem `*.panic` log no repo; `P80-FINAL-HANDOFF.md` NÃO menciona panics (fonte anterior não verificava — corrigida em M0242, P81-GLM-RISK-01) |
| S07 | Causa dos panics / ligação com GA106Lab | NÃO-PROVADA (e NÃO atribuída) | Sem evidência; ver `P81-PANIC-RISK-ASSESSMENT.md`. Proibido atribuir |
| S08 | gpuRestarts AMD Renoir/Vega antigos ligam-se ao GA106Lab | NÃO-PROVADA (e NÃO atribuída) | GPUs diferentes (AMD vs NVIDIA), contexto distinto; sem evidência |
| S09 | 1.8.0 é "live-ready" / "seguro para stage/load" | NÃO-PROVADA (falso até prova) | P80 declara OFFLINE ONLY, teto `LiveBlocked`; nenhum stage/load executado (contadores 0) |
| S10 | Lifetime/teardown (UAF/race) seguro live | NÃO-PROVADA live | PROVADA apenas offline (doubles+pthreads+TSAN, `P80-LIFETIME-SAFETY-AUDIT.md`); kernel live ≠ doubles |
| S11 | Channel/VM/submission models valem para HW real | NÃO-PROVADA live | Contratos provados contra oracles de simulação offline; semântica de engine real fora de escopo |
| S12 | EFI/config atual == known-good documentado | NÃO-PROVADA (identidade parcial do alvo INSUFICIENTE para qualquer write futuro) | Nenhum hash de EFI coletado neste turno; verificar (read-only, sem mount) é parte do teste futuro, não premissa; hash-ESP de conteúdo SÓ no Option 0 non-target (gate duro P8); fallback nunca escala para mount no alvo |
| S13 | Sistema Tahoe tem baseline de panic-rate conhecida | NÃO-PROVADA | Exige boot de controle futuro (opção B) para estimar; hoje só há 2 amostras |
| S14 | Ferramentas de coleta OS-level são de baixo risco | PROVADA-CONDICIONAL (rebaixada em M0244: `log show`/`ioreg` usam logd/IPC sob suspeita T7; sem artefato, sem fonte externa) | `ioreg/kextstat/log show/nvram -p` são interfaces read-only do SO, MAS `log show` depende de logd/IPC — o subsistema sob suspeita nos panics UAF/`_copyinmsg`. Fallback G-M0244-12: se `log show` travar (> 60 s), PULAR LOGS com nota, sem improvisar. Risco residual T7 declarado. (AL-3.) |
| S15 | Leitura de selectors do KEXT carregado é "read-only inofensiva" | NÃO-PROVADA | Mesmo selector read-only executa código do KEXT no kernel; NÃO presumir segurança; por isso o teste proposto evita até selectors (ver opção D) |

Consequência direta: o menor teste válido NÃO pode presumir S05/S07/S09/S10/S11/S12/S13/S15.
Ele só pode usar S01–S04/S06/S14-condicional. PASS de D/A/B nunca altera este quadro: GPU-STATE segue
UNKNOWN e S12-parcial segue insuficiente para writes (R1/R2, M0243).
