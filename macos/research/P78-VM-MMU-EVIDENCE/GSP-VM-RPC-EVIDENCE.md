# P78 — GSP VM RPC Evidence

## Verdict: PROVEN (VASpace alloc + reserved-PDE copy + page-directory bind)

## 1. Path selection (direct vs GSP-RM)

- `tu102_mmu_new`: `if (nvkm_gsp_rm(device->gsp)) return r535_mmu_new(&tu102_mmu,...)`.
  On GA106-with-GSP the MMU/VMM funcs are the tu102 tables but allocation/VASpace
  lifecycle goes through RM RPC (`r535_mmu_*`, `nvkm_gsp_rm_alloc_get/push/done/free`,
  `nvkm_gsp_rm_ctrl_get/wr`). Same GSP-vs-direct division as P77 (`ga102_fifo_new`).

## 2. VASpace allocation (GSP_RM_ALLOC + FERMI_VASPACE_A)

- Class `0x000090f1`, params `NV_VASPACE_ALLOCATION_PARAMETERS` (index/flags/vaSize/
  vaStartInternal/vaLimitInternal/bigPageSize/vaBase), `index=GPU_NEW`, `flags=EXT_OWNED(b3)`
  iff external. Envelope is the standard `nvkm_gsp_rm_alloc_{get,wr}` (P77-proven framing,
  M9-compatible). Free via `nvkm_gsp_rm_free`.

## 3. Server-reserved PDEs (internal VASpace)

- `NV90F1_CTRL_CMD_VASPACE_COPY_SERVER_RESERVED_PDES (0x90f10106)`, params
  `{hSubDevice, subDeviceId, pageSize=512MiB, virtAddrLo=4G, virtAddrHi=4G+512M-1,
  numLevelsToCopy, levels[6]{physAddress,size,aperture,pageShift}}`.
- Driver walks `page->desc` accumulating `pageShift`, copies each level's
  `pd->pt[0]->addr`, `size=(1<<bits)*entrySize`, `aperture=1`, descending pageShift.
  `GMMU_FMT_MAX_LEVELS=6` bounds the copy. Exact placement asserted (4G/512M).

## 4. Page-directory bind (external VASpace)

- `NV0080_CTRL_CMD_DMA_SET_PAGE_DIRECTORY (0x801813)`: `{physAddress=rootPD, numEntries=1<<L0bits,
  flags=APERTURE_VIDMEM, hVASpace, chId, subDeviceId, pasid}` → `external=true`.
- Teardown `NV0080_CTRL_CMD_DMA_UNSET_PAGE_DIRECTORY (0x801814)`: `{hVASpace}` → `external=false`.
- Flags companions PRESERVE_PDES/ALL_CHANNELS/IGNORE_CHANNEL_BUSY/EXTEND_VASPACE documented
  (not exercised on this path; model accepts 0 and rejects unknown high bits).

## 5. Sequencing (alloc before use)

VASpace alloc → (reserved-copy | set-PD) → VMA get → map → flush → lookup → unmap → put →
(unset-PD) → VASpace free. Every step has a distinct error return; skipping (e.g. map
before VASpace ready) is a model fault `Stale`/`OutOfRange`. Mirrors P77 RPC sequencing
(GspInitDone + GetGspStaticInfo before channel alloc).
