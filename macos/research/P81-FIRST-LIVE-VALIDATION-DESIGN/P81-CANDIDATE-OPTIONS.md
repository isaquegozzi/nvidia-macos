# P81 — Candidate Options A–D (risco × evidência)

```text
P81_EXECUTION_AUTHORIZATION = NONE / LIVE_EXECUTION_AUTHORIZED = NO
Nenhuma opção está autorizada. Isto é comparação de design para review.
```

## Option 0) Dress-rehearsal do pipeline em máquina non-target / VM descartável (PRÉ-REQUISITO de D)
- O quê (se um dia autorizado): ensaiar o pipeline de coleta/rollback/recovery de D
  (ordem T+00..T+10, timeouts, SAVE, INDEX.md, CLOSE) numa máquina secundária ou VM
  descartável, com as mesmas formas de comando — SEM tocar o alvo NVIDIA/Tahoe.
  Inclui o hash-ESP (`shasum` com mount RO) que foi REMOVIDO do alvo.
- Risco: ZERO para o alvo (nenhum comando executa nele).
- Evidência ganha: prova o pipeline (PASS-1 de D) sem expor o alvo a T1/T2/T5/T7;
  valida runbook, bounds de tempo e formato de evidência antes da janela real.
- O que NÃO prova (exige a máquina real): S05-atual, S12 (hash EFI atual),
  S13-amostra — só coletáveis no alvo.
- Veredito de design: PRECONDIÇÃO DURA P8 de D (R2/R8). Option 0 PASS (nenhum comando
  > 60 s com bounds R6, `INDEX.md` + `shasum -a 256` válidos, hash-ESP coletado no
  non-target) é OBRIGATÓRIO antes de D abrir. Se a evidência Option 0 falhar, D NÃO abre
  (gate duro). Durações reais medidas no Option 0 valem como teto de D.
  (Adicionado em M0242, P81-QWEN-OPT-01; endurecido para gate duro em M0243.)
- Fidelidade mínima G-M0244-13 (DURA, UA-1, M0244; endurecida M0248/F9): Option 0 SÓ conta como PASS com
  fidelidade publicada no INDEX do ensaio — mesma major macOS do alvo, mesmas formas
  de comando (incl. comando-filtro G-M0244-04 e predicado `log show` idênticos), bounds
  R6 medidos. Sem fidelidade publicada, Option 0 = ensaio informal, D NÃO abre.
  Campos verificáveis (major macOS + formas de comando + bounds medidos) CONFERIDOS POR SEGUNDA PESSOA (nome + assinatura no INDEX do ensaio) ANTES de D abrir; sem segunda conferência, D NÃO abre.

## A) Manter live 1.5.3, só observar o já-carregado (sem stage/load novo)
- O quê (se um dia autorizado): comandos OS-level read-only + leitura de estado
  do 1.5.3 já residente; nenhum stage, nenhum load, nenhum unload, nenhum selector
  de write; discutir separadamente se selectors read-only entram (hoje: NÃO, ver S15).
- Pré-requisito R3: o método read-only de hash carregado-vs-disco sem unload (Q10) tem de
  estar DEFINIDO em A antes de qualquer leitura do residente além de presença via kextstat;
  até lá, A fica SÓ-OS-level como D (mesma superfície de D, nada além de kextstat-presença).
- Risco: MUITO BAIXO (não muda estado; risco residual = T7 observer effect + T1 coincidência + T3/T4 observer-vs-teardown até Q10 definido).
- Evidência ganha: confirma se o sistema atual ainda espelha o live histórico
  (S04→S05 parcial: "o que está carregado agora"); baseline de logs/panics recentes.
- Contra: não prova nada sobre 1.8.0; não prova lifetime; não prova HW state.
- Veredito de design: ACEITÁVEL como segunda fase (depois de D).

## B) Boot de controle SEM GA106Lab (baseline Tahoe)
- O quê (se autorizado): boot com config que não carrega GA106Lab; observar
  estabilidade/panics por janela fixa; voltar ao known-good.
- Risco: BAIXO-MÉDIO. Não toca GPU NVIDIA, mas mexe em seleção de boot/EFI-config
  (risco T6 erro humano) e perde vídeo/confunde operador (T2). Exige recovery path testado no papel.
- Evidência ganha: única forma honesta de estimar panic-rate basal do Tahoe
  (S13) e separar "Tahoe panica sozinho" de "panica com KEXT" — sem atribuir causa caso a caso.
- Contra: 1 janela não prova taxa; só gera 1 amostra; custo operacional (reboots) sem ganho direto sobre 1.8.0.
- Veredito de design: ÚTIL, mas DEPOIS de D/A; nunca antes de ter backup EFI verificado + fotos + runbook impresso.

## C) Stage controlado de 1.8.0 (qualquer stage/load, mesmo sem writes)
- O quê: stage/load do 1.8.0 offline-candidate, ainda que só para reads.
- Risco: ALTO e INACEITÁVEL agora. 1.8.0 nunca foi carregado (S09 NÃO-PROVADA);
  lifetime live NÃO-PROVADO (S10); panics Tahoe sem baseline (S13); estado HW
  desconhecido (S05). Stage/load exercita exatamente o caminho de teardown (T3/T4).
