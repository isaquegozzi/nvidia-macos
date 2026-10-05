# M13 part 2 — surface-verb → engine-state mapping (read-only research, no code)

Author: Muse, 2026-09-09 03:44 -0300. Host-only doc. Verbs from SDK 26.5
`IOAccelSurfaceConnect.h` (`eIOAccelSurfaceMethods`); engine side is our M4–M9
models + 1.7.3 flows. Confidence tags: DIRECT (same mechanism), ANALOGY
(same discipline, new family), PROPOSAL (new recommendation, marked as such).

## 1. Lock family → exclusive-access discipline (ANALOGY)
Verbs: ReadLock/ReadUnlock (+Options), WriteLock/WriteUnlock (+Options), QueryLock.
Engine analogs: `fFlushBusy` minimized window + F-L1 narrowing; `PinOwnerForExternalMethod`
(NotOpen on failure); GateB non-abortable commit window; `ExclusiveAccess` slot;
double-commit Busy; post-terminal AlreadyTerminal. Rule carried over: every mutating
verb takes a named lock first, releases on all paths (our balanced-teardown asserts
are the test template). Open: lock granularity (per-surface vs global) — TBD live.

## 2. Shape family → staged-validation discipline (DIRECT philosophy, ANALOGY fields)
Verbs: SetShape, SetShapeBacking(+AndLength), SetScale, SetIDMode.
Engine analogs: M7 `reserve` (size>0, align≥256 pow2, NoWrap, pairwise Disjoint →
BadMeta/Overlap, state unchanged on failure); M8 meta Inside/disjoint/aligned;
M7 `program` (descriptor fields + GO). Rule carried over: validate EVERYTHING
before committing ANYTHING (atomic staging); reject with typed errors, never
partial state. Our 48B static_assert-both-sides discipline applies to any future
surface descriptor.

## 3. Flush → commit/visibility point (DIRECT analogy)
Verb: `kIOAccelSurfaceFlush`. Engine analogs: GateB HI/LO post + readback verify
(PartialCommitFailure terminal, no rollback writes); FBDMA chunk walk with 2s/chunk
budget (Timeout with bytesDone accounting). Flush is the visibility point exactly
as commit is ours: bounded, accounted, terminal-on-failure.

## 4. Read/GetState → verify-path discipline (DIRECT)
Verbs: Read, GetState. Engine analogs: selector 8/9 verify flows — read-only,
zero-leak construction (pre-use zeroing, exact-size copies, reserved-zero, no
IOVA/HI/LO exposure per T11–T14). Rule carried over: observation verbs never
mutate and never leak addresses.

## 5. Control → dispatch-table discipline (DIRECT mechanism)
Verb: `kIOAccelSurfaceControl`. Engine analog: our `externalMethod` trap table
(selector → fixed struct sizes → N1–N6 BadArgument matrix). Identical IOKit
mechanism; a future Control() switch inherits our transport-negative matrix
1:1 (null, non-zero-input, short-buffer, unpinned-owner, error propagation).

## 6. Cross-verb sequencing (PROPOSAL)
Recommended order lock→shape→flush→control mirrors our stage machines (M8
Unloaded→…→Done, M7 program→run). Recommended failure semantics: terminal states
with fresh-object retry (reboot model, M8 §3), no auto-retry, staged state pinned.
This is a proposal for the future live design, not a claim about Apple semantics.

## Verdict of this survey
M13 research complete at the mapped level: every verb family has a homed engine
analog with honest confidence. No code, no execution, no invented interfaces.
Remaining: live GPU + authorization before any of this becomes implementation.
