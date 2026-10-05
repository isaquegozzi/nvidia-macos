# P77 — Heap Constants + Layouts (values resolved)

## Verdict: PROVEN (values from OpenRM `main` + Nova alignment)

Source: `NVIDIA/open-gpu-kernel-modules`, `src/nvidia/inc/kernel/gpu/gsp/gsp_fw_heap.h`
(blob `35373b43…`, 6658 B, commit `57130a27…` `610.43.02`). Alignment from Nova
`drivers/gpu/nova-core/gsp/fw.rs:66` (`GSP_HEAP_ALIGNMENT = 1<<20`, local snapshot
in `GSP-NEXTSTEP-P65/UPSTREAM-EVIDENCE/nova-core-gsp-fw.rs`).

| Constant | Value | Notes |
|---|---|---|
| `GSP_HEAP_ALIGNMENT` | `1 << 20` (1 MiB) | Nova `fw.rs:66`. All `client_alloc`/`management_overhead` aligned up to this. |
| `GSP_FW_HEAP_PARAM_BASE_RM_SIZE_TU10X` | `8 << 20` (8 MiB) | "Turing thru Ada" — includes GA106 Ampere. Nova `base_rm_size()` selects this for Turing/Ampere/Ada. |
| `GSP_FW_HEAP_PARAM_BASE_RM_SIZE_GH100` | `14 << 20` (14 MiB) | Hopper+ only; NOT used for GA106. |
| `GSP_FW_HEAP_PARAM_SIZE_PER_GB` | `96 << 10` (96 KiB) | OpenRM name. Nova `fw.rs:94` references `GSP_FW_HEAP_PARAM_SIZE_PER_GB_FB` — same semantic (per-GB-FB overhead); naming drift noted, value taken from OpenRM. `management_overhead(fb)=ceil(fb/1G)*96KiB` aligned to 1 MiB. |
| `GSP_FW_HEAP_PARAM_CLIENT_ALLOC_SIZE` | `(48 << 10) * 2048` = `100663296` (96 MiB) | "Support 2048 channels"; per-channel ~48 KiB. Nova `client_alloc_size()=align_up(VALUE,1MiB)` = 96 MiB (already aligned). GA106 max 2048 channels (Turing 4096 noted but heap sized to 2048). |
| `GSP_FW_HEAP_PARAM_OS_SIZE_LIBOS2` | `0` | Turing/GA100 path; not GA106. |
| `GSP_FW_HEAP_PARAM_OS_SIZE_LIBOS3_BAREMETAL` | `22 << 20` (22 MiB) | GA102+ baremetal — GA106 uses LIBOS3 (`LibosParams::from_chipset`: `chipset>=GA102 → LIBOS3`). |
| `GSP_FW_HEAP_PARAM_OS_SIZE_LIBOS3_VGPU` | `36 << 20` | vGPU only; documented, not modeled. |
| `GSP_FW_HEAP_SIZE_VGPU_DEFAULT` | `581 << 20` | vGPU heap; documented, not modeled for baremetal. |
| `GSP_FW_HEAP_SIZE_OVERRIDE_LIBOS3_BAREMETAL_MIN/MAX_MB` | `88` / `280` (MiB) | Allowed heap window for LIBOS3 baremetal. Model enforces `wpr_heap` within `[88MiB,280MiB]` when checking full-heap sizing (overflow-safe). |
| `GSP_FW_HEAP_SIZE_OVERRIDE_LIBOS2_MIN/MAX_MB` | `64` / `256` | LIBOS2 window; documented for completeness. |
| `GSP_MSG_QUEUE_ELEMENT_SIZE_MAX` | via `bindings::GSP_MSG_QUEUE_ELEMENT_SIZE_MAX` (Nova `fw.rs`); queue maths `MIN=GSP_PAGE_SIZE, MAX=16*PAGE` (`r535/rpc.c`) | Referenced; queue itself modeled in M9, not duplicated. |
| `AMPERE_CHANNEL_GPFIFO_A` | `0x0000c56f` | `nvif/class.h:89`. Channel class for GA106. |
| `NVA06F_CTRL_CMD_BIND` | `0xa06f0104` | `nvrm/fifo.h`. Payload `NVA06F_CTRL_BIND_PARAMS{engineType}`. |
| `NVA06F_CTRL_CMD_GPFIFO_SCHEDULE` | `0xa06f0103` | Payload `{bEnable[,bSkipSubmit]}`; P77 uses `bEnable` only (proven field). |
| `NV2080_CTRL_CMD_CE_GET_FAULT_METHOD_BUFFER_SIZE` | `0x20802a08` | Returns `{size}` → `mthdbuf_size`. |
| `NV2080_CTRL_CMD_GPU_PROMOTE_CTX` | `0x2080012b` | `{engineType,hClient,ChID,hChanClient,hObject,hVirtMemory,virtAddress,size,...}` (Falcon ctx bind; documented, not in M10 model scope). |
| `CHID_PER_USERD` | `8` | `r535/fifo.c:72`, `r570/fifo.c:17`. |
| `RAMFC_SIZE` | `0x200` | `r535_chan_alloc`: `ramfcMem.size=0x200`, base aliases instance. |
| `GPFIFO_ENTRY_SIZE` | `8` | `gpFifoEntries=len/8`. |
| `TU102_RUNL` (direct corroboration) | `runqs=2, size=16` | `tu102.c`. GSP path does not expose these; model runq range `{0,1}` derives from here + `GROUP_CHANNEL_RUNQUEUE` single-bit flag. |

Formulas modeled (overflow-checked, 1 MiB-aligned):
- `client_alloc_size() = align_up(96MiB, 1MiB) = 96MiB`.
- `management_overhead(fb) = align_up(ceil(fb/1GiB)*96KiB, 1MiB)`.
- `wpr_heap(libos,chipset,fb) = carveout(libos) + base_rm(TU10X=8MiB) + client_alloc(96MiB) + overhead(fb)`,
  with `carveout(LIBOS3_BAREMETAL)=22MiB` for GA106; result checked against `[88MiB,280MiB]`.
