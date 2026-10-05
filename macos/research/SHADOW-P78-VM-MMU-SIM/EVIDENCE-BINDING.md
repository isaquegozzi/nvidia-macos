# P78 VM/MMU — Evidence Binding (model claim → upstream line)

- `VA_BITS=47, LIMIT=1<<47` → `tu102-mmu.c` (`tu102_mmu.dma_bits=47`); `vmm.c`
  (`limit = 1ULL<<bits`, wrap/range checks in ctor/get/pfn_map).
- Page table `{47,38,29,21,16,12}` + descs `gp100_vmm_desc_16/12` (LPT 5b/SPT 9b,
  PD0 8b/16B, PD1 9b/9b/2b, align 0x100/0x1000) → `vmmtu102.c`, `vmmgp100.c:403-421`.
- Leaf types Sxxx/SVxC/SVHx + `NVKM_VMM_PAGE_*` flags → `vmm.h` (`nvkm_vmm_page`).
- Dual-PDE (`MAX_SUB_LEVELS=2`, big+small parallel) → `mmu_fmt_types.h` + `vmm.h` LPT/SPT.
- PTE `(addr>>4)|type`, VALID b0, aper 2:1, VOL b3, PRIV b5, RO b6, atomic-dis b7,
  `next=(1<<shift)>>4` → `vmmgp100.c` (`gp100_vmm_pgt_pte/pfn`, `pfn_clear/unmap`,
  `gp100_vmm_valid` setting `type|=aper<<1|vol<<3|priv<<5|ro<<6|kind<<56`).
- Aperture VRAM 0/HOST 2/NCOH 3 → `vmmgf100.c:324-333` (`gf100_vmm_aper`); PDE same +
  VOL for HOST → `vmmgp100.c` (`gp100_vmm_pde`); PFN check `(data&(3<<1))!=0`.
- Sparse = VOL-only, LPT-invalid = PRIV-only, unmap = 0 → `vmmgp100.c`
  (`gp100_vmm_pgt_sparse/lpt_invalid`, `gf100_vmm_pgt_unmap`).
- Kind range `<16, !=0x07` → `tu102-mmu.c` (`kind[16]`, `invalid=0x07`).
- Comptag `<<36`, 1/64 KiB → `vmmgp100.c` (`comptag_nr/comptagline_base/incr`).
- Targets VRAM/HOST-coherent/NCOH → `nvkm-memory.h` (`nvkm_memory_target`).
- RM apertures VIDMEM 0/SYSMEM_COH 1/SYSMEM_NONCOH 2 + flags → `nvrm-vmm.h`
  (`NV0080_SET_PAGE_DIRECTORY_FLAGS_APERTURE_*`); alloc LOCATION/COHERENCY +
  ACCESS RW/RO/WO → `nvos-excerpt.h` (blob `c061f0a2…`).
- GSP division `gsp_rm→r535_mmu_new` → `tu102-mmu.c`; VASpace FERMI_VASPACE_A 0x90f1,
  GPU_NEW, EXT_OWNED b3, SPLIT_VAS 4G/512M, COPY_PDES 0x90f10106 (levels≤6),
  SET/UNSET_PD 0x801813/14 → `r535-vmm.c` + `nvrm-vmm.h`.
- Lifecycle get/put/map/unmap + ENOENT/EINVAL/ENOMEM + split/merge + dtor order
  (vaspace_del first, WARN_ON non-empty) → `vmm.c` (ctor/dtor/get/put/map/unmap/pfn).
- Fault fields + replay/cancel/invalidate + flush tokens (0xb830a0/a4/b0 2s;
  0x100cb8/cec; PAGE_ALL/HUB_ONLY) → `nvkm-fault.h`, `vmmgp100.c` (`mthd/flush/
  invalidate_pdb`), `vmmtu102.c` (`tu102_vmm_flush`); RM buffer lifecycle →
  `mmu_fault_buffer.c`.
