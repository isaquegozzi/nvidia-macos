# SHADOW-P79-COMMAND-SUBMISSION-SIM — host-only GPFIFO/submission model (scoped)

Zero production wiring. Synthetic addresses only. No MMIO/PCI/BME/DMA/GSP/VRAM/IRQ/IOKit,
no real command submission.

## Scope (grounded only)

Models: GPFIFO entry encode/decode (8B, GET/GET_HI/LENGTH/LEVEL/SYNC/FETCH/OPCODE with
documented 7:0 dual-view rule), ring (power-of-two count from channel geometry, PUT/GET
indices, full/empty/wrap, doorbell-kick token, logical consumption), pushbuffer windows
(addressed by entries, residency via integrator-provided live map), method packets
(ADDRESS/SUBCHANNEL/COUNT/SEC_OP classes incl. RESERVED6 detection, 18-method allowlist),
semaphore fences (RELEASE/ACQUIRE/free, Reference counter), work-submit token lifecycle,
error pinning, teardown ordering.

Does NOT model: interrupt delivery, CRC verification, conditional-fetch predication,
subroutine linkage internals, token value formats, BE hosts, MEM_OP engine internals,
IOGPU/Metal.

## Integration (no divergent copies)

`test-integration.cpp` includes `channel_sim.hpp` (P77) + `vm_sim.hpp` (P78) directly
and proves: no submit without Scheduled channel; ring geometry == P77 entries;
no submit on unmapped PB (P78 gate); stale-PB refusal after P78 unmap; PUT bounded by
P77 ring; one doorbell token per PUT batch.

## Build / test

```sh
clang++ -O2 -std=c++17 -Wall -Wextra -o /tmp/test-submission test-submission.cpp -I. && /tmp/test-submission
clang++ -O2 -std=c++17 -Wall -Wextra -o /tmp/test-integration test-integration.cpp -I. && /tmp/test-integration
clang++ -O2 -std=c++17 -fsanitize=address,undefined -o /tmp/test-sub-asan test-submission.cpp -I. && /tmp/test-sub-asan
./mutation-submission-harness.sh  # each mutant must be CAUGHT (exit != 0)
clang --analyze -std=c++17 submission_sim.hpp test-submission.cpp test-integration.cpp
```

## Files

- `submission_sim.hpp` — model (single header).
- `test-submission.cpp` — baseline tests (P79 §8 coverage).
- `test-integration.cpp` — P77+P78+P79 cross-model invariants.
- `mutation-submission-harness.sh` — per-guard mutants.
- `EVIDENCE-BINDING.md` — claim→source line map.

Note: `*.plist` + `*.pch` (if present) are `clang --analyze` byproducts (exit 0).
