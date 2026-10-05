# P77 — Channel-Alloc Evidence (GA106 / Ampere GSP-RM path)

## Verdict: PROVEN (scoped)

Channel allocation on GA106-with-GSP goes through **GSP-RM**, not direct PFIFO.
Proven payload: `NV_CHANNELGPFIFO_ALLOCATION_PARAMETERS` (=`NV_CHANNEL_ALLOC_PARAMS`)
carried by `GSP_RM_ALLOC` (103), followed by `NVA06F` BIND + GPFIFO_SCHEDULE.
Legacy `ALLOC_CHANNEL_DMA` (=6) exists as a vGPU function code but is NOT the
GSP-RM channel path used by `r535_chan_alloc`.

## 1. Class + entry point

- Repo `torvalds/linux`, `drivers/gpu/drm/nouveau/nvkm/engine/fifo/ga102.c`
  (blob `18a0b1f4…`, 1949 B):
  `ga102_fifo.chan = {{0,0,AMPERE_CHANNEL_GPFIFO_A}, &ga100_chan}` (~line 40).
- Same file `ga102_fifo_new` (~line 47-48):
  `if (nvkm_gsp_rm(device->gsp)) return r535_fifo_new(&ga102_fifo,...);`
  else `nvkm_fifo_new_(...)`. This is the exact GSP-vs-direct division.
- Repo `torvalds/linux`, `drivers/gpu/drm/nouveau/include/nvif/class.h`:
  `AMPERE_CHANNEL_GPFIFO_A ... 0x0000c56f` (line 89). GA106 (Ampere) uses `0xc56f`.
- Repo `torvalds/linux`, `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/fifo.c`
  (blob `76ee938e…`, 17800 B): `r535_fifo_new` sets
  `rm->chan.user.oclass = gpu->fifo.chan.class; rm->chan.func = &r535_chan;`
  i.e. the `0xc56f` class flows into `r535_chan_alloc` via
  `nvkm_gsp_rm_alloc_get(&device->object, handle, fifo->func->chan.user.oclass, ...)`.

## 2. Alloc payload layout (authoritative)

- Repo `torvalds/linux`, `.../r535/nvrm/fifo.h` (blob `325fdd8b…`, excerpt OpenRM `535.113.01`),
  `NV_CHANNEL_ALLOC_PARAMS` (~line 160-207), aliased line 207:
  `typedef NV_CHANNEL_ALLOC_PARAMS NV_CHANNELGPFIFO_ALLOCATION_PARAMETERS;`
  Fields in order:
  `hObjectError, hObjectBuffer(unused), gpFifoOffset(u64,8), gpFifoEntries(u32), flags(u32),`
  `hContextShare, hVASpace, hUserdMemory[8], userdOffset[8](u64,8), engineType(u32), cid(u32),`
  `subDeviceId, hObjectEccError, instanceMem/userdMem/ramfcMem/mthdbufMem (NV_MEMORY_DESC_PARAMS,8),`
  `hPhysChannelGroup(reserved), internalFlags(reserved), errorNotifierMem/eccErrorNotifierMem(reserved),`
  `ProcessID/SubProcessID(reserved), encryptIv/nonce(reserved)`.
- `NV_MEMORY_DESC_PARAMS`: `{base(u64,8), size(u64,8), addressSpace(u32), cacheAttrib(u32)}`.
- `NVOS04_FLAGS_*` (~line 209-270): `CHANNEL_TYPE[1:0]` (PHYSICAL=0),
  `VPR[2]`, `CHANNEL_SKIP_MAP_REFCOUNTING[3]`, `GROUP_CHANNEL_RUNQUEUE[4]`,
  `PRIVILEGED_CHANNEL[5]`, `DELAY_CHANNEL_SCHEDULING[6]`, `CHANNEL_DENY_PHYSICAL_MODE_CE[7]`,
  `CHANNEL_USERD_INDEX_VALUE[10:8]`, `CHANNEL_USERD_INDEX_FIXED[11]`,
  `CHANNEL_USERD_INDEX_PAGE_VALUE[20:12]`, `CHANNEL_USERD_INDEX_PAGE_FIXED[21]`,
  `CHANNEL_DENY_AUTH_LEVEL_PRIV[22]`, `CHANNEL_SKIP_SCRUBBER[23]`, `CHANNEL_CLIENT_MAP_FIFO[24]`,
  `SET_EVICT_LAST_CE_PREFETCH_CHANNEL[25]`, `CHANNEL_VGPU_PLUGIN_CONTEXT[26]`,
  `CHANNEL_PBDMA_ACQUIRE_TIMEOUT[27]`, `GROUP_CHANNEL_THREAD[29:28]`, `MAP_CHANNEL[30]`,
  `SKIP_CTXBUFFER_ALLOC[31]`.
- `NV_KERNELCHANNEL_ALLOC_INTERNALFLAGS_*`: `PRIVILEGE[1:0]` (USER=0/ADMIN=1/KERNEL=2),
  `ERROR_NOTIFIER_TYPE[3:2]`, `ECC_ERROR_NOTIFIER_TYPE[5:4]` (NONE=1).

## 3. How `r535_chan_alloc` fills it (exact template — model basis)

File `r535/fifo.c`, `r535_chan_alloc(device,handle,nv2080_engine_type,runq,priv,chid,
inst_addr,userd_addr,mthdbuf_addr,vmm,gpfifo_offset,gpfifo_length,chan)` (~line 72-150):

