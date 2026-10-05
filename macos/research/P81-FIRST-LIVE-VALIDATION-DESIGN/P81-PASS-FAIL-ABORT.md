# P81 — Pass / Fail / Abort + Timeouts (exato)

```text
P81_EXECUTION_AUTHORIZATION = NONE / LIVE_EXECUTION_AUTHORIZED = NO
```

## PASS (conjunção — todos obrigatórios)
- PASS-1: janela total ≤ 15 min, ordem T+00..T+10 seguida sem improvisação.
- PASS-2: todos os artefactos de EVIDENCE salvos, legíveis, com timestamps (FOTO sem relógio visível = OK com nota em INDEX.md, desde que E02 traga o timestamp da janela; só é FAIL-1 se faltar o artefacto, estiver ilegível pelo bound FOTO, ou sem nenhuma fonte de timestamp). E02 (data/hora do SO) + EXIF DateTimeOriginal da câmera (leitura nomeada: `mdls -name kMDItemContentCreationDate` ou `sips -g dateTimeOriginal`; `stat` do arquivo = auxiliar, sem valor probatório) são OBRIGATÓRIOS em TODAS as FOTOs (E01/E08/E11) — foto sem E02 válido e sem EXIF = FAIL-1 (E02 válido = G-M0244-01-R1 PASS com B1+B2(sanidade)+B3(EXIF ≤ 120 s, único detector; step intra-gate NÃO coberto), P81.1 read-only; `sntp -sS` PROIBIDO). (R1/F1/F4.)
- PASS-3: IDENT-2 (T+02) == T+08 (repetição): mesma lista de KEXTs relevantes, sem diffs fora da ALLOWLIST (§ Allowlist de diffs abaixo), conferido por CHECKLIST-KEXTSTAT linha-a-linha (M0248/F9): 2 OLHOS (operador + segunda pessoa, ambos assinam) OU `diff -u E03 E09` exato com saída arquivada; sem checklist/saída arquivada, PASS NÃO declarado. Qualquer diff em linha de KEXT (aparecer/sumir/mudar versão ou índice, incl. GA106Lab) = nunca explicável = FAIL-3.
- PASS-4: zero panic / hang / no-signal / reboot espontâneo na janela.
- PASS-5: checklist CLOSE assinado ("nada alterado, nada improvisado") + carimbo PASS-OB observabilidade (ver Efeito de PASS).
- PASS-6 (CHECK GPU-UNKNOWN persistente): o veredito PASS registra explicitamente `GPU-STATE = UNKNOWN` (T5/S05). PASS nunca afirma BAR/BME/DMA/GSP/PUT/GET/VRAM/IRQ — 15 min sem panic não medem panic-rate (S13). (R1/F1.)
- Efeito de PASS: registra baseline + pipeline OK da JANELA. Carimbo obrigatório em CLOSE: "PASS afirma SÓ observabilidade OS-level desta janela; NÃO afirma HW/EFI/KEXT além disso; NÃO autoriza A/B/C, stage/load, writes, nem repetição automática; NÃO ler PASS como live-ready/stage-ready." Próximo passo (A) exige decisão + review próprios.

## FAIL (disjunção — qualquer um)
- FAIL-1: artefacto faltante/ilegível (pelo bound FOTO G-M0244-03)/sem nenhuma fonte de timestamp. Foto duvidosa/borderline = FAIL-1 automático, operador NÃO auto-julga. (FOTO sem relógio visível, com E02 válido (= G-M0244-01-R1 PASS) + EXIF + nota, NÃO é FAIL-1 — exceção só para relógio-na-foto, nunca para nitidez.) `stat` citado/usado como B3 no lugar do EXIF = FAIL-1 explícito (M0248/F9; comparador EXIF único, ver EVIDENCE FOTO). INDEX sem `INDEX.sha256` separado assinado = FAIL-1 (M0248/F9).
- FAIL-2: comando read-only com erro persistente após 1 retry bounded.
- FAIL-3: diff IDENT-2 vs T+08 fora da ALLOWLIST, ou sem explicação registrada.
- FAIL-4: panic/hang/no-signal/reboot na janela (evidência, SEM atribuição de causa). PRECEDÊNCIA: se o mesmo evento dispara ABORT-HEALTH e FAIL-4, veredito = ABORT (ABORT-HEALTH > FAIL-4) e aplica-se o recovery path do ABORT (foto, VERIFY-pós-ABORT só se houver vídeo, SAVE parcial `ABORTED-`, CLOSE com motivo, sem retry no dia).
- Efeito de FAIL: janela encerrada como FAIL; evidência parcial preservada marcada `FAILED-`; proibido "completar rapidinho"; nova tentativa só com novo review do FAIL (§ Review do FAIL). Caminho FAIL declara estado pós-janela DESCONHECIDO (ver § Pós-janela: PASS-limpo vs FAIL/ABORT).

