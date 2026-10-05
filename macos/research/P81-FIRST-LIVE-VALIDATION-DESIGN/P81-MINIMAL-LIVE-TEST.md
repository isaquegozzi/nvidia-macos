# P81 — Minimal Live Test (opção D; DESIGN-ONLY, NÃO autorizado, NÃO executado)

```text
P81_EXECUTION_AUTHORIZATION = NONE
LIVE_EXECUTION_AUTHORIZED = NO
FIRST_LIVE_VALIDATION_CANDIDATE = Option 0 PASS (hard gate P8) + D-minus/D-split
  + D completo, nesta ordem (R8). Classe: precondition-only + read-only
  identity/lifecycle observation (opt D). Classe de write: NENHUM.
Única hipótese (H1): "É possível observar e registrar o estado atual do sistema
  (OS-level read-only) e voltar ao estado anterior sem mudar nada e sem improvisar."
Este teste NÃO afirma segurança do KEXT, do HW, ou prontidão para writes.
```

## As 13 perguntas do objetivo (respostas explícitas)
1. **Menor teste?** P81 = Option 0 non-target PASS primeiro; depois D-minus (FOTO-0+IDENT-1+CLOSE, ≤5 min) ou D-split (2 micro-janelas J1=T+00..T+04, J2=T+05..T+10 em dias distintos); só então D completo (~15 min de comandos OS-level read-only + fotos + hashes). Nada mais. (R8.)
2. **Única hipótese?** H1 acima. Nada sobre KEXT/HW além de observabilidade.
3. **HW state exigido?** NENHUM estado específico exigido; qualquer estado serve porque
   nada é tocado. Se vídeo ausente no início => ABORT antes de qualquer comando. Veredito PASS carrega `GPU-STATE = UNKNOWN` persistente (PASS-6).
4. **Menor superfície?** Interfaces OS read-only (`ioreg`, `kextstat`, `log show`,
   `nvram -p` leitura, `diskutil info -plist` SEM montar a ESP). Sem selectors,
   sem execução de código de KEXT / sem IOUserClient (`ioreg`/`kextstat` só observam
   presença), sem mounts (nem RO), sem PCI/MMIO, sem Metal. D não lê do residente
   nada além de presença via kextstat; qualquer leitura além disso pertence à fase A
   e exige antes o método Q10 (hash carregado-vs-disco sem unload). (R3.)
5. **Write mínimo, se algum?** NENHUM — ZERO writes VERDADEIRO no alvo (nenhum
   mount no alvo, nem RO: mount-table do alvo intocada, nenhum driver FAT acionado; escopo = ESP/volumes DO ALVO — mídia externa de evidência é montada por desenho em T+09). Conclusão válida
   e adotada: "ainda não fazer write nenhum" (nenhum MMIO/BME/DMA/GSP/firmware/
   VRAM/IRQ/reset/doorbell/PUT/command). (Mount-ESP-RO removido em M0242.)
6. **Ordem exata?** Ver ACTION abaixo (T+0..T+15, numerada, sem branches além de ABORT).
7. **Timeout?** Janela total 15 min; cada comando 60 s; sem vídeo inicial 5 min => ABORT;
   qualquer comando travado > 60 s => pular para ABORT ordenado (não insistir).
   Bounds quantificados em EVIDENCE-COLLECTION; medidos no Option 0; D reprova se
   estourar 60 s/comando. (R6.)
8. **Abort?** Ver ABORT abaixo (1 comando de parada + checklist de estado + foto).
9. **PASS?** Ver PASS (todos os artefatos salvos + sistema inalterado + sem panic na janela + GPU-UNKNOWN persistente + carimbo de observabilidade em CLOSE). (R1.)
10. **FAIL?** Ver FAIL (artefato faltante/corrompido, comando read-only com erro
    persistente, panic na janela — sem atribuir causa; diff IDENT-2 vs T+08 julgado
    pela ALLOWLIST, nunca por improvisação). (R1.)
11. **Recuperação?** Nenhuma mudança => nada a desfazer; pós-panic segue
    `P81-ROLLBACK-RECOVERY.md` (não executar agora). Exceção recovery-only explícita
    autorizada por humano (ver matriz + ABORT). (R5.)
