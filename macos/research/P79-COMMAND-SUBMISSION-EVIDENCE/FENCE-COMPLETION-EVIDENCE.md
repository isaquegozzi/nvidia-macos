# P79 — Fence/Completion Evidence

## Verdict: PROVEN (semaphore + reference-counter mechanism; interrupt delivery abstract)

## 1. Semaphore methods (`clc56f.h`, method addresses 0x10-0x1C)

`SEMAPHOREA` offset-upper 7:0, `SEMAPHOREB` offset-lower 31:2 (4B-aligned address),
`SEMAPHOREC` payload 31:0, `SEMAPHORED` operation 4:0:

```text
ACQUIRE 1 | RELEASE 2 | ACQ_GEQ 4 | ACQ_AND 8 | REDUCTION 0x10
+ ACQUIRE_SWITCH b12, RELEASE_WFI b20 (WFI_EN=0 means wait-for-idle on release),
  RELEASE_SIZE b24 (16B/4B), REDUCTION 30:27 (MIN/MAX/XOR/AND/OR/ADD/INC/DEC),
  FORMAT b31 (SIGNED/UNSIGNED)
```

`SEM_EXECUTE_*` adds release-WFI and release-timestamp controls. Firmware-side events
`SEMAPHORE_SCHEDULE_CALLBACK 0x100b` / `TIMED_SEMAPHORE_RELEASE 0x1018` (`nvrm-msgfn.h`)
show the same semaphore objects scheduled/released through GSP-RM.

## 2. Reference counter

`SET_REFERENCE (0x50)` writes the reference value; control word `Reference @0x48` is the
RO counter the engine exposes. Model: fence = `{id (monotonic), payload, signaled}`;
RELEASE with payload P on semaphore S signals all fences waiting with threshold ≤ P;
`Reference` mirrors the last completed fence id (CPU-visible, polled — no interrupt needed).

## 3. Ordering primitives

`WFI (0x78)` (scope current/all), `YIELD` (NOP/TSG), `END_PB_SEGMENT` (packet class 7)
and entry `SYNC=WAIT` give in-stream ordering without interrupts. `MEM_OP` TLB-invalidate
forms exist but are P78-domain (not modeled here beyond address passthrough).

## 4. Explicitly abstract (NOT modeled as HW)

- `NON_STALL_INTERRUPT (0x20)` delivery: the method word is encodable, but interrupt
  routing/delivery to the host is NOT pinned in the vendored set — the model encodes the
  method but completion is observed only via Reference/semaphore memory (poll model).
- Timestamp values (`RELEASE_TIMESTAMP`): carried as opaque u64, never interpreted.
- This matches the prompt: completion mechanism appropriate to scope, no invented
  interrupt packet.
