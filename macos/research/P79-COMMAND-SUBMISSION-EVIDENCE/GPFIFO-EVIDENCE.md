# P79 — GPFIFO Evidence (Ampere GA106, GSP-RM channel path)

## Verdict: PROVEN (entry layout + ring sizing + channel binding)

## 1. Entry layout (8 bytes, `clc56f.h` "GPFIFO entry format")

`NVC56F_GP_ENTRY__SIZE = 8`. Word 0 / word 1:

| Word | Bits | Field | Values |
|---|---|---|---|
| 0 | 0 | FETCH | UNCONDITIONAL 0 / CONDITIONAL 1 |
| 0 | 31:2 | GET | pushbuffer offset (word granularity) |
| 0 | 31:0 | OPERAND | full-word view |
| 1 | 7:0 | GET_HI / OPCODE | high address bits / opcode |
| 1 | 9 | LEVEL | MAIN 0 / SUBROUTINE 1 |
| 1 | 30:10 | LENGTH | pushbuffer length (21-bit field) |
| 1 | 31 | SYNC | PROCEED 0 / WAIT 1 |
| 1 | 7:0 | OPCODE | NOP 0 / ILLEGAL 1 / GP_CRC 2 / PB_CRC 3 |

Model rule: entry = `{get, getHi, length, level, sync, fetch, opcode}`; GET 4B-aligned;
LENGTH>0 and within pushbuffer bounds; OPCODE ∈ {0..3} (ILLEGAL never emitted, only detected);
SYNC=WAIT allowed (records a wait token, never satisfied by HW here).

## 2. Ring sizing (driver-programmed)

- Alloc: `r535_chan_alloc(..., gpfifo_offset, gpfifo_length, ...)` →
  `args->gpFifoOffset = gpfifo_offset`, `args->gpFifoEntries = gpfifo_length / 8`
  (`r535-fifo.c:91-92`). Length must be a nonzero multiple of 8 (P77 model pins this).
- Instance programming: `ga100_chan_ramfc_write(chan, offset, length, ...)` writes
  `inst[0x048] = offset.lo`, `inst[0x04c] = offset.hi | (limit2 << 16)` with
  `limit2 = ilog2(length / 8)` — i.e. **entry count is a power of two** (`ga100.c:72-78`).
- Model rule: `entries = length/8 = 2^limit2`, `1 ≤ limit2 ≤ 31`; ring indexes are
  `position mod entries` (wrap by construction).

## 3. Channel relation

- One GPFIFO ring per channel (offset+length carried in the channel alloc params,
  class `0xc56f` via `GSP_RM_ALLOC=103`). Ring memory lives in the channel's VM
  (P78 mapping required — integration invariant).
- `Nvc56fControl` (per-channel DMA flow-control window): `Put` (0x40, R/W),
  `Get` (0x44, RO), `Reference` (0x48, RO), `PutHi`/`GetHi`/`TopLevelGet(Hi)`,
  `GPGet` (0x88, RO), `GPPut` (0x8c). Two index domains exist (PB-level Put/Get and
  GP-level GPPut/GPGet); the model tracks both with the documented RO/RW split
  (guest advances Put/GPPut; Get/GPGet/Reference advance only via logical consumption).

## 4. Dual-view note (bits 7:0 of word 1)

The header defines both `GET_HI` and `OPCODE` at bits 7:0 — overlapping views, not
separate fields. Modeling rule (documented, no HW behavior invented): decode records
both views (`getHi` always; `op` when ≤ 3, else address-entry reading); `ILLEGAL`(1)
always faults; all-zero pair faults as unmapped-ring memory; `LENGTH==0` faults.
CRC opcodes keep LENGTH semantics; checksum verification is unpinned (see OPEN-QUESTIONS).
