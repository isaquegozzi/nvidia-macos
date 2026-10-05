# P82 — First Step (candidata C: GFW readiness read-only, DESIGN-ONLY)

```text
SESSION_ID: FASTTRACK-DMINUS-D-P82-RELAY-V3
P82_LIVE_EXECUTION_AUTHORIZED = NO
Este passo NÃO está autorizado e NÃO foi executado. Janela live = 0.
Classe: 1 selector read-only pré-existente, 1 invocação, zero writes.
```

```text
FIRST_REAL_GPU_VALIDATION_CANDIDATE = C (GFW readiness read-only, zero writes)
```

## Hipótese única (H-P82)
"É possível interrogar UMA vez, com bound de tempo, o estado GFW readiness via
caminho read-only já existente — sem mutar HW/SW — e voltar ao estado anterior
com artefato verificável."
Este passo NÃO afirma: segurança do KEXT, estado do HW, prontidão para writes,
identidade do residente (Q10 BLOCKED), taxa de panic.
Prior GA106 (2d): sem GSP/FW o resultado esperado de C é FAIL protocolar
(timeout/erro de readiness), não veredito sobre o KEXT; PASS seria surpresa.

## Pré-condições (todas ANTES; qualquer uma falsa = NÃO abre, sem improvisar)
- P-D1. D-minus resolvido: Option 0 PASS em non-target/VM existe e publicado (P8);
  sem isso, nada abre (gate duro herdado de P81).
- P-D2. Reviewer + ALTERNATIVA nomeados no runbook/INDEX (P9); SLA 7 dias.
  Nomes pinados em F-P82-AUTH-03; operador não é reviewer.
- P-D3. Caminho de foto/evidência + watchdog/serial resolvido para headless
  (P3/FOTO-0) e publicado antes de C; sem câmera/serial/watchdog pinado, sem
  janela (gap bloqueante; ver Timeout/hang).
