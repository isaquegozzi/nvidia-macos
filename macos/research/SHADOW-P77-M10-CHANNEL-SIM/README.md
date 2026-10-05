# SHADOW-P77-M10-CHANNEL-SIM — host-only channel/runlist model (scoped)

Zero production wiring. Synthetic addresses only. No MMIO/PCI/BME/DMA/GSP/VRAM/IRQ/IOKit.

## Scope (grounded only)

Models: channel alloc payload validation (`NV_CHANNELGPFIFO_ALLOCATION_PARAMETERS`
template from `r535_chan_alloc`), FIFO lifetime (alloc→bind→schedule→error→free),
runlist management as chid/runq/USERD math + schedule enable (GSP-abstracted, no entry
bytes), heap reservation (`client_alloc`, `management_overhead`, `wpr_heap`), RPC
function routing (`GSP_RM_ALLOC=103` channel path vs legacy `ALLOC_CHANNEL_DMA=6`
recognized-but-not-path), doorbell composition, RC/error pinning.

Does NOT model: runlist entry bytes, `NV2080_ENGINE_TYPE` numerics, inst/userd byte
counts, GPFIFO emission, fault/GR contents, real queues/firmware.

## Build / test

```sh
clang++ -O2 -std=c++17 -Wall -Wextra -o /tmp/test-channel test-channel.cpp -I.
./tmp/test-channel            # baseline, expect pass
clang++ -O2 -std=c++17 -fsanitize=address,undefined -o /tmp/test-channel-asan test-channel.cpp -I. && /tmp/test-channel-asan
./mutation-channel-harness.sh # each mutant must be CAUGHT (exit != 0)
clang --analyze -std=c++17 channel_sim.hpp test-channel.cpp
```

## Files

- `channel_sim.hpp` — model (single header).
- `test-channel.cpp` — baseline tests (P77 §6 where applicable).
- `mutation-channel-harness.sh` — per-guard mutants.
- `EVIDENCE-BINDING.md` — claim→source line map.
