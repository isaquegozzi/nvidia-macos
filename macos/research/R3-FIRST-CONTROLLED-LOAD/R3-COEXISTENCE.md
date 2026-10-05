# R3 — Coexistence with resident 1.5.3 (DESIGN-ONLY, OFFLINE ONLY)

```text
SESSION_ID: V4-R3-LOAD-PACKAGE / MILESTONE: R3_FIRST_CONTROLLED_LOAD_PACKAGE
LIVE_EXECUTION_AUTHORIZED = NO
```

Fact: resident live state is 1.5.3 (`TG-KEXT5-…-TESTPATH-FIX`, Count=8, no
selector 8), `Enabled=true` in the active config
(`06cf0222…315ef`, verified read-only P61); 1.4.1 `Enabled=false`.
  Note: `41e2a921…002b63d` = pre-audio historic config, not current; current = `06cf022258d796c942b71786aeff5b9f730d9ebf507d2ca8f85732ab538315ef` (post-audio, CURRENT-EFI-BASELINE-NOTE.md:4).

## Strategies considered (exactly ONE adopted)

- A. Boot ONLY the candidate (resident disabled in the SAME EFI transaction).
  Single-active by construction; rollback = inverse transaction. REQUIRES an EFI
  write + reboot, both gated behind dedicated human authorization.
- B. Stage candidate for a LATER boot (dual-present, single-active). REJECTED:
  dual presence in EFI invites misboot/ambiguity and complicates rollback proof.
- C. Runtime load alongside the resident. REJECTED as impossible-by-design:
  identical IOClass/provider matching collides; two revisions active violates
  the single-active invariant and the Q10 identity gap makes the result
  uninterpretable.
- D. No other valid strategy identified (external-media boot is a variant of A,
  not a separate strategy, and adds its own trust chain without need).

## Decision

```text
COEXISTENCE_STRATEGY = A (boot-only-candidate, single EFI transaction)
CURRENT_1_5_3_HANDLING = DISABLED-IN-PLACE-SAME-TRANSACTION
COEXISTENCE_ALLOWED = NO
```

`DISABLED-IN-PLACE` means: 1.5.3 `Enabled=false` in the same config edit that
sets candidate `Enabled=true`; the 1.5.3 binary is RETAINED untouched in EFI
for rollback and never deleted; at no boot is more than one GA106Lab revision
`Enabled=true`. Post-boot L2 must show exactly one GA106Lab entry or the window
is FAIL-ROLLBACK.

No EFI write, no reboot, no stage, no load occurs under this package.

```text
LIVE_ACTIONS = 0 / LIVE_EXECUTION_AUTHORIZED = NO
```
