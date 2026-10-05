# P77 M10 — Source Manifest (pinned upstream evidence)

Author: Muse, 2026-09-09 -0300. Read-only research; zero live HW; candidate 1.7.3 frozen.
Retrieval date: 2026-09-09. Method: GitHub REST contents API (`ref=master` / `ref=main`),
base64 payload stored under `upstream-excerpts/`. No blog/post used as layout authority.

## A. torvalds/linux (Nova + Nouveau) — ref `master`

| File (repo path) | Blob SHA (git) | Size | Local excerpt SHA-256 | Commit touching file (via commits API) | Key symbols |
|---|---|---|---|---|---|
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/fifo.c` | `76ee938efea3ef3f0735787baa5fb6485aab1d85` | 17800 | `b0aff084…fcc55f` | `bf4afc53b77aeaa48b5409da5c8da6bb4eff7f43` (2026-02-22, Linus alloc_obj conversion; file content verified by blob) | `r535_chan_alloc`, `r535_chan_ramfc_write/clear`, `r535_runl` (block/allow no-op), `r535_fifo_new`, `r535_fifo_xlat_rm_engine_type`, `CHID_PER_USERD=8`, `NVA06F_CTRL_BIND/GPFIFO_SCHEDULE` sequence |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/alloc.c` | `46e3a29f2ad7f0c6dc80cb4f1a1e21e22437aae2` | 3535 | `df817dcd…4767f` | `0c6aa94f991b00b6799be66880918b68344eb92b` | `r535_gsp_rpc_rm_alloc_get/push/done/free`, `rpc_gsp_rm_alloc_v03_00` (`hClient/hParent/hObject/hClass/status/paramsSize`), `NV_VGPU_MSG_FUNCTION_GSP_RM_ALLOC`, `NV_VGPU_MSG_FUNCTION_FREE` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/rpc.c` | `3ca3de8f434082d881675e49c6c08b2cdefdc60b` | 17822 | `6ab6fa46…ee2ddd` | `90caca3b7264cc3e92e347b2004fff4e386fc26e` | `r535_gsp_msg` (auth_tag/aad/checksum/sequence/elem_count), `nvfw_gsp_rpc` (header_version/signature/length/function/rpc_result/sequence), `GSP_MSG_MIN/MAX_SIZE`, continuation-record split, `r535_gsp_rpc_get/push` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/nvrm/fifo.h` | `325fdd8b60907d4bf7752b07496f5cad690f1fe2` | 16592 | `9ed8b895…1150` | `0c6aa94f991b00b6799be66880918b68344eb92b` (excerpt header: OpenRM `535.113.01`) | `NV_CHANNEL_ALLOC_PARAMS` (=`NV_CHANNELGPFIFO_ALLOCATION_PARAMETERS`: `gpFifoOffset/gpFifoEntries/flags/hVASpace/engineType/instanceMem/userdMem/ramfcMem/mthdbufMem/internalFlags`), `NVOS04_FLAGS_*`, `NVA06F_CTRL_BIND/GPFIFO_SCHEDULE_PARAMS`, `NV2080_CTRL_GPU_PROMOTE_CTX_PARAMS`, `ENGINE_INFO_TYPE_RUNLIST_*`, `NV_MEMORY_DESC_PARAMS` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/nvrm/alloc.h` | `cbc7e611fbda77676f380e6de2947d99f519b756` | 839 | `062ddea5…b76` | `0c6aa94f991b00b6799be66880918b68344eb92b` | `rpc_gsp_rm_alloc_v03_00`, `rpc_free_v03_00`/`NVOS00_PARAMETERS_v03_00` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/nvrm/rpcfn.h` | `2a037acc6b1e4c193f5af1f466244b43aa7be432` | 11259 | `844ba52f…29b39` | `0c6aa94f991b00b6799be66880918b68344eb92b` | `NV_VGPU_MSG_FUNCTION_*`: `ALLOC_CHANNEL_DMA=6`, `FREE=10`, `CONTINUATION_RECORD=71`, `GSP_SET_SYSTEM_INFO=72`, `GSP_RM_CONTROL=76`, `GET_GSP_STATIC_INFO=65`, `GSP_RM_ALLOC=103` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r535/nvrm/msgfn.h` | `642c13aec32544b53dad76ba61d1a98689d5f075` | 2391 | `3b569a69…1137` | rate-limited at fetch (blob pinned; content verified) | `NV_VGPU_MSG_EVENT_*`: `GSP_INIT_DONE=0x1001`, `GSP_RUN_CPU_SEQUENCER=0x1002`, `RC_TRIGGERED`, `MMU_FAULT_QUEUED` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/rm/r570/fifo.c` | `79132805cfcf6ed6e93339de2935b8c9fcebaa43` | 7596 | `78f77443…91dfe` | rate-limited (blob pinned) | `r570_chan_alloc` (same template + `internalFlags` privilege/admin, `mthdbufMem` addrSpace 1/cache 0), `r570_fifo_xlat` (adds COPY8-19, SEC2, OFA0-1, SW), `rsvd_chids=1` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/ga102.c` | `18a0b1f4eab76a435a987942afaff1043f367f2c` | 1949 | `69bce749…0bd64` | rate-limited (blob pinned) | `ga102_fifo` (`.chan={{0,0,AMPERE_CHANNEL_GPFIFO_A},&ga100_chan}`), `ga102_fifo_new`: `if (nvkm_gsp_rm()) return r535_fifo_new(...);` — GSP/RM vs direct division |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/tu102.c` | `c5a03298e88c1fded07d325d837561fb744f73a8` | 7950 | `5b512476…491b` | rate-limited (blob pinned) | `tu102_chan_doorbell_handle=(runl->id<<16)\|chan->id`, `tu102_runl` (`runqs=2,size=16,commit` writes `0x002b00/04/08`, `pending` `0x002b0c&0x8000`), `tu102_fifo` GSP branch to `r535_fifo_new` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/ga100.c` | `6848a56f20c076770cc3624499107f637733b68b` | 16609 | `b505d7e1…12b` | rate-limited (blob pinned) | `ga100_chan` (inst `gf100_chan_inst`, userd `gv100_chan_userd`), `ga100_runl/runq/engn/cgrp`, `ga100_fifo` GSP branch, engine-info/RC paths |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/runl.h` | `19e6772ead11fc78627eae42640dec3d55ece407` | 4732 | `3269cfac…aa48` | rate-limited (blob pinned) | `nvkm_runl` (`id/addr/chan/doorbell/cgid/chid/cgrp_nr/chan_nr/changed/mem/offset`), `nvkm_engn`, `nvkm_runl_func` (`runqs/size/update/insert_cgrp/insert_chan/commit/wait/pending/block/allow/preempt`) |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/chan.h` | `445db5dfd1e42c2e92a0817f06eeb05cc45db698` | 2526 | `d430ec1f…cd5c` | rate-limited (blob pinned) | `nvkm_chan_func` (`inst/userd/ramfc/bind/unbind/start/stop/preempt/doorbell_handle`), `nvkm_cctx` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/chan.c` | `418a8918bcb8fd7395f9ef7f0b0c22ea376ee308` | 12038 | `3bf4da22…90df6` | rate-limited (blob pinned) | `nvkm_chan_new_` validation, `nvkm_chan_insert/remove/block/error/preempt`, inst `0x1000`-align alloc, userd bounds, `ramfc->write` dispatch |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/runl.c` | `2092521053ef213db649eb6d05b54fbdcd86c5cf` | 10292 | `4beb94d4…af3` | rate-limited (blob pinned) | `nvkm_runl_new/get/add/update_locked`, chid/cgid alloc, `changed` flag, `preempt_wait` |
| `drivers/gpu/drm/nouveau/include/nvif/class.h` | (rate-limited; local excerpt SHA-256 `accf5976…95478d`) | 18931 | `accf5976…95478d` | rate-limited (content: `AMPERE_CHANNEL_GPFIFO_A 0x0000c56f` lines 89-90) | `AMPERE_CHANNEL_GPFIFO_A=0x0000c56f`, `KEPLER_CHANNEL_GROUP_A=0x0000a06c`, generation chain `NV50→G82→FERMI→KEPLER→MAXWELL→PASCAL→VOLTA→TURING→AMPERE→HOPPER→BLACKWELL` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/priv.h` (stored as `nvkm-fifo-priv.h`) | `fff1428ef267ba1dc655976dbf0f07a9c5b84b76` | 9196 | `91a0e918…7310` | rate-limited (blob pinned) | `r535_fifo_new` decl, `nvkm_fifo_func` (`cgrp/chan` classes + `runl/runq/engn`), per-generation `chan_inst/userd/ramfc` externs, `ga100_chan`, `tu102_chan_doorbell_handle` |

