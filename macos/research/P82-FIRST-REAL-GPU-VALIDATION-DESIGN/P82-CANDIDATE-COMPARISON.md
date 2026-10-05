# P82 — Candidate Comparison A–E (risco × evidência × superfície)

```text
SESSION_ID: FASTTRACK-DMINUS-D-P82-RELAY-V3
MILESTONE: FAST_TRACK_DMINUS_D_P82_DESIGN (P82 = DESIGN OFFLINE ONLY)
STATE: P82_DESIGN / ACTOR: MUSE_BUILD
P82_LIVE_EXECUTION_AUTHORIZED = NO
Nenhum candidato está autorizado. Comparação de design para review. Nada executado.
D completo NÃO executado. D-minus = ABORT-at-preconditions (P8 Option 0 HUMAN_BLOCK
sem VM; P9 sem reviewer nomeado; P3/FOTO-0 sem câmera headless). Zero passos de
runbook executados, zero panic/hang/no-signal, state íntegro.
```

```text
FIRST_REAL_GPU_VALIDATION_CANDIDATE = C (GFW readiness read-only, zero writes),
  gated by D + Option 0 PASS + auditoria de selector P-D4.0..P-D4.5 + baseline HW
  GA106 + watchdog/serial + preconditions P8/P9/P3 resolvidas + F-P82-AUTH completa.
  Ver P82-FIRST-STEP.md. DESIGN-ONLY, NÃO autorizado, NÃO executado.
```

## Base comum (herdada de P81, não re-provada aqui)
- S15: selector read-only ≠ inofensivo (executa código no kernel). Qualquer selector
  live exige auditoria P-D4.0..P-D4.5 + bound ANTES (lifetime/UAF, lista fechada,
  clear-on-read, copyout, locks/IRQ, hash pinado).
- R3/Q10: ler do 1.5.3 residente além de presença exige método Q10 DEFINIDO antes.
  Q10 resolvida neste pacote: `Q10 = BLOCKED` (ver `P82-Q10-RESOLUTION.md`;
  AMFI-load-time rebaixado M0255: mecanismo não-verificado, sem atenuação).
- GPU-STATE = UNKNOWN persiste após qualquer PASS read-only.
- D-minus abortou nas pré-condições; D NOT_RUN. P82 não herda nenhuma autorização live.
- Futura autorização live só na forma pinada F-P82-AUTH-01..07 (comando exato, janela,
  reviewer+alternativa, P-D1..P-D6 verdes, hash, watchdog); fora da forma = NO-GO.

## A) Observar 1.5.3 além de presença, SE Q10 provado read-only
- O quê (se um dia autorizado): qualquer leitura do residente além de presença
  via kextstat (ex.: identidade do código carregado vs binário em disco).
- Risco: BAIXO na forma OS-level, mas a parte "além de presença" é BLOQUEADA:
  Q10 = BLOCKED — não existe método read-only confiável documentado (kextstat /
  `kmutil showloaded` não expõem UUID/hash; `kmutil inspect --show-kext-uuids` fala
  da coleção em disco, não da memória viva; codesign valida arquivo, não RAM;
  version-string via selector executa código do KEXT = circular). Sem improvisar.
- Evidência: SE o método existisse, vincularia "o que está carregado" ao binário
  histórico. Como não existe, A colapsa para presença-OS-level = mesma superfície de D.
- Superfície: OS-level read-only (kextstat-presença) + nada mais.
- Veredito: A-PLUS = NO-GO (Q10 BLOCKED). A-OS (só presença) = permitido como parte
  de D, sem evidência nova sobre a RTX.

## B) Selector read-only previamente existente, se side-effect-free provado
- O quê (se um dia autorizado): 1 invocação de um selector read-only já existente
  no 1.5.3 (ex.: identidade/versão), após auditoria offline P-D4.0..P-D4.5 que prove:
  hash pinado (anti 1.5.3 vs 1.6.2/1.7.x), lifetime/UAF (retain/release, detach/unload
  concorrente), lista fechada BARs/registradores, ausência de clear-on-read/poll-write,
  bound + copyout limitado, análise locks/IRQ; zero writes
  (MMIO/PCI/BME/DMA/GSP/firmware/VRAM/IRQ/reset/doorbell/PUT/command).
- Risco: BAIXO-MÉDIO. Mesmo "read-only" executa código do KEXT no kernel (S15,
  T3/T4 lifetime/teardown). Auditoria reduz, não elimina, o risco; o caminho de
  dispatch/matching do KEXT é exercitado live pela primeira vez nesta classe.
- Evidência: prova SÓ o caminho de dispatch do driver. Nenhum sinal derivado do
  HW/FW. Estritamente dominada por C (mesma classe de risco, menos evidência).
- Superfície: 1 trap userspace→kernel + código do KEXT; zero HW writes (se auditado).
- Veredito: B standalone = NÃO executar separadamente (dominada por C). A auditoria
  exigida para B é REAPROVEITADA como pré-condição de C. Sem auditoria publicada +
  review + hash pinado, B = NO-GO.

