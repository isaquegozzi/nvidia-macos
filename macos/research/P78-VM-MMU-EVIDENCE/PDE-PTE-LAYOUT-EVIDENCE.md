# P78 — PDE/PTE Layout Evidence (gp100-line, used by tu102/Ampere)

## Verdict: PROVEN (PTE + PDE bit placement for the tu102 GSP path)

Scope: `vmmgp100.c` + `vmmgf100.c` + `vmm.h` describe the page-table *format the driver
programs on this lineage*. `tu102_vmm` reuses `gp100_vmm_desc_16/12`, `gp100_vmm_valid`,
`gf100_vmm_aper`, `gp100_vmm_mthd`, `gv100_vmm_join` — so this IS the Ampere-relevant layout
here, not a foreign arch. Fermi-line `gf100_vmm_pgt_pte (addr>>8)` is documented as contrast only.

## 1. Entry sizes / level descriptors

- `gp100_vmm_desc_16[]` (big-page chain): `{LPT,5b,8B,align 0x100}`, `{PGD,8b,16B,0x1000}`,
  `{PGD,9b,8B,0x1000}` ×2, `{PGD,2b,8B,0x1000}`.
- `gp100_vmm_desc_12[]` (small-page chain): `{SPT,9b,8B,0x1000}` then same PGD chain.
- Root PD0 entries are 16 B (`VMM_WO128`), all other levels 8 B (`VMM_WO064`/`VMM_FO064`).
- Generic frame (`mmu_fmt_types.h`): `MMU_FMT_LEVEL{virtAddrBitLo/Hi, entrySize, bPageTable,
  numSubLevels}`, `MMU_FMT_MAX_SUB_LEVELS=2` (dual PDE: big+small in parallel).

## 2. PTE (leaf, 8 B): `data = (addr >> 4) | type`

From `gp100_vmm_pgt_pte`, `gp100_vmm_pgt_pfn`, `gp100_vmm_pfn_clear/unmap`:

| Bit(s) | Meaning | Provenance |
|---|---|---|
| 0 | VALID | `BIT_ULL(0)` set on map; `data & ~BIT_ULL(0)` on clear; `(data & (3ULL<<1))!=0` aperture test only meaningful when valid |
| 2:1 | aperture: 0 = VRAM, 2 = SYSTEM_COHERENT(HOST), 3 = NCOH | `gf100_vmm_aper` returns 0/2/3; `2ULL<<1` written for sysmem-coherent; `1` never produced → INVALID |
| 3 | VOL (volatile/coherent) | set with HOST aperture (`gp100_vmm_pde`, join); sparse marker (see §4) |
| 5 | PRIV (privileged) | `map->type \|= priv<<5`; LPT-invalid marker (see §4) |
| 6 | RO (read-only; 0 = writable) | `if (!W) data \|= BIT(6)`; `map->type \|= ro<<6` |
| 7 | atomic-disable (1 = atomics off) | `if (!A) data \|= BIT(7)` |
| 63:56 (8b) | kind | `map->type \|= kind<<56`, validated `< kindn` and `!= kind_inv` (tu102 kind table) |
| 47:36~ | comptagline (compressed) | `(1+nr(addr))<<36` base, `nr(size)<<36` incr; `nr = size>>16` (1 tag/64 KiB VRAM); Turing+ fills `PTE_COMPTAGLINE` |
| addr field | `(PA >> 4)`; decode `(data>>8)<<12` (pfn path, dma-mapped page) | `data=(addr>>4)\|type`; `addr=(data>>8)<<12` after dma_map |

## 3. PDE (directory): aperture + child address

- `gp100_vmm_pde`: `VRAM→1ULL<<1`, `HOST→2ULL<<1 + VOL(b3)`, `NCOH→3ULL<<1`, then `\|= pt->addr>>4`.
- `gf100_vmm_pgd_pde` (line): same 1:0 aperture + `VOL b35` for HOST on the older line.
- PD0 root: 16 B entries (`WO128(data,0)`), same low-qword format.
- `nvkm_mmu_pt{addr, base, memory, sub}` backs each level; `vmm->pd->pt[0]->addr` is the root
  physical address programmed to HW (flush path `0xb830a0 = pd_addr>>8`).

## 4. Sentinel encodings (no VALID, still meaningful)

- Sparse PTE: `VALID=0 + VOL=1` (`gp100_vmm_pgt_sparse`: fill `BIT(3)`).
- LPT-invalid (dual-table LPTE with no live SPTEs): `VALID=0 + PRIV=1` (`gp100_vmm_lpt_invalid`).
- Unmap: fill `0` (`gf100_vmm_pgt_unmap`).
- Model rule: decode VALID=0 as fault `InvalidPte`, except VOL-only → `Sparse` (benign hole),
  PRIV-only at LPT position → `InvalidLpte` (ignored SPTEs). All three byte-patterns proven.

## 5. Explicitly NOT claimed

- Per-kind numeric *meanings* (tu102 kind[16] table values are opaque indices; only
  range `<16` + `!=0x07` invalid is modeled).
- Comptag *allocation* (backing store is PMU/GSP-RM owned; model carries the field, never allocates).
- `gf100 (addr>>8)` Fermi encoding is NOT the Ampere path (contrast only).
