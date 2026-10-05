# P76 Future Runbooks (draft — all steps REQUIRE explicit live authorization first)

Author: Muse, 2026-09-09 02:42 -0300. Host-only doc; executes nothing.
Context: candidate 1.7.3 APPROVED offline; every live flag is NO (G0008 §5).
Active EFI baseline `06cf0222…` + ALC892 layout-id 20 must stay untouched until staged.

## RB1 — Prelive staging checklist (all must be YES before any staging)
1. Genuine (non-fallback) Gemini APPROVE on all pending shadow milestones (M7, M8…).
2. `SHADOW-P76-REPRO-1.7.3/` rebuild bit-identical to production (KEXT/CLI/MANIFEST shas).
3. Evidence map + risk register current (see `EVIDENCE-MAP-AND-RISK-REGISTER.md`).
4. Staging target is a CLONE/secondary volume — never the active EFI.
5. `STAGING_FINAL_CANDIDATE_AUTHORIZED = YES` recorded in a Gemini verdict.
STOP on any NO. P70 tree remains INVALID / DO NOT USE.

## RB2 — Live selector order (after RB1 + per-selector authorization)
1. Selector 8 (read-only paths) before selector 9; small-N sysmem flush first.
2. Abort criteria: any unexpected return, timeout over GFW ~4s bound, dmesg anomaly.
3. Rollback: reboot to known-good EFI baseline; no retries on torn state.

## RB3 — Gate B / BME / DMA / FW live sequencing (far future, each needs own flag)
Order: Gate B zero-write validation → BME policy review → FBDMA chunk-timeout live
confirmation → FWSEC sequencing. Any stage failure returns to RB2-baseline; never
auto-retry a `PartialCommitFailure`/`Failed(stage)` — fresh boot required (models:
gateb-sim stop policy, M8 plan §3).

## RB4 — Evidence to capture per live step
Command log, return codes, dmesg excerpts, before/after `shasum` of staged trees,
explicit flag flips. Live evidence must map 1:1 onto the offline evidence map axes
(callgraph/state/concurrency/ABI/negative-scope/tests/mutations/binary-repro).
