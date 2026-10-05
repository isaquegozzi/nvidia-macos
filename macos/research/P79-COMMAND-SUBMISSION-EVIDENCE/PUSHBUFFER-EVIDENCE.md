# P79 — Pushbuffer Evidence

## Verdict: PROVEN (addressing via GPFIFO + residency/lifetime via P78 VM + word format)

## 1. Where it is / how it is addressed

- A pushbuffer is a byte range in GPU virtual address space, designated by one GPFIFO
  entry's `{GET, GET_HI, LENGTH}` triple (`clc56f.h` entry format). GET is a 4B-granular
  offset (bits 31:2); GET_HI carries high bits; LENGTH bounds the fetch window.
- The GPFIFO ring itself is designated by channel alloc `{gpFifoOffset, gpFifoEntries}`
  (`r535-fifo.c`) and programmed into instance memory (`ga100.c` 0x048/0x04c).
- Chain-of-custody: channel (P77) → GPFIFO ring (this doc) → pushbuffer window (this doc)
  → method words (METHOD-PACKET-EVIDENCE.md). Each level's address must resolve inside a
  live P78 mapping (integration invariant: unmapped/stale PB ⇒ submit refused).

## 2. Alignment / limits

- GPFIFO entries: 8 B (`GP_ENTRY__SIZE`), ring length multiple of 8, count = power of two
  (`ilog2(length/8)`); entry index wraps mod count.
- Method words: 4 B (`DMA_*_DATA 31:0`, `DMA_NOP = 0`); pushbuffer start 4B-aligned
  (GET field granularity); LENGTH>0.
- Pushbuffer window `[pbBase, pbBase+LENGTH)` must lie within a single live P78 VMA
  (model rule; upstream analogue: `nvkm_vmm_node_search` + mapref/memory checks in `vmm.c`).
- SUBROUTINE level (`LEVEL=1`) denotes a nested PB window; model requires the nested window
  to be independently mapped (no implicit trust).

## 3. Validity / lifetime (P78 reuse, no divergent copy)

- Map (P78 `Vm::Map` + `Flush`) → append entries referencing it → submit → consume →
  unmap only after completion observed (model: `needsFlush`/completion token; upstream
  analogue: dtor `WARN_ON(!list_empty)` + `mapped=false` gating in `vmm.c`).
- Free/unmap-before-completion is a model fault (`StalePb`), mirroring P78 `Stale`.
