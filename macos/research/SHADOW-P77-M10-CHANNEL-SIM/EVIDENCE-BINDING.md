# P77 M10 — Evidence Binding (model claim → upstream line)

- `kClassAmpereGpfifoA=0xc56f` → `nvif/class.h:89` (`AMPERE_CHANNEL_GPFIFO_A 0x0000c56f`);
  used in `ga102.c:40` (`.chan={{0,0,AMPERE_CHANNEL_GPFIFO_A},&ga100_chan}`).
- GSP-vs-direct division → `ga102.c:47-48` (`if (nvkm_gsp_rm(...)) return r535_fifo_new`).
- `CHID_PER_USERD=8`, `userd_p=chid/8`, `userd_i=chid%8` → `r535/fifo.c:72,82-83`
  (+ `r570/fifo.c:17,27-28` identical).
- `gpFifoEntries=len/8` → `r535/fifo.c` (`args->gpFifoEntries = gpfifo_length / 8`).
- Flags template (PHYSICAL/VPR_FALSE/.../RUNQUEUE(runq)/PRIVILEGED/USERD_INDEX/PAGE/...) →
  `r535/fifo.c` `r535_chan_alloc` flag block + `nvrm/fifo.h:209-270` (`NVOS04_FLAGS_*`).
- `hVASpace/engineType` passthrough → `r535/fifo.c` (`hVASpace=vmm->rm...`, `engineType=...`).
- `instanceMem/userdMem AS=2/cache=1`, `ramfcMem={inst,0x200,2,1}`, `mthdbufMem AS=1/cache=0` →
  `r535/fifo.c` `r535_chan_alloc` mem block (+ `r570` identical).
- `internalFlags` USER/ADMIN + NONE/NONE → `r570/fifo.c` explicit + `nvrm/fifo.h`
  (`NV_KERNELCHANNEL_ALLOC_INTERNALFLAGS_*`).
- Alloc envelope `{hClient,hParent,hObject,hClass,status,paramsSize}` →
  `nvrm/alloc.h` (`rpc_gsp_rm_alloc_v03_00`) + `r535/alloc.c` get/push/done/free.
- Status classes `0x55/0x66→BUSY, 0x51→NOMEM, else EINVAL` → `r535/rpc.c:128-140`.
- `FREE=10` + `{hRoot,hObjectParent=0,hObjectOld}` → `r535/alloc.c` (`r535_gsp_rpc_rm_free`).
- Post-alloc `BIND(0xa06f0104)+SCHEDULE(0xa06f0103,bEnable=1)` →
  `r535/fifo.c` `r535_chan_ramfc_write` + `nvrm/fifo.h` (`NVA06F_CTRL_*_PARAMS`).
- Engine table `{GR0,COPY0-7,NVDEC0-7,NVENC0-3,NVJPG0-7,SW,SEC2,OFA}` + unknown→EINVAL →
  `r535/fifo.c` `r535_fifo_xlat_rm_engine_type` (+ `r570` extends COPY8-19).
- Doorbell `(runl<<16)|chid` → `tu102.c:35-38` (`tu102_chan_doorbell_handle`); `r535`
  delegates (`r535_chan_doorbell_handle`, `:42-46`).
- Runlist no-op block/allow on GSP → `r535/fifo.c` (`r535_runl` empty funcs).
- Direct corroboration `runqs=2,size=16,commit 0x002b00/04/08,pending 0x002b0c&0x8000` →
  `tu102.c` (`tu102_runl/commit/pending`); NOT executed on GSP path (documented only).
- Channel hierarchy/insert/remove/block/error/preempt + inst `0x1000` align + USERD bounds →
  `chan.c` (`nvkm_chan_new_/insert/remove/block/error/preempt`, `:409`).
- RC `→chan_error` pinning → `r535/fifo.c` (`r535_fifo_rc_chid`) + `chan.c` (`nvkm_chan_error`).
- Free clears (`rm_free` + mthdbuf free + grctx put) → `r535/fifo.c` (`r535_chan_ramfc_clear`).
- Function codes `6/10/65/71/72/76/103`, events `0x1001/0x1002` →
  `nvrm/rpcfn.h` + `nvrm/msgfn.h` (+ Nova `fw.rs:274-282,309-360` corroboration).
- Element/RPC framing + continuation split + 1P/16P limits →
  `r535/rpc.c` DOC + structs + `GSP_MSG_MIN/MAX_SIZE` (+ OpenRM `message_queue_priv.h`
  FW-side `_checkSum32`/element view; M9 algorithm already proven).
- Sequencing (allocs after `GspInitDone`+`GetGspStaticInfo`) →
  `GSP-NEXTSTEP-P65/FIRST-RPC.md` + `GSP-BOOT-SEQUENCE.md` + Nova `gsp/boot.rs`/`gpu.rs`.
- Heap values/align/formulas → OpenRM `gsp_fw_heap.h` (table in CONSTANTS doc) +
  Nova `fw.rs:66,83-98` (`1<<20`, `client_alloc_size`, `management_overhead`).
- `mthdbuf_size` via `CE_GET_FAULT_METHOD_BUFFER_SIZE` → `r535/fifo.c` (`r535_fifo_ectx_size`).
- `NV_CHANNEL_ALLOC_PARAMS` field order → `nvrm/fifo.h:160-207`.