- Evidência ganha: provaria load/unload — mas ao preço errado nesta altura.
- Veredito de design: REJEITADA para P81. Reconsiderar só após D+A+B + review novo.
  (Se um dia proposta: milestone próprio, não "extensão" deste.)

## D) Sequência ainda mais conservadora: só OS-level, sem tocar em KEXT algum (RECOMENDADA como P81)
- O quê (se autorizado): somente `ioreg/kextstat/log show/nvram -p` read-only,
  identidade EFI SEM montar (comando EXATO pinado: `diskutil info -plist <device>` — ex.: `diskutil info -plist disk0s1`, device transcrito do runbook; sem `shasum` de conteúdo
  no alvo; hash-ESP adiado para o ensaio Option 0 non-target; qualquer `diskutil mount*` no alvo = violação ABORT-SCOPE, ver EVIDENCE PROIBIDO), fotos de tela,
  salvamento de evidência. ZERO selectors (nem read-only — ver S15),
  ZERO stage/load/unload, ZERO MMIO/PCI/BME/DMA/GSP/firmware/VRAM/IRQ/reset/
  doorbell/PUT/command/IOGPU/Metal, ZERO mounts (nem RO).
- ESCOPO-ZERO-MOUNT (M0248, fecha F6): ZERO mounts / mount-table intocada = ESP/volumes DO ALVO. Mídia externa de evidência É montada por desenho em T+09 (SAVE em MÍDIA EXTERNA IMEDIATA G-M0244-06) — montar a mídia externa NÃO viola o zero-mount do alvo; montar ESP/volume do alvo (mesmo RO, mesmo `diskutil mount readOnly`) VIOLA.
- Estado transitório restante declarado: NENHUM — sem mount no alvo, a mount-table do kernel
  não é tocada e nenhum driver FAT é acionado; ZERO writes VERDADEIRO no alvo.
  (Mount-ESP-RO removido em M0242, P81-QWEN-OPT-01: mount RO muta mount-table e pode
  disparar touch/repair FAT.)
- Risco: MÍNIMO (menor superfície possível que ainda gera evidência; residuais T7 + T10).
- Evidência ganha: estabelece S05-atual ("o que há agora"), S12 (EFI atual vs known-good),
  S13-amostra (+1 janela sem mudar nada), e prova o próprio pipeline de
  coleta/rollback/recovery SEM exercitar o KEXT. É o pré-requisito honesto de A/B/C.
- Contra: não prova nada sobre KEXT/HW além de observabilidade. É o ponto: P81 não
  promete prova de KEXT; promete baseline + pipeline.
- Veredito de design: RECOMENDADA como único teste P81, em staging R8: P81 = Option 0
  non-target PASS primeiro (gate duro P8 + fidelidade G-M0244-13); depois D-minus (FOTO-0 + IDENT-1 + mini-VERIFY G-M0244-02 + CLOSE,
  ≤ 5 min; sem mini-VERIFY = só ensaio fotográfico, sem status de pipeline OK) ou D-split (2 micro-janelas em dias distintos: J1 = T+00..T+04, J2 = T+05..T+10, cada uma com CLOSE próprio + SAVE em mídia externa IMEDIATA G-M0244-06; sem destino externo, D-split PROIBIDO);
  só então D completo. A segue depois, B depois de A, C fora de P81.

## Tabela comparativa

| Opt | Muda estado? | Superfície | Risco | Evidência real | Ordem proposta |
|---|---|---|---|---|---|
| 0 | Não (fora do alvo) | Pipeline em non-target/VM | Zero p/ o alvo | Pipeline provado + bounds medidos + hash-ESP, sem S05/S12/S13 | Gate duro P8 (antes de D; falha fecha D) |
| D-minus (com mini-VERIFY) / D-split (CLOSE+janela + mídia externa) | Não | OS read-only mínimo / metade de D | Mínimo | Pipeline no alvo + S05/S12-identidade parcial | 1ª (P81: Option 0 → D-minus/D-split → D; D-minus sem VERIFY = só foto, D-split sem mídia externa = PROIBIDO) |
| D | Não | OS read-only, sem mounts | Mínimo | Baseline + pipeline + S05/S12-identidade | 2ª (P81, após D-minus/D-split) |
| A | Não | OS + observar residente (S10 NÃO-PROVADO; Q10 antes de ler além de kextstat) | Muito baixo | Estado atual do live histórico | 3ª (P81b/futuro) |
| B | Boot-select | Boot/EFI-select | Baixo-médio | Panic-rate basal (1 amostra) | 3ª (futuro) |
| C | Sim (stage/load) | Kernel/KEXT/HW | Alto | Load/unload (preço errado agora) | FORA de P81 |

```text
FIRST_LIVE_VALIDATION_CANDIDATE (proposto) = Option 0 PASS (gate duro P8) + D-minus/D-split + D completo, nesta ordem
(+ A somente-observação como extensão futura separada, não no mesmo dia). Classe: precondition-only + read-only
identity/lifecycle observation. Write mínimo: NENHUM — "ainda não fazer write nenhum".
```
