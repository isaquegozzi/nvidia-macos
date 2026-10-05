# P79 — Channel-Submission Lifecycle Evidence

## Verdict: PROVEN (ordered stages; each gate has an upstream analogue)

```text
channel allocated (P77: Alloc → Allocated)
  → bound + scheduled (P77: Bind → Bound, Schedule(enable) → Scheduled)
  → VM mapped + flushed (P78: Map → Flush)
  → GPFIFO configured (alloc gpFifoOffset/Entries + ramfc 0x048/0x04c limit2)
  → pushbuffer mapped (P78 VMA covering entry windows)
  → packets encoded (METHOD doc)
  → entries appended (PUT advance, RingFull-guarded)
  → doorbell kicked (P77 doorbell_handle)
  → logical consumption (GET advance, in-order)
  → completion observed (FENCE doc: Reference/semaphore)
  → unmap/free only after completion (P78 Stale rules)
```

Gates (model faults): `NoChannel` (P77 state ≠ Scheduled), `UnmappedPb` (no live P78 VMA),
`InvalidEntry` (GPFIFO field rules), `PutOverflow` (beyond ring), `StaleMapping`
(P78 lookup failure at consume time), `UnkickedSubmit` (PUT advanced without Kick),
`PrematureFree` (unmap/put while entries unconsumed or fence unsignaled).
Channel ops used: `bind/unbind/start/stop/preempt/doorbell_handle` (`chan.h`),
`insert/remove/block/error` (`chan.c`); error pinning (P77 Errored) halts submission.
