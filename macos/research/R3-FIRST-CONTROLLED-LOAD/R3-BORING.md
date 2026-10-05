# R3 — Boring First Load (DESIGN-ONLY, OFFLINE ONLY)

```text
SESSION_ID: V4-R3-LOAD-PACKAGE / MILESTONE: R3_FIRST_CONTROLLED_LOAD_PACKAGE
LIVE_EXECUTION_AUTHORIZED = NO
```

## Inside the window (IF ever authorized; narrow by design)

Exactly once each, in order, inside published bounds: resident-disabled boot of
the pinned 1.8.0 candidate (strategy A) → OS-level presence check (L2) →
responsiveness check → STOP. Success bar: system responsive throughout, no
panic, no hang, no no-signal, exactly one GA106Lab entry, rollback verified
(see `R3-RECOVERY.md`), evidence E1–E4 complete (see `R3-EVIDENCE-PLAN.md`).

## Outside the window (explicitly FORBIDDEN, each = ABORT-SCOPE)

Selector invocation of ANY number (including C-class GFW readiness); GFW/FW
reads; MMIO/BAR access; PCI config write; BME/BusMaster enable; DMA; GSP;
firmware/VRAM/command submission; IOGPU/IOAccelerator; Metal; interrupts/reset;
second load or re-invocation same day; window extension into writes; EFI
re-edit after boot; reboot-to-clear-and-continue.

## Boring criterion

The window is a PASS only if nothing interesting happened: no new signal about
the GPU, no state mutation, return to baseline verified. Any interesting event
is FAIL or ABORT, never a discovery to pursue same-day.

```text
LIVE_ACTIONS = 0 / LIVE_EXECUTION_AUTHORIZED = NO
```
