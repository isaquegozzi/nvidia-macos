# M11 part 1 — VRAM/MMU architecture survey (read-only research, model deferred)

Author: Muse, 2026-09-09 06:54 -0300. Host-only doc. Directed by valid verdict
G0101 §6 (M11 work package). Outcome: executable `gmmu-sim` DEFERRED — local
evidence grounds ranges/registers/DMA truths but NOT PTE/aperture/fault semantics.
Fabricating those bit layouts would violate house standards (M10 precedent).

## What exists locally (grounded, cited)
1. FB range layout (`UPSTREAM-EVIDENCE/nova-core-fb.rs:148-330`): framebuffer starts
   at 0; VGA workspace at end; `FbRanges::new` computes fb/vga/frts/boot/wpr regions;
   GSP-FMC boot region sizes. Our M7/M8 inventories already mirror these ranges.
2. Sysmembar registers (`nova-core-fb-regs.rs:33-39` + HSHUB0 base :65): low/high bits
   of the sysmem address for GPU sysmembar ops. Our M4/M5 Gate-B model drives the
   GA106 equivalents (HI 0x100c40 + LO 0x100c10) with readback verification.
3. DMA source of truth (`nova-core-gpu.rs:344` `dma_set_mask_and_coherent`;
   `driver.rs:81` BAR0 `iomap_region_sized`): all device addresses derive from
   coherent allocations — our M7 IOVA model follows this exactly.
4. Staging chain (GSP-BOOT-SEQUENCE.md): WPR2→FWSEC→Booter→GSP-RM places every
   image before any MMU programming could matter. MMU work is strictly post-boot.

## What is missing (all required before any gmmu-sim)
- PTE entry layout (address bits, aperture select, cache attrs, permissions): zero hits.
- Aperture encoding values (Sysmem Coherent/Non-Coherent/Local VRAM): names only,
  no bit positions (`grep -il aperture` → nothing).
- Page-table depth, VA width, page sizes for GA106 GMMU: absent locally.
- Fault buffer/packet formats and classification codes: absent locally.
- TLB invalidate command sequencing: absent locally.

## Unblock criteria for the executable model
Vendor (semantics only) ONE of: Nova MMU driver page-table code with field layouts;
OpenRM `mmu_` path with PTE/aperture/fault definitions; then model walks +
aperture decode + privilege + fault packets + TLB invalidate with tests + 5
mutants per house practice (mirrors the G0101 §6 package item for item).

## Deliberately NOT done
No invented PTE bitfields, no guessed aperture values, no placeholder walker whose
tests would restate guesses. Partial credit honestly: ranges/registers/DMA truths
are modeled upstream of MMU (M4–M9); the MMU layer itself awaits sources.
