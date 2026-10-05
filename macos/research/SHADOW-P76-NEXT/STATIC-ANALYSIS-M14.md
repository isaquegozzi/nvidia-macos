# M14 — static-analyzer sweep (clang --analyze, Apple clang 21.0.0)

Author: Muse, 2026-09-09 03:45 -0300. Read-only analysis; reports in /tmp/scan
(not in tree). Directed by valid verdict G0061 §5 (M14 alternative).

## Scope (10 translation units)
Sims (7): gateb/test-gateb, bme/test-bme, fbdma/test-fbdma,
fbdma/test-fbdma-overflow, fwsec/test-fwsec, rpc/test-rpc, rpc/test-rpc-trailing
— each covering its model header. Candidate flows (3): test-flush-verify.cpp,
test-gfw-readiness.cpp, ga106ctl-validate.c (covering FlushPrewriteFlow,
GfwReadinessFlow, GspSysmemFlow, InitFreeLockFlow, MockOps, protocol + validate
headers via inclusion).

## Commands
Sims: `xcrun clang++ --analyze -std=c++17 -I./<sim>-sim <test>.cpp`
Candidate C++: `+ -I. -idirafter <KernelHeaders>` (same libc++-safe recipe as builds)
Candidate C: `xcrun clang --analyze -std=c17 ... ga106ctl-validate.c`

## Results
10/10 TUs exit 0, ZERO analyzer warnings, all plist `diagnostics` arrays empty
(verified: 368-byte skeleton reports). No dead stores, no null derefs, no
use-after-scope, no leaks flagged in models, flows, or mocks.

## Incidents (honest log)
- First candidate run passed .cpp+.c together under --analyze with one -o:
  driver error "cannot specify -o with multiple outputs" (my invocation bug, not
  a finding). Re-ran per-TU: clean.
- The pre-existing `-Wunused-label 'fail_clean'` build warning is NOT an analyzer
  finding and is out of scope (upstream file, untouched).

## Verdict of this unit
M14 static sweep complete: no findings. Reports reproducible via commands above.