12. **Evidência?** Ver EVIDENCE_TO_SAVE + `P81-EVIDENCE-COLLECTION.md` (predicado `log show` + limite de linhas, profundidade ioreg, threshold GB, formato HH-MM-SS + INDEX com shasum). (R6.)
13. **Volta ao estado anterior?** Caminho PASS limpo: por construção zero-write, estado
    posterior = anterior (verificação = repetir IDENT-2 + comparar hashes). Caminho
    FAIL/ABORT-HEALTH: estado pós-janela DESCONHECIDO + HW possivelmente preso +
    reboot altera logs; coleta pós-reboot = NOVA janela com review. (R4.)

## PRECONDITIONS (todas verificadas ANTES, sem executar nada neste turno)
- P1. Operador leu threat model + rollback + pass/fail/abort + authorization (tudo NO).
- P2. Runbook impresso/em segundo dispositivo (não depender de improvisar após reboot).
- P3 (DURA; G-M0244-01-R1 + G-M0244-09). Destino de evidência com ≥ 5 GB livres verificados (`df -h`, anotado em INDEX; abaixo de 5 GB a janela NÃO abre) + relógio VERIFICADO sem disciplinar (método G-M0244-01-R1, P81.1: `date -u` antes/depois + `sysctl -n kern.boottime` + EXIF DateTimeOriginal da foto (`mdls -name kMDItemContentCreationDate` ou `sips -g dateTimeOriginal`; `stat` = auxiliar sem valor probatório), bounds B1+B2(sanidade, NÃO prova step)+B3(EXIF, único detector); `sntp -sS` PROIBIDO, NTP fora do gate; sem registro das 3 fontes + bounds, a janela NÃO abre). (R6; FPP-1/RG-4.)
- P4. Fotos do setup (cabos, monitor, LEDs) tiradas antes (E02 + metadados de câmera obrigatórios). (R1.)
- P5. Sabido de cor: boot recovery path + tecla Option/Recovery + config known-good.
- P6. Janela de 30 min reservada (15 teste + 15 margem); sem pressa; sem plateia pedindo "só mais um".
- P7. `P81_EXECUTION_AUTHORIZATION` segue NONE — este documento NÃO autoriza execução.
- P8 (DURA, R2/R8). Option 0 non-target PASS com hash-ESP + INDEX válido ANTES de D abrir: pipeline ensaiado, nenhum comando > 60 s, `INDEX.md` + `shasum` válidos, hash-ESP coletado no non-target. Sem P8 PASS, D NÃO abre. Se evidência Option 0 falhar, D não abre (gate duro).
- P9 (DURA; G-M0244-08). Reviewer + ALTERNATIVA NOMEADOS no runbook modelo + transcritos no INDEX antes da 1ª janela; SLA 7 dias. Sem os dois nomes, janela NÃO abre. (R7; RG-3; fecha parte de Q11.)
- P10 (DURA; G-M0244-06). D-split SÓ com CLOSE por micro-janela (J1 e J2 com CLOSE próprio assinado) + SAVE em MÍDIA EXTERNA IMEDIATA ao fim de cada micro-janela (fecha Q3). Sem destino externo imediato, D-split é PROIBIDO até Q3 resolvida. (RG-1.)
- P11 (DURA; G-M0244-07). Dono explícito por faixa de hang: 60 s–2 min operador (só retry, sem power-off); 2–5 min reviewer (sem contato => esperar); ≥ 5 min operador via procedimento do fabricante. Power-off fora da faixa do dono = violação com FAIL automático. (RG-2.)
- P12 (DURA; G-M0244-02). D-minus inclui mini-VERIFY (IDENT-2 reduzido pelo comando-filtro G-M0244-04, bound 60 s) ou conta SÓ como ensaio fotográfico, nunca como "pipeline OK" para D. (FPP-2.)
- P13 (DURA; G-M0244-13; endurecida M0248/F9). Fidelidade mínima do Option 0 publicada no INDEX do ensaio: mesma major macOS do alvo, mesmas formas de comando, bounds R6 medidos; sem ela, Option 0 NÃO conta como PASS e D NÃO abre. Campos verificáveis (major macOS + formas de comando + bounds medidos) CONFERIDOS POR SEGUNDA PESSOA (nome + assinatura no INDEX do ensaio) ANTES de D abrir; sem segunda conferência, D NÃO abre. (UA-1.)
- P14 (DURA; G-M0244-10). Limite numérico de recovery boots: MÁXIMO 2 recovery boots por janela abortada, cada um registrado como evento numerado R-1/R-2 (hora + motivo + fotos); estouro => ABORT + review antes de qualquer nova janela. (AL-1.)
- P15 (DURA; UA-4/UA-7). Checklist de ambiente assinado no CLOSE: AC ligado, tampa aberta, sem `sleep` manual na janela, ≥ 5 GB verificados; assinatura do operador por extenso. T10 residual declarado NÃO-ZERO mesmo com checklist OK. (G-M0244-14.)
- P16 (DURA; G-M0244-12). Fallback LOGS: se `log show` travar (> 60 s), PULAR LOGS-1/LOGS-2 com nota em INDEX (sem improvisar, sem retry extra); S14 rebaixada (ver STATE-ASSUMPTIONS). (AL-3.)

