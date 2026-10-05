# P79 — Open Questions (honest remainder; no fabrication)

1. **Ring-full disambiguation slot** — `put==get` ⟺ empty is forced by the index model;
   whether HW keeps one slot empty or uses a separate full flag is not pinned. Model keeps
   one slot empty (`used ≤ entries-1`) and documents it as a modeling choice (conservative:
   never overfills the allocated ring).
2. **GET_HI/TopLevelGet-Hi exact width use** — high-bit fields exist in `Nvc56fControl`
   and ENTRY1; the model carries full 64-bit indices and maps low 32 bits to the words.
   Wrap of the 32-bit visible counters is handled by mod-2^32 comparison (documented).
3. **Subroutine (LEVEL=1) call/return linkage** — LEVEL bit proven; return-address stacking
   is firmware/PBDMA-internal and not modeled (nested window must be independently mapped;
   consumption treats it inline in order).
4. **FETCH_CONDITIONAL predicate source** — bit proven; the condition register is not pinned.
   Model records the flag; CONDITIONAL entries consume in order like UNCONDITIONAL
   (no predication modeled — disclosed, never blocks on unproven state).
5. **GP_CRC/PB_CRC opcode checking** — opcodes 2/3 proven as values; checksum algorithm and
   failure behavior not pinned. Model accepts the opcodes as packet types with LENGTH
   semantics identical to NOP (no CRC verification claimed).
6. **Work-submit token value format** — RPC ids 186/187 proven; token/notifier-index *formats*
   are opaque u64/u32 in the model (obtained/attached/verified present, never interpreted).
7. **Interrupt delivery** — `NON_STALL_INTERRUPT` method encodable; host delivery unpinned.
   Completion observed via Reference/semaphore memory only (poll model).
8. **Endianness on BE hosts** — header bit-numbering assumes the LE layouts the driver
   writes with 32-bit stores; big-endian host behavior out of scope.
9. **TLB-invalidate MEM_OP / engine methods beyond 0xc56f class** — out of scope (P78/P77
   domains); ADDRESS allowlist stops at the documented 0xc56f set.

None blocks the scoped model: every modeled transition traces to a vendored line above.
