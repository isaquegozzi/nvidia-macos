# P65 INTERRUPTS

## Respostas

```text
INTERRUPTS_REQUIRED_BEFORE_FIRST_GSP_BOOT = NO
FIRST_INTERRUPT_DEPENDENCY_POINT = steady-state RPC/notificacoes GSP->CPU via SWGEN0 apos INIT_DONE (cmdq); bring-up ate INIT_DONE e 100% polling.
```

## Prova

- Nova-core boot nao registra IRQ: `grep -ri irq/msi` no source set so acerta
  `NV_PFALCON_FALCON_IRQSCLR.swgen0` + `clear_swgen0_intr()` ("Clears the SWGEN0 bit ... to allow GSP
  to signal CPU for processing new messages"). `Falcon::new(gsp)` chama `clear_swgen0_intr()` uma vez;
  nenhuma `request_irq/enable_msi` no caminho `probe -> Gpu::new -> gsp.boot -> sequencer ->
  wait_gsp_init_done`.
- `gsp/cmdq.rs`: `RECEIVE_TIMEOUT=5s`, `send_command*`, `receive_msg/wait_for_msg` via
  `read_poll_timeout` (TX space poll 1us; RX msg poll 1ms; sequencer cmds RegPoll com timeout do
  firmware, default 4s). `wait_gsp_init_done` = loop `receive_msg::<GspInitDone>` com ERANGE=>continue.
  `GspSequencer::run` = loop `receive_msg::<GspSequence>` idem. Tudo polling; SWGEN0 e otimizacao
  futura, nao dependencia.
- `falcon/gsp.rs:check_reload_completed` (BSI_SCRATCH_14, 2s), `falcon.rs:wait_till_halted` (2s),
  `gsp/boot.rs` RISC-V poll (10ms/5s) e shutdown mailbox bit31 (10ms/5s): todos polls.

## Implicacao macOS

- Nenhum gate de interrupt antes do primeiro boot GSP. `INTERRUPTS_AUTHORIZED` permanece NO.
  Quando (bem depois) o steady-state precisar, o ponto sera `clear_swgen0_intr` + MSI — fora do escopo P65.
