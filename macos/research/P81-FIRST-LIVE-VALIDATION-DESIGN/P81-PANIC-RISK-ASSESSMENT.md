# P81 — Panic Risk Assessment (sem atribuir causalidade)

```text
P81_EXECUTION_AUTHORIZATION = NONE / LIVE_EXECUTION_AUTHORIZED = NO
Regra metodológica: coincidência temporal NÃO é causalidade. Nenhuma seção abaixo
afirma que o GA106Lab causou, deixou de causar, ou "é inocente" de qualquer panic.
```

## Histórico (o que sabemos)
- Panic 1: Tahoe, `mediaanalysisd`, IPC, UAF — confiança média na *assinatura*
  (não na causa). UAF/IPC é classe de lifetime/teardown; P80 endureceu lifetime
  offline, mas prova live = zero.
- Panic 2: Tahoe, `opencode`, `_copyinmsg`, general protection. Sem ligação estabelecida.
- gpuRestarts: AMD Renoir/Vega antigos, contexto distinto (GPU diferente, sem GA106Lab
  NVIDIA no caminho). Sem ligação estabelecida.
- Live 1.5.3: histórico carregado sem panic atribuído — mas "sem panic atribuído"
  ≠ "provado seguro"; amostra pequena, sem baseline controlada.

## Risco por opção (qualitativo; escala Mínimo/Muito baixo/Baixo-médio/Alto)
- **D (OS read-only, sem execução de código de KEXT / sem IOUserClient): Mínimo.** Não exercita teardown/HW (`ioreg`/`kextstat` só observam presença); único
  acoplamento com panics é coincidência basal do Tahoe (T1) + observer effect (T7).
  Um panic durante D NÃO incrimina nem inocenta o KEXT — registra-se como ABORT (ABORT-HEALTH > FAIL-4).
- **A (observar 1.5.3 residente): Muito baixo.** Não adiciona stage/load; mas lê
  estado de um KEXT residente cujo lifetime live é NÃO-PROVADO (S10). Selectors
  read-only (se um dia incluídos) executam código no kernel => risco acima de D
  (ver S15). Por isso A vem depois de D e hoje sem selectors.
- **B (boot controle sem GA106Lab): Baixo-médio.** Remove o KEXT da equação (bom
  para baseline), mas adiciona risco operacional (boot-select/EFI-select, T2/T6).
  Um panic em B sugere (sem provar) instabilidade basal do Tahoe; zero panics em
  1 janela prova quase nada (1 amostra). Valor é cumulativo ao longo de janelas.
- **C (stage 1.8.0): Alto, inaceitável em P81.** Exercita exatamente o caminho
  temido (load/teardown T3/T4) com um binário nunca carregado (S09), sem baseline
  (S13), com HW desconhecido (S05). Qualquer panic aqui seria ininterpretável
  (causa indistinguível entre "1.8.0", "Tahoe basal", "HW") e custaria a janela
  mais o risco de estado preso. Rejeitada.

## Staging P81 (R8) e pós-janela (R4)
- Ordem: Option 0 non-target PASS (gate duro P8 + fidelidade G-M0244-13) → D-minus (≤ 5 min, com mini-VERIFY G-M0244-02 ou só ensaio fotográfico) ou D-split
  (2 micro-janelas em dias distintos, CLOSE por micro-janela + mídia externa imediata G-M0244-06) → D completo. Se Option 0 falhar, D não abre.
- Caminho FAIL/ABORT-HEALTH: estado pós-janela DESCONHECIDO + HW possivelmente preso;
  coleta pós-reboot bounded = NOVA janela com review (reviewer + SLA antes da primeira
  janela, ver PASS-FAIL-ABORT), nunca continuação.

## O que um panic durante P81-D significaria (e o que NÃO significaria)
- Significaria: ABORT registrado (precedência sobre FAIL-4) + evidência nova (1 amostra) + janela encerrada + estado pós-janela DESCONHECIDO (R4).
- NÃO significaria: que D causou (D não muda estado), que 1.5.3 causou, que Tahoe
  causou, ou qualquer ranking de culpa. Próximo passo seria review do FAIL, não retry.

## Lacunas que impedem quantificação honesta
- Sem panic-rate basal (S13 NÃO-PROVADA): 2 amostras não estimam taxa.
- Sem denominador (horas estáveis vs horas observadas) por config.
- Sem separar configs (com/sem GA106Lab, quais versões, qual EFI).
- Ação: D+A+B futuros preenchem o denominador; até lá, só linguagem qualitativa.