Origin URLs: `https://github.com/torvalds/linux/blob/master/<path>` and
`https://raw.githubusercontent.com/torvalds/linux/master/<path>`.
Directory proof (no `fifo/` under Nova): `GET /repos/torvalds/linux/contents/drivers/gpu/nova-core`
lists `driver/falcon/fb/firmware/fsp/gpu/gsp/mctp/...` with NO `fifo` entry (2026-09-09).

## B. NVIDIA/open-gpu-kernel-modules — ref `main`

| File | Blob SHA | Size | Local SHA-256 | Commit | Key symbols |
|---|---|---|---|---|---|
| `src/nvidia/inc/kernel/gpu/gsp/gsp_fw_heap.h` | `35373b438ceeb4bb64f032667b0df817befd285f` | 6658 | `f0b46d90…058954` | `57130a2702d565be81200ea2e114abcb0455e8bb` (`610.43.02`, 2026-05-26) for the file path (via commits API before rate limit) | `GSP_FW_HEAP_PARAM_OS_SIZE_LIBOS3_BAREMETAL=(22<<20)`, `BASE_RM_SIZE_TU10X=(8<<20)`, `SIZE_PER_GB=(96<<10)`, `CLIENT_ALLOC_SIZE=((48<<10)*2048)`, `VGPU_DEFAULT=(581<<20)`, `OVERRIDE_LIBOS3_BAREMETAL_{MIN,MAX}_MB={88,280}` |
| `src/nvidia/inc/kernel/gpu/gsp/message_queue_priv.h` | `2a55ce4be8c8b2ef722bf4ad165e681aedb0b1bd` | 7184 | `1ccdf688…ce7a2` | blob-pinned (rate-limited for commit) | `GSP_MSG_QUEUE_ELEMENT` (`mctpHeader/nvdmHeader/checkSum/seqNum/payload[]`), `_checkSum32` XOR-fold, `gspMsgQueueGetRpcMessageHeader/Length`, CC encrypt tag |
| `src/nvidia/inc/kernel/gpu/gsp/message_queue.h` | `91193dd43b78b669fbf81f5dc1e766326af42f4e` | 1988 | `218fb1f9…348d` | blob-pinned | `RPC_TASK_RM_QUEUE_IDX=0`, `RPC_QUEUE_COUNT=1` |
| `src/common/sdk/nvidia/inc/class/cla06f.h` | `336cab57f70137e75dd16fa2aff1e76c8b9457a7` | 15198 | `8dd36f72…fa51` | blob-pinned | `KEPLER_CHANNEL_GPFIFO_A=0x0000A06F`, GPFIFO Put/Get/Reference/TopLevelGet, `NVA06F_DMA_*` pushbuffer method formats |
| `src/common/sdk/nvidia/inc/class/clc56f.h` | `bc8b675c5dff20d410537e2c5e6274562c55c7b8` | 25985 | `8b7a6d6d…3f658e` | blob-pinned | Ampere `C56F` channel GPFIFO class companion to `0x0000c56f` (register/method companion; alloc params live in `nvrm/fifo.h`, not here) |
| `src/common/sdk/nvidia/inc/nvos.h` | `c061f0a2463a38ddbfc5f2ec550c138fef7f2548` | 157855 | `c218e5e2…16d` | blob-pinned | `NV_CHANNEL_GROUP_ALLOCATION_PARAMETERS`, `NV_SWRUNLIST_ALLOCATION_PARAMS` (`engineId/maxTSGs/qosIntrEnableMask`, double-buffer sizing `2*maxTSGs*maxChannelPerTSG*sizeof(RunlistEntry)`), `NV_ME/NVDEC_ALLOCATION_PARAMETERS` |

