# P76 Evidence Map + Risk Register (M4–M9 shadow models + 1.7.3 baseline)

Maintained by Muse. Last refresh: 2026-09-09 06:56 -0300 (G0101 approvals + M11 survey). Zero production writes; zero live HW.

## §1 Evidence map

| Unit | Spec | Files (sha256) | Tests (fresh 02:51) | Mutations | ASAN | Verdict |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| M4+M5 Gate B state machine | G0004 §2, protocol M5 | `gateb-sim/gateb_sim.hpp` be5aa09e…02a59; `test-gateb.cpp` b67bced6…53bf19 | 15/15 PASS | 6/6 CAUGHT | clean | APPROVED (G0008 checkpoint) |
| M6 BME fail-closed policy + mock PCI | G0004 §3 | `bme-sim/bme_sim.hpp` 81641726…7d5ca160; `test-bme.cpp` c056f496…b7550095 | 12/12 + 20k fuzz + 255-bit sweep | 5/5 exit 134 | 12/12 clean | APPROVE (G0008) |
| M7 FBDMA inventory + engine (b8c4d7e3; was FROZEN e33f8c20, see R8) | G0004 §4, G0008 §4 | `fbdma-sim/fbdma_sim.hpp` b8c4d7e3…381e0e; `test-fbdma.cpp` ca918c7e…f46d8e584; `test-fbdma-overflow.cpp` 042c892c…499 | 10/10 + 10k oracle + overflow 3/3 | 5/5 exit 134 (live 03:05, post-guard) | 10/10 + 3/3 clean | SUPPLEMENT M0041 (R8 closed) |
| M8 FWSEC sequencing (PROVISIONAL, self-contained) | M8-FWSEC-PLAN §§1–3+5 | `fwsec-sim/fwsec_sim.hpp` 4e6d71ab…713fa; `test-fwsec.cpp` 66c746b9…604cdd | 7/7 + 5k oracle | 5/5 exit 134 (live 02:47) | 7/7 clean | REVIEW REQUESTED M0017 |
| M9 RPC cmdq framing (independent layer) | Nova cmdq/fw paths via GSP-NEXTSTEP-P65 | `rpc-sim/rpc_sim.hpp` 77c62ea2…690e; `test-rpc.cpp` b83018f5…b5e2; `test-rpc-trailing.cpp` ba4df6ed…89a05 | 7/7 + 5k oracle + trailing 4/4 | 5/5 exit 134 (live 02:50) | 7/7 + 4/4 clean | REVIEW REQUESTED M0019 + F1 closed M0041 |
| M13 IOGPU map + method mapping (research-only) | SDK 26.5 IOAccel*/IOGPU.tbd | `IOGPU-INTERFACE-MAP.md` 7bd4f758…1977; `IOGPU-METHOD-MAPPING.md` 1e2610dc…03aa9b7 | n/a (no code) | n/a | n/a | PENDING (M0072/M0073) |
| M14 static sweep | Apple clang 21 --analyze | 10 TUs (7 sim + 3 flow), 0 warnings, empty diagnostics | n/a (analysis) | n/a | PENDING (M0074) |
| M15 GSP-boot sequencer | local GSP-BOOT-SEQUENCE.md | `gboot-sim/gboot_sim.hpp` 26a8050f…6e6f; `test-gboot.cpp` bcd0d103…24dea | 7/7 + 5k oracle | 5/5 exit 134 (live) | 7/7 clean | REVIEW REQUESTED M0076 |
| M11 VRAM/MMU survey (model deferred) | local fb.rs/fb-regs/gpu.rs | `VRAM-MMU-ARCHITECTURE.md` 9df297af…62c80 | n/a (no model: no PTE/aperture/fault sources) | n/a | n/a | SURVEYED M0121 |
| Baseline 1.7.3 | protocol §11 | repro tree bit-identical (G0008 §3 axis 8) | selector9 12/12, GFW 12, regressions 0–8; flush-verify 12/12 + ASAN fresh 02:44 | 36/36 | per M0001–M0006 | APPROVED (M1–M6 + checkpoint) |

Supporting analyses (docs, no code): `SELECTOR9-NEGATIVE-MATRIX.md` (N1–N6, one
accepted IOKit-trap-only gap); `FBDMA-OVERLAP-LEMMA.md` (overlap unreachable proof +
residual R8); `FUTURE-RUNBOOKS.md` (RB1–RB4, all live-gated).

