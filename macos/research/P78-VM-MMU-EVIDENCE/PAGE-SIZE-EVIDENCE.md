# P78 — Page-Size Evidence (tu102/Ampere)

## Verdict: PROVEN (shifts, leaf-vs-directory, alignment)

## 1. Shifts (tu102_vmm.page + gp100_vmm.page)

```text
shift 47 → desc_16[4] Sxxx (directory-only)
shift 38 → desc_16[3] Sxxx (directory-only)
shift 29 → desc_16[2] Sxxx (512 MiB, directory-only on tu102; server-reserved unit)
shift 21 → desc_16[1] SVxC (2 MiB leaf-capable)
shift 16 → desc_16[0] SVxC (64 KiB leaf-capable)
shift 12 → desc_12[0] SVHx (4 KiB leaf-capable)
```

(gp100 base table marks 21/16 as SVxx; tu102 specializes to SVxC and 12 to SVHx —
the tu102 row is authoritative for GA106.)

Sizes: 4 KiB / 64 KiB / 2 MiB / 512 MiB / 256 GiB(dir) / 128 TiB(dir).
`map->next = (1ULL<<shift)>>4` (PTE stride per page).

## 2. Leaf capability (type flags, vmm.h)

- `Sxxx` = SPARSE-only → upper levels cannot hold leaf PTEs (only PDE/sparse).
- `SVxC` = SPARSE|VRAM|COMP → 64K/2M leaves in VRAM or sysmem, compressible.
- `SVHx` = SPARSE|VRAM|HOST → 4K leaves in VRAM or HOST sysmem.
- `VMM_MAP_ITER_*` walks split big pages into small PTEs when the backing is smaller
  (dual-PDE support, `MMU_FMT_MAX_SUB_LEVELS=2`).

## 3. Alignment / coverage rules (modeled)

- VA and size must be `1<<shift` aligned (`pfn_map` + `valid` paths → `-EINVAL` otherwise).
- `addr+size` wrap → `-EINVAL`; `> vmm->limit` → `-EINVAL`.
- DESC bit-counts (16-chain: LPT 5 + PD0 8 + PD1 9+9+2 = 33 + shift 12/16... ) sum with the
  leaf shift to the 47-bit VA (ctor `bits` accumulation; `WARN_ON(levels>MAX)`).
- PDE boundary: crossing a directory entry requires the parent PDE programmed
  (`pde[0]` chain in COPY_SERVER_RESERVED_PDES with per-level pageShift).
- PTE boundary: `ptes` counts within one PT; spanning PTs splits across PDEs (model tests both).
