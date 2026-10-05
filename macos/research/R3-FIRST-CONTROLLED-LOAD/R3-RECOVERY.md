# R3 — Recovery / Rollback (DESIGN-ONLY, OFFLINE ONLY; no reboot live now)

```text
SESSION_ID: V4-R3-LOAD-PACKAGE / MILESTONE: R3_FIRST_CONTROLLED_LOAD_PACKAGE
LIVE_EXECUTION_AUTHORIZED = NO
```

## Known-good boot path

Internal EFI, config with 1.5.3 `Enabled=true` + 1.4.1 `Enabled=false`
(P61-verified config SHA `06cf0222…315ef`), which is the CURRENT live state.
  Note: `41e2a921…002b63d` = pre-audio historic config, not current; current = `06cf022258d796c942b71786aeff5b9f730d9ebf507d2ca8f85732ab538315ef` (post-audio, CURRENT-EFI-BASELINE-NOTE.md:4).
This is the rollback target for every failure class below.

## Backup EFI identity (read-only, verified)

- `EFI-BACKUP-PROMPT50-L0-20260908-154333`: config `f6650d22…fcd3f`,
  1.5.3 kext `b2ea2218…6854`, KEXT4B `f0444f5c…93323`.
- `EFI-BACKUP-PROMPT46-20260908-143529`: prior backup (identity in
  `BACKUP-SHA256.txt` therein).
No backup is modified by any R3 step, ever.

## Removal path of the candidate

Inverse of strategy A in a single transaction: candidate `Enabled=false`
(and staged binary removed from EFI), 1.5.3 `Enabled=true` restored, config
re-shasum-compared to `06cf0222…315ef`, boot, L2 presence shows ONLY 1.5.3.
  Note: `41e2a921…002b63d` = pre-audio historic config, not current; current = `06cf022258d796c942b71786aeff5b9f730d9ebf507d2ca8f85732ab538315ef` (post-audio, CURRENT-EFI-BASELINE-NOTE.md:4).
Only a human operator executes this, only under the pre-authorized recovery
clause of the dedicated future authorization.

## Max recovery attempts

```text
MAX_RECOVERY_ATTEMPTS = 2 (R-1, R-2; human-only; mirrors P81 authorization matrix)
```

After R-2 fails, or on any FAIL-ROLLBACK (post-rollback IDENT diverges from
baseline): stop, preserve evidence, no further boot attempts that day; physical
recovery becomes a new milestone with its own review. A third attempt under the
same authorization = violation.

## What counts as ABORT

ABORT-PRECONDITIONS (any hard gate false at open); ABORT-HASH (any shasum
divergence from pinned values); ABORT-SCOPE (any out-of-window act per
`R3-BORING.md`); ABORT-LOAD (panic/hang/no-signal or failed EFI-revert/rollback path).
Every ABORT zeroes all counters, ends the window, and goes to evidence-save
first, recovery second — never recovery-first (reboot destroys logs).
NOTE: strategy A has no live-unload step (no `kextunload` live) — teardown/rollback is a static EFI transaction only. Hung-live case: an EFI edit alone does not unload a live-hung candidate; the reboot clears it (declared, correct).

## Evidence saved BEFORE recovery

E1–E4 partial bundle + ABORT label + one state photo (LEDs/fans/screen) copied
to EXTERNAL media with shasum BEFORE any recovery boot. Post-recovery analysis
of a window without pre-recovery evidence = invalid.

## Package status declaration

The recovery PROCEDURE is clear on paper (target, backups, removal path,
attempt cap, abort classes, evidence-first order). Live executability is
nevertheless BLOCKED on: P8 Option 0 non-target PASS, P9 named reviewer +
alternate, P3 headless watchdog/serial path, P-D4.0..P-D4.5 pin of the exact
1.8.0 binary, and a dedicated one-use human authorization. No reboot, no EFI
write, no recovery act occurs under this package.

```text
R3_RECOVERY_PROCEDURE = CLEAR-ON-PAPER / R3_LIVE_STATUS = BLOCKED (gates above)
LIVE_ACTIONS = 0 / LIVE_EXECUTION_AUTHORIZED = NO
```
