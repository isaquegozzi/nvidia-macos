# P79 — Fault/Error Evidence

## Verdict: PROVEN (error classes from channel + VM + ring guards)

| Model fault | Upstream analogue |
|---|---|
| NoChannel / InvalidOrder | P77 `InvalidOrder`, `ErrorPinned` (Errored refuses all ops) |
| UnmappedPb / StalePb | P78 `OutOfRange`/`Stale` (node_search miss, mapped=false) |
| InvalidEntry (align/length/opcode/level) | entry field rules (GPFIFO doc) + `Misaligned`/`InvalidSize` (P77) |
| RingFull / RingEmpty (consume on empty) | ring arithmetic + `used` accounting |
| PutOverflow (PUT beyond ring) | `entries` bound from alloc (P77 `InvalidSize` family) |
| PacketOverflow (COUNT overruns PB) | window bound `[pbBase, pbBase+len)` |
| UnknownMethod / ReservedEncoding | method ADDRESS set + SEC_OP 6 reserved |
| SubchannelBounds (subc ≥ 8) | `NUMBER_OF_SUBCHANNELS = 8` |
| PrematureFree | P78 `Stale` on free-while-mapped + dtor WARN |
| SemaphoreMismatch (acquire on never-released) | ACQUIRE vs RELEASE ordering (fence lifecycle) |
| StaleFence (wait on freed fence) | fence lifecycle (create/signal/wait/free) |
| ErrorPinned (channel errored mid-stream) | P77 Errored + `RC_TRIGGERED`/notifier types (`r570` ERROR_NOTIFIER, `nvos` NOTIFIER type 13) |

Error reporting path (not modeled as delivery): TSG error notifiers
(`NVOS32 hObjectError/hObjectEccError`, `r570` NOTIFIER_TYPE NONE on kernel channels),
`CLEAR_FAULTED (0x84)` with PBDMA/ENG faulted type — the model records `Errored` state
and refuses further submission (fail-closed), matching `nvkm_chan_error` semantics.