## §2 Risk register

| # | Risk | Severity | Mitigation (present) | Owner |
| :--- | :--- | :--- | :--- | :--- |
| R1 | Real PCI config write / BME on live HW | CRITICAL | 1.7.3 has zero PCI writes (G0008 axis 5); mock-array model only; all live flags NO | M6 + prelive gate |
| R2 | Real GPU DMA / command submission | CRITICAL | Fake-clock/descriptor structs only; no DMA/VRAM/Falcon/GSP in production | M7 + prelive gate |
| R3 | Partial Gate-B commit torn state | HIGH | Irreversible PartialCommitFailure + pinned + Busy-after in model and review | M4/M5 |
| R4 | Verdict ambiguity (fallback/corrupt text) | MEDIUM | Never treat fallback-marked text as approval; hold dependent work (M8 provisional contract §0/M0016 is the documented exception with freeze + re-base promise) | relay discipline |
| R5 | M8 built on unapproved M7 | MEDIUM | Provisional contract: M7 frozen, M8 self-contained, explicit re-base promise | M0016 |
| R6 | Prelive wrong-candidate identity | HIGH | Prelive DEFERRED; active EFI `06cf0222…` + ALC892/20 untouched | prelive gate |
| R7 | Mutation escape in future edits | MEDIUM | Per-model `-D` harnesses retained; re-run each touch | all sims |
| R8 | u64 base+size wrap in range math | LOW (model-only) — CLOSED M0041 | Proved live fail-open (top-pinned allocator wrapped to low IOVAs); two-line strict-tightening guard in `fbdma_sim.hpp` (b8c4d7e3); 10/10+ASAN+5/5 re-verified post-change; overflow 3/3 | M7 |
| R9 | Reviewer workspace misconfiguration | MEDIUM (process) | Fallback tool-leaks reference `<PROJECTS>/Nvidia/nvidia-macos`; real workspace is `~/Documents/nvidia-macos-BARE0` with evidence in `Conversations/`. RELAY NOTE in M0020 asks the operator to check Gemini's workspace mapping. No evidence withheld on my side: every message lists exact PATHS. | relay operator |

## §3 Honest history (mutation/test bug-hunts, all resolved)
- M8: first harness run 0/5 (MY hook-prefix mismatch) → fixed; 3/5 (two hooks
  unreachable-in-effect) → overlap scan to documented defense-in-depth (M7
  precedent), terminal guard restructured → final 5/5. Baseline 7/7 + ASAN
  re-verified after every edit. Full log in M0017.
- M9: two oracle corrections (length-field tamper → Short, not Checksum; fuzz
  accepts Short|Checksum for length hits). Model unchanged. Full log in M0018.
- Flush-verify group count corrected 13 → 12 (M0015).

## §4 Reviewer-status log
G0009–G0066: all fallback except ONE valid APPROVE (G0061: M7/M8/M9/M10+M0070). Third bare-APPROVE trap (G0066, corrupt fallback body, wrong root) declined per R4 (M0077). Open: NOTHING — G0101 approved M13/M14/M15 (Muse M0121 accepted after KEXT-hash verification). M11 executable deferred with unblock criteria.
verdicts. G0011 (bare APPROVE + garbage) and G0013/G0016/G0019/G0029 (bare
NEED_EVIDENCE naming nothing + tool-leaks) explicitly NOT accepted as verdicts per
R4. G0021/G0022 briefly showed correct-path/coherent text (R9 flicker), G0029
regressed to the wrong path. Open requests: M7 (M0010 hash SUPERSEDED by M0041 b8c4d7e3 — exact 2-line diff inside), M8 (M0017), M9 (M0019 + F1 addendum M0041). Second APPROVE-trap (G0038, same corrupt pattern as G0011) declined per R4 (M0043). Addenda independently reproduced from bare rebuilds (M0043). Reviewer root cause diagnosed: wrong root is a different repo without Conversations/ (M0040, operator action pending). Reviewer aids published: location index (M0032),
`Conversations/REVIEWER-START-HERE.md` (M0033). Coverage completed meanwhile:
17/17 suites ALL PASS + ASAN-clean, mutations 26/26 live (M0023–M0031).
