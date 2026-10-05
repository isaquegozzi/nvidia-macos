# P82.2 — Evidence Value (por selector: o que repetir 5/6 provaria?)

```text
SESSION_ID: P82-2-ROUTE1-RELAY-V3
MILESTONE: P82_2_ROUTE1_DESIGN_SELECTORS_5_6 (OFFLINE ONLY)
STATE: MUSE_WORK / ACTOR: MUSE_BUILD
LIVE_EXECUTION_AUTHORIZED = NO (live=0)
```

Fatos históricos preservados (não assumir que PASS deva ser repetido):
selector 5 = BOOT0 (live PASS **0xb76000a1**, decode **0x176**/GA106);
selector 6 = BOOT42 (live PASS **0x176a1000**, decode **0x176**);
1.5.3 tem selectors 0–7; **selector 8 NÃO existe** (P82.1 Caso B).

## 1. Classificação por selector

```text
SELECTOR5_VALUE = DUPLICATES_HISTORICAL_EVIDENCE
SELECTOR5_NEW_EVIDENCE = NO
SELECTOR5_UNLOCKS_NEXT_MILESTONE = NO
SELECTOR5_CURRENT_VALUE = LOW
SELECTOR6_VALUE = DUPLICATES_HISTORICAL_EVIDENCE
SELECTOR6_NEW_EVIDENCE = NO
SELECTOR6_UNLOCKS_NEXT_MILESTONE = NO
SELECTOR6_CURRENT_VALUE = LOW
```

`NEW_EVIDENCE` = sinal sobre GPU/driver ainda não registrado. `DUPLICATES` =
repete PASS já registrado. `UNLOCKS` = pré-condição de milestone futuro.
`CURRENT_VALUE` julga HOJE, contra o histórico + risco 1.5.3.

## 2. Seis perguntas (respostas vinculantes)

1. **Repetir BOOT0 hoje prova algo novo? NÃO.** Raw 0xb76000a1/chip 0x176 já
   registrados (`TG-KEXT4-FIRST-MMIO-READ-LIVE/result.md`; BOOT0 COMPLETE em
   `CURRENT-STATE.md`). Melhor caso de re-leitura: mesmo valor (confirma
   estabilidade, não identidade nova); qualquer valor distinto seria FAIL sem
   atribuição (sem baseline de deriva + Q10 BLOCKED impede vincular árvore).
   Repetição ≠ revalidação: o PASS histórico veio de geração KEXT4 anterior,
   mas a semântica (offset/width/decode) é idêntica e o fato "MMIO-BAR0 lê ID
   GA106" já está estabelecido. Nenhum fato novo sobre GPU/driver/KEXT.
2. **Repetir BOOT42 prova algo novo? NÃO.** Mesma estrutura: 0x176a1000/0x176
   já registrados (result + boot42-decode + BOOT42 COMPLETE). O fix de decode
   10-bit (contraexemplo 0x376) já foi provado offline; re-ler o HW não
   re-prova o decode. Idem: divergência futura = FAIL sem atribuição.
3. **Mudança de macOS/estado torna revalidação necessária? NÃO, como
   re-execução cega.** Qualquer mudança macOS/EFI/HW/cabos/KEXT **invalida**
   este audit e exige **re-auditoria na geração exata** (P82.1 Q11/advisory
   17) — não re-execução do selector. Re-ler sem re-auditar troca a ordem
   correta (auditar→autorizar→executar) por "executar para ver se mudou", o que
   é improvisação. Estado que justifica re-auditoria ≠ estado que justifica
   janela live (gates P8/P9/P3 continuam vermelhos).
4. **Risco 1.5.3 compensa? NÃO.** Ambos os caminhos carregam
   `LIFETIME = HIGH-OPEN` (stop-race/UAF sem pin) + `clear-on-read = OPEN`
   (sem fonte primária) + S15 (código em kernel) — contra evidência
   `DUPLICATES` de valor LOW. Troca assimétrica: risco real e aberto por
   confirmação redundante. "Read-only" e "1 load rápido" não fecham a conta
   (ver `P82_2-LIFETIME-RISK.md`).
5. **Há leitura residente com valor maior que 5/6? NÃO.** Inventário 1.5.3
   (0–7): 0/1/2 = constantes/cache (sem sinal HW; Q10 BLOCKED impede alegar
   identidade); 3 = PCI config (classe OS-adjacente, sem MMIO; D já cobre
   presença); 4 = probe de metadata (mapeia/desmapeia SEM dereferenciar —
   por desenho não lê a GPU); 7 = sysmem já **LIVE COMPLETE** (ADDRESS_READY,
   mapping LEFT PREPARED — reler prova no máximo idempotência
   AlreadyPrepared, não GPU nova). 5/6 são as ÚNICAS leituras MMIO residentes,
   e ambas já têm PASS. Nada residente supera 5/6; 5/6 já esgotaram o novo.
6. **Selector 7: excluir ou considerar? EXCLUIR como validação.**
   Selector 7 já foi executado live EXATAMENTE UMA vez (ADDRESS_READY,
   `CURRENT-STATE.md`) com mapping deixado PREPARED — estado persistente
   deliberado. Re-invocar 7 hoje: (a) não é leitura da GPU (é alloc/mapping
   sysmem, sem HW); (b) retorna pelo caminho `AlreadyPrepared` (prova o
   estado deixado, não fato novo); (c) viola a regra single-invocation/ABORT-
   SCOPE herdada (2ª invocação = violação no dia); (d) toca o subsistema com
   o teardown mais delicado (T3/T4). 7 = EXCLUÍDO de ROUTE_1.

## 3. Conclusão de valor

Nenhum selector residente oferece `NEW_EVIDENCE` hoje; 5/6 oferecem o MÁXIMO
da classe residente e esse máximo já foi capturado. ROUTE_1 como re-execução
é redundante por construção — a decisão segue em `P82_2-DECISION.md` (R1-D).