## ACTION_TO_BE_AUTHORIZED_LATER (ordem exata, só executar se um dia autorizado por humano + review)
```text
T+00  FOTO-0: tela + torre/LEDs (pré). E02 + metadados de câmera obrigatórios.
T+01  IDENT-1 (60s): registrar data/hora, versão macOS, config OpenCore ativa (só leitura).
T+02  IDENT-2 (60s): kextstat filtrado (só leitura; observar se GA106Lab 1.5.3 aparece; sem tocar; só presença — nada além disso até o método Q10).
T+03  IDENT-3 (60s): ioreg resumido, profundidade ≤ 3, linhas ≤ 300 (só leitura; sem dump gigante sem bound).
T+04  IDENT-4 (60s): nvram -p (leitura) + identidade EFI SEM montar (comando EXATO `diskutil info -plist <device>` da ESP; sem shasum de conteúdo no alvo; hash-ESP adiado para o ensaio Option 0 non-target; `diskutil mount*` PROIBIDO por prefixo no alvo). S12-identidade parcial do alvo é declarada INSUFICIENTE para qualquer write futuro; fallback nunca escala para mount no alvo (nem RO). ESCOPO (M0248): zero-mount = ESP/volumes DO ALVO; mídia externa de evidência É montada por desenho em T+09. (R2.)
T+05  LOGS-1 (60s): últimos panic logs existentes (`log show --predicate 'eventMessage CONTAINS "panic"' --last 24h`, cap 200 linhas; só leitura).
T+06  LOGS-2 (60s): gpuRestart history existente (só leitura, cap 200 linhas).
T+07  FOTO-1: tela com resultados resumidos (E02 + metadados de câmera obrigatórios).
T+08  VERIFY: repetir IDENT-2 (kextstat, comando-filtro EXATO G-M0244-04) para confirmar nada mudou (julgar pela ALLOWLIST sem cláusula de processo).
T+09  SAVE: copiar artefatos para destino em MÍDIA EXTERNA IMEDIATA (G-M0244-06) + anotar horários; nomes `<HH-MM-SS>_<E##>_<descrição>.txt/jpg/md` + `INDEX.md` com `shasum -a 256` por arquivo; bound 60 s / 500 MB + regra INDEX parcial G-M0244-09. (R6; RG-1/RG-4.)
T+10  CLOSE: checklist "nada foi alterado" assinado + carimbo PASS-OB observabilidade + `GPU-STATE = UNKNOWN`; fim da janela. (R1.)
NOTA: nenhum passo contém stage/load/unload/selector/MMIO/PCI/BME/DMA/GSP/firmware/
      VRAM/IRQ/reset/doorbell/PUT/command/IOGPU/IOAccelerator/Metal. Se qualquer passo
      sugerir isso no dia => ABORT imediato (escopo-creep, T9).
