# P79 Command-Submission — Source Manifest (pinned upstream evidence)

Author: Muse, 2026-09-09 -0300. Read-only research; zero live HW; candidate 1.7.3 frozen.
Method: GitHub REST contents API base64 (`engine/fifo/chan.h` this round) + byte-identical
local copies of P77-vendored files (same bytes, same git blob SHAs — re-verified by shasum).
GitHub API rate limit hit mid-round (60/60); no new fetches after `chan.h`. Nothing needed
beyond the set below was invented to fill gaps — see OPEN-QUESTIONS.md.
torvalds HEAD at round start: `893e1178` (2026-09-08). No blog/post as layout authority.

## A. torvalds/linux (Nouveau, master)

| File | Blob SHA (git) | Size | Local SHA-256 | Symbols / why |
|---|---|---|---|---|
| `nvkm/engine/fifo/chan.h` | `445db5dfd1e42c2e92a0817f06eeb05cc45db698` | 2526 | `d430ec1f…cd5c` | `nvkm_chan_func` (inst/userd/ramfc/bind/unbind/start/stop/preempt/doorbell_handle), `nvkm_cctx`, chan lifecycle API |
| `nvkm/engine/fifo/chan.c` (P77 bytes) | `418a8918bcb8fd7395f9ef7f0b0c22ea376ee308` | 12038 | `3bf4da22…90df6` | `nvkm_chan_new_` validation, insert/remove/block/error/preempt, inst 4K alloc, userd bounds, ramfc->write dispatch |
| `nvkm/engine/fifo/ga100.c` (P77 bytes) | `6848a56f20c076770cc3624499107f637733b68b` | 16609 | `b505d7e1…d300` | `ga100_chan_ramfc_write` (GPFIFO offset→0x048/0x04c, `limit2=ilog2(length/8)`), doorbell `(runl<<16)\|chid`, start/stop/unbind |
| `nvkm/subdev/gsp/rm/r535/fifo.c` (P77 bytes) | `76ee938efea3ef3f0735787baa5fb6485aab1d85` | 17800 | `b0aff084…fcc55f` | `r535_chan_alloc` (`gpFifoOffset`, `gpFifoEntries=len/8`), BIND+SCHEDULE order, `r535_runl` no-op, doorbell delegate |
| `nvkm/subdev/gsp/rm/r535/nvrm/rpcfn.h` (P77 bytes) | `2a037acc6b1e4c193f5af1f466244b43aa7be432` | 11259 | `844ba52f…a29b39` | `GSP_RM_ALLOC=103`, `CTRL_GPFIFO_GET_WORK_SUBMIT_TOKEN=186`, `CTRL_GPFIFO_SET_WORK_SUBMIT_TOKEN_NOTIF_INDEX=187` (GSP boundary) |
| `nvkm/subdev/gsp/rm/r535/nvrm/msgfn.h` (P77 bytes) | `642c13aec32544b53dad76ba61d1a98689d5f075` | 2391 | `3b569a69…91137` | `SEMAPHORE_SCHEDULE_CALLBACK=0x100b`, `TIMED_SEMAPHORE_RELEASE=0x1018` (firmware semaphore events) |
| `include/nvif/class.h` (P77 bytes) | (rate-limited; local `accf5976…95478d`) | 18931 | `accf5976…95478d` | `AMPERE_CHANNEL_GPFIFO_A=0x0000c56f` + generation chain |

## B. NVIDIA/open-gpu-kernel-modules (main)

| File | Blob SHA | Size | Local SHA-256 | Symbols / why |
|---|---|---|---|---|
| `src/common/sdk/nvidia/inc/class/clc56f.h` (P77 bytes) | `bc8b675c5dff20d410537e2c5e6274562c55c7b8` | 25985 | `8b7a6d6d…3f658e` | `Nvc56fControl` (Put/Get/Reference/PutHi/TopLevelGet/GPGet/GPPut), GPFIFO entry format (8B, FETCH/GET/OPERAND/LENGTH/LEVEL/SYNC/OPCODE), DMA method formats (INC/NONINC/IMMD/ONE_INC/RESERVED6/END_PB_SEGMENT), SEMAPHORE A-D + SEM_EXECUTE, SET_REFERENCE, WFI, 8 subchannels, SET_OBJECT/NOP/ILLEGAL |
| `src/common/sdk/nvidia/inc/class/cla06f.h` (P77 bytes) | `336cab57f70137e75dd16fa2aff1e76c8b9457a7` | 15198 | `8dd36f72…fa51` | `NVA06F_DMA_*` same method family (Kepler-line corroboration of encoding stability) |

## C. Prior-round carry-forwards (referenced, not copied)

- `P77-M10-CHANNEL-EVIDENCE/` — channel alloc (class 0xc56f, GSP_RM_ALLOC=103, BIND+SCHEDULE),
  FIFO lifetime, doorbell, runlist abstraction. P79 integrates the P77 *model*.
- `P78-VM-MMU-EVIDENCE/` — VA space, PTE/aperture/permissions, mapping lifecycle with
  flush-token ordering. Pushbuffer residency/lifetime reuses the P78 *model*.
- `SHADOW-P76-NEXT/FUTURE-RUNBOOKS.md` — live-gating runbooks (P79 stays offline per RB1).

## D. Retrieval discipline

- Network: read-only GitHub contents API GETs only (this round: 1 success + rate-limit).
- `main`/`master` mutability mitigated by git blob SHA + byte size + local SHA-256.
- One 0-byte failed-fetch placeholder (`uchan.c`) was created by a rate-limited call and
  removed (verified empty before removal); it is NOT part of the evidence set.