## ABORT (exato — condições e ação)
Condições (qualquer uma):
- ABORT-VIDEO: sem vídeo 5 min após ligar/iniciar.
- ABORT-HANG: qualquer comando > 60 s; 1 retry bounded de 60 s; persistindo => ABORT.
- ABORT-SCOPE: qualquer sugestão de stage/load/unload/selector/write (T9) => ABORT imediato.
- ABORT-HEALTH: panic/hang/no-signal/reboot espontâneo (inclusive no-signal mid-test, não só sem-vídeo inicial) => ABORT + recovery path. Precede FAIL-4 (ver acima).
- ABORT-HUMAN: operador inseguro/confuso/cansado/pressionado => ABORT sem justificar.
Ação (ordem):
1. Parar no passo atual (não "terminar o comando").
2. FOTO-abort (tela + LEDs) com E02 + metadados de câmera (obrigatórios).
3. Se vídeo presente E após coleta mínima (FOTO-abort + anotação de hora/sintoma em papel, sem diagnóstico): 1× VERIFY-pós-ABORT (IDENT-2 pelo comando-filtro G-M0244-04, bound 60 s) + SAVE parcial `ABORTED-`. ATRIBUIÇÃO AMBÍGUA DECLARADA (G-M0244-11): se este VERIFY desencadear segundo panic, a causa é INATRIBUÍVEL por escrito (pode ser estado preso prévio ou o próprio VERIFY); registra-se como ABORT-HEALTH com nota de ambiguidade, sem diagnóstico. VERIFY-pós-ABORT é a ÚNICA execução permitida pós-ABORT (bound 60 s, só se vídeo presente + coleta mínima); sem vídeo ou sem coleta mínima, nenhuma verificação é executada e o estado segue DESCONHECIDO. (R5/F5; AL-2.)
4. CLOSE com motivo do abort. Sem retry no mesmo dia.

## Pós-janela: PASS-limpo vs FAIL/ABORT (R4/F5)
- Caminho PASS limpo (PASS-1..PASS-6 todos OK): "sistema idêntico ao pré" vale — por construção zero-write, verificado por IDENT-2 == T+08.
- Caminho FAIL / ABORT-HEALTH: estado pós-janela = DESCONHECIDO; HW possivelmente preso (latch/hang pré-existente ou induzido, T2/T5); reboot ALTERA logs (novo panic log, contadores NVRAM, uptime) — logo a afirmação "idêntico ao pré" é FALSA aqui. Coleta pós-reboot (fotos LEDs + panic log novo + IDENT-1..4 bounded) = NOVA janela com review próprio (§ Review do FAIL), nunca continuação da janela abortada.

## Allowlist de diffs explicáveis (R1/F4 — fecha OPEN Q9; G-M0244-04 DURA, M0244)
- Comando-filtro EXATO (único admitido para E03/E09): `kextstat -l | egrep -i 'ga106lab|com\.apple\.'`. Sem variação no dia; saída fora dessa forma = FAIL-2.
- ALLOW (explicável somente com nota em INDEX.md; não dispensa registro): mudanças restritas a campos que variam por construção — data/hora, uptime, contadores de log. (Cláusula de processo efêmero REMOVIDA em M0244: processos nunca aparecem em `kextstat`; desculpar linha extra por "processo" é proibido — FPP-4.)
- SUSPECT (sempre FAIL-3, parar, preservar): QUALQUER diff em linha de KEXT — aparecer/sumir/mudar versão/índice de qualquer KEXT (incl. GA106Lab 1.5.3), qualquer linha contendo `.kext`, QUALQUER linha extra com prefixo `com.apple.*` (prefixo é spoofável; sem comando-filtro exato não se distingue). Diffs de KEXT nunca são "explicáveis".
- Operador não improvisa no dia: o que não estiver na ALLOWLIST acima = SUSPECT.

