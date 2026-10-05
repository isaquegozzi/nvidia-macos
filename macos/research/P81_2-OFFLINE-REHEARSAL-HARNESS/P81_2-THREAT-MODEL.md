# P81.2 Threat Model (harness scope, offline only)

```text
HARNESS_IS_NOT_LIVE_EVIDENCE = YES
GPU_STATE = UNKNOWN
LIVE_EXECUTION_AUTHORIZED = NO
```

## Assets (rehearsal)
Pipeline verdict integrity, INDEX/SHA256 chain, CLOSE signature presence,
redaction guarantee, extractor fail-closedness.

## Adversaries / faults modelled
1. Missing/truncated/tampered artifact (S02/S04, hash variants).
2. Malformed or duplicated INDEX (S03, hash tests).
3. Out-of-allowlist identity diff / rogue driver-inventory line (S05/S11).
4. Timeout persistence + retry exhaustion (S06/S07).
5. Simulated panic / no-signal / no-video (S08–S10).
6. Invalid clock gate, small destination, SAVE failure, unsigned CLOSE (S12–S15).
7. Operator abort (S16); injected forbidden host action (S17 → ABORT).
8. Stale allowlist accepted by mistake (S18 → must FAIL).
9. Fragment verdict misread as APPROVE (S19 → must BLOCKED).
10. Secret persisted in clear (S20 → must redact + FAIL-closed).
11. Mutations M1–M9 (status flip, removal, truncation, injection, timeout,
    dropped CLOSE, forbidden token, duplicate/partial verdict).

## Explicitly out of scope
Live host execution, real driver/hardware state, KEXT safety, panic-rate,
DMA/GSP/VRAM/command submission, real clock/destination. Ver
`P81_2-LIMITATIONS.md`. Option 0 non-target real segue pendente de humano.

12. V2: tamper+reindex (M10 -> FAIL ground-truth); inventario vazio (M11);
    forbidden normalizado UPPER/args/path/whitespace/multi (M12-M16 -> ABORT);
    segredos ghp/Bearer/password/aws (M17-M20 -> FAIL redacted);
    CLOSE prefix/suffix injection (M21-M22 -> FAIL shape);
    campo timeout/clock/space ausente (M23-M25 -> FAIL missing-field).
