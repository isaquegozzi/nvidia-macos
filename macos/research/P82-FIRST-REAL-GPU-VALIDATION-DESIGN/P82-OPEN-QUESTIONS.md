# P82 — Open Questions (honestas, em aberto; endurecidas M0255 FT-SMOKE-01)

1. **Auditoria do selector C: qual o critério de "side-effect-free"? [ENDURECIDO 2c]**
   P-D4.0..P-D4.5: hash pinado do binário auditado (anti 1.5.3 vs 1.6.2/1.7.x),
   lifetime/UAF (retain/release, detach/unload concorrente), lista fechada
   BARs/registradores, prova de ausência de clear-on-read/poll-write, bound+copyout,
   análise locks/IRQ + segunda conferência. Sem isso C NÃO abre. Quem audita e quem confere?
2. **C lê quais registradores/estado exatamente? [ENDURECIDO 2c]** Lista fechada
   P-D4.2 pinada em F-P82-AUTH-05 (BAR#, offset, size), sem expandir no dia; fora da
   lista = violação ABORT-SCOPE. Onde termina "readiness" e começa "controle"?
3. **Q10 BLOCKED muda a interpretação de C?** SIM: mesmo com C-PASS, o residente
   segue sem identidade vinculada ao binário histórico. C prova comportamento do
   caminho, nunca "qual binário respondeu". AMFI-load-time rebaixado M0255: mecanismo
   não-verificado, sem atenuação de Q10.
4. **Quantas janelas C até a evidência bastar?** Proposta inicial (arbitrária,
   conservadora, sem power analysis): mínimo 3 D PASS + auditoria P-D4 + 1 C-PASS antes
   de sequer DISCUTIR qualquer passo além de C; qualquer FAIL/ABORT zera.
5. **Headless sem câmera (P3/FOTO-0): qual caminho? [ENDURECIDO 2e, gap bloqueante]**
   D-minus abortou aqui. Exige antes de C: segundo dispositivo + serial/watchdog com ID
   pinado em F-P82-AUTH-06, capaz de observar hang sem depender do alvo; bound userspace
   60 s NÃO basta. Sem caminho publicado, C NÃO abre.
6. **Reviewer (P9): quem + alternativa + SLA? [ENDURECIDO 2a]** D-minus abortou aqui.
   Sem dois nomes pinados em F-P82-AUTH-03 no runbook/INDEX, C NÃO abre; operador não
   revisa o próprio FAIL. Forma pinada exige SLA 7 dias + assinatura.
7. **Option 0 sem VM (P8): onde ensaiar?** Sem non-target real com fidelidade
   publicada + segunda conferência, C NÃO abre.
8. **O que um C-PASS prova / não prova? [ENDURECIDO 2c/2d]** Prova: caminho read-only
   respondeu 1x com bound e sem mutação observável. NÃO prova: segurança do KEXT,
   estado do HW, ausência de UAF, taxa de panic, prontidão para E (MMIO write),
   identidade do binário (Q10 BLOCKED). Em GA106 sem GSP/FW, PASS seria surpresa.
9. **O que um panic durante C significaria?** Nada causal (como P81-T1): FAIL sem
   atribuição, review dedicado, contagens zeradas. Proibido inocentar ou culpar.
   Inclui FAIL protocolar GA106 esperado (GSP/FW ausente) sem atribuir ao KEXT.
10. **Limite BAR-read vs MMIO-write [ENDURECIDO 2c]:** qualquer write acidental ou
   leitura fora da lista P-D4.2 descoberto na auditoria ou no dia = ABORT-SCOPE + FAIL
   + milestone novo. Limite instrumentado pela lista fechada + auditoria, sem executar.
11. **Este design envelhece? [ENDURECIDO 2f]** SIM: P82-DESIGN-V3 (2026-09-09).
   Qualquer mudança macOS/EFI/HW/GPU/cabos/KEXT invalida e exige re-review completo
   antes de qualquer janela. Auditoria futura deve citar DESIGN_VERSION + hash do
   binário (P-D4.0); geração errada (1.5.3 vs 1.6.2/1.7.x) = FAIL.
12. **E (MMIO write) tem pré-condição alcançável?** Hoje NÃO. Reabrir E exige:
   D + C-PASS + baseline HW + pipeline + milestone próprio + review + autorização
   dedicada F-P82-AUTH. P82 não a concede.
13. **GA106 sem GSP/FW: C é FAIL esperado? [NOVO 2d]** SIM: declarar C-em-GA106 como
   provável FAIL protocolar; exigir baseline HW (BME/DMA estado, BARs mapeados?,
   GSP/FW ausente) documentada em D antes de C. Sem baseline, C = NO-GO.
14. **Hang sem watchdog: como recuperar? [NOVO 2e]** Bound userspace não basta;
   exigir watchdog/serial externo + exceção recovery-only só na forma F-P82-REC
   (trigger, comandos exatos só-recovery, janela+reviewer, SAVE RECOVERY-ONLY).
   Sem isso, C = NO-GO.
15. **Forma da autorização dedicada está pinada? [NOVO 2a]** SIM M0255:
   F-P82-AUTH-01..07 (comando exato, janela, reviewer+alternativa, P-D1..P-D6 verdes,
   hash+lista fechada, watchdog ID, assinatura 1-uso). Fora da forma = NO-GO.
   Este arquivo NÃO é essa autorização. P82_LIVE_EXECUTION_AUTHORIZED = NO.
