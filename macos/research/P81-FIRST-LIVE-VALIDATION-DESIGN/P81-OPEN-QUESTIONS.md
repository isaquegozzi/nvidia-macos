# P81 — Open Questions (honestas, em aberto)

1. **Selectors read-only do 1.5.3 são aceitáveis na fase A?** S15 diz que executam
   código no kernel. Precisamos de auditoria side-effect-free + bound antes de
   incluí-los, ou A fica para sempre só-OS-level? (`REQUIRED_FUTURE_CHANGE-01`.)
2. **Quantas janelas D/A/B até a baseline ser "suficiente"?** RESOLVIDO em M0243, MARCADO ARBITRÁRIO em M0244 (G-M0244-05):
   mínimo 3 janelas D PASS consecutivas + 1 A + 1 B antes de sequer DISCUTIR C — critério ARBITRÁRIO/CONSERVADOR,
   SEM power analysis, sem estimar panic-rate; qualquer FAIL/ABORT zera a contagem; antes de DISCUTIR C exige-se
   RE-JUSTIFICATIVA escrita + milestone próprio + review Gemini + autorização humana dedicada.
3. **Onde salvar evidência de forma que sobreviva a um panic?** DECIDIDO em M0244 (G-M0244-06, fecha Q3):
   MÍDIA EXTERNA IMEDIATA ao fim de cada (micro-)janela, antes do CLOSE; cópia pós-janela NÃO conta como
   procedimento primário. Sem destino externo imediato, janela NÃO abre (BLOQUEADORA de abertura até haver mídia).
   D-split exige CLOSE por micro-janela (J1/J2) + SAVE externo imediato em cada uma.
4. **Identidade EFI sem mount basta para S12 no alvo?** RESOLVIDO em M0243: NÃO basta
   para writes — S12-identidade parcial do alvo é declarada explicitamente INSUFICIENTE
   para qualquer write futuro (ver ROLLBACK §1 + MINIMAL T+04). `diskutil info -plist`
   serve SÓ como identidade observável da janela; hash-ESP de conteúdo SÓ no Option 0
   non-target (gate duro P8). Fallback nunca escala para mount no alvo (nem RO).
5. **Como fotografar/registrar sem vazar seriais?** DECIDIDO em M0244 (fecha Q5):
   política de redação = seriais/MACs/UUIDs REDIGIDOS em qualquer evidência publicada fora da máquina
   (foto com serial visível => crop/redação antes de publicar; original íntegro preservado offline com shasum);
   INDEX anota quais campos foram redigidos. Sem política aplicada, evidência NÃO é publicada
   (BLOQUEADORA de publicação, não de abertura — a janela pode abrir, mas nada sai sem redação).
6. **Critério para reabrir C (stage 1.8.0)?** RESOLVIDO em M0243: 3 D PASS + 1 A + 1 B
   (contagem zerada por qualquer FAIL/ABORT) + milestone próprio + review Gemini +
   autorização humana dedicada. Hoje: C segue REJEITADA para P81.
7. **Risco de observer effect é real ou teórico aqui?** RESOLVIDO em M0243: medir
   duração/impacto no Option 0 (bounds R6 publicados em EVIDENCE-COLLECTION); qualquer
   comando > 60 s reprova D antes de abrir. Atualização P81.1: o próprio gate de clock
   agora é provadamente não-perturbador (G-M0244-01-R1 read-only; `sntp -sS` removido) —
   a verificação de horário deixa de ser fonte de observer effect; bounds R6 seguem medidos no Option 0.
8. **Boot de controle (B) usa qual mídia/config?** Entrada separada vs USB vs
   snapshot: decisão adiada para o design de B (não improvisar no dia de D).
9. **O que fazer com divergência IDENT-2 vs T+08?** RESOLVIDO em M0243, SINCRONIZADO em P81.1 com a allowlist normativa de
   PASS-FAIL-ABORT (§ Allowlist): ALLOW só data/hora/uptime/contadores; processos user-space não aparecem em `kextstat`;
   qualquer diff de linha KEXT fora da allowlist normativa = FAIL-3; sem improvisação no dia. (Texto stale "user-space
   `com.apple.*` efêmero (com nota)" REMOVIDO — era vácuo e ensinava a desculpar linha extra.)
10. **1.5.3 residente hoje é o mesmo binário do histórico?** ESCOPADO em M0243 (R3):
    método read-only de hash carregado-vs-disco sem unload tem de estar DEFINIDO em A
    antes de qualquer leitura do residente além de presença via kextstat; até lá, A fica
    SÓ-OS-level como D. D lê do residente SÓ presença (kextstat).
11. **Panics futuros: quem faz o review do FAIL?** RESOLVIDO em M0243, ENDURECIDO em M0244 (G-M0244-08):
    reviewer + ALTERNATIVA NOMEADOS no runbook modelo + transcritos no INDEX ANTES da primeira
    janela (fecha parte de Q11; operador não revisa o próprio FAIL); SLA = 7 dias; sem os dois nomes
    ou sem review concluído, nenhuma nova janela abre.
12. **Este design envelhece?** RESOLVIDO em M0243 (ver PASS-FAIL-ABORT § Review do FAIL):
    qualquer mudança de versão macOS/Tahoe, EFI/config ou HW (GPU/cabos/monitor/máquina)
    INVALIDA a baseline e exige re-review completo antes de nova janela.
