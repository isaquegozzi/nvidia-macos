# Self-audit M4–M9 against the G0008 §3 eight axes (Muse, 2026-09-09 02:54 -0300)

Purpose: pre-check what a valid checkpoint verdict would examine. Not a substitute
for independent review. Method: source re-read + targeted greps + fresh runs
(02:51: 15/12/10/7/7 PASS). One NEW finding (F1, low) + two confirmations.

## Axis results (M7/M8/M9; M4–M6 already checkpoint-approved in G0008)
1. Callgraph: M8 boot() guard order (terminal-then-order) re-verified by reading;
   each mutant maps to exactly one guard (harness 5/5 proves independence). M9
   receive() validates length→checksum→function (Nova order). No illegal recursion;
   all loops bounded (chunk counts, poll budgets, fuzz iters). PASS.
2. State/lifetime: M8 terminal pinning tested both directions (Failed + Done);
   staged members retained, no clear API exists (fresh-object retry only). M7
   post-Done re-run returns InvalidArg via programmed_=false (read-confirmed;
   covered under unprogrammed group semantics). M9 stateless per message + monotone
   seq/readPtr. PASS.
3. Concurrency: all sims single-threaded by design; real-kext serialization covered
   by flush-verify concurrency groups (fresh 12/12 02:44). No shared state added.
   PASS (TSAN n/a everywhere).
4. ABI: no production ABI touched by any shadow unit. M9 envelope is own-layout by
   design (documented, algorithm-identical). FlushPrewriteV1 static_asserts intact.
   PASS.
5. Negative scope: grep over SHADOW-P76-NEXT for sudo/IOConnectCall/IOServiceOpen/
   kextload//dev/mem/bless → ZERO hits (02:54). Production 1.7.3 zero-write claims
   stand (G0008 axis 5 + ZERO-WRITE-PROOF.md). PASS.
6. Test coverage: see FINDING F1 (sole gap). Otherwise: M7 overlap proven
   unreachable (lemma); M8 meta-overlap documented unreachable (disjoint+Inside);
   every other branch exercised incl. 10k/5k/5k/20k fuzz oracles. CONDITIONAL PASS.
7. Mutation coverage: 6/6 + 5/5 + 5/5 + 5/5 + 5/5 live-caught; the two unmutated
   branches are both proven/documented-unreachable defense-in-depth (no mutant
   claimed, M7 precedent accepted G0008). PASS.
8. Binary & repro: all suites one-command deterministic rebuilds; hashes stable
   across turns (M7 e33f8c20×5 turns; M8/M9 since creation). PASS.

## FINDING F1 (new, LOW, untested branch — no fix applied)
M9 `receive()`: the `availLen > declared` path (trailing bytes belong to the next
message — the core queue-concatenation claim) is never executed: unit tests pass
exact sizes, fuzz passes avail ≤ declared (verified by reading test-rpc.cpp:124).
Proposed group `trailing-bytes-next-message` (concatenate two envelopes, receive
first with avail=both, expect Ok + readPtr+1): DEFERRED to post-verdict touch so
M9 sources stay frozen under review. Not a correctness doubt (branch is a
commented no-op by construction), a coverage hole in an important claim.

## Disposition
No REJECT-worthy defect. F1 + R8 (overflow) are the complete known-unfixed list
for M7/M8/M9 alongside previously accepted gaps (N1–N4 trap-only, WPR2 payloads,
fence/IRQ, Nova layout/lock/notify). All recorded with homes (post-verdict touch
or explicit deferral).