```

## EXPECTED_OBSERVATION
- Comandos retornam em < 60 s cada (medido antes no Option 0; estouro reprova D); outputs coerentes entre IDENT-2 e T+08 (pela ALLOWLIST);
  identidade EFI legível (`diskutil info -plist`, sem mount); logs históricos
  acessíveis dentro dos bounds; fotos nítidas com E02 + metadados de câmera.

## PASS (todos necessários; PASS não autoriza nada futuro)
- Todos os artefatos em EVIDENCE_TO_SAVE salvos e legíveis, com E02 + metadados de câmera em todas as FOTOs.
- IDENT-2 == T+08 pela ALLOWLIST (nada mudou fora do explicável).
- Nenhum panic/hang/no-signal na janela.
- Checklist CLOSE assinado sem improvisação + carimbo: "PASS afirma SÓ observabilidade OS-level desta janela; NÃO afirma HW/EFI/KEXT além disso; NÃO autoriza A/B/C; NÃO ler como live-ready." + `GPU-STATE = UNKNOWN` persistente. (R1.)

## FAIL (qualquer um; FAIL não atribui causa)
- Qualquer artefato faltante/ilegível; comando read-only com erro persistente;
  divergência IDENT-2 vs T+08 fora da ALLOWLIST (ou sem explicação registrada); panic/hang/no-signal na janela
  (registrar, NÃO diagnosticar causa).
- Caminho FAIL declara estado pós-janela DESCONHECIDO (ver POSTCONDITION). (R4.)

## ABORT (exato; ver também P81-PASS-FAIL-ABORT.md)
- Condições: sem vídeo 5 min no início; no-signal a qualquer momento da janela
  (=> ABORT-VIDEO/ABORT-HEALTH); qualquer comando > 60 s travado após 1 retry
  com bound; qualquer proposta de write/stage/load/selector/mount; qualquer
  panic/hang/reboot espontâneo (precedência: ABORT-HEALTH > FAIL-4);
  operador inseguro a qualquer momento (sem justificar).
- Ação: parar no passo atual, FOTO-abort (E02 + metadados obrigatórios), executar VERIFY-pós-ABORT (IDENT-2, 60 s) SOMENTE se vídeo presente — única execução permitida pós-ABORT —,
  SAVE parcial marcado `ABORTED-`, CLOSE com motivo. Não retentar no mesmo dia. (R5.)

## TIMEOUT
- Total 15 min; por-comando 60 s; sem-vídeo 5 min; pós-comando-travado: 1 retry com
  bound de 60 s, depois ABORT. Sem extensões ("só mais 5 min") — nova janela exige novo review.

## POSTCONDITION (R4)
- Caminho PASS limpo: sistema idêntico ao pré (por construção zero-write, verificado por IDENT-2 == T+08 pela ALLOWLIST); evidência salva fora da máquina
  ou em volume separado; checklist CLOSE arquivado.
- Caminho FAIL/ABORT-HEALTH: estado pós-janela DESCONHECIDO + HW possivelmente preso (latch/hang) + reboot altera logs (novo panic log, contadores NVRAM, uptime). "Sistema idêntico ao pré" NÃO vale aqui. Fotos LEDs + coleta pós-reboot bounded (panic log novo + IDENT-1..4 com os mesmos bounds) = NOVA janela com review próprio, nunca continuação.

## ROLLBACK
- Nada a reverter (ZERO writes verdadeiro; ESP nunca montada). Se B um dia for feito,
  rollback = reselecionar config known-good + verificar hashes (detalhes em
  `P81-ROLLBACK-RECOVERY.md`, não executar).

## RECOVERY (pós-panic/hang/no-signal — resumo; detalhe no ROLLBACK-RECOVERY)
- Não retentar automaticamente; fotografar (E02 + metadados obrigatórios); forçar desligamento só se hang total
  (> 5 min sem resposta, procedimento do fabricante — exceção recovery-only autorizada por humano, ver matriz); boot recovery path documentado;
  coleta pós-reboot bounded como NOVA janela com review (panic log novo entra como evidência, sem diagnóstico de causa). (R4/R5.)

## EVIDENCE_TO_SAVE (resumo; detalhe em EVIDENCE-COLLECTION)
- FOTO-0/FOTO-1/FOTO-abort(se houver, todas com E02 + metadados de câmera); IDENT-1..4 outputs + hashes; LOGS-1/2 (bounded);
  T+08 repetição; checklist CLOSE com carimbo PASS-OB + `GPU-STATE = UNKNOWN`; `P81-BUILD-STATE` da janela (hora, operador, veredito).

## REQUIRED_FUTURE_CHANGE (marcados, NÃO implementados neste turno)
- `REQUIRED_FUTURE_CHANGE-01`: se futuro teste quiser selectors read-only do 1.5.3,
  exigir antes: (a) auditoria de que o selector é side-effect-free + (b) bound de
  tempo + (c) review dedicado + (d) método Q10 (hash carregado-vs-disco sem unload) definido em A. NÃO implementar nada agora.
- `REQUIRED_FUTURE_CHANGE-02`: se futuro teste quiser qualquer write (mesmo 1× MMIO),
  exigir antes: estado HW baseline + BME/DMA/GSP design + pipeline D/A/B provado.
  NÃO implementar nada agora.
