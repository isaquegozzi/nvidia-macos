# P78 — VM / Address-Space Evidence (GA106 / Turing-Ampere GSP path)

## Verdict: PROVEN (scoped: VA width, limit rule, managed areas, VASpace objects)

## 1. VA width: 47 bits (tu102/Ampere)

- `tu102.c` (`tu102_mmu`): `.dma_bits = 47`.
- `vmmtu102.c` / `vmmgv100.c` page tables top out at shift 47 (`{47,...}` first entry).
- `vmm.c` `nvkm_vmm_ctor`: `bits` = sum(desc->bits) + page->shift; `vmm->limit = 1ULL << bits`
  (managed: `limit = 1ULL << bits`; unmanaged: `limit = size ? addr+size : 1ULL << bits`).
- For tu102 chain this yields a 47-bit VA space = 128 TiB.
- Model bound: `VA_BITS=47`, `VA_LIMIT=1<<47`. Verified by construction (desc chain below).

## 2. Managed vs client areas

- `nvkm_vmm_ctor(managed, addr, size)`:
  - managed=true: client areas `[0,addr)` + `[addr+size,limit)` marked used/mapref,
    NVKM area `[addr,addr+size)` free. Wrap check `addr+size<addr || >limit → -EINVAL`.
  - managed=false: single free VMA `[addr, limit-addr)`; `start>limit || limit>1<<bits → -EINVAL`.
- `nvkm_vmm_get(page,size,&vma)` allocates VA; `nvkm_vmm_put(&vma)` frees (rbtree + split/merge).
- Server-reserved carve-out (GSP path): `r535-vmm.c` reserves 512 MiB at 4 GiB
  (`SPLIT_VAS_SERVER_RM_MANAGED_VA_START=0x100000000`, `SIZE=0x20000000`, page_shift 29)
  and asserts exact placement (`WARN_ON(addr!=START || size!=SIZE)`).

## 3. Handles/objects (GSP/RM path)

- VASpace: `FERMI_VASPACE_A = 0x000090f1` (`nvrm-vmm.h`), allocated via `GSP_RM_ALLOC`
  (`nvkm_gsp_rm_alloc_get(handle, FERMI_VASPACE_A, sizeof(NV_VASPACE_ALLOCATION_PARAMETERS))`).
- Params: `{index=NV_VASPACE_ALLOCATION_INDEX_GPU_NEW(0x00), flags=[IS_EXTERNALLY_OWNED BIT3],
  vaSize/vaStartInternal/vaLimitInternal/bigPageSize/vaBase}`.
- Memory: `nvkm_memory` + target (`VRAM/HOST/NCOH`, `nvkm-memory.h`); `NVOS32` alloc types
  (`IMAGE/DEPTH/.../DMA/INSTANCE/...`, `nvos-excerpt.h`); flags VIRTUAL/SPARSE/ALIGNMENT.
- Mapping: `nvkm_vma` (addr/size/page/shift, memory, mapped/mapref/sparse/used) in
  `vmm->root` rbtree + `vmm->list`; `nvkm_vmm_map(vmm,vma,argv,argc,map)` /
  `nvkm_vmm_unmap(vmm,vma)`; lookup `nvkm_vmm_node_search(vmm,addr)`.
- Channel linkage (P77 carry): `r535_chan_alloc` passes `hVASpace=vmm->rm...` — channel
  allocation is always inside a VASpace; P78 models the VA side, P77 the channel side.

## 4. Valid/reserved ranges + alignment

- Valid: `[0, 1<<47)` minus client-managed areas minus server-reserved `[4G,4G+512M)` (GSP).
- Reserved: server-reserved PDEs copied to RM (`COPY_SERVER_RESERVED_PDES` with explicit
  virtAddrLo/Hi, pageSize=512MiB, per-level physAddress/size/aperture/pageShift).
- Minimum alignment: page size (4 KiB smallest, shift 12); VA+size must be page-aligned
  (`pfn_map`: `!IS_ALIGNED(addr|size, 1<<shift) → -EINVAL`); wrap `addr+size<addr → -EINVAL`.
