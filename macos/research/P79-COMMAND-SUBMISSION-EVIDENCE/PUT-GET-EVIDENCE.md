# P79 — PUT/GET Evidence

## Verdict: PROVEN (register split + ring math + kick mechanism)

## 1. Registers (`Nvc56fControl`, `clc56f.h`)

```text
Put @0x40 R/W | Get @0x44 RO | Reference @0x48 RO | PutHi @0x4c
TopLevelGet @0x58 RO | TopLevelGetHi @0x5c | GetHi @0x60
GPGet @0x88 RO | GPPut @0x8c
```

PB-domain Put/Get and GP-domain GPPut/GPGet are separate index pairs over the same
power-of-two ring. Model: `put` (guest-owned, R/W), `get` (engine-owned, RO —
guest writes rejected), indices in entries (not bytes).

## 2. Ring semantics (entries = 2^limit2, from GPFIFO doc)

- `used = (put - get) mod entries`; empty ⇔ `put == get`; full ⇔ `used == entries - 1`
  (one slot kept empty to disambiguate full vs empty — standard ring rule; the model
  documents this as a modeling choice for the disambiguation, the wrap arithmetic
  `mod entries` being upstream-forced by the power-of-two count).
- Append of N entries requires `used + N ≤ entries - 1`, else `RingFull`.
- PUT advance wraps mod entries (`PUT/GET wrap`); byte offset of slot i is `i*8`.
- `entries` comes from the channel's GPFIFO config (P77 `entries = length/8`), so PUT/GET
  can never exceed the allocated ring (integration invariant with P77).

## 3. Kick (guest → engine notification)

- PUT visibility to the engine is via doorbell: `(runl<<16)|chid`
  (`ga100_chan_doorbell_handle` / `tu102_chan_doorbell_handle`, P77-proven).
- Model: `Kick()` records a doorbell token `{runl, chid}` and requires it after every PUT
  advance before logical consumption may proceed (mirrors the driver ordering where the
  engine learns PUT through the doorbell, not through polling the R/W Put word).
- `TopLevelGet` tracks the top-level (non-subroutine) progress; SUBROUTINE entries do not
  advance it (model rule from the LEVEL field semantics).
