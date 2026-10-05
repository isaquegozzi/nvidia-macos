# R3 — Open Questions (DESIGN-ONLY, OFFLINE ONLY)

```text
SESSION_ID: V4-R3-LOAD-PACKAGE / MILESTONE: R3_FIRST_CONTROLLED_LOAD_PACKAGE
LIVE_EXECUTION_AUTHORIZED = NO
```

1. Q10 (BLOCKED, held): no read-only method binds the resident/loaded image to
   a historical binary; L2 presence stays non-identifying. Any Q10-resolution
   claim needs its own method proof + review before it can gate anything.
2. P8 Option 0 (BLOCKING): non-target/VM PASS for the 1.8.0 binary unpublished;
   no window opens without it (hard gate inherited from P81).
3. P9 (BLOCKING): no named reviewer + alternate with 7-day SLA; operator is
   never the reviewer.
4. P3/FOTO-0 (BLOCKING): no headless camera/watchdog/serial path pinned;
   userspace bound ≠ observability (D-minus abort precedent).
5. P-D4 pin (OPEN): P-D4.0..P-D4.5 audit against the EXACT 1.8.0 binary hash
   (`b6d4ea31…`) unpublished, including load/unload-path lifetime, closed
   register/BAR list, clear-on-read primary-source proof (P-D4.3), bound+copyout,
   locks/IRQ analysis.
6. OS-load posture (OPEN, ex-Gate 1 of M0264 fix): SIP/secure-boot/boot-args/
   kext-signing posture for strategy-A boot and the exact no-EFI-intermediate
   mechanism must be pinned at F-AUTH; any EFI/reboot veto contradiction = NO-GO.
7. Aging/Q11: any macOS/EFI/HW/GPU/cabling/KEXT change after 2026-09-09
   invalidates this design and forces full re-review; the future authorization
   must cite DESIGN_VERSION + binary hash.

```text
LIVE_ACTIONS = 0 / LIVE_EXECUTION_AUTHORIZED = NO
```