- P-D4. Auditoria offline do selector publicada + review + hash pinado (gate duro;
  sem ela C = NO-GO, cai para D/A-OS). Exige TODOS:
  P-D4.0 hash-pin: sha256 do binário-fonte auditado + CFBundleVersion pinados na
  autorização dedicada (anti contaminação 1.5.3 residente vs 1.6.2/1.7.x nunca-live;
  auditoria em geração errada = FAIL, sem reaproveitar).
  P-D4.1 lifetime/UAF: prova por leitura de código de retain/release balanceado no
  caminho (open/client, matching/dispatch, teardown), ausência de use-after-free em
  detach/unload concorrente e em falha no meio do caminho; sem ponteiro BAR/mmapped
  retido após free/unmap.
  P-D4.2 lista fechada: enumerar exaustivamente BARs/registradores/offsets lidos
  (BAR#, offset, size); qualquer leitura fora da lista = violação ABORT-SCOPE.
  P-D4.3 sem efeito de leitura: prova de ausência de clear-on-read, poll-write,
  FIFO-consume, doorbell/handshake implícito ou fault com BME/DMA off no conjunto P-D4.2.
  P-D4.4 bound+copyout: bound publicado + copyout limitado ao buffer declarado, sem
  overflow/OOB, tamanho máximo pinado.
  P-D4.5 locks/IRQ: análise de locks (sem lock que cunha, ordem documentada), sem
  enable/mask de IRQ, sem sleep despercebido; caminho IRQ-free ou IRQ-analysis publicada.
- P-D5. D PASS (ou D-minus+D-split conforme runbook) anterior íntegro; destino de
  evidência externa ≥ 5 GB + relógio G-M0244-01-R1 registrados; runbook impresso em
  segundo dispositivo; janela 30 min reservada; recovery path sabido de cor.
  Mais baseline HW GA106 documentada em D antes de C: estado BME/DMA, BARs mapeados
  (qual BAR/size), GSP/FW ausente declarado, expectativa FAIL protocolar registrada.
  Sem baseline HW pinada, C = NO-GO.
- P-D6. Autorização humana dedicada para ESTA invocação (não herdada de P81/D),
  SOMENTE na forma pinada F-P82-AUTH abaixo; fora da forma = NO-GO.

## Forma pinada da autorização dedicada (F-P82-AUTH — forma, não autorização)
Nenhuma janela C abre sem documento dedicado contendo EXATAMENTE (2a):
F-P82-AUTH-01 comando exato byte-for-byte (binário userspace + argv + selector +
size + E-P82-1 forma); F-P82-AUTH-02 janela (data, hora início/fim, duração ≤15+15,
operador presente); F-P82-AUTH-03 reviewer + alternativa nomeados (P-D2) + SLA 7d,
nenhum é o operador; F-P82-AUTH-04 checklist P-D1..P-D6 todas VERDES com evidência
anexada (Option 0 PASS ID, foto-path ID, auditoria hash P-D4.0, D PASS ID + baseline
HW, relógio); F-P82-AUTH-05 hash pinado do binário auditado (sha256 + version) +
lista fechada P-D4.2; F-P82-AUTH-06 watchdog/serial path ID (P-D3); F-P82-AUTH-07
assinatura humana explícita com data; validade = 1 invocação, 1 dia, sem reuso.
Sem qualquer campo = NÃO abre, sem improvisar. Este arquivo NÃO é essa autorização.

## Prior GA106 (2d — FAIL protocolar provável)
C-em-GA106 sem GSP/FW = provável FAIL protocolar esperado (timeout/erro readiness,
BAR sem resposta FW), NÃO prova de driver quebrado. PASS seria surpresa, não baseline.
Baseline HW exigida em P-D5 antes de C; FAIL protocolar entra em FAIL sem atribuição.

## Ordem (fases, sem branches além de ABORT)
1. T+00 IDENT pré: presença-OS-level mínima (mesma superfície de D) + foto setup.
2. T+01..T+02 Leitura GFW: UMA invocação do selector auditado, bound 60 s (R6).
   Travou > 60 s => ABORT (não insistir, não re-invocar).
3. T+03 IDENT pós: repetir presença-OS-level + foto; comparar (allowlist P81:
   só data/hora/uptime/contadores; qualquer diff de linha KEXT = FAIL).
4. T+04 CLOSE + SAVE externo imediato + INDEX com shasum antes de qualquer análise.

## Timeout / hang (2e — bound userspace NÃO é watchdog)
Janela total ≤ 15 min de instrumentação (+15 margem operacional). Cada comando 60 s.
Sem vídeo/operador-resposta no início por > 5 min => ABORT antes de T+01.
Bound 60 s é limite userspace, NÃO recupera hang de kernel/GPU. Janela C só abre com
watchdog/serial externo publicado antes (P-D3): path serial/segundo-dispositivo com ID
pinado em F-P82-AUTH-06, capaz de observar hang sem depender do alvo. Sem watchdog/
serial pinado, C = NO-GO (gap bloqueante P3). Hard reset destrói logs e contradiz SAVE
imediato: proibido "reboot para limpar e continuar" no dia.

## Abort (exato, sem improvisar)
Parar após a invocação em curso expirar (nunca matar/repetir); 1 foto de estado
(LEDs/ventoinhas/tela); checklist de estado (o que foi/não foi invocado); SAVE do
parcial com nota ABORT; nenhuma nova invocação no dia; coleta pós-reboot = NOVA
janela com review. Qualquer mount-EFI, stage/load/unload, 2ª invocação, leitura fora
da lista P-D4.2, ou write de qualquer classe no dia = violação ABORT-SCOPE.

## PASS (todos, senão FAIL)
1. Artefato da leitura + IDENT pré/pós + fotos + INDEX com shasum salvos em mídia
   externa imediata. 2. Sistema inalterado (IDENT pós == pré pela allowlist).
3. Sem panic/hang/no-signal na janela. 4. Veredito carrega `GPU-STATE = UNKNOWN`
   (exceto o estreito sinal GFW lido, sem extrapolação) + carimbo de observabilidade.
5. PASS não autoriza nada futuro (nem 2ª invocação, nem writes).

## FAIL (sem atribuir causa)
Artefato faltante/corrompido; erro persistente do selector; timeout > 60 s; panic
na janela (sem dizer "foi/não foi o KEXT"); diff IDENT fora da allowlist;
FAIL protocolar GA106 esperado (GSP/FW ausente) sem atribuição ao KEXT.
FAIL zera contagens (3-D/1-A/1-B/C-gates) e exige review dedicado antes de reabrir.

## Rollback / volta ao estado
Por construção read-only: nada a desfazer no caminho PASS (estado posterior =
anterior, verificado por IDENT pós). Pós-panic: seguir rollback P81 (recovery path,
sem re-tentar no dia); estado pós-janela = DESCONHECIDO. Exceção recovery-only SÓ com
autorização humana explícita separada na forma F-P82-REC: REC-01 trigger (panic/hang/
no-signal com foto), REC-02 comandos exatos permitidos (só recovery P81, sem stage/
load/invocação), REC-03 janela + reviewer, REC-04 SAVE com nota RECOVERY-ONLY.
Fora da forma = violação. Nenhum reboot "para limpar e continuar" no dia.

## Evidência (mínima, suficiente)
E-P82-1 saída da invocação (forma exata SÓ em F-P82-AUTH-01/05, não aqui);
E-P82-2 IDENT pré/pós + kextstat-presença; E-P82-3 fotos + EXIF; E-P82-4
INDEX (comando-forma, bounds, P-D1..P-D6 marcadas, shasums, redação de
seriais/MACs/UUIDs como P81-Q5). Sem evidência externa imediata, janela NÃO abre.

## Pinning & expiry (2f — anti cross-generation contamination)
P82-DESIGN-V3 (2026-09-09, FASTTRACK-DMINUS-D-P82-RELAY-V3). Escopo: 1.5.3 residente
(alvo) vs 1.6.2/1.7.x nunca-live (auditar geração errada = FAIL). Validade: qualquer
mudança macOS/EFI/HW/GPU/cabos/KEXT invalida e exige re-review completo antes de
qualquer janela (Q11). Auditoria futura deve citar este DESIGN_VERSION + hash do
binário auditado (P-D4.0); sem pin = NO-GO. P82_LIVE_EXECUTION_AUTHORIZED = NO.