Origin: `https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/<path>`.

## C. Local Nova snapshots (already vendored, P65/P76)

- `GSP-NEXTSTEP-P65/UPSTREAM-EVIDENCE/nova-core-gsp-fw.rs` — `GspFwHeapParams::base_rm_size/client_alloc_size/management_overhead`, `GSP_HEAP_ALIGNMENT=1<<20` (line 66), `MsgFunction::{AllocChannelDma=6,...}` (line 274-282), `TryFrom` (309-360). Commits recorded in `GSP-NEXTSTEP-P65/PRIMARY-SOURCES.md`.
- `GSP-NEXTSTEP-P65/UPSTREAM-EVIDENCE/nova-core-gsp-cmdq.rs` — `GspMsgElement`/`Cmdq::calculate_checksum`/`send_command_no_wait`/`receive_msg` (M9 model basis).
- `GSP-NEXTSTEP-P65/UPSTREAM-EVIDENCE/nova-core-gsp-fw-commands.rs`, `nova-core-gsp-commands.rs`, `nova-core-gsp-sequencer.rs` — sequencer opcodes, `GspInitDone`/`GetGspStaticInfo` ordering (`FIRST-RPC.md`, `GSP-BOOT-SEQUENCE.md`).
- `GSP-NEXTSTEP-P65/PRIMARY-SOURCES.md` + `UPSTREAM-EVIDENCE-SHA256.txt` — per-file origins/commits/SHAs (unchanged).

## D. Retrieval discipline

- Network used ONLY for read-only upstream `GET` (contents/commits APIs). No clone, no write, no submodule.
- Every external layout claim in the following docs cites `repository + file + blob + symbol + line-context + URL`.
- `main`/`master` mutability mitigated by blob SHA + size + local SHA-256 + commit where the API returned one before rate limiting; where the commits API was rate-limited the blob SHA still pins the exact bytes analyzed.
- Small excerpts stored (`upstream-excerpts/`); analysis prefers semantics + hashes over full copies.
