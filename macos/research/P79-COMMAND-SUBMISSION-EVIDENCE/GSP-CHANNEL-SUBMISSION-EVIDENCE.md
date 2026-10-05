# P79 — GSP Submission-Boundary Evidence

## Verdict: PROVEN (RM configures; queues execute in channel/VM domain)

## 1. What GSP/RM owns

- Channel allocation (`GSP_RM_ALLOC=103` + class `0xc56f`), VASpace allocation
  (`FERMI_VASPACE_A`, P78), GPFIFO geometry (`gpFifoOffset/gpFifoEntries`),
  BIND (`0xa06f0104`) + SCHEDULE (`0xa06f0103`) — all P77/P78-proven RPCs.
- Work-submit administration: `CTRL_GPFIFO_GET_WORK_SUBMIT_TOKEN (186)` and
  `CTRL_GPFIFO_SET_WORK_SUBMIT_TOKEN_NOTIF_INDEX (187)` (`nvrm-rpcfn.h:202-203`) —
  RM mediates *tokens/notifier indices* for work submission; the model treats the token
  as an opaque u64 that must be obtained before first Kick on a GSP-managed channel
  (token lifecycle: obtain → attach → submit → release).
- Semaphore scheduling callbacks through firmware (`SEMAPHORE_SCHEDULE_CALLBACK`,
  `TIMED_SEMAPHORE_RELEASE`, `nvrm-msgfn.h`).

## 2. What stays channel/queue-domain

- Entry append, PUT advance, doorbell kick, GET consumption, method decode, semaphore
  payload comparison, Reference counting — none of these is an RM RPC in the vendored
  set; they are queue semantics over channel/VM memory. The model executes them
  host-only without any GSP call.
- Non-conflation rule (P77 precedent): RM *allocation* (103) ≠ command *execution*;
  P79 never claims an RM RPC executes pushbuffer contents.

## 3. Sequencing across the boundary

RM-configured (alloc/bind/schedule + token obtain) must precede queue use; queue teardown
(complete → unmap → free) must precede RM free. Crossing the boundary out of order is a
model fault (`NoChannel`/`PrematureFree`).
