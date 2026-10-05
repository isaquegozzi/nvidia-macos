# R3 — Phases (DESIGN-ONLY, OFFLINE ONLY)

```text
SESSION_ID: V4-R3-LOAD-PACKAGE / MILESTONE: R3_FIRST_CONTROLLED_LOAD_PACKAGE
LIVE_EXECUTION_AUTHORIZED = NO
```

## L1 — stage/load only

Place the pinned 1.8.0 binary into its boot position and load it into the kernel
exactly once under supervision, inside pre-published bounds and abort rules.
No userspace→kernel trap beyond the load itself; no `ga106ctl`, no selector of
any number, no read derived from the candidate.

## L2 — verify presence only

OS-level presence check only (`kextstat`-class output): candidate entry present
exactly once with the expected version, system responsive, no panic/hang/
no-signal observed. No identity claim beyond presence (Q10 = BLOCKED: presence
is NOT a binding between loaded code and a historical binary).

## L3 — selector read-only (OUT OF THIS PACKAGE)

Any selector invocation, including the future GFW-readiness read (C-class), is
EXCLUDED from the first window. L3 requires its own milestone: P-D4.0..P-D4.5
audit of the exact binary + Option 0 PASS + D PASS + baseline HW + watchdog/
serial + reviewer + dedicated human authorization. This package designs no L3
runbook step.

## First-window decision

```text
FIRST_LIVE_WINDOW = LOAD + PRESENCE VERIFY + STOP
```

The first window, IF ever authorized in a dedicated future act, ends after L2
and STOPS. No selector, no second invocation, no window extension. Any of the
following converts the window to ABORT: second load attempt same day, any
selector call, any read/write outside E1–E4 evidence form, hash divergence,
panic/hang/no-signal.

```text
LIVE_ACTIONS = 0 / LIVE_EXECUTION_AUTHORIZED = NO
```
