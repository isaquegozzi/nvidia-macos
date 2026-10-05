# M7 lemma: overlap branch unreachable under the bump allocator (proof, no code change)

Status: supporting analysis for M7 review. No model sources modified (hashes unchanged).
Scope: `SHADOW-P76-NEXT/fbdma-sim/fbdma_sim.hpp`, `Inventory::reserve`, default build
(`MUT_SKIP_ALIGN_CHECK` off). Method: manual proof by induction, checked against the
source quoted below. All line facts verified 2026-09-09 02:45 -0300.

## Code facts used (nothing else)
- F1: early rejects — `size == 0`, `align < 256`, non-pow2 align all return -1.
- F2: `base = align_up(nextIova_, align) >= nextIova_` (round-up; `nextIova_` starts at `kIovaBase`).
- F3: `end = base + size`; overlap scan returns -1 if any prior `[b.iova, oEnd)` intersects `[base, end)`.
- F4: on success only: push `{spec, base}`, then `nextIova_ = end`.
- F5: the class exposes no removal/shrink/reset API (`reserve`/`size`/`at` only), and
  `nextIova_` is written only at F4. Hence `nextIova_` is monotonically non-decreasing
  and always equals the maximum `end` of all successful allocs.

## Proof
Invariant I(n): after n successful reserves, allocations are pairwise disjoint and
`nextIova_ >=` every prior end.
- Base n=0: vacuous (empty scan).
- Step: assume I(n). The (n+1)-th call computes `base >= nextIova_ >=` all n prior
  ends (F2+F5). For every prior alloc, `base >= oEnd`, so the overlap condition
  `base < oEnd && b.iova < end` is false for all — the branch cannot fire. On success,
  F4 sets `nextIova_ = end >= base >=` all prior ends and the new range starts at or
  after every prior end, so I(n+1) holds (pairwise disjointness preserved).
By induction the overlap return is unreachable. QED.

## Corollaries
- C1: the 10-kind inventory disjointness test passes by construction, not luck; the
  fuzz oracle (10k transfers) exercises disjoint IOVAs only.
- C2: even under `MUT_SKIP_ALIGN_CHECK`, `base = nextIova_ >=` all ends, so overlap
  is still unreachable — the mutant is caught by alignment assertions
  (`align-up-enforced` group), not by the overlap scan. The harness claim is accurate.
- C3: keeping the scan is correct defense-in-depth for a future non-monotonic
  allocator (as the header comment states); no mutant is claimed for it.

## Residual (new, minor, model-only — NOT a defect in current scope)
- R8: `end = base + size` has no overflow guard; a hostile `size` near 2^64 would wrap
  and could break F5/I(n). Caller-controlled in the model, unreachable from the
  10-kind inventory (KB-scale sizes) and unexercised by any test. Recommendation (for
  M8 or later, NOT applied now to avoid churning M7 sources under review): reject when
  `end < base`. No action taken this turn.
