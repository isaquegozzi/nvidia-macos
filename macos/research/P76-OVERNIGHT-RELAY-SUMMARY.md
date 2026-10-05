> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# P76-DUAL-OVERNIGHT-RELAY — operator summary (Muse, 2026-09-09 03:31 -0300)

For the human operator waking up. One file, everything that matters.

## Bottom line
- Baseline 1.7.3 APPROVED and frozen; production tree byte-identical to repro tree
  (zero non-build diffs); prelive/EFI untouched; every live flag still NO.
- Built + proved: M7 FBDMA, M8 FWSEC, M9 RPC-framing host-only models
  (tests + ASAN + live mutations, all green), plus 8 analysis/record docs.
- Full candidate suite refreshed: 17/17 ALL PASS, all ASAN-clean.
- Review requests M7/M8/M9 are open with complete evidence. The reviewer never
  returned a valid verdict: 33 consecutive relay fallbacks (details + root cause
  below). NOTHING was approved-or-rejected overnight past the G0008 checkpoint.

## What was built (hashes = current truth)
| Unit | Lib header | Tests | Mutations | ASAN | Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| M7 FBDMA | fbdma_sim.hpp `b8c4d7e3…` | 10/10 + 10k oracle + overflow 3/3 | 5/5 exit-134 | clean | review req. M0010 + supplement M0041 |
| M8 FWSEC | fwsec_sim.hpp `4e6d71ab…` | 7/7 + 5k oracle | 5/5 exit-134 | clean | review req. M0017 (provisional contract §0) |
| M9 RPC | rpc_sim.hpp `77c62ea2…` | 7/7 + 5k oracle + trailing 4/4 | 5/5 exit-134 | clean | review req. M0019 |
| M4/M5 GateB, M6 BME | (approved G0008) | 15/15, 12/12 | 6/6, 5/5 | clean | APPROVED |
| Candidate suites | 1.7.3 tree | 17/17 ALL PASS | 36/36 (retained) | all clean | APPROVED baseline |

Docs: evidence-map+risks R1–R9, runbooks RB1–RB4, sel9 matrix N1–N6, overlap
lemma, self-audit, M10 + IOGPU deferral surveys, reviewer entry point
(`Conversations/REVIEWER-START-HERE.md` — start any review there).

## Real bugs found by testing (all closed or homed)
- M8 harness 0/5 → hook-prefix mismatch (mine), fixed; 3/5 → two unreachable
  hooks redesigned (overlap→documented defense-in-depth, terminal guard made
  primary) → 5/5. (M0017)
- R8: M7 allocator wrapped to low IOVAs at exhaustion — proved live fail-open,
  fixed with 2-line strict guard, fully re-proven. M0010 hash superseded by
  M0041 (`b8c4d7e3`); exact diff in M0041. (F1 trailing-bytes closed additive-only.)
- Two M9 oracle corrections (length-field tamper semantics). (M0018)

## Known-unfixed (each has a home — none hidden)
N1–N4 (IOKit-trap-only, by review+assert); WPR2 payload bytes (M8 payload stage);
fence/IRQ (not modeled); Nova binary layout/lock/notify (documented deltas);
M10 channels + IOGPU (deferred: no local evidence — surveys state unblock criteria).

## Reviewer loop: broken, diagnosed, needs YOU
- 33/33 peer turns are `RELAY_FALLBACK...` (zero valid verdicts). Two bare-APPROVE
  traps (G0011, G0038) correctly declined per R4 (corrupt fallback bodies).
- ROOT CAUSE (verified): the reviewer works in `/Volumes/Downloads/Isaque
  Projetos/Nvidia/nvidia-macos/` — a DIFFERENT repo with NO `Conversations/`
  directory. Every evidence read from its side fails. (M0040)
- YOUR actions: (1) pin the reviewer workspace root to
  `~/Mac/Documents/nvidia-macos-BARE0`; (2) sanity-check by asking it to
  quote line 1 of `Conversations/P76-PROTOCOL.md`; (3) re-request M7/M8/M9 verdicts
  (files verified intact, M0035). Nothing on my side withholds evidence.

## Suggested next session
1. Fix reviewer root → collect M7/M8/M9 verdicts.
2. Apply F1/R8-style small findings only via disclosed supplements (house rule).
3. Then: M10 source vendoring (needs network) or prelive review (needs explicit
   authorization — still deferred, EFI untouched).
