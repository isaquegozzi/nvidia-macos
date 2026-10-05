# P81 — Threat Model (primeiro live validation)

```text
SESSION_ID: P81-DESIGN-RELAY-V3
STATUS: DESIGN-ONLY (nada executado, nada autorizado)
P81_EXECUTION_AUTHORIZATION = NONE
LIVE_EXECUTION_AUTHORIZED = NO
```

Contexto: P77+P78+P79+P80 APPROVED (offline). Candidato 1.8.0
`TG-KEXT8-PRODUCTION-ARCHITECTURE` offline (hashes em `P80-FINAL-HANDOFF.md`).
1.7.3 read-only (452/452). Live histórico: GA106Lab 1.5.3 carregado.
Dois panics Tahoe SEM causalidade atribuída + gpuRestarts AMD Renoir/Vega antigos.

## T1. Kernel panic durante/fóra da janela de teste
- Descrição: panic Tahoe (qualquer assinatura) ocorre antes, durante ou após a
  janela proposta, com ou sem nosso KEXT envolvido.
- Por que importa: risco de misatribuição (culpar ou inocentar o KEXT sem evidência).
- Controles de design: teste proposto é zero-stage/zero-load/zero-write (§P81-MINIMAL-LIVE-TEST);
  baseline de panics coletada antes (só leitura de logs já existentes);
  PASS/FAIL/ABORT nunca inferem causalidade a partir de coincidência temporal
  (ver `P81-PANIC-RISK-ASSESSMENT.md`); toda coleta pós-panic é read-only.
- Residual: panics Tahoe podem ocorrer independentemente do teste. Aceito e declarado.

## T2. No-signal / perda de vídeo
- Descrição: monitor sem sinal após boot de controle ou após qualquer interação
  com GPU (mesmo read-only, pelo risco de estado pré-existente instável).
- Por que importa: impede coleta de evidência e induz a improvisar ("tentar mais uma coisa").
- Controles: nenhuma etapa depende de improvisação após reboot (ordem exata + fallback
  impresso em papel antes); teste proposto não toca em display/GPU piece;
  path de boot recovery documentado em `P81-ROLLBACK-RECOVERY.md` (não executado);
  ABORT inclui "sem vídeo por > N min => parar, fotografar LEDs/ventoinhas, ir para recovery path".
- Residual: HW pode estar em estado desconhecido. Declarado NÃO-PROVADO.

## T3. Use-after-free / lifetime race no teardown (start/stop/clientClose/unload)
- Descrição: UAF ou double-release em teardown, padrão historicamente associado a
  panics ipc-ports (panic 1, medium-confidence, SEM atribuição ao GA106Lab).
- Por que importa: é a classe de bug mais temida em KEXT; P80 endureceu lifetime
  offline mas nada foi provado live.
- Controles: o teste proposto NÃO faz load/unload/stage de KEXT algum
  (logo não exercita esse caminho); D lê do residente SÓ presença via kextstat —
  qualquer leitura além disso pertence à fase A e exige antes o método Q10 (hash
  carregado-vs-disco sem unload) definido; até lá, A fica SÓ-OS-level como D (R3);
  qualquer futuro teste que exercite teardown exigirá design próprio +
  `REQUIRED_FUTURE_CHANGE` se precisar de instrumentação.
- Residual: lifetime live de 1.5.3 e 1.8.0 segue NÃO-PROVADO live (observer-vs-teardown
  declarado até Q10 definido).

## T4. Teardown race (stop × close × unload concorrentes)
- Descrição: `stop()` concorrendo com `clientClose()`/unload, double-stop/close.
- Controles: idem T3 — fora da superfície do teste proposto (superfície = zero).
  P80 prova disciplina offline (pthreads+TSAN em doubles); não extrapolar para kernel live.
- Residual: NÃO-PROVADO live.

## T5. Estado GPU desconhecido (BAR, BME, DMA, GSP, Falcon, VRAM, doorbell, PUT)
- Descrição: não sabemos o estado real do HW agora (BME ligado? DMA pendente?
  GSP em que estado? PUT/GET onde? VRAM com o quê?).
- Por que importa: qualquer write futuro sem baseline de estado é tiro no escuro.
- Controles: teste proposto NÃO lê nem escreve HW novo; baseline de estado é
  coletada só via fontes OS-level read-only já existentes (ioreg/kextstat/logs);
  nenhuma inferência de "estado seguro para write" a partir desses dados; veredito
  PASS carrega `GPU-STATE = UNKNOWN` persistente (PASS-6) e carimbo de observabilidade
  em CLOSE (R1).
- Residual: estado HW real = DESCONHECIDO. Assumpção marcada NÃO-PROVADA (persiste após PASS).

