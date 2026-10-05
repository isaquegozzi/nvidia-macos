# LIFETIME — Gate A persiste (P68 §9-10, P67 CAN_RELEASE_AFTER_GATE_B=NO)

Estados fFlushState: 0 Unprepared, 1 AddressReady (interno pos-template),
2 PreconditionsReady (pos-verify). Nunca `Programmed` (sem Gate B).
fFlushBusy = exclusao; fFlushStopping terminal (stop inicia, nunca limpa).

Owner flow 1.6.2 preservado: pin/retain sob trava compartilhada em
externalMethod (selector9 usa PinOwnerForExternalMethod como selector8),
drain de active-calls em stop(), clientClose bloqueia novos pins, stop drena
antes de liberar base, provider retain/release, sem write-after-stop, sem
alloc/MMIO bloqueante sob trava. Pagina/mapping (fFlushDesc/Cmd/Addr/Len)
vivem no servico apos sucesso; close nao libera; stop libera via helper
TeardownFlushPrewritePageFlow (complete/clear/releases/reset, zero MMIO).
Segunda chamada revalida e retorna AlreadyReady sem segunda alloc.
