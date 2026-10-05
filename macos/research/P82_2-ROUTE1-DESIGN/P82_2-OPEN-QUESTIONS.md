# P82.2 — Open Questions (pós-decisão R1-D; nenhuma autoriza live)

```text
SESSION_ID: P82-2-ROUTE1-RELAY-V3
MILESTONE: P82_2_ROUTE1_DESIGN_SELECTORS_5_6 (OFFLINE ONLY)
STATE: MUSE_WORK / ACTOR: MUSE_BUILD
LIVE_EXECUTION_AUTHORIZED = NO (live=0)
```

1. Q1 (lifetime): quantificar a janela stop-race/UAF 1.5.3 (ciclos entre
   `getOwnerService()` e `releaseMap`/copyout) ou fechar por construção via
   backport do pin? Dono: futuro LOAD milestone. Sem isso, 5/6-na-1.5.3 segue HIGH-OPEN.
2. Q2 (clear-on-read): fonte primária Nova/OpenRM provando BOOT_0/BOOT_42
   (e futuramente 0x118128/0x118234) sem semântica clear-on-read? Sem fonte,
   P-D4.3 segue OPEN para qualquer leitura MMIO.
3. Q3 (Q10): existe método documentado Apple que exponha medida
   criptográfica do KEXT carregado (Q10 BLOCKED mantido)? Fato novo reabre em
   milestone próprio, nunca como extensão silenciosa.
4. Q4 (P8): há non-target físico para Option 0 com fidelidade G-M0244-13 +
   segunda conferência? Sem ele, qualquer rota live segue BLOCKED.
5. Q5 (P9): quem são reviewer + alternativa (nomes, SLA 7d)? Nominação é
   pré-condição, não detalhe operacional.
6. Q6 (P3): qual caminho foto/watchdog/serial (ID pinado, observa hang sem o
   alvo)? Meio é substituível (PARCIAL), existência não.
7. Q7 (bundle pin): re-executar `shasum -a 256` na árvore 1.5.3 exata +
   arquivar saída antes de qualquer janela (advisory 16); pin hoje é textual.
8. Q8 (1.6.2 scope): confirmar por diff que o pin 1.6.2 cobre SÓ selector 8
   (este audit leu 5/6 `:275-360` sem pin vs GFW `:418-453` com pin) e que
   nenhum outro hardening 1.6.x atenua 5/6 — não citar 1.6.2 como atenuante.
9. Q9 (comment stale): corrigir "map→2 leituras fixas" no handler 6 1.5.3
   (flow faz 1 load) em higiene futura sem mudar semântica? Rotina, sem review.
10. Q10 (deriva de valor): se baseline HW futura mostrar deriva (ex. BAR0
    remapeado, MSE off), R1-D é revisitado por re-auditoria — nunca por
    re-execução cega. Gatilho de revisit = fato novo documentado.
11. Q11 (expiry): este pacote é P82.2-AUDIT-V1 (2026-09-09). Qualquer mudança
    macOS/EFI/HW/GPU/cabos/KEXT invalida hashes e vereditos; re-auditoria na
    geração exata antes de qualquer janela; auditoria em geração errada = FAIL.
12. Q12 (LOAD milestone): qual binário-alvo do FIRST_CONTROLLED_LOAD (1.6.2
    pinado vs 1.7.x/1.8.0 com re-auditoria)? Decisão humana + review; este
    turno só desenha a forma, não escolhe o alvo.
