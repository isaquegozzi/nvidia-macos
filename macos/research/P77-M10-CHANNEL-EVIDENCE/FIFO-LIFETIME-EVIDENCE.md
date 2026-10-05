# P77 — FIFO / Channel Lifetime Evidence

## Verdict: PROVEN

## 1. What constitutes a valid channel

- `torvalds/linux`, `chan.c`, `nvkm_chan_new_(func,runl,runq,cgrp,name,priv,devm,vmm,dmaobj,offset,length,userd,ouserd)` (~line 348-480):
  validates `runq < runl->func->runqs`, `inst/userd/ramfc` presence vs class requirements,
  `ramfc->devm/priv` gates; allocates instance block (`nvkm_gpuobj_new(...,0x1000,zero)`),
  joins VMM if `inst->vmm`, binds ctxdma if `ramfc->ctxdma`, maps USERD (bounds-checked
  `ouserd+userd_size < mem_size` or shared `fifo->userd.mem + id*userd_size`), clears USERD,
  then `ramfc->write(chan,offset,length,devm,priv)` — on GSP path this IS `r535_chan_ramfc_write`
  (alloc + BIND + SCHEDULE).
- GSP specialization `r535_chan = { .inst=&gf100_chan_inst, .userd=&gv100_chan_userd,`
  `.ramfc=&r535_chan_ramfc(.write=r535_chan_ramfc_write,.clear=r535_chan_ramfc_clear,.devm=0xfff,.priv=true),`
  `.start/stop=no-op, .doorbell_handle=r535_chan_doorbell_handle }` (`r535/fifo.c` ~line 231-237).
- Channel valid ⇔ {unique chid, runq in range, VASpace handle, engine known,
  inst/userd/mthdbuf addrs present, `GSP_RM_ALLOC` status 0, BIND ok, SCHEDULE `bEnable=1`}.

## 2. States (model states mirror these)

`Uninit → RpcReady → Allocated → Bound → Scheduled ⇄ (Blocked) → Errored (pinned) → Freed`.
- `RpcReady` requires `GspInitDone` + `GetGspStaticInfo` (see GSP-RPC doc).
- `Allocated/Bound/Scheduled` from `r535_chan_ramfc_write` steps 2/3/4.
- `Blocked`: `nvkm_chan_block` (atomic `blocked`, calls `stop`); `nvkm_runl_block` on GSP is
  a no-op, so per-channel block is the only host mechanism — model reflects this.
- `Errored`: `nvkm_chan_error(chan,preempt)` (`chan.c`): first error pins (`errored==1`),
  blocks + preempts + notifies `NVKM_CHAN_EVENT_ERRORED`; further ops refused.
  GSP trigger: `r535_fifo_rc_chid` ← `RC_TRIGGERED` event (`r535/fifo.c`, `r570/fifo.c`).
- `Freed`: `r535_chan_ramfc_clear`: `nvkm_gsp_rm_free(object)` + `dma_free_coherent(mthdbuf)` +
  `nvkm_cgrp_vctx_put(grctx)`. After free the chid is reusable; use-after-free refused.

## 3. USERD / RAMFC / GPFIFO memories

- USERD: per-channel notify/control area; 8 channels share one USERD page
  (`CHID_PER_USERD=8`, `userd_p/userd_i`); `userdMem` descriptor `{base,size,AS=2,cache=1}`.
- RAMFC: channel context area; `ramfcMem={base=inst_addr,size=0x200,AS=2,cache=1}` —
  aliases instance base, fixed 512 B. `ramfc->write` carries `(offset=GPFIFO offset,length)`.
- GPFIFO: pushbuffer queue; `gpFifoOffset` + `gpFifoEntries=len/8`; entries are 8-byte units.
  Backing memory is client-provided (VMM); `instanceMem` holds the channel instance.
- MTHDBUF: fault method buffer; `mthdbufMem={base, size=mthdbuf_size, AS=1, cache=0}`;
  `mthdbuf_size` from `NV2080_CTRL_CMD_CE_GET_FAULT_METHOD_BUFFER_SIZE` (`r535/fifo.c`
  `r535_fifo_ectx_size`); freed on channel clear.
- Sysmem vs VRAM: descriptors carry `addressSpace` (2 vs 1) + `cacheAttrib`; model preserves
  the proven values without claiming physical placement beyond them.

## 4. Bind / unbind / free rules

- Bind: `NVA06F_CTRL_CMD_BIND{engineType}` AFTER alloc, BEFORE schedule. Requires allocated.
- Schedule: `NVA06F_CTRL_CMD_GPFIFO_SCHEDULE{bEnable=1}` AFTER bind. `bEnable=0` only from scheduled.
- Unbind: host has no separate unbind RPC on this path; unbind ⇔ free
  (`nvkm_gsp_rm_free` + mthdbuf free + grctx put). Double-free refused.
- Reuse: chid reusable only after free; realloc re-runs full alloc→bind→schedule.
- GR ctx: `r535_gr_ctor` takes extra `vctx` ref held until channel destroy
  (`chan->rm.grctx`); `r535_flcn_ctor/bind` uses `GPU_PROMOTE_CTX{hClient,hObject,hChanClient,virtAddress,size,engineType,ChID}`.

## 5. Doorbell

- `r535` delegates to `gpu->fifo.chan.doorbell_handle`; reference-leaf
  `tu102_chan_doorbell_handle = (runl->id << 16) | chan->id`. Model composes the same
  way with 16-bit halves and validates chid; runl id passed through as u16.
