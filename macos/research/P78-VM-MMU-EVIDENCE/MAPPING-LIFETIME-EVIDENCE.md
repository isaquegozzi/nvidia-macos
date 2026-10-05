# P78 — Mapping-Lifetime Evidence

## Verdict: PROVEN (create→map→lookup→unmap→free→reuse + GSP vaspace tie-in)

## 1. VM create / destroy

- `nvkm_vmm_new_/ctor(func,mmu,managed,addr,size)`: computes VA bits from desc chain,
  allocates root PD (`nvkm_vmm_pt_new` + `nvkm_mmu_ptc_get(size = hdr + desc->size<<desc->bits,
  align)`), builds free rbtree. Failures: `-ENOMEM` (PD alloc), `-EINVAL` (bad range/levels).
- `nvkm_vmm_dtor`: GSP vaspace_del first (`r535_mmu_vaspace_del`), then drains all VMAs
  (`rb_first→vmm_put` loop), releases bootstrap PTEs, `WARN_ON(!list_empty)`, frees null page
  and root PD. Ordering is load-bearing: firmware VASpace torn down before host PTEs.
- GSP create: `r535_mmu_vaspace_new(handle, external)` — client+device ctor, `GSP_RM_ALLOC`
  FERMI_VASPACE_A, then internal (reserve+`COPY_SERVER_RESERVED_PDES`) or external
  (`DMA_SET_PAGE_DIRECTORY`, `external=true`) branch.
- GSP destroy: `r535_mmu_vaspace_del` — `UNSET_PAGE_DIRECTORY` (if external) + `rm_free` +
  device/client dtor + `vmm_put(rsvd)`.

## 2. Memory create / import

- `nvkm_memory_new(device,target,size,align,zero)`; `nvkm_mem_new_type(mmu,type,page,size)`;
  `gf100_mem_new/map` wires target→page-size→VMA.
- `NVOS32` alloc: `{owner,hMemory,type,flags,attr,format,size,alignment,offset,limit,
  address,rangeBegin/End,...}` with VIRTUAL/SPARSE/ALIGNMENT_HINT/FORCEEXTERNALLY_MANAGED etc.

## 3. Map / lookup / unmap / free / reuse

- `nvkm_vmm_get(page,size,&vma)` → VA carve (rbtree best-fit + split); `nvkm_vmm_put(&vma)` → free+merge.
- `nvkm_vmm_map(vma,argv,argc,map)`: `map_valid` (target/page/perm/kind/aperture) →
  `map_choose` (page-size select) → per-PTE writers (`mem/dma/sgl/pfn`) via desc funcs.
  Reject: unmanaged/no-mapref/occupied (`-EINVAL`), bad args (`-EINVAL`).
- Lookup: `nvkm_vmm_node_search(addr)` → VMA or NULL (`-ENOENT`); `nvkm_umem_search/uvmm_search`
  for client handles.
- `nvkm_vmm_unmap(vma)`: `unmap_locked→unmap_region` (PTE clear + `ptes_unmap_put`), `mapped=false`.
- `pfn_map/unmap`: PFN-flag (`V/W/A/VRAM/ADDR`) path with per-page V-map coherence + split/merge;
  `NVKM_VMM_PFN_NONE` marks holes.
- Reuse: freed VMA returns to free rbtree; adjacent compatible VMAs merge (`split_merge`);
  stale use is refused (`mapped=false`, `memory=NULL`).
- Double-map, map-over-live, unmap-missing, free-while-mapped (dtor WARNs; model errors),
  wrap/overflow — all have explicit error paths above; each is a model mutant + test.

## 4. TLB / PDB ordering tokens (modeled as sequence, never MMIO)

- After map/unmap: `flush` (PAGE_ALL/HUB_ONLY) or `invalidate_pdb(addr)`; GSP path additionally
  owns coherence via RM (COPY_PDES / SET_PAGE_DIRECTORY). Model records
  `needsFlush` per mapping and requires a flush-token before lookup succeeds post-mutation
  (mirrors driver ordering without touching HW).