- `args = nvkm_gsp_rm_alloc_get(&device->object, handle, <0xc56f class>, sizeof(*args), chan)`.
- `gpFifoOffset = gpfifo_offset; gpFifoEntries = gpfifo_length / 8;` — entries are 8-byte units.
- `flags = PHYSICAL | VPR_FALSE | SKIP_MAP_REFCOUNTING_FALSE | RUNQUEUE(runq)`
  `| PRIVILEGED(priv?TRUE:FALSE) | DELAY_FALSE | DENY_PHYS_CE_FALSE`
  `| USERD_INDEX_VALUE(chid%8) | USERD_INDEX_FIXED_FALSE`
  `| USERD_PAGE_VALUE(chid/8) | USERD_PAGE_FIXED_TRUE`
  `| DENY_AUTH_PRIV_FALSE | SKIP_SCRUBBER_FALSE | CLIENT_MAP_FIFO_FALSE`
  `| EVICT_LAST_CE_FALSE | VGPU_PLUGIN_FALSE | PBDMA_TIMEOUT_FALSE`
  `| THREAD_DEFAULT | MAP_FALSE | SKIP_CTXBUF_FALSE`.
- `CHID_PER_USERD=8; userd_p=chid/8; userd_i=chid%8;` (~line 72,82-83). Corroborated by
  `r570/fifo.c` lines 17,27-28 (identical).
- `hVASpace = vmm->rm.object.handle; engineType = nv2080_engine_type;` (opaque passthrough).
- `instanceMem={base=inst_addr,size=<inst_size>,addrSpace=2,cache=1};`
  `userdMem={base=userd_addr,size=<userd_size>,addrSpace=2,cache=1};`
  `ramfcMem={base=inst_addr,size=0x200,addrSpace=2,cache=1};` — RAMFC aliases instance base,
  fixed `0x200` size.
- `mthdbufMem={base=mthdbuf_addr,size=fifo->rm.mthdbuf_size,addrSpace=1,cache=0};` (r570 identical).
- `internalFlags = PRIVILEGE(USER|ADMIN) | ERROR_NOTIFIER(NONE) | ECC(NONE)` (r570 lines: explicit).
- Returns `nvkm_gsp_rm_alloc_wr(chan, args)` — i.e. `GSP_RM_ALLOC` push + status check.

## 4. Post-alloc sequence (same file, `r535_chan_ramfc_write` ~line 155-228)

1. `dma_alloc_coherent(mthdbuf_size)` for method buffer.
2. `rmapi->fifo->chan.alloc(...)` (above).
3. `NVA06F_CTRL_CMD_BIND` (`0xa06f0104`, `NVA06F_CTRL_BIND_PARAMS{engineType}`) via
   `nvkm_gsp_rm_ctrl_get/wr`.
4. `NVA06F_CTRL_CMD_GPFIFO_SCHEDULE` (`0xa06f0103`,
   `NVA06F_CTRL_GPFIFO_SCHEDULE_PARAMS{bEnable=1}`) via ctrl get/wr.
- Structs pinned in `nvrm/fifo.h`: `NVA06F_CTRL_BIND_PARAMS`, `NVA06F_CTRL_GPFIFO_SCHEDULE_PARAMS`.
- Failure of any step returns the error; no partial-channel success is claimed.

## 5. Identifiers / ownership / errors

- Handles: `rpc_gsp_rm_alloc_v03_00{hClient,hParent,hObject,hClass,status,paramsSize,flags,reserved,params[]}`
  (`nvrm/alloc.h`). `hClass=0xc56f`, `paramsSize=sizeof(NV_CHANNEL_ALLOC_PARAMS)`.
  Status `0` = success; nonzero maps via `r535_rpc_status_to_errno`:
  `0x55/0x66→-EBUSY`, `0x51→-ENOMEM`, else `-EINVAL` (`r535/rpc.c` ~line 128-140).
- `nvkm_gsp_rm_alloc_wr` fails on `rpc->status!=0` (logs `RM_ALLOC: 0x%x` unless EAGAIN/EBUSY).
- Free: `NV_VGPU_MSG_FUNCTION_FREE` (=10) with `{hRoot=client,hObjectParent=0,hObjectOld}` (`r535/alloc.c`).
- Engine selection: `r535_fifo_xlat_rm_engine_type` table (GR0, COPY0-7, NVDEC0-7, NVENC0-3,
  NVJPG0-7, SW, SEC2, OFA) → `(NVKM type, NV2080_ENGINE_TYPE)`. Unknown RM engine → `-EINVAL`.
  r570 adds COPY8-19 + same set; both reject unknown with `-EINVAL`.
- Doorbell: `r535_chan_doorbell_handle` delegates to `gpu->fifo.chan.doorbell_handle`;
  non-GSP reference `tu102_chan_doorbell_handle=(runl->id<<16)|chan->id` (`tu102.c` ~line 35-38;
  `ga100.c` ~line 37 analogous).
- `userd` sharing: 8 channels per USERD page (`CHID_PER_USERD`), index/page derived from chid —
  duplicate chid would alias the same USERD slot (model forbids double-alloc).

## 6. What is NOT claimed

- Numeric `NV2080_ENGINE_TYPE_*` encodings (rate-limited `cl2080.h`; model treats engine as
  validated opaque token, no invented values).
- `inst_size`/`userd_size` byte counts (`gf100_chan_inst`/`gv100_chan_userd` externs in
  `nvkm-fifo-priv.h`; sizes live in other TUs not pinned here — model takes them as
  positive opaque parameters with 4K-alignment on inst per `chan.c:409`).
- GPFIFO entry byte format / pushbuffer semantics (`cla06f.h` documents `NVA06F_DMA_*`
  method formats for the channel's pushbuffer, but entry emission is command-submission
  scope — explicitly excluded from this M10 model per P77 §5/§10).
