# Selector 9 ABI negative matrix — read-only review (1.7.3, no writes)

Reviewer: Muse, 2026-09-09 02:43 -0300. Tree:
`GA106Lab-kext7-sysmem-flush-prewrite-fix3-src/` — READ ONLY, zero modifications
(verify: `git` n/a; no writes performed; tree mtimes untouched).
Handler: `GA106LabUCVerifyFlushPrewrite` (`GA106LabUserClient.cpp:399-442`).
Dispatch: table entry 9 (`:522-523`), Count = 10. Struct: `GA106LabFlushPrewriteV1`
48B, dual static_asserts C + C++ (`GA106LabProtocol.h:445-446,538-539`).

## Transport negatives (handler, in order)
| # | Condition (source line) | Result | Host-testable? |
| :--- | :--- | :--- | :--- |
| N1 | `!self \|\| !args` (:407) | BadArgument | No (requires IOKit trap) — code review only |
| N2 | `scalarInputCount != 0 \|\| structureInputSize != 0` (:410) | BadArgument (zero-input enforced) | No — code review only |
| N3 | `!structureOutput \|\| size < sizeof(info)` (:413) | BadArgument | No — code review only |
| N4 | `PinOwnerForExternalMethod` fails (:419) | NotOpen | No — code review only |
| N5 | `owner->verifySysmemFlushPreconditions` fails (:430) | propagate kr | Yes — via flow template tests |
| N6 | success path | full-struct copy + exact size (:438-439) | Yes — via flow template tests |

Defense in depth: dispatch table fixes struct-output size to `sizeof` (:523), so
IOKit rejects undersized buffers before the handler runs; `info` zeroed pre-use
(:424-429) so no stack leak on any path; impl null-checks `out` (`GA106Lab.cpp:1482`).

## Session negatives (unchanged, confirmed present)
`newUserClient`: `type != 0` → Unsupported; non-root → NotPrivileged; slot busy →
ExclusiveAccess (`GA106Lab.cpp` ~:1505-1535).

## Coverage mapping
`test-flush-verify.cpp` (13 PASS groups) drives the REAL `RunVerifyFlushPrewriteFlow`
template with mock ops + `assertZeroHw` (mmio/config/bme/dma counts all 0) — covers
N5/N6 incl. concurrency, AlreadyReady, stop/close, teardown. N1–N4 are IOKit-trap
only and CANNOT run host-side; they rest on code review + static_asserts + table.
GAP (accepted, recorded): no host-executable test exists for N1–N4; recommend a
minimal IOKit-less arg-validation shim only if a future milestone justifies it.

## Verdict of this review
No defect found. Negative matrix is complete at transport level (5 rejections +
exact-size success), zero-leak construction verified by reading, session gates
intact. Nothing to fix; no files changed.
