# P78 — Aperture Evidence (three layers, separated)

## Verdict: PROVEN (each layer's values; correspondences marked as cross-layer map, not same enum)

## Layer 1 — Nouveau memory target (driver-internal)

`nvkm-memory.h` `enum nvkm_memory_target`:

```text
NVKM_MEM_TARGET_VRAM = video memory
NVKM_MEM_TARGET_HOST = coherent system memory
NVKM_MEM_TARGET_NCOH = non-coherent system memory
(+ INST / INST_SR_LOST = instance memory, not mappable Aperture)
```

## Layer 2 — PTE/PDE wire encoding (HW, gp100-line)

`gf100_vmm_aper(target)`: `VRAM→0`, `HOST→2`, `NCOH→3`; written to PTE bits 2:1
(`vmmgp100.c`/`vmmgf100.c`). Value `1` is never produced → model rejects as
`ApertureMismatch`. HOST additionally sets VOL (b3). PDE uses the same 2:1 values
(`gp100_vmm_pde`). PFN path checks `(data & (3<<1)) != 0` to detect a programmed aperture.

## Layer 3 — RM GSP control (firmware ABI)

`nvrm-vmm.h` `NV0080_CTRL_DMA_SET_PAGE_DIRECTORY_FLAGS_APERTURE`:

```text
VIDMEM = 0x0 | SYSMEM_COH = 0x1 | SYSMEM_NONCOH = 0x2   (bits 1:0)
+ PRESERVE_PDES b2, ALL_CHANNELS b3, IGNORE_CHANNEL_BUSY b4, EXTEND_VASPACE b5
```

`r535-vmm.c` external path programs `APERTURE VIDMEM` for the root PD
(`NVDEF(...APERTURE, VIDMEM)`); per-level `COPY_SERVER_RESERVED_PDES.levels[].aperture = 1`
(small-int aperture domain, consistent with page-table memory in sysmem).

## Layer 4 — RM allocation attributes (nvos.h)

`NVOS32_ATTR_LOCATION` (bits 26:25): `VIDMEM=0`, `PCI=1` (sysmem via PCI), `ANY=3`.
`NVOS32_ATTR_COHERENCY` (31:29): UNCACHED 0 / CACHED 1 / WC 2 / WT 3 / WP 4 / WB 5.
`NVOS32_ATTR_PHYSICALITY` (28:27): DEFAULT/NONCONTIGUOUS/CONTIGUOUS/ALLOW_NONCONTIGUOUS.

## Correspondence (proven by construction, values NOT interchangeable)

| Concept | L1 target | L2 PTE 2:1 | L3 NV0080 | L4 LOCATION |
|---|---|---|---|---|
| VRAM / video memory | VRAM | 0 | VIDMEM 0 | VIDMEM 0 |
| Coherent sysmem | HOST | 2 | SYSMEM_COH 1 | PCI 1 (+CACHED-ish) |
| Non-coherent sysmem | NCOH | 3 | SYSMEM_NONCOH 2 | PCI 1 (+UNCACHED-ish) |

Model enforces each layer's own enum and tests the correspondence table explicitly;
a raw integer is never moved across layers without mapping (regression test pins this).
