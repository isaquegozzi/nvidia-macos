# P81.2 rehearsal results INDEX (synthetic, not live evidence)

generated: 2026-09-09T20:36:53Z // P81.2-RELAY-V3
HARNESS_IS_NOT_LIVE_EVIDENCE=YES GPU_STATE=UNKNOWN LIVE_EXECUTION_AUTHORIZED=NO

| scenario | expected | got | match | reasons |
|---|---|---|---|---|
| S01 | PASS | PASS | MATCH | pass:nominal |
| S02 | FAIL | FAIL | MATCH | fail:missing-required:EVENT-2 |
| S03 | FAIL | FAIL | MATCH | fail:malformed-index-header |
| S04 | FAIL | FAIL | MATCH | fail:hash-mismatch:IDENT-1 |
| S05 | FAIL | FAIL | MATCH | fail:ident2-diff-outside-allowlist |
| S06 | ABORT | ABORT | MATCH | abort:timeout-persistent |
| S07 | ABORT | ABORT | MATCH | abort:timeout-persistent-retry-exhausted |
| S08 | ABORT | ABORT | MATCH | abort:no-video-simulated |
| S09 | ABORT | ABORT | MATCH | abort:panic-simulated |
| S10 | ABORT | ABORT | MATCH | abort:no-signal-simulated |
| S11 | FAIL-3 | FAIL-3 | MATCH | fail-3:kext-unexpected-line |
| S12 | FAIL | FAIL | MATCH | fail:clock-gate-invalid |
| S13 | FAIL | FAIL | MATCH | fail:evidence-destination-space |
| S14 | FAIL | FAIL | MATCH | fail:save-failure |
| S15 | FAIL | FAIL | MATCH | fail:close-missing-signature |
| S16 | ABORT | ABORT | MATCH | abort:operator |
| S17 | ABORT | ABORT | MATCH | abort:forbidden-action |
| S18 | FAIL | FAIL | MATCH | fail:stale-allowlist |
| S19 | BLOCKED | BLOCKED | MATCH | blocked:partial-verdict |
| S20 | FAIL | FAIL | MATCH | fail:secret-detected-redacted |

MUTATION_KILL_RATE = 100.0% (25/25)

- M1-flip-status: FAIL killed=True
- M2-remove-artifact: FAIL killed=True
- M3-truncate-hash: FAIL killed=True
- M4-inject-extra-driver-line: FAIL-3 killed=True
- M5-change-timeout: ABORT killed=True
- M6-drop-close: FAIL killed=True
- M7-inject-forbidden-token: ABORT killed=True
- M8-duplicate-verdict: FAIL killed=True
- M9-partial-verdict: BLOCKED killed=True
- M10-tamper-reindex: FAIL killed=True
- M11-empty-inventory: FAIL killed=True
- M12-forbidden-upper: ABORT killed=True
- M13-forbidden-args: ABORT killed=True
- M14-forbidden-path: ABORT killed=True
- M15-forbidden-padded: ABORT killed=True
- M16-forbidden-multi: ABORT killed=True
- M17-secret-ghp: FAIL killed=True
- M18-secret-bearer: FAIL killed=True
- M19-secret-password: FAIL killed=True
- M20-secret-aws: FAIL killed=True
- M21-close-prefix: FAIL killed=True
- M22-close-suffix: FAIL killed=True
- M23-missing-timeout: FAIL killed=True
- M24-missing-clock: FAIL killed=True
- M25-missing-space: FAIL killed=True

HARNESS_IS_NOT_LIVE_EVIDENCE = YES
live=0
