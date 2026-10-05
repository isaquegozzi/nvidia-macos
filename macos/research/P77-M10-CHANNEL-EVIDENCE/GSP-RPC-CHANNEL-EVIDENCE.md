# P77 — GSP-RPC Channel-Path Evidence

## Verdict: PROVEN (framing + alloc path + sequencing)

## 1. Two function codes — do not conflate

- `NV_VGPU_MSG_FUNCTION_ALLOC_CHANNEL_DMA = 6` (`nvrm/rpcfn.h` line ~22; Nova
  `nova-core-gsp-fw.rs:276` `AllocChannelDma = bindings::...` + `TryFrom` line 315).
  Proves the code EXISTS and round-trips. It is the legacy vGPU channel-DMA function.
- `NV_VGPU_MSG_FUNCTION_GSP_RM_ALLOC = 103` (`nvrm/rpcfn.h` line ~119).
  This is what `r535_chan_alloc` actually uses via `nvkm_gsp_rm_alloc_get/push`
  (`r535/alloc.c`). The channel payload is `NV_CHANNEL_ALLOC_PARAMS`, NOT a
  6-specific payload. P76's open question "How does AllocChannelDma fit in M9 framing?"
  is answered: **it doesn't — the GSP-RM channel path uses 103 + class `0xc56f`**;
  function 6 remains a valid enum value but is not on this path.
- `NV_VGPU_MSG_FUNCTION_FREE = 10`, `CONTINUATION_RECORD = 71`,
  `GSP_SET_SYSTEM_INFO = 72`, `SET_REGISTRY = 73`, `GSP_RM_CONTROL = 76`,
  `GET_GSP_STATIC_INFO = 65`, events `GSP_INIT_DONE = 0x1001`,
  `GSP_RUN_CPU_SEQUENCER = 0x1002` (`nvrm/rpcfn.h`, `nvrm/msgfn.h`).

## 2. Framing (M9-compatible, GSP-RM specialization)

- Element: `struct r535_gsp_msg { auth_tag[16], aad[16], checksum(u32), sequence(u32), elem_count(u32), pad, data[] }`
  (`r535/rpc.c` DOC + struct). OpenRM `GSP_MSG_QUEUE_ELEMENT{mctpHeader,nvdmHeader,checkSum,seqNum,payload[]}`
  (`message_queue_priv.h`) is the FW-side view of the same queue; `r535` adds the
  auth/aad front for its local copy. Checksum: XOR-fold over 8-byte lanes,
  `check(F)==c` / `fold(whole)==0` — identical algorithm to Nova `Cmdq::calculate_checksum`
  already modeled in M9 (`rpc_sim.hpp`); P77 does not re-model it.
- RPC: `struct nvfw_gsp_rpc { header_version=0x03000000, signature='CPRV', length, function,`
  `rpc_result, rpc_result_private, sequence, spare/cpuRmGfid, data[] }` (`r535/rpc.c`
  `r535_gsp_rpc_get`). `length = sizeof(*rpc)+payload_size`, 8-byte aligned.
- Limits: `GSP_MSG_MIN_SIZE=GSP_PAGE_SIZE`, `GSP_MSG_MAX_SIZE=16*PAGE` (`r535/rpc.c`);
  payload larger than `max_payload_size` splits: first element keeps `fn`, rest use
  `CONTINUATION_RECORD` (71). Queue full/sequence handling per `r535_gsp_msgq_wait`,
  `r535_gsp_cmdq_push` (wptr/elem math, `falcon_wr32(0xc00,0)` notify).
- Alloc RPC: `rpc_gsp_rm_alloc_v03_00{hClient,hParent,hObject,hClass,status,paramsSize,flags,reserved,params[]}`
  (`nvrm/alloc.h`); `hClass=0xc56f`, `paramsSize=sizeof(NV_CHANNEL_ALLOC_PARAMS)`,
  `params=params` inline. Push checks `rpc->status` via `r535_rpc_status_to_errno`
  (`0x55/0x66→EBUSY`, `0x51→ENOMEM`, else `EINVAL`).
- Control RPCs: `NVA06F BIND (0xa06f0104)` + `GPFIFO_SCHEDULE (0xa06f0103)` via
  `nvkm_gsp_rm_ctrl_get/wr` (same queue, `GSP_RM_CONTROL` family).
- Reply matching: by function (`r535_gsp_msg_recv(gsp,fn,len)` loops consuming
  non-matching recognized functions as `ERANGE`-equivalent, unknown as `EINVAL`-equivalent —
  same semantics as M9 `rpc_sim.hpp` `ErangeMismatch/EinvalFunction`; P77 model reuses
  the M9 vocabulary without duplicating the queue).

## 3. Order vs boot + other allocs

- `GSP-NEXTSTEP-P65/FIRST-RPC.md` + `GSP-BOOT-SEQUENCE.md` + Nova `gsp/boot.rs`, `gpu.rs`:
  ALL RM allocs come strictly after first boot: `SetSystemInfo` + `SetRegistry` →
  sequencer → `GspInitDone` (event `0x1001`) → `GetGspStaticInfo` (65) → RM allocs.
- `r535` channel alloc assumes `device->object` (GSP device), `vmm->rm` (VASpace),
  `fifo->rm.mthdbuf_size` (from `CE_GET_FAULT_METHOD_BUFFER_SIZE` ctrl) — all exist only
  post-boot. Model enforces `RpcReady` (initDone ∧ staticInfo) before any channel alloc.
- Success confirmation: `GSP_RM_ALLOC` reply with `status==0` + `FREE` later completing with
  `RECV`; BIND/SCHEDULE ctrls returning 0. Model returns these as the only success signals.