## Review do FAIL + critério de janelas + re-review (R7/F8 — fecha OPEN Q11/Q2/Q6/Q12)
- Reviewer + alternativa NOMEADOS + SLA (Q11; G-M0244-08 DURA, fecha parte de Q11): reviewer E alternativa são pessoas NOMEADAS no runbook modelo + transcritas no INDEX ANTES da primeira janela; sem os dois nomes, janela NÃO abre. Operador não revisa o próprio FAIL. SLA = review em até 7 dias; sem review concluído, nenhuma nova janela abre. (RG-3.)
- N janelas e parada antes de C (Q2/Q6; G-M0244-05 DURA): critério 3 D PASS + 1 A + 1 B é ARBITRÁRIO e CONSERVADOR, SEM power analysis — 5 amostras NÃO estimam panic-rate (S13) nem provam segurança; qualquer FAIL/ABORT zera a contagem. Antes de sequer DISCUTIR C exige-se RE-JUSTIFICATIVA escrita do critério (denominador/horas/config-split) + milestone próprio + review Gemini + autorização humana dedicada (nunca "extensão" deste design). (FPP-5.)
- Re-review em mudança (Q12): qualquer mudança de versão macOS/Tahoe, EFI/config, ou HW (GPU/cabos/monitor/máquina) invalida a baseline e exige re-review completo deste pacote antes de nova janela.

## Gate de staging: Option 0 → D-minus/D-split → D (R8)
- P81 = Option 0 non-target PASS primeiro (PRECONDIÇÃO DURA P8 de D + fidelidade G-M0244-13, ver MINIMAL-LIVE-TEST); depois D-minus (FOTO-0 + IDENT-1 + mini-VERIFY G-M0244-02 + CLOSE, ≤ 5 min) ou D-split (2 micro-janelas em dias distintos: J1 = T+00..T+04, J2 = T+05..T+10, cada uma com CLOSE próprio + SAVE em mídia externa IMEDIATA G-M0244-06); só então D completo. D-minus SEM mini-VERIFY conta SÓ como ensaio fotográfico, NUNCA como "pipeline OK" para D (G-M0244-02).
- Se a evidência Option 0 falhar (qualquer comando > 60 s, INDEX inválido, hash-ESP ausente), D NÃO abre (gate duro).

## Timeouts (tabela)
| Escopo | Bound | Após expirar |
|---|---|---|
| Janela total | 15 min teste + 15 margem reserva (não extensão) | Fechar teste em ≤15; margem só para ABORT/recovery (VERIFY+SAVE parcial+FOTO-abort), fotos finais e CLOSE/INDEX — NUNCA para "só mais um comando" ou retry; margem não consumida = janela encerra antes; estouro da margem não reabre a janela |
| Por comando | 60 s | 1 retry bounded; depois ABORT. Se Option 0 medir qualquer comando > 60 s, D é reprovado antes de abrir (R6) |
| Sem vídeo inicial | 5 min | ABORT-VIDEO |
| Hang total (sem resposta alguma) | 5 min, com DONO por faixa G-M0244-07 | 60 s–2 min: dono = OPERADOR (só retry bounded, sem power-off); 2–5 min: dono = REVIEWER (operador só age sob contato; sem contato => esperar até 5 min); ≥ 5 min: dono = OPERADOR via procedimento do fabricante + recovery (exceção recovery-only numerada, ver matriz). Power-off fora da faixa do dono = VIOLAÇÃO com FAIL automático, sem reabertura no dia. (RG-2.) |
| Pós-panic reboot | 10 min para voltar a vídeo | Se exceder => no-signal path (fotos, parar, pedir ajuda ao reviewer/alternativa NOMEADOS G-M0244-08; sem improvisar) |

## Anti-regras (o que NÃO fazer)
- Não estender timeout "só mais um pouco". Não trocar ordem. Não pular SAVE.
- Não diagnosticar causa de FAIL/ABORT no dia (registrar apenas).
- Não converter FAIL em "PASS parcial". Não converter ABORT em "FAIL técnico".
- Não ler PASS como live-ready/stage-ready/write-ready: PASS afirma só observabilidade da janela (ABORT→FAIL→PASS: proibição estendida a PASS, R7/F1).