## T6. Corrupção de EFI / config (boot não volta)
- Descrição: edição de EFI/config impede retorno ao known-good.
- Controles: ZERO writes e ZERO mounts em EFI/config neste design; `P81-ROLLBACK-RECOVERY.md`
  documenta identidade SEM montar (`diskutil info -plist`) sem executar; hash de
  conteúdo ESP só no ensaio Option 0 non-target, que é PRECONDIÇÃO DURA P8 de D —
  sem Option 0 PASS, D não abre; S12-identidade parcial declarada INSUFICIENTE para
  qualquer write futuro; fallback nunca escala para mount no alvo (R2); qualquer futuro
  teste com EFI-write exigirá backup verificado + review separado.
- Residual: erro humano ao copiar comandos no dia live. Mitigado por runbook
  sem improvisação + matriz de autorização tudo-NO.

## T7. Observer effect (o próprio ato de observar desestabiliza)
- Descrição: `log stream`, `spindump`, profiling ou polling agressivo pode
  piorar pressão de memória/IPC num sistema já com panics.
- Controles: coleta limitada a comandos read-only de baixo impacto, com bounds
  quantificados (predicado `log show` + cap 200 linhas, ioreg profundidade ≤ 3 / 300
  linhas, destino ≥ 5 GB, 60 s/comando) medidos no Option 0 — estouro reprova D antes
  de abrir (R6); sem `log stream` contínuo sem bound; sem sampling de kernel.
- Fallback G-M0244-12 (DURA, AL-3): se `log show` travar (> 60 s), PULAR LOGS-1/LOGS-2
  com nota em INDEX; sem retry extra, sem improvisar, sem trocar predicado no dia.
  (S14 rebaixada para PROVADA-condicional.)
- Residual: baixo, mas não-zero. (Exaustão de disco movida para T10.)

## T8. Misatribuição / confirmação (viés)
- Descrição: "não panicou => KEXT é seguro" ou "panicou => KEXT causou".
- Controles: hipótese única e fraca (ver MINIMAL-LIVE-TEST: só afirma
  observabilidade, nunca segurança); PASS não autoriza writes futuros e NÃO pode ser
  lido como live-ready/stage-ready (proibição ABORT→FAIL→PASS estendida a PASS, R7);
  FAIL não atribui causa; ABORT não diagnostica; reviewer + SLA do FAIL definidos antes
  da primeira janela (R7).
- Residual: viés humano. Mitigado por critérios escritos antes.

## T9. Escopo-creep no dia live ("já que estamos aqui, vamos só...")
- Descrição: pressão para aproveitar a janela e executar um write "rapidinho".
- Controles: `P81-AUTHORIZATION-MATRIX.md` tudo = NO; runbook sem nenhum campo
  de write; ABORT explícito para "qualquer proposta de write = parar".
- Residual: depende de disciplina humana. Declarado.

## T10. Transições de energia / sleep-wake + exaustão de disco na janela
- Descrição: (a) transição de energia durante a janela (fechar tampa, perda de AC,
  sleep/wake, power-gating da GPU) altera estado observável ou interrompe a coleta;
  (b) coleta de logs/evidência enche o disco de destino (ou o volume do sistema),
  fazendo comandos falharem ou o SO se degradar.
- Por que importa: ambas ocorrem SEM nenhum write nosso — residuais mesmo num
  design zero-touch/zero-write verdadeiro.
- Controles: janela curta (15 min, ou D-minus ≤ 5 min / D-split em 2 micro-janelas, R8)
  com operador presente e tampa aberta; AC ligado antes de T+00 (parte de PRE); sem
  `sleep` manual na janela; destino de evidência com ≥ 5 GB verificados em PRE (P3,
  pré-condição de abertura G-M0244-09); coleta com bounds R6 (sem `log stream`
  contínuo, sem sysdiagnose); qualquer sleep/wake ou disco cheio => ABORT
  (ABORT-HEALTH), sem retry no dia.
- Checklist assinado G-M0244-14 (DURA, UA-4/UA-7, M0244): AC + tampa aberta +
  sem-sleep + espaço ≥ 5 GB como PRÉ-CONDICÃO assinada no CLOSE (nome por extenso +
  hora); sem assinatura, veredito NÃO é PASS.
- Residual: NÃO-ZERO e declarado (operador/ambiente não têm enforcement técnico;
  sleep/wake power-gating da GPU pode ocorrer sem write nosso). (Adicionado em M0242, P81-GLM-RISK-01.)

## O que este threat model NÃO cobre (fora de escopo P81)
- Writes de qualquer classe (MMIO/BME/DMA/GSP/firmware/VRAM/irq/reset/doorbell/
  PUT/command/IOGPU/IOAccelerator/Metal): ameaças dessas classes serão modeladas
  no milestone que as propuser, nunca neste.
- Segurança de supply-chain, assinatura, notarização: já cobertas offline; sem mudança.
