# EXPECTED MATRIX — P81.2 (synthetic)

HARNESS_IS_NOT_LIVE_EVIDENCE=YES GPU_STATE=UNKNOWN LIVE_EXECUTION_AUTHORIZED=NO

| scenario | expected | rule |
|---|---|---|
| S01 | PASS | nominal only |
| S02 | FAIL | missing required artifact |
| S03 | FAIL | malformed INDEX |
| S04 | FAIL | hash mismatch |
| S05 | FAIL | IDENT-2 diff outside allowlist |
| S06 | ABORT | timeout > 60s |
| S07 | ABORT | bounded retry (1) then ABORT |
| S08 | ABORT | no-video |
| S09 | ABORT | panic |
| S10 | ABORT | no-signal |
| S11 | FAIL-3 | unexpected driver-inventory line |
| S12 | FAIL | clock gate invalid |
| S13 | FAIL | destination < 5GB |
| S14 | FAIL | SAVE failure |
| S15 | FAIL | CLOSE unsigned |
| S16 | ABORT | operator abort |
| S17 | ABORT | forbidden action |
| S18 | FAIL | stale allowlist |
| S19 | BLOCKED | partial verdict fragment |
| S20 | FAIL | secret detected, redacted, fail-closed |

MUTATION_KILL_RATE required = 100% (critical). Else P81_2 = REJECT.
