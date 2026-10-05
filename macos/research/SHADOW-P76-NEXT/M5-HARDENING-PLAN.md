# SHADOW — M5 Gate B state-machine hardening PLAN (draft, NOT implemented, awaiting M4 verdict)
Scope (G0004 §2 + protocol M5), isolated in SHADOW-P76-NEXT/gateb-sim/, zero production wiring:
1. Explicit states: Idle / HiPosted / Verified / PartialCommitFailure / UnknownReadback.
   - HiPosted models the instant after HI write (observable only via fault hooks).
2. Forbidden-transition table (asserted): Verified->commit (Busy), Partial->commit (Busy),
   Verified->reset-to-Idle except via fresh object, any-state->rollback-writes (never).
3. Stop policy matrix: stop pre-commit => Aborted zero-writes; stop mid-window => ignored
   (non-abortable, both writes posted); stop post-commit => passive (state stands).
4. Readback-mismatch matrix: HI-mismatch / LO-mismatch / reconstruct-mismatch / read-fault
   => all PartialCommitFailure + pinned + write-count==2 + Busy-after.
5. Invariant checker: after every op, assert (writes<=2) && (state!=Idle || writes记账) ...
   (to be coded: check writes/history/state/pinned consistency).
6. Mutation suite (-D hooks in header, default-off): MUT_SWAP_ORDER (LO first),
   MUT_SKIP_READBACK, MUT_ROLLBACK_ON_MISMATCH, MUT_RETRY_AFTER_FAILURE,
   MUT_ABORT_MID_WINDOW, MUT_UNALIGNED_ACCEPT. Each must be CAUGHT by tests.
7. Concurrency smoke: two threads racing commit on shared backend with atomic winner;
   loser sees Busy-equivalent; TSAN clean.
Out of scope: barriers (deferred per M0005 Q3 unless directed), BME/DMA (M6/M7).
