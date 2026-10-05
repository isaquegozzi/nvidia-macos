# SHADOW-P78-VM-MMU-SIM — host-only Ampere VM/MMU model (scoped)

Zero production wiring. Synthetic addresses only. No MMIO/PCI/BME/DMA/GSP/VRAM/IRQ/IOKit.

## Scope (grounded only)

Models: VA space (47-bit limit, managed areas, server-reserved hole 512 MiB @ 4 GiB),
page sizes (4K/64K/2M/512M leaf; 256G/128T directory-only), PTE encode/decode
(VALID b0, aperture 2:1 = 0/2/3, VOL b3, PRIV b5, RO b6, atomic-dis b7, kind<<56,
comptag<<36, PA>>4), PDE aperture, 3-layer aperture map (target/PTE/RM), permissions
(PTE RO/PRIV + RM ACCESS RW/RO/WO), guard-level fault classes (12 faults), lifecycle
(VASpace alloc → bind → get → map → flush → lookup → unmap → put → free), GSP VM RPC
param validation (VASPACE_A 0x90f1/GPU_NEW/EXT_OWNED, COPY_SERVER_RESERVED_PDES,
SET/UNSET_PAGE_DIRECTORY), TLB flush as ordering token.

Does NOT model: HW fault-packet wire bytes, kind numeric meanings, comptag allocation,
BAR2_PDB aliasing policy, GH100+ formats, GPFIFO/pushbuffer/command submission.

## Build / test

```sh
clang++ -O2 -std=c++17 -Wall -Wextra -o /tmp/test-vm test-vm.cpp -I. && /tmp/test-vm
clang++ -O2 -std=c++17 -fsanitize=address,undefined -o /tmp/test-vm-asan test-vm.cpp -I. && /tmp/test-vm-asan
./mutation-vm-harness.sh   # each mutant must be CAUGHT (exit != 0)
clang --analyze -std=c++17 vm_sim.hpp test-vm.cpp
```

## Files

- `vm_sim.hpp` — model (single header).
- `test-vm.cpp` — baseline tests (P78 §7 coverage).
- `mutation-vm-harness.sh` — per-guard mutants.
- `EVIDENCE-BINDING.md` — claim→source line map.

Note: `test-vm.plist` + `vm_sim.hpp.pch` are `clang --analyze` byproducts (exit 0).
