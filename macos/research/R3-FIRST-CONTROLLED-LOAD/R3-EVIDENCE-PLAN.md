# R3 — Evidence Plan for the future window (DESIGN-ONLY, OFFLINE ONLY)

```text
SESSION_ID: V4-R3-LOAD-PACKAGE / MILESTONE: R3_FIRST_CONTROLLED_LOAD_PACKAGE
LIVE_EXECUTION_AUTHORIZED = NO (nothing below is collected now)
```

## E1 — command form

Byte-for-byte command sequence executed, operator + watchdog IDs, window bounds
(start/end, per-command 60 s cap, total ≤15+15 min). Source: the dedicated
future authorization (1-use, 1-day); any deviation = ABORT-SCOPE.

## E2 — IDENT pre/post + hashes

- IDENT pre/post: OS-level presence (`kextstat`-class), exactly-one-GA106Lab
  comparison by allowlist (only date/time/uptime/counters may differ).
- Re-shasum of the staged candidate vs pinned `b6d4ea31…` (binary) and
  `2d0d3cf9…` (plist); config vs `06cf0222…315ef` pre-window and vs expected
  post-rollback value. Any divergence = ABORT-HASH / FAIL-ROLLBACK.
  Note: `41e2a921…002b63d` = pre-audio historic config, not current; current = `06cf022258d796c942b71786aeff5b9f730d9ebf507d2ca8f85732ab538315ef` (post-audio, CURRENT-EFI-BASELINE-NOTE.md:4).

## E3 — external observation

Setup photos + EXIF via the P3/FOTO-0 path, plus watchdog/serial-external logs
covering the whole window (userspace bound is NOT a watchdog). No window opens
without the P3 path pinned in the authorization.

## E4 — INDEX + redaction

Single INDEX file: E1–E3 inventory with shasums, P-D1..P-D3 + P-D4.0..P-D4.5
checklist with evidence IDs, baseline HW GA106 note (GSP/FW absent declared),
PASS/FAIL/ABORT verdict with criterion citation. Redact serials/MACs/UUIDs
(P81-Q5 rule) before any publication. Saved to EXTERNAL media immediately at
CLOSE, before any analysis; pre-recovery save on ABORT.

## Verdict mapping

- PASS(load): L1+L2 1x each in bounds, responsive, no panic/hang/no-signal,
  rollback verified, E1–E4 complete. Proves ONLY supervised load/unload; GPU
  state stays UNKNOWN (Q10).
- FAIL: protocol error, incomplete evidence, or post-state divergence with
  clean recovery. No causal attribution; dedicated review; counters zeroed.
- ABORT: per `R3-RECOVERY.md` classes; counters zeroed; no same-day retry.

```text
LIVE_ACTIONS = 0 / LIVE_EXECUTION_AUTHORIZED = NO
```
