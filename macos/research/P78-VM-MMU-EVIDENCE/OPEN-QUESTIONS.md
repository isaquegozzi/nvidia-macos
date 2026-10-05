# P78 — Open Questions (honest remainder; no fabrication)

1. **HW fault-packet wire bytes** — `fault_data` field *names* proven, fault replay/cancel
   *actions* proven, RM buffer *lifecycle* proven; but the HW packet bit layout (access/client/
   reason numeric codes, timestamp/compression of inst) is not pinned. Model classifies at the
   guard level. Closer: vendor `nvkm/subdev/fault/{gv100,tu102}.c` parsing + OpenRM packet structs.
2. **Kind numeric meanings** — tu102 kind[16] + invalid 0x07 proven as a table; what each kind
   *means* for compression/tiling is not needed for mapping correctness and not modeled.
   Closer: vendor `tu102_mmu_kind` consumers + OpenRM kind defs (only if compression modeling needed).
3. **Comptag backing-store allocation** — field placement (`<<36`, 1/64 KiB) proven; who allocates
   the store (PMU/GSP-RM) is out of scope. Model carries `ctag` as opaque u64, never allocates.
   Closer: PMU/GSP-RM compbit init sequence (not required for P78).
4. **BAR2 PDB / `bar2_pdb` aliasing** — `tu102_vmm_flush` uses `rm.bar2_pdb` when set, else root PD;
   when/why RM sets it is GSP-internal. Model treats flush root as opaque u64 token.
5. **`aperture=1` in COPY_SERVER_RESERVED_PDES.levels** — small-int domain differing from both PTE
   (0/2/3) and NV0080 (0/1/2) enums; value 1 observed in code, semantic label not pinned to a header
   enum in the vendored set. Model preserves the value literally and flags cross-enum comparison
   as a fault. Closer: OpenRM `NV90F1` aperture enum header.
6. **VA start for external VASpaces** — internal path start=0 proven via ctor; external (promoted)
   VASpace VA policy is RM-owned. Model assumes `[0,2^47)` with server-reserved hole for both.
7. **GH100+ formats** — `vmmgh100.c`/`gh100.c` exist but are Hopper+; deliberately excluded
   (same anti-contamination rule as P77). GA106 stays on the tu102/gp100 line.

None blocks the scoped model: every modeled bit/transition traces to a vendored line above.
