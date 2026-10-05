# R3 — Candidate Decision (DESIGN-ONLY, OFFLINE ONLY)

```text
SESSION_ID: V4-R3-LOAD-PACKAGE / MILESTONE: R3_FIRST_CONTROLLED_LOAD_PACKAGE
STATE: MUSE_WORK / ACTOR: MUSE_BUILD
LIVE_EXECUTION_AUTHORIZED = NO (this file authorizes nothing; no stage/load/boot)
```

## 1. Comparison (only 1.6.2 / 1.7.3 / 1.8.0)

| Dimension | 1.6.2 | 1.7.3 | 1.8.0 |
|---|---|---|---|
| Version / tree | 1.6.2 `GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src` | 1.7.3 `GA106Lab-kext7-sysmem-flush-prewrite-fix3-src` | 1.8.0 `GA106Lab-kext8-production-architecture-src` |
| CFBundleIdentifier | `org.nvidia-macos.GA106Lab` (identical) | `org.nvidia-macos.GA106Lab` (identical) | `org.nvidia-macos.GA106Lab` (identical) |
| Purpose | GFW-readiness read-only (selector 8) + UserClient owner-lifetime fix | Gate A read-only (selector 9 verify-flush-preconditions), preserves 1.6.2 owner flow | Exact copy of 1.7.3 + P80 production layer (phase machine, fail-closed HW stubs, model contracts) |
| Known fixes | Astra 1.6.1 read→race→retain window fixed: atomic pin+retain under owner-state lock (`GA106LabUserClientOwnerFlow.hpp`, shared template prod+test) | Gate A composition read-only; `fFlush*` disjoint state; `AlreadyReady` without realloc; inherits 1.6.2 owner flow | Inherits all 1.6.2/1.7.3 fixes; adds `resetProductionPhaseForTeardown`, ledger-not-pointer audit, LiveBlocked terminal |
| Known lifetime state | Owner pin/drain proven for UC path; flush path absent (no selector 9 state) | `fFlushState` 3-state (never Programmed); `fFlushBusy` exclusion; `fFlushStopping` terminal; `TeardownFlushPrewritePageFlow` | P80 lifetime audit: explicit per-state teardown targets, contracts invalidated at teardown, idempotent double-stop/close, TSAN concurrency proof |
| Selector count | Count=9 (0–8; selector 8 `ReadGfwBootReadiness`) | Count=10 (adds selector 9 `VerifySysmemFlushPreconditions`) | Count=10, zero new selectors, ABI intact (audited) |
| Production architecture status | Pre-production: no Gate A, no P80 layer | Immediate predecessor of P80 (P80 base) | IS the current production architecture (P80) |
| Historical live status | NEVER live (offline only; prestage `PRESTAGE-P60-1.6.2/` never staged) | NEVER live (1.7.0 Astra FAIL D DO NOT STAGE; 1.7.1 fix unapproved/reaudit pending; 1.7.2 intermediate) | NEVER live |
| Offline test status | GFW harnesses (mutation-gfw, executable-proof), `test-gfw-{readiness,concurrency,lifetime,deadline}`, `test-userclient-lifetime`, `test-stop-free-regression` ALL PASS | `test-flush-prewrite` 6 groups PASS; ASAN/TSAN PASS; sysmem 16/16 mutations inherited | 5 new suites (phase/blocks/contracts/lifetime/concurrency) PASS; mutation-production 4/4 + 16/16 + GFW + flush inherited; ASAN/UBSAN + TSAN; `clang --analyze` |
| Hash identity status | `SHA256.txt` source hashes; prestage binary SHA `05611254…` (candidate EFI copy, never staged) | `SHA256.txt` source hashes; no production manifest | `MANIFEST-P80-1.8.0.txt`: binary `b6d4ea31…`, plist `2d0d3cf9…`, version consistency plist×2+protocol, audited rebuild method (P64) |

## 2. Verdict

```text
FIRST_CONTROLLED_LOAD_CANDIDATE = 1.8.0 (TG-KEXT8-PRODUCTION-ARCHITECTURE)
```

Why (criterion by criterion, NOT newest-is-best):
- Lifetime correctness: 1.8.0 strictly contains the 1.6.2 owner-flow fix and the
  1.7.3 flush-lifetime model, plus the only published teardown/lifetime audit
  (P80) with concurrency proof. 1.6.2's fix is scoped to selector 8 and covers
  neither flush state nor the load path; 1.7.3 has the model but not the audit.
- Auditability: only 1.8.0 ships a production manifest + state-machine doc +
  lifetime audit + hardware-blocks doc + open questions as one pinned set.
- Identity/hash: only 1.8.0 pins binary+plist+protocol versions in one manifest
  with a documented rebuild method.
- Predictability: only 1.8.0 has an explicit kernel-side phase machine with a
  terminal fail-closed state (`LiveBlocked`) and a per-state teardown map, so a
  load-only window has a deterministic ceiling even under misuse.
- Least untested paths: the 1.8.0 delta over 1.7.3 is fail-closed-only (13 stubs
  that always return NotPermitted, pure validators, zero HW, zero new selectors);
  for an L1/L2 window that invokes no selector at all, it adds no exequible HW
  path while adding a structural ceiling against accidental activation.
- Production alignment: 1.8.0 IS the current production architecture; auditing
  and loading anything older would validate a lineage already superseded on paper.

Rejected: 1.6.2 — smallest surface but pre-Gate-A, pre-P80, weakest audit/identity;
choosing it would discard two generations of lifetime fixes for no window benefit
(L1/L2 invokes no selector, so Count=9 vs 10 is not a live-surface saving).
Rejected: 1.7.3 — strictly dominated by 1.8.0 (identical proven flows, minus the
fail-closed ceiling, minus the manifest, minus the lifetime audit).

Identical `CFBundleIdentifier = org.nvidia-macos.GA106Lab` in all 3 versions is the documentary reason coexistence is impossible, hence strategy A (single-active, `COEXISTENCE_ALLOWED = NO`).

## 3. Blocking note (unchanged by this choice)

No candidate is authorized for stage/load/boot. The choice binds the FUTURE audit
pin (P-D4.0..P-D4.5 must be published against the EXACT 1.8.0 binary hash above)
and nothing else. Any binary divergence at future F-AUTH = FAIL, no audit reuse.

```text
LIVE_ACTIONS = 0 / LIVE_EXECUTION_AUTHORIZED = NO
```
