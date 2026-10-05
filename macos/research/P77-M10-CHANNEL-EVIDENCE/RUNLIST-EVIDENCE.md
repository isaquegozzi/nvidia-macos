# P77 — Runlist Evidence (Ampere GA106: GSP-abstracted)

## Verdict: FORMAT NOT PROVEN for GSP path (by design); MANAGEMENT semantics proven

On GA106 with GSP-RM, the host does NOT program runlist entries. The runlist is
firmware-managed inside GSP-RM. The host-visible runlist semantics reduce to:
chid allocation, runqueue selection, USERD slot math, schedule enable via RM control,
and no-op block/allow. Direct runlist MMIO exists only on the non-GSP path and is
documented here as corroboration, NOT as executable GA106-GSP behavior.

## 1. GSP abstraction (authoritative — this is the GA106 path)

- `torvalds/linux`, `r535/fifo.c`:
  `static const struct nvkm_runl_func r535_runl = { .block = r535_runl_block, .allow = r535_runl_allow };`
  with both functions EMPTY (`{}`).
- `r535_fifo_new`: `rm->runl = &r535_runl; rm->runl_ctor = r535_fifo_runl_ctor;`
  No `size`, no `runqs`, no `commit`, no `update`, no `insert_cgrp/chan`, no `pending/wait`.
  Contrast `nvkm_runl_func` in `runl.h` which HAS all those hooks for the direct path.
- Therefore: for the exact configuration P77 targets (GA106 + GSP-RM, `ga102_fifo_new`
  GSP branch), there is NO host runlist-entry format to vendor — GSP-RM owns it.
  Any host-only model claiming runlist entry bytes for this path would be fabrication.

## 2. Direct-path runlist (corroboration only — NOT modeled as executable)

- `tu102.c`, `tu102_runl = { .runqs=2, .size=16, .update=nv50_runl_update,`
  `.insert_cgrp=gv100_runl_insert_cgrp, .insert_chan=gv100_runl_insert_chan,`
  `.commit=tu102_runl_commit, .wait=nv50_runl_wait, .pending=tu102_runl_pending, ... }`.
- `tu102_runl_commit(runl,memory,start,count)`: `addr=memory+start;`
  `wr32(0x002b00+id*0x10, lo(addr)); wr32(0x002b04+id*0x10, hi(addr)); wr32(0x002b08+id*0x10, count);`
- `tu102_runl_pending`: `rd32(0x002b0c+id*0x10) & 0x8000`.
- `runl.h`: runlist object `{id, addr, chan, doorbell, cgid/chid, cgrp_nr/chan_nr, changed, mem/offset}`.
- `ENGINE_INFO_TYPE_*` (`nvrm/fifo.h`): Ampere+ entries `RUNLIST_PRI_BASE`,
  `RUNLIST_ENGINE_ID`, `CHRAM_PRI_BASE`, `IS_HOST_DRIVEN_ENGINE` prove per-engine
  runlist bases exist in HW, but on the GSP path they are programmed by firmware.
- `nvos.h`, `NV_SWRUNLIST_ALLOCATION_PARAMS{engineId,maxTSGs,qosIntrEnableMask}` with
  sizing `2*maxTSGs*maxChannelPerTSG*sizeof(RunlistEntry)` — this is the SW-runlist
  (Esched/SW) path, a different allocation from GSP-RM channel scheduling; noted for
  completeness, not conflated.

## 3. What the host DOES control (modeled)

| Concern | Provenance | Rule |
|---|---|---|
| Slot | chid allocation (`chan.c` chid, `r535` chid param, heap 2048-channel sizing) | chid unique per FIFO; double-alloc forbidden; free+reuse allowed after `FREE` |
| Range | `runqs=2` (tu102 direct) + `GROUP_CHANNEL_RUNQUEUE` flag (r535) + USERD `chid/8`,`chid%8` | runq in `{0,1}`; USERD page/index derived; out-of-range runq rejected |
| Channel↔runlist | `chan→cgrp→runl→fifo` hierarchy (`chan.h/runl.h/chan.c` insert/remove) + engine-per-runl (`runl_add`) | channel bound to exactly one runlist/engine; removal updates `cgrp_nr/chan_nr/changed` |
| Engine/runlist selection | `r535_fifo_xlat` + `engineType` passthrough + `NVA06F BIND{engineType}` | unknown engine → `-EINVAL`; bind requires allocated channel |
| Update/commit | GSP path: `NVA06F GPFIFO_SCHEDULE{bEnable=1}` after BIND; direct `commit` NOT used | schedule requires bind; `bEnable=0` (unschedule) only from scheduled state |
| Error | `RC_TRIGGERED` event → `r535_fifo_rc_chid` → `nvkm_chan_error` (pinned, `errored` counter, notify) | errored channel refuses schedule/bind until free |
| Division | `ga102_fifo_new` GSP branch + empty `r535_runl` block/allow | host never writes `0x002b00`-class runlist regs on GA106-GSP; model asserts no MMIO |

## 4. Explicit non-goals (not fabricated)

- Runlist ENTRY byte layout for GSP-RM (firmware-private; no public header).
- `maxChannelPerTSG`, `sizeof(RunlistEntry)` numeric values (SW-runlist sizing inputs;
  not pinned for GA106-GSP).
- Runlist MMIO addresses for GA106-GSP (`0x002b00` family is TU102-direct corroboration).