## C) GFW readiness read-only — CANDIDATA (first real GPU step)
- O quê (se um dia autorizado): 1 invocação única, bounded, de leitura de GFW
  readiness (selector8-classe, só leitura; pacote 1.6.2/1.7.x nunca foi live),
  após: (a) auditoria P-D4.0..P-D4.5 + bound publicada com hash pinado; (b) Option 0
  PASS (gate duro P8); (c) D PASS + baseline HW GA106 (BME/DMA estado, BARs mapeados?,
  GSP/FW ausente declarado); (d) P8/P9/P3 resolvidas + watchdog/serial pinado;
  (e) review + autorização humana dedicada F-P82-AUTH-01..07. Zero writes de qualquer
  classe. Detalhe em `P82-FIRST-STEP.md`.
- Risco: MÉDIO-BAIXO (o maior entre as opções read-only, o menor entre as que
  tocam a GPU de verdade). Executa código do KEXT (T3/T4) + lê estado derivado do
  HW/FW via BAR (T5/T7 observer). Sem writes: sem mutação de PUT/GET/VRAM/BME/DMA.
  Prior GA106 (2d): sem GSP/FW, C-em-GA106 = provável FAIL protocolar esperado
  (timeout/erro readiness), NÃO prova de driver quebrado; PASS seria surpresa.
- Evidência: PRIMEIRO sinal derivado do par driver+RTX/FW (não do SO). É o primeiro
  passo que sai de "observabilidade do SO" e entra em "validar a RTX/driver de
  verdade": toda opção mais fraca (D, A-OS) só observa o SO; C interroga o driver
  sobre estado de FW/GPU sem mutar nada. C-PASS carrega GPU-STATE = UNKNOWN
  (Q10 BLOCKED: nunca prova "qual binário respondeu").
- Superfície: mínima superfície que ainda conta como GPU: 1 selector, 1 invocação,
  só-leitura, bounded, sem stage/load/unload, sem mounts, sem Metal.
- Veredito: ESCOLHIDA como `FIRST_REAL_GPU_VALIDATION_CANDIDATE`, estritamente como
  design (live = 0, tudo NO). B é absorvida como pré-requisito de auditoria, não
  como passo live separado.

## D) Precondition-only validation (gate duro, zero GPU)
- O quê (se um dia autorizado): validar SOMENTE pré-condições no alvo sem tocar em
  GPU/KEXT/EFI: P8 (Option 0 non-target/VM PASS existe?), P9 (reviewer + alternativa
  nomeados no runbook/INDEX + F-P82-AUTH-03?), P3/FOTO-0 (caminho de foto/evidência +
  watchdog/serial resolvido no headless e publicado?), P1/P2/P5/P6 (runbook lido,
  recovery F-P82-REC sabido, janela reservada), destino de evidência + relógio
  (G-M0244-01-R1, sem `sntp -sS`), baseline HW GA106 documentada (BME/DMA, BARs, GSP/FW).
- Risco: ~ZERO para o alvo (nenhuma interação com GPU/KEXT/EFI; OS-level mínimo ou
  só conferência documental).
- Evidência: ZERO sobre a RTX. Valor = destravar D-minus/D com honestidade: foi
  exatamente aqui que D-minus abortou (P8 sem VM, P9 sem reviewer, P3 sem câmera).
- Superfície: nenhuma superfície GPU.
- Veredito: HARD GATE antes de C (e antes de reabrir D). Não é "real GPU
  validation" — é o que impede repetir o abort.

## E) First real write (MMIO) — TETO DE RISCO, comparação apenas, NUNCA autorizada
- O quê (HIPÓTESE DE COMPARAÇÃO, não proposta): 1 write MMIO mínimo (ex.: scratch
  register BAR0), mesmo com baseline + pipeline + review.
- Risco: ALTO e INACEITÁVEL agora. Primeira mutação real de estado: T1 (panic com
  HW mutado), T2 (no-signal), T5 (estado desconhecido: BME/DMA/GSP/PUT/GET/VRAM/IRQ),
  T3/T4 (via caminho KEXT), sem rollback provado além de reboot (que destrói logs e
  muda estado). S05/S09/S10/S11/S12/S13 todas NÃO-PROVADAS para writes.
- Evidência: provaria o caminho de write — ao preço errado nesta altura. É o teto
  que calibra por que C (read-only) é o máximo discutível.
- Superfície: mutação de HW. Irreversível sem reset.
- Veredito: REJEITADA para P82. Reconsiderar só após D + A-OS + C-PASS + baseline +
  milestone próprio + review + autorização dedicada. Nenhuma frase deste pacote a autoriza.

## Ordem futura proposta (design, não autorização)
```text
D (precondition-only + baseline HW GA106, reabrir D-minus/D) -> A-OS (presença, sem Q10) ->
auditoria offline de selector P-D4.0..P-D4.5 + hash pinado (pré-condição) ->
watchdog/serial P3 publicado -> C (1x GFW readiness read-only, FAIL protocolar esperado) ->
STOP + review. B standalone NUNCA (absorvida). E NUNCA em P82.
Q10 = BLOCKED (A-plus fora). GPU-STATE = UNKNOWN persiste após qualquer PASS read-only.
```

```text
Contadores live deste turno: todos 0. Só escrita de documentação P82.
P82_LIVE_EXECUTION_AUTHORIZED = NO.
```

## Pinning & expiry (2f)
P82-DESIGN-V3 (2026-09-09, FASTTRACK-DMINUS-D-P82-RELAY-V3). Escopo: 1.5.3 residente
vs 1.6.2/1.7.x nunca-live. Validade: qualquer mudança macOS/EFI/HW/GPU/cabos/KEXT
invalida e exige re-review antes de qualquer janela. P82_LIVE_EXECUTION_AUTHORIZED = NO.
