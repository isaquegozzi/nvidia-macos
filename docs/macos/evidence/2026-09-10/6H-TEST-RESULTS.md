# 6H TEST RESULTS — HARD-6H real-relay session (offline, zero sudo, zero live)

Wall: START 1789020393 (2026-09-10T06:06:33Z) → FINISH 1789054503 → ELAPSED 34110s.

## New tests (all PASS normal; +ASAN; +TSAN where threaded)

```text
test-dma47-address-contract 8 grupos (gap + Valid47 helper + ops wiring G1) — /tmp/test-dma47 rc=0, -asan rc=0
test-bme-race-contract 4 grupos (control + flip-fails-closed + transport-fail + clearfail-retention) — rc=0, -asan rc=0, -tsan rc=0
test-gateb-fakebar-contract 7 grupos — rc=0, -asan rc=0
test-seq-fuzz-contract 4 grupos (F1 10000 + F2 10000 + ring + ledger) — rc=0, -asan rc=0
test-completion-chain-contract 7 grupos (7-state) — rc=0, -asan rc=0
test-contract-fuzz 4 grupos (9000 mutated) — rc=0, -asan rc=0
test-channel-generation-contract 6 grupos (P77 ledger) — rc=0, -asan rc=0
/tmp/alias-check encode=0 decode=1 rc=0 (F2 fix proof)
```

## Rebuilt vs new headers (PASS)

```text
sysmem 10/10; flush-prewrite 6/6; flush-verify 12/12 (rebuilt, -I src)
/tmp/GA106Lab.o + /tmp/GA106LabUserClient.o rc=0 (-mkernel; pre-existing SDK warnings only)
```

## Existing suites (PASS, post-all-edits batch)

```text
contract; production-phase 5/5; production-contracts 3/3; production-blocks 3/3;
production-lifetime 6/6; production-concurrency 2/2; gfw-readiness 12/12; gfw-deadline 8/8;
gfw-concurrency 2/2; gfw-lifetime 5/5; slot 6; bar-map-probe 10; first-mmio-read 16;
static-identity 7; prod-binding 3; lock-failure 4
```

## Mutations (re-run vs new code, zsh harnesses)

```text
flush 6/6 CAUGHT; sysmem 16/16 CAUGHT (5 runs each); production 4/4 CAUGHT
VM sim 10/10 CAUGHT; submission sim 10/10 CAUGHT (prior window, reused)
```

## Live / kernel / sudo

```text
LIVE_SELECTOR_CALLS = 0; LIVE_WRITES = 0; KEXT_LOAD_UNLOAD = 0; REBOOT = NO
SUDO_INVOCATIONS = 0 (no sudo, no sudo -n)
KERNEL_PANIC_OBSERVED = NO live-induced (no live actions; panic log not re-scanned this session — carryover CLEAN from sel5 closeout)
```
