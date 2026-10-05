# P78 VM/MMU — Source Manifest (pinned upstream evidence)

Author: Muse, 2026-09-09 -0300. Read-only research; zero live HW; candidate 1.7.3 frozen.
Retrieval: GitHub REST contents API + base64 payloads under `upstream-excerpts/`.
torvalds/linux HEAD at retrieval: `893e11787f78e43b534e252249ac3fff4d1333f8` (2026-09-08).
OpenRM ref `main` (gsp_fw_heap precedent commit `57130a27` 610.43.02; MMU files blob-pinned below).
No blog/post used as layout authority. Nova has NO mmu driver (verified: `drivers/gpu/nova-core/`
lists no mmu dir) — Nova used only for heap/DMA truths carried from P65/P77, not for PTE layout.

## A. torvalds/linux (Nouveau, master)

| File | Blob SHA (git) | Size | Local SHA-256 | Symbols / why |
|---|---|---|---|---|
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmm.h` | `4ec0a3a211690f732f3c47cf569e7361a08d113b` | 17389 | `10978e0c…3e26` | `nvkm_vmm_page/desc`, `NVKM_VMM_PAGE_Sxxx/SVxC/SVHx`, PFN `V/W/A/VRAM/ADDR`, PDE sparse, map/unmap/get/put API |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmtu102.c` | `4b30eab40bba049b2417327ea55a7d3fe9ba5519` | 2650 | `8aca422a…35d65` | `tu102_vmm` page table `{47,38,29,21,16,12}` + `tu102_vmm_flush` (0xb830a0/a4/b0, PAGE_ALL, 2s timeout) |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/tu102.c` | `7acff3642e20b8ae99e25ae451be050c1f230618` | 2112 | `4fc1ff72…6ccc1` | `tu102_mmu`: `dma_bits=47`, kind[16]+invalid 0x07, `nvkm_gsp_rm→r535_mmu_new` GSP division |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgv100.c` | `f0e21f63253afb3a7160dac94d72407b174926a2` | 2860 | `ddf0be77…3147d` | `gv100_vmm` same page table (Volta→Ampere continuity), `gv100_vmm_join` inst init |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgp100.c` | `ed15a4475181434a12114e118e5c03337f11c079` | 17080 | `75ebc733…5f3f29` | PTE encode `(addr>>4)\|type`, VALID b0, aper 2:1, VOL b3, PRIV b5, RO b6, atomic-dis b7, kind<<56, comptag<<36; descs `gp100_vmm_desc_16/12`; `gp100_vmm_valid`; sparse=VOL-only; LPT-invalid=PRIV-only; fault replay/cancel; invalidate_pdb 0x100cb8/cec |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgf100.c` | `5e857c02e9aab299fc699b82730e9f143107dd99` | 10885 | `84852368…89ea` | `gf100_vmm_aper` (VRAM 0/HOST 2/NCOH 3), PDE aper+addr>>4, `gf100_vmm_join_` inst base+limit, PTE `(addr>>8)\|type` (Fermi-line contrast, NOT Ampere path) |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmm.c` | `190c082b12c8cfae89c2d3136816accc0c92f298` | 51801 | `d8fb37f6…6b8e` | `nvkm_vmm_ctor` (limit=1<<bits, wrap checks), get/put/map/unmap/valid/choose, split/merge, ENOENT/EINVAL/ENOMEM paths, dtor+GSP vaspace_del |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/priv.h` → `mmu-priv.h` | `90efef8f0b54d926cdafb4aa6d7fc65f16c071c0` | 2064 | `9ce2d33a…9ee493` | `nvkm_mmu_func` (dma_bits/mem/vmm/kind/promote), `r535_mmu_new` decl, `nvkm_mmu_pt` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/mem.h` → `mmu-mem.h` | `234267e1b215bcec0bfd74a11b6192b3b4cd8b3c` | 858 | `2c223524…af4f667` | `gf100_mem_new/map` (mem target → VMA path) |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/mmu.h` → `nvkm-mmu.h` | `abcb0dbcde705416c6813b19f05c2e1a2d846544` | 5372 | `5a7712ae…01ada` | `nvkm_mmu` heaps/types (`VRAM/HOST/COMP/DISP`, `KIND/MAPPABLE/COHERENT/UNCACHED`), `*_mmu_new` decls |
| `drivers/gpu/drm/nouveau/include/nvkm/core/memory.h` → `nvkm-memory.h` | `fc0f38981391664cacfb107eeb733794b7a2f7ab` | 5517 | `dbf68548…631c76c` | `nvkm_memory_target` (INST/VRAM/HOST=coherent/NCOH=non-coherent) |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/fault.h` → `nvkm-fault.h` | `e40bbf378a8dda146f762a38d052545ced98d8c0` | 995 | `8617f6a5…15d5e0f` | `nvkm_fault_data` {addr,inst,time,engine,valid,gpc,hub,access,client,reason}, `tu102_fault_new` exists |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/vmm.c` → `r535-vmm.c` | `1ff42f1a8486236ff45d385fee6d0aa03191fde5` | 5383 | `2ef7ee19…3c0f047` | `r535_mmu_vaspace_new/del`, FERMI_VASPACE_A alloc, server-reserved 512MiB@4GB, COPY_SERVER_RESERVED_PDES, SET/UNSET_PAGE_DIRECTORY |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/nvrm/vmm.h` → `nvrm-vmm.h` | `f6ec04efd119396e59354ed06f5370b32800c133` | 5163 | `b80036a2…fd675` | `FERMI_VASPACE_A=0x90f1`, `NV_VASPACE_ALLOCATION_*` (GPU_NEW, EXT_OWNED b3, vaSize/Start/Limit, bigPageSize, vaBase), `SPLIT_VAS_*` (4GB/512MB), `NV90F1_COPY_SERVER_RESERVED_PDES` (GMMU_FMT_MAX_LEVELS=6), `NV0080_SET/UNSET_PAGE_DIRECTORY` (aperture VIDMEM 0/SYSMEM_COH 1/SYSMEM_NONCOH 2 + PRESERVE/ALL_CHANNELS/IGNORE_BUSY/EXTEND flags) |

## B. NVIDIA/open-gpu-kernel-modules (main)

| File | Blob SHA | Size | Local SHA-256 | Symbols / why |
|---|---|---|---|---|
| `src/common/sdk/nvidia/inc/mmu_fmt_types.h` | `13bb88f123ed8bbfcb177895e860e31897f373b6` | 5421 | `31fa4834…2c3a77` | `MMU_FMT_LEVEL` (virtAddrBitLo/Hi, entrySize, bPageTable, numSubLevels), `MMU_FMT_MAX_SUB_LEVELS=2` dual-PDE |
| `src/nvidia/src/kernel/gpu/mmu/mmu_fault_buffer.c` | `fe018f86f18f9f4f428cb2f62a5d87eb12be7493` | 6849 | `5d441eb6…3585` | Replayable fault-buffer RM lifecycle (construct/destruct/map/unmap/getMapAddrSpace, INVALID_CLASS/INVALID_OBJECT paths) |
| `src/common/sdk/nvidia/inc/nvos.h` (excerpt → `nvos-excerpt.h`) | `c061f0a2463a38ddbfc5f2ec550c138fef7f2548` | 157855 (excerpt 6823) | `af855b98…aa44` | `NVOS32_ATTR_LOCATION` (VIDMEM 0/PCI 1/ANY 3, 26:25), PHYSICALITY, COHERENCY, ALLOC VIRTUAL/SPARSE/ALIGNMENT, `NVOS33_FLAGS_ACCESS` (RW 0/RO 1/WO 2) |

## C. Local carry-forwards (P65/P77, unchanged)

- `GSP-NEXTSTEP-P65/UPSTREAM-EVIDENCE/nova-core-fb.rs` — FbRanges, WPR2 (ranges/registers grounding).
- `GSP-NEXTSTEP-P65/UPSTREAM-EVIDENCE/nova-core-gsp-fw.rs` — heap constants (client_alloc 96MiB/2048ch).
- `P77-M10-CHANNEL-EVIDENCE/` — channel-alloc/FIFO-lifetime/RPC-path (VASpace handles hVASpace carried in `r535_chan_alloc`: `hVASpace=vmm->rm...`).
- `SHADOW-P76-NEXT/VRAM-MMU-ARCHITECTURE.md` — prior survey honestly deferring PTE/aperture/fault (now unblocked by the Nouveau/OpenRM files above).

## D. Retrieval discipline

- Network: read-only GitHub contents API GETs only. No clone/submodule/write.
- `main`/`master` mutability mitigated by git blob SHA + byte size + local SHA-256 (P77 precedent).
- Excerpts stored verbatim (full small files; `vmm.c` 51KB stored full as `vmm.c`; `nvos.h` excerpted with line ranges + full-file blob pin).
