# SHADOW — M7 First DMA/FBDMA PLAN (draft, NOT implemented, awaiting M6 verdict)
Spec: G0004 section 4. Isolated dir SHADOW-P76-NEXT/fbdma-sim/, zero production wiring.
1. Buffer inventory model: 8 object kinds (flush page 4096 pinned; FRTS image 256B-aligned;
   WPR2 meta; Radix L0/L1/L2; signatures; bootloader; LibOS regions; TX/RX queues).
   Each: size, align, kind tag, fake IOVA assignment (bump allocator, 256B grains).
2. IOVA rules: 256B alignment enforced; overlap rejected; zero-length rejected.
3. FBDMA state machine per transfer: Idle -> DescProgrammed -> ChunkPoll(n/256B, 2s budget
   each, fake clock) -> Done | Timeout | Error. TRANSCFG model: target CoherentSysmem,
   memtype Physical (enum check; other targets rejected).
4. Completion: fake status register; errors injectable per chunk (timeout at chunk k).
5. Tests: inventory validation matrix, align/overlap rejects, 1-chunk + multi-chunk happy,
   timeout-at-k aborts with bytes-done accounting, wrong-target rejected, 10k deterministic
   transfer fuzz (lengths/chunks/faults).
6. Mutations (-D): SKIP_ALIGN_CHECK, ALLOW_OVERLAP, IGNORE_TIMEOUT, WRONG_TARGET_OK,
   CHUNK_SIZE_MISMATCH. All must be CAUGHT.
